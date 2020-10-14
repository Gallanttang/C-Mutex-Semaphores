#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

#define MAX_OCCUPANCY      3
#define NUM_ITERATIONS     100
#define NUM_PEOPLE         100
#define FAIR_WAITING_COUNT 4

/**
 * You might find these declarations useful.
 */
enum Endianness {LITTLE = 0, BIG = 1};
const static enum Endianness oppositeEnd [] = {BIG, LITTLE};
uthread_mutex_t gatekeeper;

struct Well {
    int numUser;
    enum Endianness endian;
    uthread_cond_t cond[2];
    int numWaiting[2];
    int time;
};

struct Well* createWell() {
    struct Well* well = malloc (sizeof (struct Well));
    well->numUser = 0;
    well->endian = 0;
    well->cond[LITTLE] = uthread_cond_create(gatekeeper);
    well->cond[BIG] = uthread_cond_create(gatekeeper);
    well->numWaiting[LITTLE] = 0;
    well->numWaiting[BIG] = 0;
    well->time = 0;
    return well;
}

struct Well* Well;

#define WAITING_HISTOGRAM_SIZE (NUM_ITERATIONS * NUM_PEOPLE)
int             entryTicker;                                          // incremented with each entry
int             waitingHistogram         [WAITING_HISTOGRAM_SIZE];
int             waitingHistogramOverflow;
uthread_mutex_t waitingHistogrammutex;
int             occupancyHistogram       [2] [MAX_OCCUPANCY + 1];

void recordWaitingTime (int waitingTime) {
    uthread_mutex_lock (waitingHistogrammutex);
    if (waitingTime < WAITING_HISTOGRAM_SIZE)
        waitingHistogram [waitingTime] ++;
    else
        waitingHistogramOverflow ++;
    uthread_mutex_unlock (waitingHistogrammutex);
}

void leaveWell() {
    uthread_mutex_lock(gatekeeper);
    Well->numUser -= 1;
    //    printf("Leaving Number of users now = %d\n", Well->numUser);
    //    printf("Leaving Num big people in queue %d\n", Well->numWaiting[BIG]);
    //    printf("Leaving Num lit people in queue %d\n", Well->numWaiting[LITTLE]);
    //    printf("Leaving Well Endianness %d\n", Well->endian);
    //    printf("Leaving time = %d\n", Well->time);
    
    if(Well->time < FAIR_WAITING_COUNT || Well->numWaiting[oppositeEnd[Well->endian]] == 0){
        uthread_cond_signal(Well->cond[Well->endian]);
        if(Well->numWaiting[oppositeEnd[Well->endian]] == 0)
            Well->time = 0;
    }
    
    if(Well->numUser == 0 && Well->numWaiting[oppositeEnd[Well->endian]] != 0) {
        Well->endian = oppositeEnd[Well->endian];
        for(int i = Well->numUser; i < MAX_OCCUPANCY; i++)
            uthread_cond_signal(Well->cond[Well->endian]);
        Well->time = 0;
    }
    uthread_mutex_unlock(gatekeeper);
}

void* enterWell (enum Endianness e) {
    uthread_mutex_lock(gatekeeper);
//    printf("Joining a %d\n", (int) e);
//    printf("Joining time %d\n", Well->time);
    while((Well->endian != e
           || Well->numUser >= MAX_OCCUPANCY
           || (Well->time >= FAIR_WAITING_COUNT && Well->endian != e))
           && Well->numUser != 0){
        Well->numWaiting[e] += 1;
        uthread_cond_wait(Well->cond[e]);
        Well->numWaiting[e] -= 1;
    }
    
    if(Well->endian == e)
        Well->time += 1;
    else
        Well->time = 0;
    
    Well->endian = e;
    Well->numUser += 1;
    assert(Well->numUser <= 3);
    occupancyHistogram[e][Well->numUser] += 1;
    entryTicker += 1;
    uthread_mutex_unlock(gatekeeper);
    return NULL;
}

void* person(void* n){
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        uthread_mutex_lock(gatekeeper);
        int b = entryTicker;
        uthread_mutex_unlock(gatekeeper);
        
        enterWell(((random() * 2 + random()) % 2));
        
        uthread_mutex_lock(gatekeeper);
        int e = entryTicker;
        uthread_mutex_unlock(gatekeeper);
        
        recordWaitingTime(e-b);
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            uthread_yield();
        }
        leaveWell();
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            uthread_yield();
        }
        
    }
    return NULL;
}

int main (int argc, char** argv) {
    entryTicker = 0;
    uthread_init (1);
    gatekeeper = uthread_mutex_create();
    uthread_t pt[NUM_PEOPLE];
    waitingHistogrammutex = uthread_mutex_create ();
    Well = createWell();
    for(int i = 0; i < NUM_PEOPLE; i++){
        pt[i] = uthread_create(person, NULL);
    }
    
    for(int i = 0; i < NUM_PEOPLE; i++){
        uthread_join(pt[i], NULL);
    }
    
    printf ("Times with 1 little endian %d\n", occupancyHistogram [LITTLE]   [1]);
    printf ("Times with 2 little endian %d\n", occupancyHistogram [LITTLE]   [2]);
    printf ("Times with 3 little endian %d\n", occupancyHistogram [LITTLE]   [3]);
    printf ("Times with 1 big endian    %d\n", occupancyHistogram [BIG] [1]);
    printf ("Times with 2 big endian    %d\n", occupancyHistogram [BIG] [2]);
    printf ("Times with 3 big endian    %d\n", occupancyHistogram [BIG] [3]);
    printf ("Waiting Histogram\n");
    for (int i=0; i<WAITING_HISTOGRAM_SIZE; i++)
        if (waitingHistogram [i])
            printf ("  Number of times people waited for %d %s to enter: %d\n", i, i==1?"person":"people", waitingHistogram [i]);
    if (waitingHistogramOverflow)
        printf ("  Number of times people waited more than %d entries: %d\n", WAITING_HISTOGRAM_SIZE, waitingHistogramOverflow);
}

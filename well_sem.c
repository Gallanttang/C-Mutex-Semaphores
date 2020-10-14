#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_sem.h"

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
uthread_sem_t gatekeeper;

struct Well {
    int numUser;
    enum Endianness endian;
    uthread_sem_t cond[2];
    uthread_sem_t queue;
    int numWaiting[2];
    int time;
};

struct Well* createWell() {
    struct Well* well = malloc (sizeof (struct Well));
    well->numUser = 0;
    well->endian = 0;
    well->cond[0] = uthread_sem_create(0);
    well->cond[1] = uthread_sem_create(0);
    well->queue = uthread_sem_create(MAX_OCCUPANCY);
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
uthread_sem_t   waitingHistogramMutex;
int             occupancyHistogram       [2] [MAX_OCCUPANCY + 1];

void recordWaitingTime (int waitingTime) {
    uthread_sem_wait (waitingHistogramMutex);
    if (waitingTime < WAITING_HISTOGRAM_SIZE)
        waitingHistogram [waitingTime] ++;
    else
        waitingHistogramOverflow ++;
    uthread_sem_signal (waitingHistogramMutex);
}

void leaveWell() {
    uthread_sem_wait(gatekeeper);
    Well->numUser -= 1;
    uthread_sem_signal(gatekeeper);
//    printf("Leaving Number of users now = %d\n", Well->numUser); fflush(stdout);
//    printf("Leaving endian = %d\n", Well->endian); fflush(stdout);
//    printf("Leaving Number of lit waiting = %d\n", Well->numWaiting[LITTLE]); fflush(stdout);
//    printf("Leaving Number of big waiting = %d\n", Well->numWaiting[BIG]); fflush(stdout);
//    printf("Leaving time = %d\n", Well->time); fflush(stdout);
    uthread_sem_wait(gatekeeper);
    
    if (Well->numWaiting[oppositeEnd[Well->endian]] == 0)
        Well->time = 0;
    
    if(Well->time < FAIR_WAITING_COUNT){
        uthread_sem_signal(Well->queue);
        uthread_sem_signal(Well->cond[Well->endian]);
    } else if (Well->numUser == 0
               && Well->numWaiting[oppositeEnd[Well->endian]] != 0
               && Well->time >= FAIR_WAITING_COUNT) {
        Well->endian = oppositeEnd[Well->endian];
        Well->time = 0;
        for(int i = 0; i < MAX_OCCUPANCY && i < Well->numWaiting[Well->endian]; i++){
            uthread_sem_signal(Well->cond[Well->endian]);
            uthread_sem_signal(Well->queue);
        }
    }
    uthread_sem_signal(gatekeeper);
}

void* enterWell (enum Endianness e) {
    uthread_sem_wait(gatekeeper);
    if((Well->endian == e && Well->time < FAIR_WAITING_COUNT)
       || (Well->endian == e && Well->numWaiting[oppositeEnd[e]] == 0)){
        uthread_sem_signal(gatekeeper);
        uthread_sem_wait(Well->queue);
        uthread_sem_wait(gatekeeper);
    } else {
        Well->numWaiting[e] += 1;
        uthread_sem_signal(gatekeeper);
        uthread_sem_wait(Well->cond[e]);
        uthread_sem_wait(Well->queue);
        uthread_sem_wait(gatekeeper);
        Well->numWaiting[e] -= 1;
    }
    uthread_sem_signal(gatekeeper);
    
    
    uthread_sem_wait(gatekeeper);
    if(Well->endian == e)
        Well->time += 1;
    else
        Well->time = 0;
    
    Well->endian = e;
    Well->numUser += 1;
//    printf("Num Users when trying to enter: %d\n", Well->numUser);
    assert(Well->numUser <= MAX_OCCUPANCY);
    entryTicker += 1;
    occupancyHistogram[e][Well->numUser] += 1;
    uthread_sem_signal(gatekeeper);
    
    
    return NULL;
}

void* person(void* n){
    for(int i = 0; i < NUM_ITERATIONS; i++){
        uthread_sem_wait(gatekeeper);
        int attempt = entryTicker;
        uthread_sem_signal(gatekeeper);
        
        enterWell((random() * 2 + random()) % 2);
        
        uthread_sem_wait(gatekeeper);
        int enter = entryTicker;
        uthread_sem_signal(gatekeeper);
        
        recordWaitingTime(enter-attempt);
        
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



//
// TODO
// You will probably need to create some additional produres etc.
//

int main (int argc, char** argv) {
    uthread_init (1);
    gatekeeper = uthread_sem_create(1);
    Well = createWell();
    uthread_t pt [NUM_PEOPLE];
    waitingHistogramMutex = uthread_sem_create (1);
    
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

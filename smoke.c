#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

struct Agent {
    uthread_mutex_t mutex;
    uthread_cond_t  match;
    uthread_cond_t  paper;
    uthread_cond_t  tobacco;
    uthread_cond_t  smoke;
};

struct Agent* createAgent() {
    struct Agent* agent = malloc (sizeof (struct Agent));
    agent->mutex   = uthread_mutex_create();
    agent->paper   = uthread_cond_create (agent->mutex);
    agent->match   = uthread_cond_create (agent->mutex);
    agent->tobacco = uthread_cond_create (agent->mutex);
    agent->smoke   = uthread_cond_create (agent->mutex);
    return agent;
}

//
// TODO
// You will probably need to add some procedures and struct etc.
//
int tracker = 0;

/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
    struct Agent* a = av;
    static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
    static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};
    
    uthread_mutex_lock (a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int r = random() % 3;
        signal_count [matching_smoker [r]] ++;
        int c = choices [r];
        if (c & MATCH) {
            VERBOSE_PRINT ("match available\n");
//            printf("giving out match\n");
            uthread_cond_signal (a->match);
        }
        if (c & PAPER) {
            VERBOSE_PRINT ("paper available\n");
//            printf("giving out paper\n");
            uthread_cond_signal (a->paper);
        }
        if (c & TOBACCO) {
            VERBOSE_PRINT ("tobacco available\n");
//            printf("giving out tobacco\n");
            uthread_cond_signal (a->tobacco);
        }
        VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
        uthread_cond_wait (a->smoke);
//        printf("has smoked\n");
    }
    uthread_mutex_unlock (a->mutex);
    return NULL;
}

struct Smoker {
    int pap;
    int mat;
    int tob;
    uthread_cond_t canSmoke;
    uthread_cond_t MP;
    uthread_cond_t MT;
    uthread_cond_t TP;
    struct Agent* agent;
};

void* createSmoker(struct Agent* a){
    struct Smoker* mon = malloc(sizeof(struct Smoker));
    mon->pap = 0;
    mon->mat = 0;
    mon->tob = 0;
    mon->canSmoke = uthread_cond_create(a->mutex);
    mon->MP = uthread_cond_create(a->mutex);
    mon->MT = uthread_cond_create(a->mutex);
    mon->TP = uthread_cond_create(a->mutex);
    mon->agent = a;
    return mon;
}

void* TP(void* smoker){
    struct Smoker* s = smoker;
    uthread_mutex_lock(s->agent->mutex);
    tracker += 1;
    while(1){
//        printf("waiting for match\n");
        uthread_cond_wait(s->TP);
        smoke_count[1] += 1;
        //printf("number of match: %d\n", s->mat);
        uthread_cond_signal(s->canSmoke);
    }
    uthread_mutex_unlock(s->agent->mutex);
    return NULL;
}

void* MT(void* smoker){
    struct Smoker* s = smoker;
    uthread_mutex_lock(s->agent->mutex);
    tracker += 1;
    while(1){
//        printf("waiting for paper\n");
        uthread_cond_wait(s->MT);
        smoke_count[2] += 1;
        //printf("number of paper: %d\n", s->pap);
        uthread_cond_signal(s->canSmoke);
    }
    uthread_mutex_unlock(s->agent->mutex);
    return NULL;
}

void* MP(void* smoker){
    
    struct Smoker* s = smoker;
    uthread_mutex_lock(s->agent->mutex);
    tracker += 1;
    while(1){
//        printf("waiting for tobacco\n");
        uthread_cond_wait(s->MP);
        smoke_count[4] += 1;
        //printf("number of tobacco: %d\n", s->tob);
        uthread_cond_signal(s->canSmoke);
    }
    uthread_mutex_unlock(s->agent->mutex);
    return NULL;
}

void* trySmokeMatch(void* smoker){
    struct Smoker* s = smoker;
    uthread_mutex_lock(s->agent->mutex);
    tracker += 1;
    while(1){
        uthread_cond_wait(s->agent->match);
        s->mat = 1;
//        if(s->pap == 1 && s->tob == 1){
//            //            printf("try smoke for match\n");
//            //            printf("in try smoke for match\n");
//            s->pap = 0;
//            s->tob = 0;
//            uthread_cond_signal(s->TP);
//            uthread_cond_wait(s->canSmoke);
//            uthread_cond_signal(s->agent->smoke);
//        }
        if (s->mat == 1 && s->tob == 1){
            //            printf("try smoke for paper\n");
            //            printf("in try smoke for paper\n");
            s->mat = 0;
            s->tob = 0;
            uthread_cond_signal(s->MT);
            uthread_cond_wait(s->canSmoke);
            uthread_cond_signal(s->agent->smoke);
        }
        if(s->mat == 1 && s->pap == 1){
            //            printf("try smoke for tobacco\n");
            //            printf("in try smoke for tobacco\n");
            s->mat = 0;
            s->pap = 0;
            uthread_cond_signal(s->MP);
            uthread_cond_wait(s->canSmoke);
            uthread_cond_signal(s->agent->smoke);
        }
    }
    uthread_mutex_unlock(s->agent->mutex);
    return NULL;
}

void* trySmokePaper(void* smoker){
    struct Smoker* s = smoker;
    uthread_mutex_lock(s->agent->mutex);
    tracker += 1;
    while(1){
        uthread_cond_wait(s->agent->paper);
        s->pap = 1;
        if(s->pap == 1 && s->tob == 1){
//            printf("try smoke for match\n");
//            printf("in try smoke for match\n");
            s->pap = 0;
            s->tob = 0;
            uthread_cond_signal(s->TP);
            uthread_cond_wait(s->canSmoke);
            uthread_cond_signal(s->agent->smoke);
        }
//        if (s->mat == 1 && s->tob == 1){
//            //            printf("try smoke for paper\n");
//            //            printf("in try smoke for paper\n");
//            s->mat = 0;
//            s->tob = 0;
//            uthread_cond_signal(s->MT);
//            uthread_cond_wait(s->canSmoke);
//            uthread_cond_signal(s->agent->smoke);
//        }
        if(s->mat == 1 && s->pap == 1){
//            printf("try smoke for tobacco\n");
//            printf("in try smoke for tobacco\n");
            s->mat = 0;
            s->pap = 0;
            uthread_cond_signal(s->MP);
            uthread_cond_wait(s->canSmoke);
            uthread_cond_signal(s->agent->smoke);
        }
    }
    uthread_mutex_unlock(s->agent->mutex);
    return NULL;
}

void* trySmokeTobacco(void* smoker){
    struct Smoker* s = smoker;
    uthread_mutex_lock(s->agent->mutex);
    tracker += 1;
    while(1){
        uthread_cond_wait(s->agent->tobacco);
        s->tob = 1;
        if(s->pap == 1 && s->tob == 1){
            //            printf("try smoke for match\n");
            //            printf("in try smoke for match\n");
            s->pap = 0;
            s->tob = 0;
            uthread_cond_signal(s->TP);
            uthread_cond_wait(s->canSmoke);
            uthread_cond_signal(s->agent->smoke);
        }
        if (s->mat == 1 && s->tob == 1){
            //            printf("try smoke for paper\n");
            //            printf("in try smoke for paper\n");
            s->mat = 0;
            s->tob = 0;
            uthread_cond_signal(s->MT);
            uthread_cond_wait(s->canSmoke);
            uthread_cond_signal(s->agent->smoke);
        }
//        if(s->mat == 1 && s->pap == 1){
//            //            printf("try smoke for tobacco\n");
//            //            printf("in try smoke for tobacco\n");
//            s->mat = 0;
//            s->pap = 0;
//            uthread_cond_signal(s->MP);
//            uthread_cond_wait(s->canSmoke);
//            uthread_cond_signal(s->agent->smoke);
//        }
    }
    uthread_mutex_unlock(s->agent->mutex);
    return NULL;
}

void randomStall() {
    int i, r = random() >> 16;
    while (i++<r);
}

int main (int argc, char** argv) {
    uthread_init (7);
    struct Agent*  a = createAgent();
    struct Smoker* s = createSmoker(a);
    
    // resource monitors
    uthread_t tryt = uthread_create (trySmokeTobacco, s);
    uthread_t tryp = uthread_create (trySmokePaper, s);
    uthread_t trym = uthread_create (trySmokeMatch, s);
    
    // Smokers
    uthread_t papers = uthread_create (MT, s);
    uthread_t matches = uthread_create (TP, s);
    uthread_t tobaccos = uthread_create (MP, s);
    
    while(tracker != 6);
    uthread_join (uthread_create (agent, a), 0);
    
    assert (signal_count [MATCH]   == smoke_count [MATCH]);
    assert (signal_count [PAPER]   == smoke_count [PAPER]);
    assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
    assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
    printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
            smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}

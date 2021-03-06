#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

#define MAX_OCCUPANCY      3
#define NUM_ITERATIONS     100
#define NUM_PEOPLE         20
#define FAIR_WAITING_COUNT 4

/**
 * You might find these declarations useful.
 */
enum Endianness {LITTLE = 0, BIG = 1};
const static enum Endianness oppositeEnd [] = {BIG, LITTLE};

struct Well {
  uthread_mutex_t mutex;
  uthread_cond_t big;
  uthread_cond_t little;
  int count;
  int entered;
  enum Endianness endianness;
};

struct Well* createWell() {
  struct Well* Well = malloc (sizeof (struct Well));
  Well->mutex = uthread_mutex_create();
  Well->big = uthread_cond_create(Well->mutex);
  Well->little = uthread_cond_create(Well->mutex);
  int count = 0;
  int entered = 0;
  enum Endianness endianness = 0;
  return Well;
}

struct Well* Well;

#define WAITING_HISTOGRAM_SIZE (NUM_ITERATIONS * NUM_PEOPLE)
int             entryTicker;                                          // incremented with each entry
int             waitingHistogram         [WAITING_HISTOGRAM_SIZE];
int             waitingHistogramOverflow;
uthread_mutex_t waitingHistogrammutex;
int             occupancyHistogram       [2] [MAX_OCCUPANCY + 1];

void signalendianness(struct Well *w, enum Endianness endianness, int n){
  for (int i = 0; i < n; i++){
    if (endianness == BIG){
      uthread_cond_signal(w->big);
    }
    else if (endianness == LITTLE){
      uthread_cond_signal(w->little);
    }
  }
}

void waitendianness(struct Well *w, enum Endianness endianness){
  if (endianness == BIG){
    uthread_cond_wait(w->big);
  }
  else if (endianness == LITTLE){
    uthread_cond_wait(w->little);
  }
}

void enterWell (enum Endianness g) {
  uthread_mutex_lock(Well->mutex);
  if (Well->count == 0){
    Well->endianness = g;
  }else{
    int c = Well->entered;
    waitendianness(Well, g);
    int waitingTime = Well->entered - c;
    if (waitingTime < WAITING_HISTOGRAM_SIZE){
      waitingHistogram[waitingTime]++;
    }
    else{
      waitingHistogramOverflow++;
    }
  }
  Well->entered++;
  Well->count++;
  occupancyHistogram[g][Well->count]++;
  uthread_mutex_unlock(Well->mutex);
}

void leaveWell() {
  uthread_mutex_lock(Well->mutex);
  Well->count--;
  occupancyHistogram[Well->endianness][Well->count]++;
  if (Well->count == 0)
  {
    Well->endianness = oppositeEnd[Well->endianness];
    signalendianness(Well, Well->endianness, 3);
  }
  uthread_mutex_unlock(Well->mutex);
}



void recordWaitingTime (int waitingTime) {
  uthread_mutex_lock (waitingHistogrammutex);
  if (waitingTime < WAITING_HISTOGRAM_SIZE)
    waitingHistogram [waitingTime] ++;
  else
    waitingHistogramOverflow ++;
  uthread_mutex_unlock (waitingHistogrammutex);
}

//
// TODO
// You will probably need to create some additional produres etc.
//
void* people(void* av){
  struct Well* w = av;
  enum Endianness endianness = random() %2;
  for (int i = 0; i < NUM_ITERATIONS; i++){
    enterWell(endianness);
    for (int i = 0; i < NUM_PEOPLE; i++){
      uthread_yield();
    }
    leaveWell();
    for (int i = 0; i < NUM_PEOPLE; i++){
      uthread_yield();
    }
  }
}

int main (int argc, char** argv) {
  uthread_init (1);
  Well = createWell();
  uthread_t pt [NUM_PEOPLE];
  waitingHistogrammutex = uthread_mutex_create ();

  // TODO
  for (int i = 0; i < NUM_PEOPLE; i++){
    pt[i] = uthread_create(people,Well);
  }
  for (int i = 0; i < NUM_PEOPLE; i++)
  {
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

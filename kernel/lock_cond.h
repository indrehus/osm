#include "kernel/sleepq.h"

typedef int lock_t;

typedef int cond_t;

/*
typedef struct {
  cond_t value;
  
} lock_t;
*/

int lock_reset(lock_t *lock);

void lock_acquire(lock_t *lock);

void lock_release(lock_t *lock);

void condition_init(cond_t *cond);

void condition_wait(cond_t *cond, lock_t *lock);

void condition_signal(cond_t *cond, lock_t *lock);

void condition_broadcast(cond_t *cond, lock_t *lock);

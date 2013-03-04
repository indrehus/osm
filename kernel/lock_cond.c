#include "kernel/lock_cond.h"
#include "kernel/sleepq.h"
#include "kernel/thread.h"
#include "kernel/interrupt.h"

#define LOCK_RESET_SUCCESS 0
#define LOCK_RESET_FAILURE -1

int lock_reset (lock_t *lock)
{
  if (*lock == 0) {
    return LOCK_RESET_FAILURE;
  }
  *lock = 1;
  return LOCK_RESET_SUCCESS;
}

void lock_acquire(lock_t *lock)
{
  interrupt_status_t state = _interrupt_disable();
  while (*lock != 1) {
    sleepq_add(lock);
    thread_switch();
  }
  *lock = 0;
  _interrupt_set_state(state);
}

void lock_release(lock_t *lock)
{
  interrupt_status_t state = _interrupt_disable();
  *lock = 1;
  sleepq_wake(lock);
  _interrupt_set_state(state);
}

void condition_init(cond_t *cond)
{
  *cond = 42;
}

void condition_wait(cond_t *cond, lock_t *lock)
{
  lock_release(lock);
  sleepq_add(cond);
  thread_switch();
  lock_acquire(lock);
}

void condition_signal(cond_t *cond, lock_t *lock)
{
  lock_acquire(lock);
  sleepq_wake(cond);
  lock_release(lock);
}

void condition_broadcast(cond_t *cond, lock_t *lock)
{
  lock_acquire(lock);
  sleepq_wake_all(cond);
  lock_release(lock);
}

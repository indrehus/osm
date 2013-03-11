#include "kernel/lock_cond.h"
#include "kernel/sleepq.h"
#include "kernel/thread.h"
#include "kernel/interrupt.h"
#include "kernel/spinlock.h"

#define LOCK_RESET_SUCCESS 0
#define LOCK_RESET_FAILURE -1
#define LOCK_OPEN 0
#define LOCK_CLOSED 1

int lock_reset (lock_t *lock)
{
  interrupt_status_t state = _interrupt_disable();
  spinlock_acquire(lock);
  if (*lock == LOCK_CLOSED) {
    spinlock_release(lock);
    _interrupt_set_state(state);
    return LOCK_RESET_FAILURE;
  }
  *lock = LOCK_OPEN;    
  spinlock_release(lock);
  _interrupt_set_state(state);
  return LOCK_RESET_SUCCESS;
}

void lock_acquire(lock_t *lock)
{
  interrupt_status_t state = _interrupt_disable();
  spinlock_acquire(lock);
  /* While lock is closed, since LOCK_CLOSED == 1 */
  while (*lock) {
    sleepq_add(lock);
    spinlock_release(lock);
    thread_switch();
    spinlock_acquire(lock);
  }
  *lock = LOCK_CLOSED;
  spinlock_release(lock);
  _interrupt_set_state(state);
}

void lock_release(lock_t *lock)
{
  interrupt_status_t state = _interrupt_disable();
  spinlock_acquire(lock);
  *lock = LOCK_OPEN;
  sleepq_wake(lock);
  spinlock_release(lock);
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
  lock = lock;
  sleepq_wake(cond);
}

void condition_broadcast(cond_t *cond, lock_t *lock)
{
  lock = lock;
  sleepq_wake_all(cond);
}

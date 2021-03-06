// Copyright 2016 Amal Cao (amalcaowei@gmail.com). All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE.txt file.

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include "futex_lock.h"
#include "coroutine.h"

void _coroc_futex_init(volatile _coroc_futex_t *lock) {
  lock->key = MUTEX_UNLOCKED;
}

void _coroc_futex_lock(volatile _coroc_futex_t *lock) {
  uint32_t i, v, wait, spin;
#if (ENABLE_DEBUG > 1)
  coroc_coroutine_t self = coroc_coroutine_self();
  uint64_t coid = (self != NULL) ? self->id : (uint64_t) - 1;
#endif

  v = TSC_XCHG(&lock->key, MUTEX_LOCKED);
  if (v == MUTEX_UNLOCKED) goto __lock_success;

  wait = v;
  spin = ACTIVE_SPIN;

  while (1) {
    for (i = 0; i < spin; i++) {
      while (lock->key == MUTEX_UNLOCKED)
        if (TSC_CAS(&lock->key, MUTEX_UNLOCKED, wait)) goto __lock_success;

      __procyield(ACTIVE_SPIN_CNT);
    }

    for (i = 0; i < PASSIVE_SPIN; i++) {
      while (lock->key == MUTEX_UNLOCKED)
        if (TSC_CAS(&lock->key, MUTEX_UNLOCKED, wait)) goto __lock_success;
      sched_yield();
    }

    // sleep
    v = TSC_XCHG(&lock->key, MUTEX_SLEEPING);
    if (v == MUTEX_UNLOCKED) goto __lock_success;
    wait = MUTEX_SLEEPING;
    _coroc_futex_sleep((uint32_t *)&lock->key, MUTEX_SLEEPING, -1);
  }

__lock_success:
#if (ENABLE_DEBUG > 1)
  lock->cookie = coid;
#endif
  return;
}

void _coroc_futex_unlock(volatile _coroc_futex_t *lock) {
  uint32_t v;

#if (ENABLE_DEBUG > 1)
  lock->cookie = (uint64_t) - 2;
#endif

  v = TSC_XCHG(&lock->key, MUTEX_UNLOCKED);
  assert(v != MUTEX_UNLOCKED);

  if (v == MUTEX_SLEEPING) _coroc_futex_wakeup((uint32_t *)&lock->key, 1);
}

int _coroc_futex_trylock(volatile _coroc_futex_t *lock) {
  if (TSC_CAS(&lock->key, MUTEX_UNLOCKED, MUTEX_LOCKED)) return 0;
  return 1;
}

void _coroc_futex_destroy(volatile _coroc_futex_t *lock) {
  // TODO
  assert(lock->key == MUTEX_UNLOCKED);
}

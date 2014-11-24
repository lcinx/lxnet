
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_OS_SOME_HAND_H_
#define _H_OS_SOME_HAND_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <assert.h>

#ifdef _USE_SPIN_LOCKS_

#define LOCK_struct		spin_struct
#define LOCK_INIT		spin_init
#define LOCK_LOCK		spin_lock
#define LOCK_UNLOCK		spin_unlock
#define LOCK_DELETE		spin_delete

#else

#define LOCK_struct		threadlock_struct
#define LOCK_INIT		threadlock_init
#define LOCK_LOCK		threadlock_lock
#define LOCK_UNLOCK		threadlock_unlock
#define LOCK_DELETE		threadlock_delete

#endif


#ifndef FORCEINLINE
  #if defined(__GNUC__)
	#define FORCEINLINE __inline __attribute__ ((always_inline))
  #elif defined(_MSC_VER)
    #define FORCEINLINE __forceinline
  #endif
#endif
/*******************************************************************/
/*							os center some    					   */
/*******************************************************************/
#ifdef WIN32

/* windows version some */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if _WIN32_WINNT < 0x0500
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include <windows.h>

static FORCEINLINE int get_cpunum ()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors;
}

#define THREAD_ID			DWORD
#define CURRENT_THREAD		GetCurrentThreadId()

#else

#include <pthread.h>
#include <unistd.h>
/*get cpu number*/
static FORCEINLINE int get_cpunum ()
{
	return sysconf(_SC_NPROCESSORS_CONF);
}

#define THREAD_ID			pthread_t
#define CURRENT_THREAD      pthread_self()
#endif

/*******************************************************************/
/*								Spin lock    					   */
/*******************************************************************/

#ifdef WIN32

#define USE_OS_SPIN
#ifdef USE_OS_SPIN

/*windows spin critical*/
#define spin_struct			CRITICAL_SECTION
#define spin_init(sl)		(!InitializeCriticalSectionAndSpinCount((sl), 4000))
#define spin_lock(sl)       (EnterCriticalSection(sl), 0)
#define spin_unlock(sl)		LeaveCriticalSection(sl)
#define spin_delete(sl)		DeleteCriticalSection(sl)
#define spin_trylock(sl)	TryEnterCriticalSection(sl)

#else

struct win32_mlock_t {
  volatile long l;
  unsigned int c;
  THREAD_ID threadid;
};

#define spin_struct         struct win32_mlock_t
#define spin_init(sl)		((sl)->threadid = 0, (sl)->l = (sl)->c = 0, 0)
#define spin_lock(sl)		win32_acquire_lock(sl)
#define spin_unlock(sl)     win32_release_lock(sl)
#define spin_delete(sl)		((void) 0)
#define spin_trylock(sl)    win32_try_lock(sl)
#define SPINS_PER_YIELD     63
static FORCEINLINE int win32_acquire_lock (spin_struct *sl) {
  int spins = 0;
  for (;;) {
    if (sl->l != 0) {
      if (sl->threadid == CURRENT_THREAD) {
        ++sl->c;
        return 0;
      }
    }
    else {
      if (!InterlockedExchange(&sl->l, 1)) {
        assert(!sl->threadid);
        sl->threadid = CURRENT_THREAD;
        sl->c = 1;
        return 0;
      }
    }
    if ((++spins & SPINS_PER_YIELD) == 0)
      SleepEx(0, FALSE);
  }
}

static FORCEINLINE void win32_release_lock (spin_struct *sl) {
  assert(sl->threadid == CURRENT_THREAD);
  assert(sl->l != 0);
  if (--sl->c == 0) {
    sl->threadid = 0;
    InterlockedExchange (&sl->l, 0);
  }
}

static FORCEINLINE int win32_try_lock (spin_struct *sl) {
  if (sl->l != 0) {
    if (sl->threadid == CURRENT_THREAD) {
      ++sl->c;
      return 1;
    }
  }
  else {
    if (!InterlockedExchange(&sl->l, 1)){
      assert(!sl->threadid);
      sl->threadid = CURRENT_THREAD;
      sl->c = 1;
      return 1;
    }
  }
  return 0;
}

#endif

#else

/* Custom pthread-style spin locks on x86 and x64 for gcc */
struct pthread_mlock_t {
  volatile unsigned int l;
  unsigned int c;
  THREAD_ID threadid;
};
#define spin_struct			struct pthread_mlock_t
#define spin_init(sl)		((sl)->threadid = 0, (sl)->l = (sl)->c = 0, 0)
#define spin_lock(sl)		pthread_acquire_lock(sl)
#define spin_unlock(sl)     pthread_release_lock(sl)
#define spin_delete(sl)		((void) 0)
#define spin_trylock(sl)    pthread_try_lock(sl)
#define SPINS_PER_YIELD     63

static FORCEINLINE int pthread_acquire_lock (spin_struct *sl) {
  int spins = 0;
  volatile unsigned int* lp = &sl->l;
  for (;;) {
    if (*lp != 0) {
      if (sl->threadid == CURRENT_THREAD) {
        ++sl->c;
        return 0;
      }
    }
    else {
      /* place args to cmpxchgl in locals to evade oddities in some gccs */
      int cmp = 0;
      int val = 1;
      int ret;
      __asm__ __volatile__  ("lock; cmpxchgl %1, %2"
                             : "=a" (ret)
                             : "r" (val), "m" (*(lp)), "0"(cmp)
                             : "memory", "cc");
      if (!ret) {
        assert(!sl->threadid);
        sl->threadid = CURRENT_THREAD;
        sl->c = 1;
        return 0;
      }
    }
    if ((++spins & SPINS_PER_YIELD) == 0) {
#if defined (__SVR4) && defined (__sun) /* solaris */
      thr_yield();
#else
#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
      sched_yield();
#else  /* no-op yield on unknown systems */
      ;
#endif /* __linux__ || __FreeBSD__ || __APPLE__ */
#endif /* solaris */
    }
  }
}

static FORCEINLINE void pthread_release_lock (spin_struct *sl) {
  volatile unsigned int* lp = &sl->l;
  assert(*lp != 0);
  assert(sl->threadid == CURRENT_THREAD);
  if (--sl->c == 0) {
    sl->threadid = 0;
    int prev = 0;
    int ret;
    __asm__ __volatile__ ("lock; xchgl %0, %1"
                          : "=r" (ret)
                          : "m" (*(lp)), "0"(prev)
                          : "memory");
  }
}

static FORCEINLINE int pthread_try_lock (spin_struct *sl) {
  volatile unsigned int* lp = &sl->l;
  if (*lp != 0) {
    if (sl->threadid == CURRENT_THREAD) {
      ++sl->c;
      return 1;
    }
  }
  else {
    int cmp = 0;
    int val = 1;
    int ret;
    __asm__ __volatile__  ("lock; cmpxchgl %1, %2"
                           : "=a" (ret)
                           : "r" (val), "m" (*(lp)), "0"(cmp)
                           : "memory", "cc");
    if (!ret) {
      assert(!sl->threadid);
      sl->threadid = CURRENT_THREAD;
      sl->c = 1;
      return 1;
    }
  }
  return 0;
}


#endif


/*******************************************************************/
/*								thread lock    					   */
/*******************************************************************/
#ifdef WIN32

/* Win32 critical sections */
#define threadlock_struct		CRITICAL_SECTION
#define threadlock_init(sl)		(InitializeCriticalSection(sl))
#define threadlock_lock(sl)     (EnterCriticalSection(sl), 0)
#define threadlock_unlock(sl)	LeaveCriticalSection(sl)
#define threadlock_delete(sl)	DeleteCriticalSection(sl)
#define threadlock_trylock(sl)	TryEnterCriticalSection(sl)

#else

/* pthreads-based locks */
#define threadlock_struct		pthread_mutex_t
#define threadlock_init(sl)     pthread_mutex_init((sl), 0)
#define threadlock_lock(sl)     pthread_mutex_lock(sl)
#define threadlock_unlock(sl)   pthread_mutex_unlock(sl)
#define threadlock_delete(sl)	pthread_mutex_destroy(sl)
#define threadlock_trylock(sl)  (!pthread_mutex_trylock(sl))

#endif

/*******************************************************************/
/*						atom hand function   					   */
/*******************************************************************/
/*
 * function atom_compare_and_swap(lock, old, new):
 * {
 *		old_value = lock;
 *		if (lock == old)
 *			lock = new;
 *		return old_value;
 * }
 *
 * function atom_fetch_add(lock, value):
 * {
 *		old_value = lock;
 *		lock += value;
 *		return old_value;
 * }
 * function atom_inc(lock)
 * {
 *		lock += 1;
 *		return lock;
 * }
 * function atom_dec(lock)
 * {
 *		lock -= 1;
 *		return lock;
 * }
 * function atom_set(lock, value)
 * {
 *		old_value = lock;
 *		lock = value;
 *		return old_value;
 * }
 * function atom_or_fetch(lock, value)
 * {
 *		newvalue = lock | value;
 *		lock = newvalue;
 *		return newvalue;
 * }
 * function atom_and_fetch(lock, value)
 * {
 *		newvalue = lock & value;
 *		lock = newvalue;
 *		return newvalue;
 * }
 * */

#ifdef WIN32

#define atom_compare_and_swap(lock, old, new) InterlockedCompareExchange(lock, new, old)
#define atom_fetch_add InterlockedExchangeAdd
#define atom_inc InterlockedIncrement
#define atom_dec InterlockedDecrement
#define atom_set InterlockedExchange

static FORCEINLINE long atom_or_fetch (volatile long *lock, long value)
{
	long old, newvalue;
	do {
		old = *lock;
		newvalue = old | value;
	} while (InterlockedCompareExchange(lock, newvalue, old) != old);

	return newvalue;
}

static FORCEINLINE long atom_and_fetch (volatile long *lock, long value)
{
	long old, newvalue;
	do {
		old = *lock;
		newvalue = old & value;
	} while (InterlockedCompareExchange(lock, newvalue, old) != old);

	return newvalue;
}

#else

#define atom_compare_and_swap __sync_val_compare_and_swap
#define atom_fetch_add __sync_fetch_and_add
#define atom_inc(lock) __sync_add_and_fetch(lock, 1)
#define atom_dec(lock) __sync_add_and_fetch(lock, -1)
#define atom_set __sync_lock_test_and_set
#define atom_or_fetch __sync_or_and_fetch
#define atom_and_fetch __sync_and_and_fetch

#endif

#ifdef __cplusplus
}
#endif
#endif


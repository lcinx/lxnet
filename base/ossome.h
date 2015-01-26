
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_OS_SOME_HAND_H_
#define _H_OS_SOME_HAND_H_

#ifdef __cplusplus
extern "C" {
#endif


#ifndef FORCEINLINE
  #if defined(__GNUC__)
	#define FORCEINLINE __inline __attribute__((always_inline))
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

#include <windows.h>

static FORCEINLINE int get_cpunum() {
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
static FORCEINLINE int get_cpunum() {
	return sysconf(_SC_NPROCESSORS_CONF);
}

#define THREAD_ID			pthread_t
#define CURRENT_THREAD      pthread_self()
#endif



/*******************************************************************/
/*								thread lock    					   */
/*******************************************************************/
#ifdef WIN32

/* Win32 critical sections */
#define thread_lock_struct			CRITICAL_SECTION
#define thread_lock_init(sl)		(InitializeCriticalSection(sl))
#define thread_lock_lock(sl)		(EnterCriticalSection(sl), 0)
#define thread_lock_unlock(sl)		LeaveCriticalSection(sl)
#define thread_lock_delete(sl)		DeleteCriticalSection(sl)
#define thread_lock_trylock(sl)		TryEnterCriticalSection(sl)

#else

/* pthreads-based locks */
#define thread_lock_struct			pthread_mutex_t
#define thread_lock_init(sl)		pthread_mutex_init((sl), 0)
#define thread_lock_lock(sl)		pthread_mutex_lock(sl)
#define thread_lock_unlock(sl)		pthread_mutex_unlock(sl)
#define thread_lock_delete(sl)		pthread_mutex_destroy(sl)
#define thread_lock_trylock(sl)		(!pthread_mutex_trylock(sl))

#endif



/*******************************************************************/
/*								spin lock    					   */
/*******************************************************************/

struct _spin_struct {
	volatile long lock;
};

#define spin_lock_struct			struct _spin_struct
#define spin_lock_init(sl)			((sl)->lock = 0)
#define spin_lock_lock(sl)			x_spin_lock(sl)
#define spin_lock_unlock(sl)		x_spin_unlock(sl)
#define spin_lock_delete(sl)		((sl)->lock = 0)

#ifdef WIN32

static FORCEINLINE void x_spin_lock(struct _spin_struct *sl) {
	while (InterlockedExchange(&sl->lock, 1) == 1) {}
}

static FORCEINLINE void x_spin_unlock(struct _spin_struct *sl) {
	InterlockedExchange(&sl->lock, 0);
}

#else

static FORCEINLINE void x_spin_lock(struct _spin_struct *sl) {
	while (__sync_lock_test_and_set(&sl->lock, 1) == 1) {}
}

static FORCEINLINE void x_spin_unlock(struct _spin_struct *sl) {
	__sync_lock_release(&sl->lock);
}

#endif


/*******************************************************************/
/*						atom hand function   					   */
/*******************************************************************/
/*
 * function atom_compare_and_swap(lock, old, new) {
 *		old_value = lock;
 *		if (lock == old)
 *			lock = new;
 *		return old_value;
 * }
 *
 * function atom_fetch_add(lock, value) {
 *		old_value = lock;
 *		lock += value;
 *		return old_value;
 * }
 * function atom_inc(lock) {
 *		lock += 1;
 *		return lock;
 * }
 * function atom_dec(lock) {
 *		lock -= 1;
 *		return lock;
 * }
 * function atom_set(lock, value) {
 *		old_value = lock;
 *		lock = value;
 *		return old_value;
 * }
 * function atom_or_fetch(lock, value) {
 *		newvalue = lock | value;
 *		lock = newvalue;
 *		return newvalue;
 * }
 * function atom_and_fetch(lock, value) {
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

static FORCEINLINE long atom_or_fetch(volatile long *lock, long value) {
	long old, newvalue;
	do {
		old = *lock;
		newvalue = old | value;
	} while (InterlockedCompareExchange(lock, newvalue, old) != old);

	return newvalue;
}

static FORCEINLINE long atom_and_fetch(volatile long *lock, long value) {
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


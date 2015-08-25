
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include <stdlib.h>
#include <string.h>
#include "cthread.h"

#ifdef WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#ifdef SYS_gettid
#define gettid() syscall(SYS_gettid)
#endif

#endif


/* initialize for nil. */
const cthread g_cthread_nil_ = NULL;
const cmutex g_cmutex_nil_ = NULL;
const cspin g_cspin_nil_ = {0};
const crwspin g_crwspin_nil_ = {0, 0};



enum {
	eState_None = 0,
	eState_Run,
	eState_Exit,
};

struct cthread_ {
	void *udata;
	void (*func_run)(cthread *);

	int state;

	unsigned int thread_id;

#ifdef WIN32
	HANDLE handle;
	HANDLE event;
#else
	int signal_flag;
	pthread_t handle;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
#endif

};

#ifdef WIN32
static unsigned __stdcall
#else
static void *
#endif
thread_run_func(void *data) {
	cthread self = (cthread)data;

	self->thread_id = cthread_self_id();

	self->state = eState_Run;
	self->func_run(&self);

	self->state = eState_Exit;

#ifdef WIN32
	return 0;
#else
	return NULL;
#endif
}

int cthread_create(cthread *tid, void *udata, void (*thread_func)(cthread *)) {
	cthread self;
	if (tid == NULL)
		return -3;

	if (thread_func == NULL)
		return -2;

	self = (cthread)malloc(sizeof(*self));
	if (!self)
		return -1;

	self->udata = udata;
	self->func_run = thread_func;

	self->state = eState_None;

	self->thread_id = 0;

	/* first set. */
	*tid = self;

#ifdef WIN32
	self->event = CreateEvent(NULL, FALSE, FALSE, NULL);
	self->handle = (HANDLE)_beginthreadex(NULL, 0, thread_run_func, (void *)self, 0, NULL);
#else
	self->signal_flag = 0;

	if (pthread_cond_init(&self->cond, NULL) != 0)
		goto err_do;

	if (pthread_mutex_init(&self->mutex, NULL) != 0)
		goto err_do;

	if (pthread_create(&self->handle, 0, thread_run_func, (void *)self) != 0)
		goto err_do;

#endif

	return 0;

#ifndef WIN32
err_do:
	pthread_cond_destroy(&self->cond);
	pthread_mutex_destroy(&self->mutex);
	free(self);
	*tid = NULL;
	return 1;
#endif
}

void cthread_release(cthread *tid) {
	cthread self;
	if (*tid == NULL)
		return;

	self = *tid;

	cthread_resume(tid);

	cthread_join(tid);

#ifdef WIN32
	if (self->handle)
		CloseHandle(self->handle);

	if (self->event)
		CloseHandle(self->event);

	self->handle = NULL;
	self->event = NULL;
#else
	pthread_cond_destroy(&self->cond);
	pthread_mutex_destroy(&self->mutex);
#endif

	free(self);
	*tid = NULL;
}

void *cthread_get_udata(cthread *tid) {
	cthread self;
	if (*tid == NULL)
		return NULL;

	self = *tid;

	return self->udata;
}

unsigned int cthread_thread_id(cthread *tid) {
	cthread self;
	if (*tid == NULL)
		return 0;

	self = *tid;
	return self->thread_id;
}

void cthread_suspend(cthread *tid) {
	cthread self;
	if (*tid == NULL)
		return;

	self = *tid;

#ifdef WIN32
	WaitForSingleObject(self->event, INFINITE);
#else
	pthread_mutex_lock(&self->mutex);
	while (self->signal_flag == 0) {
		pthread_cond_wait(&self->cond, &self->mutex);
	}

	self->signal_flag = 0;
	pthread_mutex_unlock(&self->mutex);
#endif
}

void cthread_resume(cthread *tid) {
	cthread self;
	if (*tid == NULL)
		return;

	self = *tid;

#ifdef WIN32
	SetEvent(self->event);
#else
	pthread_mutex_lock(&self->mutex);
	self->signal_flag = 1;
	pthread_cond_signal(&self->cond);
	pthread_mutex_unlock(&self->mutex);
#endif
}

void cthread_join(cthread *tid) {
	cthread self;
	if (*tid == NULL)
		return;

	self = *tid;

#ifdef WIN32
	WaitForSingleObject(self->handle, INFINITE);
#else
	pthread_join(self->handle, NULL);
#endif
}


unsigned int cthread_self_id() {
#ifdef WIN32
	return (unsigned int)GetCurrentThreadId();
#else
	return (unsigned int)gettid();
#endif
}

void cthread_self_sleep(unsigned int millisecond) {
#ifdef WIN32
	Sleep(millisecond);
#else
	usleep(millisecond * 1000);
#endif
}



struct cmutex_ {

#ifdef WIN32
	CRITICAL_SECTION mutex;
#else
	pthread_mutex_t mutex;
#endif

};

int cmutex_init(cmutex *mutex) {
	cmutex self;
	if (mutex == NULL)
		return -2;

	self = (cmutex)malloc(sizeof(*self));
	if (!self)
		return -1;

#ifdef WIN32
	InitializeCriticalSection(&self->mutex);
#else
	pthread_mutex_init(&self->mutex, 0);
#endif

	*mutex = self;
	return 0;
}

void cmutex_destroy(cmutex *mutex) {
	cmutex self;
	if (*mutex == NULL)
		return;

	self = *mutex;

#ifdef WIN32
	DeleteCriticalSection(&self->mutex);
#else
	pthread_mutex_destroy(&self->mutex);
#endif

	free(self);
	*mutex = NULL;
}

void cmutex_lock(cmutex *mutex) {
	cmutex self;
	if (*mutex == NULL)
		return;

	self = *mutex;

#ifdef WIN32
	EnterCriticalSection(&self->mutex);
#else
	pthread_mutex_lock(&self->mutex);
#endif
}

void cmutex_unlock(cmutex *mutex) {
	cmutex self;
	if (*mutex == NULL)
		return;

	self = *mutex;

#ifdef WIN32
	LeaveCriticalSection(&self->mutex);
#else
	pthread_mutex_unlock(&self->mutex);
#endif
}

int cmutex_trylock(cmutex *mutex) {
	cmutex self;
	if (*mutex == NULL)
		return -2;

	self = *mutex;

#ifdef WIN32
	if (!TryEnterCriticalSection(&self->mutex))
		return -1;
#else
	if (pthread_mutex_trylock(&self->mutex) != 0)
		return -1;
#endif

	return 0;
}



#ifdef _MSC_VER
#define _long_value_test_and_set(v, value)	InterlockedExchange(v, value)
#define _long_value_release(v)				InterlockedExchange(v, 0)
#define _long_value_inc(v)					InterlockedIncrement(v)
#define _long_value_dec(v)					InterlockedDecrement(v)
#else
#define _long_value_test_and_set(v, value)	__sync_lock_test_and_set(v, value)
#define _long_value_release(v)				__sync_lock_release(v)
#define _long_value_inc(v)					__sync_add_and_fetch(v, 1)
#define _long_value_dec(v)					__sync_add_and_fetch(v, -1)
#endif

int cspin_init(cspin *lock) {
	if (!lock)
		return -2;

	*((volatile long *)&lock->lock) = 0;
	return 0;
}

void cspin_destroy(cspin *lock) {
	if (!lock)
		return;

	*((volatile long *)&lock->lock) = 0;
}

void cspin_lock(cspin *lock) {
	if (!lock)
		return;

	while (_long_value_test_and_set(&lock->lock, 1) == 1) {}
}

void cspin_unlock(cspin *lock) {
	if (!lock)
		return;

	_long_value_release(&lock->lock);
}

int cspin_trylock(cspin *lock) {
	if (!lock)
		return -2;

	if (_long_value_test_and_set(&lock->lock, 1) != 0)
		return -1;

	return 0;
}



int crwspin_init(crwspin *lock) {
	if (!lock)
		return -2;

	*((volatile long *)&lock->read) = 0;
	*((volatile long *)&lock->write) = 0;
	return 0;
}

void crwspin_destroy(crwspin *lock) {
	if (!lock)
		return;

	*((volatile long *)&lock->read) = 0;
	*((volatile long *)&lock->write) = 0;
}

void crwspin_read_lock(crwspin *lock) {
	if (!lock)
		return;

	for (;;) {
		while (*((volatile long *)&lock->write)) {}

		_long_value_inc(&lock->read);
		if (*((volatile long *)&lock->write)) {
			_long_value_dec(&lock->read);
		} else {
			break;
		}
	}
}

void crwspin_read_unlock(crwspin *lock) {
	if (!lock)
		return;

	_long_value_dec(&lock->read);
}

void crwspin_write_lock(crwspin *lock) {
	if (!lock)
		return;

	while (_long_value_test_and_set(&lock->write, 1)) {}
	while (*((volatile long *)&lock->read) != 0) {}
}

void crwspin_write_unlock(crwspin *lock) {
	if (!lock)
		return;

	_long_value_release(&lock->write);
}

int crwspin_try_read_lock(crwspin *lock) {
	if (!lock)
		return -2;

	if (*((volatile long *)&lock->write))
		return -1;

	_long_value_inc(&lock->read);
	if (*((volatile long *)&lock->write)) {
		_long_value_dec(&lock->read);
		return -1;
	}

	return 0;
}

int crwspin_try_write_lock(crwspin *lock) {
	if (!lock)
		return -2;

	if (_long_value_test_and_set(&lock->write, 1))
		return -1;

	if (*((volatile long *)&lock->read) != 0) {
		_long_value_release(&lock->write);
		return -1;
	}

	return 0;
}


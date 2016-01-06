
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_CROSS_THREAD_H_
#define _H_CROSS_THREAD_H_

#ifdef __cplusplus
extern "C" {
#endif


struct cthread_;
struct cmutex_;
struct cspin_ {
	volatile long lock;
};

/* read write lock, and is the spin lock. */
struct crwspin_ {
	volatile long read;
	volatile long write;
};

typedef struct cthread_ * cthread;
typedef struct cmutex_ * cmutex;
typedef struct cspin_ cspin;
typedef struct crwspin_ crwspin;



extern const cthread g_cthread_nil_;
extern const cmutex g_cmutex_nil_;
extern const cspin g_cspin_nil_;
extern const crwspin g_crwspin_nil_;


/* initialize for nil. */
#define cthread_nil		g_cthread_nil_
#define cmutex_nil		g_cmutex_nil_
#define cspin_nil		g_cspin_nil_
#define crwspin_nil		g_crwspin_nil_




int cthread_create(cthread *tid, void *udata, void (*thread_func)(cthread *));

void cthread_release(cthread *tid);

void *cthread_get_udata(cthread *tid);

/* 0 is error thread id. */
unsigned int cthread_thread_id(cthread *tid);

void cthread_suspend(cthread *tid);

void cthread_resume(cthread *tid);

void cthread_join(cthread *tid);


unsigned int cthread_self_id();

void cthread_self_sleep(unsigned int millisecond);



int cmutex_init(cmutex *mutex);

void cmutex_destroy(cmutex *mutex);

void cmutex_lock(cmutex *mutex);

void cmutex_unlock(cmutex *mutex);

int cmutex_trylock(cmutex *mutex);



int cspin_init(cspin *lock);

void cspin_destroy(cspin *lock);

void cspin_lock(cspin *lock);

void cspin_unlock(cspin *lock);

int cspin_trylock(cspin *lock);



int crwspin_init(crwspin *lock);

void crwspin_destroy(crwspin *lock);

void crwspin_read_lock(crwspin *lock);

void crwspin_read_unlock(crwspin *lock);

void crwspin_write_lock(crwspin *lock);

void crwspin_write_unlock(crwspin *lock);

int crwspin_try_read_lock(crwspin *lock);

int crwspin_try_write_lock(crwspin *lock);

#ifdef __cplusplus
}
#endif
#endif



/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include "catomic.h"

#ifdef _MSC_VER
#include <windows.h>
#define win_catomic_compare_set64(counter, old, set) (InterlockedCompareExchange64(counter, set, old) == old)
#endif



int64 catomic_read(catomic *atom_value) {
	return *((volatile int64 *)&atom_value->counter);
}

void catomic_set(catomic *atom_value, int64 value) {
	atom_value->counter = value;
}


int64 catomic_inc(catomic *atom_value) {
#ifdef _MSC_VER
	return InterlockedIncrement64(&atom_value->counter);
#else
	return __sync_add_and_fetch(&atom_value->counter, 1);
#endif
}

int64 catomic_dec(catomic *atom_value) {
#ifdef _MSC_VER
	return InterlockedDecrement64(&atom_value->counter);
#else
	return __sync_sub_and_fetch(&atom_value->counter, 1);
#endif
}

int64 catomic_fetch_add(catomic *atom_value, int64 value) {
#ifdef _MSC_VER
	return InterlockedExchangeAdd64(&atom_value->counter, value);
#else
	return __sync_fetch_and_add(&atom_value->counter, value);
#endif
}

int64 catomic_fetch_or(catomic *atom_value, int64 value) {
#ifdef _MSC_VER
	int64 old, newvalue;
	do {
		old = atom_value->counter;
		newvalue = old | value;
	} while (!win_catomic_compare_set64(&atom_value->counter, old, newvalue));

	return old;
#else
	return __sync_fetch_and_or(&atom_value->counter, value);
#endif
}

int64 catomic_fetch_and(catomic *atom_value, int64 value) {
#ifdef _MSC_VER
	int64 old, newvalue;
	do {
		old = atom_value->counter;
		newvalue = old & value;
	} while (!win_catomic_compare_set64(&atom_value->counter, old, newvalue));

	return old;
#else
	return __sync_fetch_and_and(&atom_value->counter, value);
#endif
}

int64 catomic_add_fetch(catomic *atom_value, int64 value) {
#ifdef _MSC_VER
	int64 old, newvalue;
	do {
		old = atom_value->counter;
		newvalue = old + value;
	} while (!win_catomic_compare_set64(&atom_value->counter, old, newvalue));

	return newvalue;
#else
	return __sync_add_and_fetch(&atom_value->counter, value);
#endif
}

int64 catomic_or_fetch(catomic *atom_value, int64 value) {
#ifdef _MSC_VER
	int64 old, newvalue;
	do {
		old = atom_value->counter;
		newvalue = old | value;
	} while (!win_catomic_compare_set64(&atom_value->counter, old, newvalue));

	return newvalue;
#else
	return __sync_or_and_fetch(&atom_value->counter, value);
#endif
}

int64 catomic_and_fetch(catomic *atom_value, int64 value) {
#ifdef _MSC_VER
	int64 old, newvalue;
	do {
		old = atom_value->counter;
		newvalue = old & value;
	} while (!win_catomic_compare_set64(&atom_value->counter, old, newvalue));

	return newvalue;
#else
	return __sync_and_and_fetch(&atom_value->counter, value);
#endif
}

bool catomic_compare_set(catomic *atom_value, int64 old, int64 set) {
#ifdef _MSC_VER
	return win_catomic_compare_set64(&atom_value->counter, old, set);
#else
	return __sync_bool_compare_and_swap(&atom_value->counter, old, set);
#endif
}

void catomic_synchronize() {
#ifdef _MSC_VER
	MemoryBarrier();
#else
	__sync_synchronize();
#endif
}


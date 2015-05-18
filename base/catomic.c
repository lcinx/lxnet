
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#include "catomic.h"

#ifdef WIN32
#include <windows.h>
#endif



long catomic_set(volatile long *atom_value, long value) {
#ifdef WIN32
	return InterlockedExchange(atom_value, value);
#else
	return __sync_lock_test_and_set(atom_value, value);
#endif
}

long catomic_inc(volatile long *atom_value) {
#ifdef WIN32
	return InterlockedIncrement(atom_value);
#else
	return __sync_add_and_fetch(atom_value, 1);
#endif
}

long catomic_dec(volatile long *atom_value) {
#ifdef WIN32
	return InterlockedDecrement(atom_value);
#else
	return __sync_sub_and_fetch(atom_value, 1);
#endif
}

long catomic_fetch_add(volatile long *atom_value, long value) {
#ifdef WIN32
	return InterlockedExchangeAdd(atom_value, value);
#else
	return __sync_fetch_and_add(atom_value, value);
#endif
}

long catomic_fetch_or(volatile long *atom_value, long value) {
#ifdef WIN32
	long old, newvalue;
	do {
		old = *atom_value;
		newvalue = old | value;
	} while (!catomic_compare_set(atom_value, old, newvalue));

	return old;
#else
	return __sync_fetch_and_or(atom_value, value);
#endif
}

long catomic_fetch_and(volatile long *atom_value, long value) {
#ifdef WIN32
	long old, newvalue;
	do {
		old = *atom_value;
		newvalue = old & value;
	} while (!catomic_compare_set(atom_value, old, newvalue));

	return old;
#else
	return __sync_fetch_and_and(atom_value, value);
#endif
}

bool catomic_compare_set(volatile long *atom_value, long old, long set) {
#ifdef WIN32
	if (InterlockedCompareExchange(atom_value, set, old) == old)
		return true;

	return false;
#else
	return __sync_bool_compare_and_swap(atom_value, old, set);
#endif
}

void catomic_synchronize() {
#ifdef WIN32
#ifdef __GNUC__
	__sync_synchronize();
#else
	MemoryBarrier();
#endif
#else
	__sync_synchronize();
#endif
}


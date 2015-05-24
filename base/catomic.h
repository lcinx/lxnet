
/*
 * Copyright (C) lcinx
 * lcinx@163.com
 */

#ifndef _H_CROSS_ATOMIC_H_
#define _H_CROSS_ATOMIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"


/*

long catomic_inc(volatile long *atom_value) {
	(*atom_value) += 1;
	return (*atom_value);
}

long catomic_dec(volatile long *atom_value) {
	(*atom_value) += (-1);
	return (*atom_value);
}

long catomic_fetch_add(volatile long *atom_value, long value) {
	long old = *atom_value;
	*atom_value = old + value;
	return old;
}

long catomic_fetch_or(volatile long *atom_value, long value) {
	long old = *atom_value;
	*atom_value = old | value;
	return old;
}

long catomic_fetch_and(volatile long *atom_value, long value) {
	long old = *atom_value;
	*atom_value = old & value;
	return old;
}

long catomic_add_fetch(volatile long *atom_value, long value) {
	*atom_value = *atom_value + value;
	return (*atom_value);
}

long catomic_or_fetch(volatile long *atom_value, long value) {
	*atom_value = *atom_value | value;
	return (*atom_value);
}

long catomic_and_fetch(volatile long *atom_value, long value) {
	*atom_value = *atom_value & value;
	return (*atom_value);
}

bool catomic_compare_set(volatile long *atom_value, long old, long set) {
	if (*atom_value == old) {
		*atom_value = set;
		return true;
	}

	return false;
}

*/



long catomic_read(volatile long *atom_value);
void catomic_set(volatile long *atom_value, long value);



long catomic_inc(volatile long *atom_value);
long catomic_dec(volatile long *atom_value);
long catomic_fetch_add(volatile long *atom_value, long value);
long catomic_fetch_or(volatile long *atom_value, long value);
long catomic_fetch_and(volatile long *atom_value, long value);
long catomic_add_fetch(volatile long *atom_value, long value);
long catomic_or_fetch(volatile long *atom_value, long value);
long catomic_and_fetch(volatile long *atom_value, long value);
bool catomic_compare_set(volatile long *atom_value, long old, long set);
void catomic_synchronize();



#ifdef __cplusplus
}
#endif
#endif


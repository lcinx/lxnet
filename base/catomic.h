
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


typedef struct {
	volatile int64 counter;
} catomic;

#define catomic_init(i)	{ (i) }


/*

int64 catomic_inc(catomic *atom_value) {
	atom_value->counter += 1;
	return atom_value->counter;
}

int64 catomic_dec(catomic *atom_value) {
	atom_value->counter += (-1);
	return atom_value->counter;
}

int64 catomic_fetch_add(catomic *atom_value, int64 value) {
	int64 old = atom_value->counter;
	atom_value->counter = old + value;
	return old;
}

int64 catomic_fetch_or(catomic *atom_value, int64 value) {
	int64 old = atom_value->counter;
	atom_value->counter = old | value;
	return old;
}

int64 catomic_fetch_and(catomic *atom_value, int64 value) {
	int64 old = atom_value->counter;
	atom_value->counter = old & value;
	return old;
}

int64 catomic_add_fetch(catomic *atom_value, int64 value) {
	atom_value->counter += value;
	return atom_value->counter;
}

int64 catomic_or_fetch(catomic *atom_value, int64 value) {
	atom_value->counter |= value;
	return atom_value->counter;
}

int64 catomic_and_fetch(catomic *atom_value, int64 value) {
	atom_value->counter &= value;
	return atom_value->counter;
}

bool catomic_compare_set(catomic *atom_value, int64 old, int64 set) {
	if (atom_value->counter == old) {
		atom_value->counter = set;
		return true;
	}

	return false;
}

*/



int64 catomic_read(catomic *atom_value);
void catomic_set(catomic *atom_value, int64 value);



int64 catomic_inc(catomic *atom_value);
int64 catomic_dec(catomic *atom_value);
int64 catomic_fetch_add(catomic *atom_value, int64 value);
int64 catomic_fetch_or(catomic *atom_value, int64 value);
int64 catomic_fetch_and(catomic *atom_value, int64 value);
int64 catomic_add_fetch(catomic *atom_value, int64 value);
int64 catomic_or_fetch(catomic *atom_value, int64 value);
int64 catomic_and_fetch(catomic *atom_value, int64 value);
bool catomic_compare_set(catomic *atom_value, int64 old, int64 set);
void catomic_synchronize();

#ifdef __cplusplus
}
#endif
#endif


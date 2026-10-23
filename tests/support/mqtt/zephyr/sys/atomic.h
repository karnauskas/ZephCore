#pragma once
typedef unsigned atomic_t;
static inline unsigned atomic_get(const atomic_t *a) { return *a; }
static inline void atomic_or(atomic_t *a, unsigned b) { *a |= b; }
static inline void atomic_and(atomic_t *a, unsigned b) { *a &= b; }

static inline void atomic_set(atomic_t *a, unsigned b) { *a = b; }

static inline unsigned atomic_inc(atomic_t *a) { return (*a)++; }

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#define __noinit
#define ARG_UNUSED(x) (void)(x)
#define BIT(n) (1U << (n))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define K_MSEC(ms) (ms)
#define K_NO_WAIT 0
#define K_FOREVER -1
#define K_THREAD_DEFINE(...)
struct k_msgq { unsigned count, size; unsigned char data[16384]; };
#define K_MSGQ_DEFINE(name, size, count, align) static struct k_msgq name = {0, size, {0}}
static inline int k_msgq_put(struct k_msgq *q, const void *p, int wait) { ARG_UNUSED(wait); if(q->count == 8) return -1; memcpy(q->data + q->count++ * q->size,p,q->size); return 0; }
static inline int k_msgq_get(struct k_msgq *q, void *p, int wait) { ARG_UNUSED(wait); if(!q->count)return -1; memcpy(p,q->data,q->size); --q->count; memmove(q->data,q->data+q->size,q->count*q->size); return 0; }
struct k_event { unsigned bits; };
#define K_EVENT_DEFINE(name) struct k_event name
static inline unsigned k_event_test(struct k_event *e, unsigned bits) { return e->bits & bits; }
static inline unsigned k_event_clear(struct k_event *e, unsigned bits) { unsigned old=e->bits; e->bits &= ~bits; return old; }
static inline void k_event_post(struct k_event *e, unsigned bits) { e->bits |= bits; }
void mqtt_test_wait(void);
static inline void k_event_wait(struct k_event *e, unsigned bits, bool reset, int wait) { ARG_UNUSED(e); ARG_UNUSED(bits); ARG_UNUSED(reset); ARG_UNUSED(wait); mqtt_test_wait(); }
struct k_sem { int ready; };
#define K_SEM_DEFINE(name, count, limit) struct k_sem name = {count}
static inline void k_sem_take(struct k_sem *s, int wait) { ARG_UNUSED(s); ARG_UNUSED(wait); }
static inline void k_sem_give(struct k_sem *s) { s->ready=1; }
int64_t k_uptime_get(void);

#define __aligned(n) __attribute__((aligned(n)))
#define CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
struct k_thread { int unused; };
#define K_THREAD_STACK_ARRAY_DEFINE(name, count, size) static char name[count][size]
#define K_THREAD_STACK_SIZEOF(stack) sizeof(stack)
static inline void k_thread_create(struct k_thread *t, char *stack, size_t size, void (*fn)(void*,void*,void*), void *a, void *b, void *c, int pri, int opts, int delay) { ARG_UNUSED(t); ARG_UNUSED(stack); ARG_UNUSED(size); ARG_UNUSED(fn); ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c); ARG_UNUSED(pri); ARG_UNUSED(opts); ARG_UNUSED(delay); }
struct k_mutex { int unused; };
#define K_MUTEX_DEFINE(name) struct k_mutex name
static inline void k_mutex_lock(struct k_mutex *m, int wait) { ARG_UNUSED(m); ARG_UNUSED(wait); }
static inline void k_mutex_unlock(struct k_mutex *m) { ARG_UNUSED(m); }
static inline void k_msgq_init(struct k_msgq *q, char *buf, size_t size, unsigned count) { ARG_UNUSED(buf); ARG_UNUSED(count); q->count=0; q->size=size; }
static inline void k_msgq_purge(struct k_msgq *q) { q->count=0; }
static inline void k_event_init(struct k_event *e) { e->bits=0; }

#pragma once
#include <zephyr/sys/util.h>
#include <cstdint>
extern "C" uint32_t k_uptime_get_32();
#define K_MSEC(ms) (ms)
#define K_NO_WAIT 0
struct k_work {};
struct k_work_delayable { void (*handler)(k_work *) = nullptr; bool pending = false; };
inline void k_work_init_delayable(k_work_delayable *w, void (*handler)(k_work *)) { w->handler = handler; }
inline int k_work_reschedule(k_work_delayable *w, int) { CHECK(w->handler != nullptr); w->pending = true; return 0; }
inline int k_work_cancel_delayable(k_work_delayable *w) { CHECK(w->handler != nullptr); w->pending = false; return 0; }
inline bool k_work_delayable_is_pending(k_work_delayable *w) { CHECK(w->handler != nullptr); return w->pending; }
inline void k_sleep(int) {}

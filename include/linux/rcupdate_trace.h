/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Minimal compatibility shims for 4.14: sleepable (tasks trace) RCU.
 * Maps the 5.x tasks-trace RCU API onto normal RCU for build compat.
 */
#ifndef _LINUX_RCUPDATE_TRACE_H
#define _LINUX_RCUPDATE_TRACE_H

#include <linux/rcupdate.h>

static inline void rcu_read_lock_trace(void) { rcu_read_lock(); }
static inline void rcu_read_unlock_trace(void) { rcu_read_unlock(); }
static inline int rcu_read_lock_trace_held(void) { return rcu_read_lock_held(); }
static inline void rcu_read_lock_trace_notrace(void) { rcu_read_lock(); }
static inline void rcu_read_unlock_trace_notrace(void) { rcu_read_unlock(); }

#define call_rcu_tasks_trace		call_rcu
#define call_rcu_tasks			call_rcu

static inline void synchronize_rcu_tasks_trace(void) { synchronize_rcu(); }
/* 4.14 compat: CONFIG_TASKS_RCU already provides synchronize_rcu_tasks */
static inline void synchronize_rcu_mult(call_rcu_func_t a, call_rcu_func_t b)
{
	(void)a; (void)b;
	synchronize_rcu();
}

#endif /* _LINUX_RCUPDATE_TRACE_H */

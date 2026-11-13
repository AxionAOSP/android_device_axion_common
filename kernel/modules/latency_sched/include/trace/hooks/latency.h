#undef TRACE_SYSTEM
#define TRACE_SYSTEM latency

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_LATENCY_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_LATENCY_H

#include <linux/tracepoint.h>
#include <linux/version.h>
#include <trace/hooks/vendor_hooks.h>

struct task_struct;
struct sched_entity;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
DECLARE_HOOK(android_vh_latency_wakeup_offset,
	TP_PROTO(struct task_struct *curr, struct task_struct *p, s64 *offset),
	TP_ARGS(curr, p, offset));

DECLARE_HOOK(android_vh_latency_init_entity,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));
#else
DECLARE_HOOK(android_vh_latency_wakeup_offset,
	TP_PROTO(struct sched_entity *curr, struct sched_entity *se, s64 *offset),
	TP_ARGS(curr, se, offset));

DECLARE_HOOK(android_vh_latency_tick_offset,
	TP_PROTO(struct sched_entity *curr, struct sched_entity *se, s64 *offset),
	TP_ARGS(curr, se, offset));

DECLARE_HOOK(android_vh_latency_init_entity,
	TP_PROTO(struct sched_entity *se),
	TP_ARGS(se));
#endif

DECLARE_HOOK(android_vh_latency_set_user_nice,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));

#endif

#include <trace/define_trace.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM bore

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_BORE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_BORE_H

#include <linux/tracepoint.h>
#include <linux/version.h>
#include <trace/hooks/vendor_hooks.h>

struct task_struct;
struct sched_entity;

DECLARE_HOOK(android_vh_bore_update_curr,
	TP_PROTO(struct task_struct *p, u64 delta_exec, int *new_prio),
	TP_ARGS(p, delta_exec, new_prio));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
DECLARE_HOOK(android_vh_bore_dequeue_task_fair,
	TP_PROTO(struct task_struct *p, u64 now, int *new_prio),
	TP_ARGS(p, now, new_prio));

DECLARE_HOOK(android_vh_bore_yield_task_fair,
	TP_PROTO(struct task_struct *p, int *new_prio, s64 *vremain, bool *rescaled),
	TP_ARGS(p, new_prio, vremain, rescaled));

DECLARE_HOOK(android_vh_bore_task_fork,
	TP_PROTO(struct task_struct *p, struct task_struct *parent, u64 clone_flags,
		 int *new_prio),
	TP_ARGS(p, parent, clone_flags, new_prio));

DECLARE_HOOK(android_vh_bore_set_load_weight,
	TP_PROTO(struct task_struct *p, int *prio),
	TP_ARGS(p, prio));

DECLARE_HOOK(android_vh_bore_place_entity,
	TP_PROTO(struct sched_entity *se, struct task_struct *p, bool initial,
		 bool wakeup, u64 vslice, u64 now, bool *placed),
	TP_ARGS(se, p, initial, wakeup, vslice, now, placed));

DECLARE_HOOK(android_vh_bore_protect_slice,
	TP_PROTO(struct task_struct *p, bool *protect),
	TP_ARGS(p, protect));

DECLARE_HOOK(android_vh_bore_preempt_weight,
	TP_PROTO(struct sched_entity *pse, struct sched_entity *se, bool *check),
	TP_ARGS(pse, se, check));

DECLARE_HOOK(android_vh_bore_enabled,
	TP_PROTO(bool *enabled),
	TP_ARGS(enabled));

DECLARE_HOOK(android_vh_bore_switched_to_fair,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));

DECLARE_HOOK(android_vh_bore_futex_wait,
	TP_PROTO(struct task_struct *p, bool waiting),
	TP_ARGS(p, waiting));
#else
DECLARE_HOOK(android_vh_bore_dequeue_task_fair,
	TP_PROTO(struct task_struct *p, int flags),
	TP_ARGS(p, flags));

DECLARE_HOOK(android_vh_bore_set_next_entity,
	TP_PROTO(struct sched_entity *se),
	TP_ARGS(se));

DECLARE_HOOK(android_vh_bore_yield_task_fair,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));

DECLARE_HOOK(android_vh_bore_task_fork,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));

DECLARE_HOOK(android_vh_bore_init_entity,
	TP_PROTO(struct sched_entity *se),
	TP_ARGS(se));

DECLARE_HOOK(android_vh_bore_set_load_weight,
	TP_PROTO(struct task_struct *p, int *prio),
	TP_ARGS(p, prio));
#endif

#endif

#include <trace/define_trace.h>


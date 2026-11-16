#ifndef _LATENCY_SCHED_H
#define _LATENCY_SCHED_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/prio.h>
#include <linux/types.h>
#include <linux/version.h>

#define MAX_LATENCY_NICE 19
#define MIN_LATENCY_NICE -20
#define DEFAULT_LATENCY_NICE 0
#define LATENCY_NICE_WIDTH (MAX_LATENCY_NICE - MIN_LATENCY_NICE + 1)
#define DEFAULT_LATENCY_PRIO (DEFAULT_LATENCY_NICE + LATENCY_NICE_WIDTH / 2)
#define LATENCY_TO_NICE(prio) ((prio) - DEFAULT_LATENCY_PRIO)
#define NICE_TO_LATENCY(nice) ((nice) + DEFAULT_LATENCY_PRIO)

#ifndef task_of
#define task_of(_se) container_of(_se, struct task_struct, se)
#endif

#define DEFAULT_SCHED_LATENCY_MAX 24000000L

extern uint sched_latency_nice_enabled;
#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
extern long sched_latency_max;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 0, 0)
#define LATENCY_SHORT_SLICE (500 * NSEC_PER_USEC)
#define LATENCY_LONG_SLICE (4 * NSEC_PER_MSEC)

extern unsigned int sysctl_sched_base_slice;
#else
struct latency_sched_entity {
	s32 latency_offset;
	s16 latency_nice;
	u16 flags;
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
static inline struct latency_sched_entity *latency_get_task(struct task_struct *p)
{
	return (struct latency_sched_entity *)&p->__kabi_reserved6;
}
#else
static inline struct latency_sched_entity *latency_get_se(struct sched_entity *se)
{
	return (struct latency_sched_entity *)&se->android_kabi_reserved1;
}

static inline struct latency_sched_entity *latency_get_task(struct task_struct *p)
{
	return latency_get_se(&p->se);
}
#endif
#endif

static inline bool latency_task_is_eligible(struct task_struct *p)
{
	if (p->policy != SCHED_NORMAL && p->policy != SCHED_BATCH && p->policy != SCHED_IDLE)
		return false;
	if (p->flags & PF_IDLE)
		return false;
	return true;
}

#endif

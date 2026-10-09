/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2025-2026 AxionOS
 */

#ifndef _AX_DRAGONITE_H_
#define _AX_DRAGONITE_H_

#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#endif
#include <linux/pid.h>
#include <linux/cpumask.h>

#define AXD_NAME "ax_dragonite"
#define AXD_VERSION "1.0.0"

#define AX_COMM_LEN 16
#define AXD_MAX_COMM_LEN 16
#define AXD_MAX_THREAD_GROUP_TASKS 512

#define AXD_MATCH_ALL_COMM '*'
#define AXD_INVALID_PID 0

#define AXD_PROC_NTA_DIR "ax_named_thread_affinity"
#define AXD_PROC_DRAGONITE_DIR "ax_dragonite"
#define AXD_PROC_PID "pid"
#define AXD_PROC_NTA "named_thread_affinity"
#define AXD_PROC_RESET "reset"
#define AXD_PROC_STATS "stats"

#define AXD_MAX_KBUF_PID 32
#define AXD_MAX_KBUF_AFFINITY 256
#define AXD_RADIX_DEC 10
#define AXD_RADIX_HEX 16

struct axd_stats {
    atomic64_t affinity_set_count;
    atomic64_t reset_count;
};

extern struct axd_stats g_axd_stats;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
static inline struct task_struct *axd_find_task_by_vpid(pid_t nr)
{
    return pid_task(find_vpid(nr), PIDTYPE_PID);
}
#define find_task_by_vpid(nr) axd_find_task_by_vpid(nr)
#else
static inline struct task_struct *axd_find_task_by_vpid(pid_t nr)
{
    return find_task_by_vpid(nr);
}
#endif

int axd_set_affinity(pid_t pid, const char *comm_name, const struct cpumask *mask);
int axd_reset_affinity(pid_t pid);

int axd_proc_init(void);
void axd_proc_exit(void);

#endif

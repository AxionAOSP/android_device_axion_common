// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2025-2026 AxionOS
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task.h>
#endif
#include <linux/cpumask.h>
#include <linux/rcupdate.h>
#include <linux/string.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include "ax_dragonite.h"

struct axd_stats g_axd_stats;

static inline bool axd_comm_matches(const char *task_comm, const char *comm_name)
{
    if (comm_name[0] == AXD_MATCH_ALL_COMM && comm_name[1] == '\0') {
        return true;
    }
    return strncmp(task_comm, comm_name, AX_COMM_LEN) == 0;
}

int axd_set_affinity(pid_t pid, const char *comm_name, const struct cpumask *mask)
{
    struct task_struct *task;
    struct task_struct *t;
    struct task_struct **threads;
    int num_threads = 0;
    int matched = 0;
    int i;

    if (pid <= AXD_INVALID_PID || !comm_name || !mask ||
        cpumask_empty(mask) || !cpumask_intersects(mask, cpu_online_mask)) {
        return -EINVAL;
    }

    threads = kmalloc_array(AXD_MAX_THREAD_GROUP_TASKS, sizeof(struct task_struct *), GFP_KERNEL);
    if (!threads) {
        return -ENOMEM;
    }

    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (!task || !task->signal || (task->flags & PF_EXITING)) {
        rcu_read_unlock();
        kfree(threads);
        return -ESRCH;
    }
    get_task_struct(task);

    for_each_thread(task, t) {
        if (!axd_comm_matches(t->comm, comm_name)) {
            continue;
        }
        if (num_threads >= AXD_MAX_THREAD_GROUP_TASKS) {
            break;
        }
        get_task_struct(t);
        threads[num_threads++] = t;
    }
    rcu_read_unlock();

    for (i = 0; i < num_threads; i++) {
        if (set_cpus_allowed_ptr(threads[i], mask) == 0) {
            matched++;
        }
        put_task_struct(threads[i]);
    }
    kfree(threads);
    put_task_struct(task);

    if (matched > 0) {
        atomic64_inc(&g_axd_stats.affinity_set_count);
    }

    return matched > 0 ? 0 : (num_threads > 0 ? -EINVAL : -ENOENT);
}

int axd_reset_affinity(pid_t pid)
{
    struct task_struct *task;
    struct task_struct *t;
    struct task_struct **threads;
    int num_threads = 0;
    int reset_count = 0;
    int i;

    if (pid <= AXD_INVALID_PID) {
        return -EINVAL;
    }

    threads = kmalloc_array(AXD_MAX_THREAD_GROUP_TASKS, sizeof(struct task_struct *), GFP_KERNEL);
    if (!threads) {
        return -ENOMEM;
    }

    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (!task || !task->signal || (task->flags & PF_EXITING)) {
        rcu_read_unlock();
        kfree(threads);
        return -ESRCH;
    }
    get_task_struct(task);

    for_each_thread(task, t) {
        if (num_threads >= AXD_MAX_THREAD_GROUP_TASKS) {
            break;
        }
        get_task_struct(t);
        threads[num_threads++] = t;
    }
    rcu_read_unlock();

    for (i = 0; i < num_threads; i++) {
        if (set_cpus_allowed_ptr(threads[i], cpu_possible_mask) == 0) {
            reset_count++;
        }
        put_task_struct(threads[i]);
    }
    kfree(threads);
    put_task_struct(task);

    if (reset_count > 0) {
        atomic64_inc(&g_axd_stats.reset_count);
    }

    return reset_count > 0 ? 0 : -ESRCH;
}

static int __init axd_init(void)
{
    int ret;

    atomic64_set(&g_axd_stats.affinity_set_count, 0);
    atomic64_set(&g_axd_stats.reset_count, 0);

    ret = axd_proc_init();
    if (ret)
        return ret;

    pr_info("%s: driver initialized v%s\n", AXD_NAME, AXD_VERSION);
    return 0;
}

static void __exit axd_exit(void)
{
    axd_proc_exit();
    pr_info("%s: driver exited\n", AXD_NAME);
}

module_init(axd_init);
module_exit(axd_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AxionOS AxDragonite Performance Subsystem Driver");
MODULE_VERSION(AXD_VERSION);

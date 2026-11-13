/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020-2023 Vincent Guittot <vincent.guittot@linaro.org>
 * Copyright (C) 2020 Parth Shah <parth@linux.ibm.com>
 * Copyright (C) 2023 Peter Jung <admin@ptr1337.dev> (CachyOS)
 * Copyright (C) 2025-2026 AxionOS
 */

#include "latency_sched.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/seq_file.h>
#include <linux/sysctl.h>
#include <linux/version.h>
#include <trace/hooks/latency.h>

uint sched_latency_nice_enabled = 1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
long sched_latency_max = DEFAULT_SCHED_LATENCY_MAX;
#endif

module_param_named(sched_latency_nice_enabled, sched_latency_nice_enabled, uint, 0644);
MODULE_PARM_DESC(sched_latency_nice_enabled, "Enable CFS Latency-Nice scheduler module");

#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
module_param_named(sched_latency_max, sched_latency_max, long, 0644);
MODULE_PARM_DESC(sched_latency_max, "CFS maximum latency offset clamp in ns");
#endif

static bool latency_hooks_active;
static struct ctl_table_header *latency_sysctl_header;
static struct proc_dir_entry *latency_proc_entry;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 0, 0)
static s16 task_latency_nice(struct task_struct *p)
{
	if (!sched_latency_nice_enabled || !latency_task_is_eligible(p))
		return DEFAULT_LATENCY_NICE;
	if (p->static_prio < DEFAULT_PRIO)
		return MIN_LATENCY_NICE;
	if (p->static_prio > DEFAULT_PRIO)
		return MAX_LATENCY_NICE;
	return DEFAULT_LATENCY_NICE;
}

static u64 latency_slice(s16 latency_nice)
{
	return latency_nice < 0 ? LATENCY_SHORT_SLICE : LATENCY_LONG_SLICE;
}

static bool latency_owns_slice(struct sched_entity *se)
{
	return se->slice == latency_slice(MIN_LATENCY_NICE) ||
	       se->slice == latency_slice(MAX_LATENCY_NICE);
}

static void update_task_latency_slice(struct task_struct *p)
{
	struct sched_entity *se = &p->se;
	s16 latency_nice;

	if (se->custom_slice && !latency_owns_slice(se))
		return;

	latency_nice = task_latency_nice(p);
	if (latency_nice == DEFAULT_LATENCY_NICE) {
		se->custom_slice = 0;
		se->slice = sysctl_sched_base_slice;
		return;
	}

	se->slice = latency_slice(latency_nice);
	se->custom_slice = 1;
}

static void probe_latency_set_user_nice(void *data, struct task_struct *p)
{
	update_task_latency_slice(p);
}
#else
static inline s32 calc_latency_offset(s16 latency_nice)
{
	return (s32)(sched_latency_max * (long)latency_nice / 20);
}

static void update_task_latency_nice(struct task_struct *p)
{
	struct latency_sched_entity *lse;
	s16 nice_val;

	if (!p || !latency_task_is_eligible(p))
		return;

	lse = latency_get_task(p);

	if (p->static_prio < 120) {
		nice_val = MIN_LATENCY_NICE;
	} else if (p->static_prio > 120) {
		nice_val = MAX_LATENCY_NICE;
	} else {
		nice_val = DEFAULT_LATENCY_NICE;
	}

	lse->latency_nice = nice_val;
	lse->latency_offset = calc_latency_offset(nice_val);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
static long wakeup_latency_gran(struct latency_sched_entity *lcurr, struct latency_sched_entity *lse)
{
#else
static long wakeup_latency_gran(struct sched_entity *curr, struct sched_entity *se)
{
	struct latency_sched_entity *lcurr = latency_get_se(curr);
	struct latency_sched_entity *lse = latency_get_se(se);
#endif
	long latency_offset = lse->latency_offset;

	if ((latency_offset < 0) || (lcurr->latency_offset < 0))
		latency_offset -= lcurr->latency_offset;

	if (latency_offset > sched_latency_max)
		latency_offset = sched_latency_max;
	else if (latency_offset < -sched_latency_max)
		latency_offset = -sched_latency_max;

	return latency_offset;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
static void probe_latency_wakeup_offset(void *data, struct task_struct *curr, struct task_struct *p, s64 *offset)
{
	if (!sched_latency_nice_enabled)
		return;

	*offset = (s64)wakeup_latency_gran(latency_get_task(curr), latency_get_task(p));
}

static void probe_latency_init_entity(void *data, struct task_struct *p)
{
	struct latency_sched_entity *lse = latency_get_task(p);

	memset(lse, 0, sizeof(*lse));
}
#else
static void probe_latency_wakeup_offset(void *data, struct sched_entity *curr, struct sched_entity *se, s64 *offset)
{
	if (!sched_latency_nice_enabled || !curr || !se || !offset)
		return;

	*offset = (s64)wakeup_latency_gran(curr, se);
}

static void probe_latency_tick_offset(void *data, struct sched_entity *curr, struct sched_entity *se, s64 *offset)
{
	if (!sched_latency_nice_enabled || !curr || !se || !offset)
		return;

	*offset = (s64)wakeup_latency_gran(curr, se);
}

static void probe_latency_init_entity(void *data, struct sched_entity *se)
{
	struct latency_sched_entity *lse;

	if (!se)
		return;

	lse = latency_get_se(se);
	memset(lse, 0, sizeof(*lse));
}
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
static void probe_latency_set_user_nice(void *data, struct task_struct *p)
{
	if (!sched_latency_nice_enabled)
		return;

	update_task_latency_nice(p);
}
#endif

static int latency_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CFS Latency-Nice Scheduler Module (CachyOS / Vincent Guittot)\n");
	if (!latency_hooks_active) {
		seq_printf(m, "status:                      disabled (kernel hooks not present / no-op mode)\n");
		return 0;
	}
	seq_printf(m, "sched_latency_nice_enabled:  %u\n", sched_latency_nice_enabled);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 0, 0)
	seq_printf(m, "short slice:                 %llu ns\n", latency_slice(MIN_LATENCY_NICE));
	seq_printf(m, "long slice:                  %llu ns\n", latency_slice(MAX_LATENCY_NICE));
#else
	seq_printf(m, "sched_latency_max:           %ld ns\n", sched_latency_max);
#endif
	return 0;
}

static int latency_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, latency_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops latency_proc_fops = {
	.proc_open = latency_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations latency_proc_fops = {
	.open = latency_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static struct ctl_table latency_table[] = {
	{
		.procname	= "sched_latency_nice_enabled",
		.data		= &sched_latency_nice_enabled,
		.maxlen		= sizeof(uint),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
	},
#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
	{
		.procname	= "sched_latency_max",
		.data		= &sched_latency_max,
		.maxlen		= sizeof(long),
		.mode		= 0644,
		.proc_handler	= proc_doulongvec_minmax,
	},
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
	{ }
#endif
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
static struct ctl_table latency_root_table[] = {
	{
		.procname	= "kernel",
		.mode		= 0555,
		.child		= latency_table,
	},
	{ }
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
static void init_latency_tasks(void)
{
	struct task_struct *p;
	struct task_struct *t;

	rcu_read_lock();
	for_each_process_thread(p, t) {
		update_task_latency_nice(t);
	}
	rcu_read_unlock();
}
#endif

static int __init latency_sched_init(void)
{
	int ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
	BUILD_BUG_ON(sizeof(struct latency_sched_entity) > sizeof(u64));
#else
	BUILD_BUG_ON(sizeof(struct latency_sched_entity) > 4 * sizeof(unsigned long));
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 0, 0)
	ret = register_trace_android_vh_latency_set_user_nice(probe_latency_set_user_nice, NULL);
	if (ret)
		goto fail_hooks;
#else
	init_latency_tasks();

	ret = register_trace_android_vh_latency_wakeup_offset(probe_latency_wakeup_offset, NULL);
	if (ret)
		goto fail_hooks;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
	ret = register_trace_android_vh_latency_tick_offset(probe_latency_tick_offset, NULL);
	if (ret)
		goto fail_tick;
#endif

	ret = register_trace_android_vh_latency_init_entity(probe_latency_init_entity, NULL);
	if (ret)
		goto fail_init_entity;

	ret = register_trace_android_vh_latency_set_user_nice(probe_latency_set_user_nice, NULL);
	if (ret)
		goto fail_user_nice;
#endif

	latency_hooks_active = true;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	latency_sysctl_header = register_sysctl("kernel", latency_table);
#else
	latency_sysctl_header = register_sysctl_table(latency_root_table);
#endif
	latency_proc_entry = proc_create("latency_sched", 0444, NULL, &latency_proc_fops);

	pr_info("latency_sched: CFS Latency-Nice scheduler module initialized\n");
	return 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
fail_user_nice:
	unregister_trace_android_vh_latency_init_entity(probe_latency_init_entity, NULL);
fail_init_entity:
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
	unregister_trace_android_vh_latency_tick_offset(probe_latency_tick_offset, NULL);
fail_tick:
#endif
	unregister_trace_android_vh_latency_wakeup_offset(probe_latency_wakeup_offset, NULL);
#endif
fail_hooks:
	pr_info("latency_sched: kernel hooks not present (ret=%d), running in no-op mode\n", ret);
	sched_latency_nice_enabled = 0;
	latency_hooks_active = false;
	latency_proc_entry = proc_create("latency_sched", 0444, NULL, &latency_proc_fops);
	return 0;
}

static void __exit latency_sched_exit(void)
{
	if (latency_proc_entry)
		proc_remove(latency_proc_entry);

	if (!latency_hooks_active)
		return;

	if (latency_sysctl_header)
		unregister_sysctl_table(latency_sysctl_header);

	unregister_trace_android_vh_latency_set_user_nice(probe_latency_set_user_nice, NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
	unregister_trace_android_vh_latency_init_entity(probe_latency_init_entity, NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
	unregister_trace_android_vh_latency_tick_offset(probe_latency_tick_offset, NULL);
#endif
	unregister_trace_android_vh_latency_wakeup_offset(probe_latency_wakeup_offset, NULL);
#endif

	pr_info("latency_sched: CFS Latency-Nice scheduler module unloaded\n");
}

module_init(latency_sched_init);
module_exit(latency_sched_exit);

MODULE_AUTHOR("Vincent Guittot <vincent.guittot@linaro.org>");
MODULE_AUTHOR("Parth Shah <parth@linux.ibm.com>");
MODULE_AUTHOR("Peter Jung <admin@ptr1337.dev>");
MODULE_AUTHOR("AxionOS");
MODULE_DESCRIPTION("CFS Latency-Nice / Latency-First Scheduler Module");
MODULE_LICENSE("GPL");

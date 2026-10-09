// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2025-2026 AxionOS
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>
#include "ax_dragonite.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0) || defined(AXD_HAS_PROC_OPS)
#define AXD_PROC_OPS_RW(name, _open, _read, _write, _lseek, _release) \
    static const struct proc_ops name = { \
        .proc_open = _open, \
        .proc_read = _read, \
        .proc_write = _write, \
        .proc_lseek = _lseek, \
        .proc_release = _release, \
    }
#define AXD_PROC_OPS_RO(name, _open, _read, _lseek, _release) \
    static const struct proc_ops name = { \
        .proc_open = _open, \
        .proc_read = _read, \
        .proc_lseek = _lseek, \
        .proc_release = _release, \
    }
#define AXD_PROC_OPS_WO(name, _write, _lseek) \
    static const struct proc_ops name = { \
        .proc_write = _write, \
        .proc_lseek = _lseek, \
    }
#else
#define AXD_PROC_OPS_RW(name, _open, _read, _write, _lseek, _release) \
    static const struct file_operations name = { \
        .owner = THIS_MODULE, \
        .open = _open, \
        .read = _read, \
        .write = _write, \
        .llseek = _lseek, \
        .release = _release, \
    }
#define AXD_PROC_OPS_RO(name, _open, _read, _lseek, _release) \
    static const struct file_operations name = { \
        .owner = THIS_MODULE, \
        .open = _open, \
        .read = _read, \
        .llseek = _lseek, \
        .release = _release, \
    }
#define AXD_PROC_OPS_WO(name, _write, _lseek) \
    static const struct file_operations name = { \
        .owner = THIS_MODULE, \
        .write = _write, \
        .llseek = _lseek, \
    }
#endif

static struct proc_dir_entry *ax_nta_dir;
static struct proc_dir_entry *ax_dragonite_dir;

static pid_t g_current_target_pid = 0;
static DEFINE_MUTEX(g_proc_lock);

static ssize_t nta_pid_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char kbuf[AXD_MAX_KBUF_PID];
    pid_t pid;
    int ret;

    if (count >= sizeof(kbuf))
        return -EINVAL;

    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';
    ret = kstrtoint(strim(kbuf), AXD_RADIX_DEC, &pid);
    if (ret)
        return ret;

    mutex_lock(&g_proc_lock);
    g_current_target_pid = pid;
    mutex_unlock(&g_proc_lock);

    return count;
}

static int nta_pid_show(struct seq_file *m, void *v)
{
    mutex_lock(&g_proc_lock);
    seq_printf(m, "%d\n", g_current_target_pid);
    mutex_unlock(&g_proc_lock);
    return 0;
}

static int nta_pid_open(struct inode *inode, struct file *file)
{
    return single_open(file, nta_pid_show, NULL);
}

AXD_PROC_OPS_RW(nta_pid_ops, nta_pid_open, seq_read, nta_pid_write, seq_lseek, single_release);

static ssize_t nta_affinity_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char *kbuf;
    char *comm_str;
    char *mask_str;
    char *token1;
    char *token2;
    char *token3;
    struct cpumask mask;
    pid_t pid = 0;
    pid_t parsed_pid = 0;
    int ret;

    if (count > AXD_MAX_KBUF_AFFINITY) {
        return -EINVAL;
    }

    kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf) {
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    kbuf[count] = '\0';

    token1 = strim(kbuf);
    token2 = strchr(token1, ' ');
    if (!token2) {
        kfree(kbuf);
        return -EINVAL;
    }

    *token2 = '\0';
    token2 = strim(token2 + 1);
    token3 = strchr(token2, ' ');

    if (token3 && kstrtoint(token1, AXD_RADIX_DEC, &parsed_pid) == 0 && parsed_pid > 0) {
        *token3 = '\0';
        token3 = strim(token3 + 1);
        pid = parsed_pid;
        comm_str = token2;
        mask_str = token3;
    } else {
        comm_str = token1;
        mask_str = token2;
        mutex_lock(&g_proc_lock);
        pid = g_current_target_pid;
        mutex_unlock(&g_proc_lock);
    }

    cpumask_clear(&mask);
    ret = cpulist_parse(mask_str, &mask);
    if (ret) {
        kfree(kbuf);
        return ret;
    }

    if (cpumask_empty(&mask) || !cpumask_intersects(&mask, cpu_possible_mask)) {
        kfree(kbuf);
        return -EINVAL;
    }
    cpumask_and(&mask, &mask, cpu_possible_mask);

    ret = axd_set_affinity(pid, comm_str, &mask);
    kfree(kbuf);

    if (ret < 0 && ret != -ENOENT)
        return ret;

    return count;
}

AXD_PROC_OPS_WO(nta_affinity_ops, nta_affinity_write, noop_llseek);

static ssize_t nta_reset_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char kbuf[AXD_MAX_KBUF_PID];
    pid_t pid = 0;
    int ret;

    if (count >= sizeof(kbuf)) {
        return -EINVAL;
    }

    if (count > 0 && !copy_from_user(kbuf, buf, count)) {
        kbuf[count] = '\0';
        if (kstrtoint(strim(kbuf), AXD_RADIX_DEC, &pid) != 0) {
            pid = 0;
        }
    }

    if (pid <= 1) {
        mutex_lock(&g_proc_lock);
        pid = g_current_target_pid;
        mutex_unlock(&g_proc_lock);
    }

    ret = axd_reset_affinity(pid);
    if (ret != 0) {
        return ret;
    }

    return count;
}

AXD_PROC_OPS_WO(nta_reset_ops, nta_reset_write, noop_llseek);



static int dragonite_stats_show(struct seq_file *m, void *v)
{
    seq_printf(m, "ax_dragonite version: %s\n", AXD_VERSION);
    seq_printf(m, "target_pid: %d\n", g_current_target_pid);
    seq_printf(m, "affinity_sets: %lld\n", atomic64_read(&g_axd_stats.affinity_set_count));
    seq_printf(m, "affinity_resets: %lld\n", atomic64_read(&g_axd_stats.reset_count));
    return 0;
}

static int dragonite_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, dragonite_stats_show, NULL);
}

AXD_PROC_OPS_RO(dragonite_stats_ops, dragonite_stats_open, seq_read, seq_lseek, single_release);

int axd_proc_init(void)
{
    ax_nta_dir = proc_mkdir(AXD_PROC_NTA_DIR, NULL);
    if (ax_nta_dir) {
        proc_create(AXD_PROC_PID, 0666, ax_nta_dir, &nta_pid_ops);
        proc_create(AXD_PROC_NTA, 0222, ax_nta_dir, &nta_affinity_ops);
        proc_create(AXD_PROC_RESET, 0222, ax_nta_dir, &nta_reset_ops);
    }

    ax_dragonite_dir = proc_mkdir(AXD_PROC_DRAGONITE_DIR, NULL);
    if (ax_dragonite_dir) {
        proc_create(AXD_PROC_STATS, 0444, ax_dragonite_dir, &dragonite_stats_ops);
    }

    return 0;
}

void axd_proc_exit(void)
{
    if (ax_nta_dir)
        remove_proc_subtree(AXD_PROC_NTA_DIR, NULL);

    if (ax_dragonite_dir)
        remove_proc_subtree(AXD_PROC_DRAGONITE_DIR, NULL);
}

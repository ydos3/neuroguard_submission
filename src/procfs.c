

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include "include/neuroguard.h"
#include "include/ng_internal.h"

static struct proc_dir_entry *ng_proc_dir;

static int ng_summary_show(struct seq_file *m, void *v)
{
    int i, count = 0;

    seq_printf(m, "%-8s %-18s %-14s %-12s %-8s\n",
               "PID", "COMM", "RSS-QUOTA(MB)", "EWMA-SCORE", "ACTION");
    seq_puts(m,   "-------- ------------------ -------------- ------------ --------\n");

    spin_lock(&ng_table_lock);
    for (i = 0; i < NEUROGUARD_MAX_PIDS; i++) {
        struct ng_sandbox *sb = &ng_sandboxes[i];
        if (sb->pid < 0)
            continue;

        count++;
        seq_printf(m, "%-8d %-18s %-14lu %-12u %-8s\n",
                   sb->pid,
                   sb->comm,
                   sb->mem_quota >> 20,     
                   sb->ewma_score >> 16,    
                   sb->breach_action == NG_ACTION_KILL      ? "KILL"     :
                   sb->breach_action == NG_ACTION_THROTTLE  ? "THROTTLE" :
                                                              "ALERT");
    }
    spin_unlock(&ng_table_lock);

    if (count == 0)
        seq_puts(m, "(no processes currently monitored)\n");

    return 0;
}

static int ng_summary_open(struct inode *inode, struct file *file)
{
    return single_open(file, ng_summary_show, NULL);
}

static const struct proc_ops ng_summary_ops = {
    .proc_open    = ng_summary_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int ng_config_show(struct seq_file *m, void *v)
{
    seq_printf(m,
               "alpha_pct  : %u\n"
               "threshold  : %u\n"
               "poll_ms    : %u\n"
               "max_pids   : %d\n"
               "ring_size  : %d\n",
               ng_alpha_pct, ng_threshold, ng_poll_ms,
               NEUROGUARD_MAX_PIDS, NEUROGUARD_RING_SIZE);
    return 0;
}

static int ng_config_open(struct inode *inode, struct file *file)
{
    return single_open(file, ng_config_show, NULL);
}

static const struct proc_ops ng_config_ops = {
    .proc_open    = ng_config_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

int ng_procfs_init(void)
{
    ng_proc_dir = proc_mkdir("neuroguard", NULL);
    if (!ng_proc_dir)
        return -ENOMEM;

    if (!proc_create("summary", 0444, ng_proc_dir, &ng_summary_ops))
        goto err;

    if (!proc_create("config", 0444, ng_proc_dir, &ng_config_ops))
        goto err;

    pr_info("neuroguard: procfs at /proc/neuroguard/\n");
    return 0;

err:
    remove_proc_subtree("neuroguard", NULL);
    return -ENOMEM;
}

void ng_procfs_exit(void)
{
    remove_proc_subtree("neuroguard", NULL);
}

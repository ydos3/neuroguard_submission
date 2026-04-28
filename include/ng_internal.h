
#ifndef _NG_INTERNAL_H
#define _NG_INTERNAL_H

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/kfifo.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/poll.h>
#include "neuroguard.h"

struct ng_sandbox {
    pid_t           pid;
    pid_t           tgid;
    char            comm[TASK_COMM_LEN];
    unsigned long   mem_quota;
    unsigned long   prev_rss;
    __u32           ewma_score;
    unsigned int    breach_action;
    spinlock_t      lock;
};

extern int              ng_major;
extern struct class    *ng_class;
extern struct device   *ng_device;
extern struct cdev      ng_cdev;

extern struct ng_sandbox ng_sandboxes[NEUROGUARD_MAX_PIDS];
extern spinlock_t        ng_table_lock;

extern unsigned int ng_alpha_pct;   
extern unsigned int ng_threshold;   
extern unsigned int ng_poll_ms;     

extern const struct file_operations ng_fops;

int  ng_events_init(void);
void ng_events_exit(void);
void ng_events_push(const struct neuroguard_event *ev);
bool ng_events_available(void);
int  ng_events_wait(struct file *filp);
int  ng_events_pop(struct neuroguard_event *ev, unsigned int *copied);
void ng_events_poll_wait(struct file *filp, poll_table *wait);

int  ng_sysfs_init(void);
void ng_sysfs_exit(void);

int  ng_procfs_init(void);
void ng_procfs_exit(void);

struct ng_sandbox *ng_find_sandbox(pid_t pid);
struct ng_sandbox *ng_alloc_sandbox(pid_t pid);
void ng_free_sandbox(pid_t pid);

__u32 ng_ewma_update(__u32 prev_score, unsigned long delta_bytes);
void  ng_check_sandbox(struct ng_sandbox *sb);

int  ng_poller_init(void);
void ng_poller_exit(void);

#endif 

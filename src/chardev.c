

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include "include/neuroguard.h"
#include "include/ng_internal.h"

static DEFINE_MUTEX(ng_ioctl_mutex);

static int ng_open(struct inode *inode, struct file *filp)
{
    pr_debug("neuroguard: agent opened fd (pid=%d)\n", current->pid);
    return 0;
}

static int ng_release(struct inode *inode, struct file *filp)
{
    pr_debug("neuroguard: agent closed fd (pid=%d)\n", current->pid);
    return 0;
}

static ssize_t ng_read(struct file *filp, char __user *buf,
                        size_t count, loff_t *ppos)
{
    struct neuroguard_event ev;
    unsigned int copied;
    int ret;

    if (count < sizeof(ev))
        return -EINVAL;

    if (filp->f_flags & O_NONBLOCK) {
        if (!ng_events_available())
            return -EAGAIN;
    } else {
        ret = ng_events_wait(filp);
        if (ret)
            return ret;
    }

    ret = ng_events_pop(&ev, &copied);
    if (ret < 0)
        return ret;
    if (copied == 0)
        return -EAGAIN;

    if (copy_to_user(buf, &ev, sizeof(ev)))
        return -EFAULT;

    return sizeof(ev);
}

static ssize_t ng_write(struct file *filp, const char __user *buf,
                         size_t count, loff_t *ppos)
{
    pid_t pid;
    struct ng_sandbox *sb;
    struct task_struct *task;
    struct neuroguard_event ev = {};

    if (count < sizeof(pid_t))
        return -EINVAL;

    if (copy_from_user(&pid, buf, sizeof(pid_t)))
        return -EFAULT;

    if (pid <= 0)
        return -EINVAL;

    mutex_lock(&ng_ioctl_mutex);

    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task) {
        rcu_read_unlock();
        mutex_unlock(&ng_ioctl_mutex);
        return -ESRCH;
    }
    rcu_read_unlock();

    spin_lock(&ng_table_lock);
    if (ng_find_sandbox(pid)) {
        spin_unlock(&ng_table_lock);
        mutex_unlock(&ng_ioctl_mutex);
        return -EEXIST;
    }

    sb = ng_alloc_sandbox(pid);
    if (!sb) {
        spin_unlock(&ng_table_lock);
        mutex_unlock(&ng_ioctl_mutex);
        return -ENOSPC;
    }

    get_task_comm(sb->comm, task);
    sb->tgid = task_tgid_nr(task);
    spin_unlock(&ng_table_lock);

    ev.pid        = pid;
    ev.event_type = NG_EVENT_ATTACH;
    ev.timestamp_ns = ktime_get_ns();
    strscpy(ev.comm, sb->comm, sizeof(ev.comm));
    ng_events_push(&ev);

    pr_info("neuroguard: attached PID %d (%s)\n", pid, sb->comm);
    mutex_unlock(&ng_ioctl_mutex);
    return sizeof(pid_t);
}

static long ng_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    pid_t pid;
    struct ng_sandbox *sb;
    struct task_struct *task;
    struct neuroguard_event ev = {};
    unsigned long quota;
    unsigned int action;
    struct neuroguard_query qr;
    long ret = 0;

    mutex_lock(&ng_ioctl_mutex);

    switch (cmd) {

    case NG_ATTACH_PID:
        if (copy_from_user(&pid, (void __user *)arg, sizeof(pid))) {
            ret = -EFAULT; break;
        }
        rcu_read_lock();
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        if (!task) { rcu_read_unlock(); ret = -ESRCH; break; }
        rcu_read_unlock();

        spin_lock(&ng_table_lock);
        if (ng_find_sandbox(pid)) {
            spin_unlock(&ng_table_lock);
            ret = -EEXIST; break;
        }
        sb = ng_alloc_sandbox(pid);
        if (!sb) {
            spin_unlock(&ng_table_lock);
            ret = -ENOSPC; break;
        }
        get_task_comm(sb->comm, task);
        sb->tgid = task_tgid_nr(task);
        spin_unlock(&ng_table_lock);

        ev.pid = pid; ev.event_type = NG_EVENT_ATTACH;
        ev.timestamp_ns = ktime_get_ns();
        strscpy(ev.comm, sb->comm, sizeof(ev.comm));
        ng_events_push(&ev);
        pr_info("neuroguard: ioctl ATTACH PID %d (%s)\n", pid, sb->comm);
        break;

    case NG_DETACH_PID:
        if (copy_from_user(&pid, (void __user *)arg, sizeof(pid))) {
            ret = -EFAULT; break;
        }
        spin_lock(&ng_table_lock);
        sb = ng_find_sandbox(pid);
        if (!sb) {
            spin_unlock(&ng_table_lock);
            ret = -ENOENT; break;
        }
        strscpy(ev.comm, sb->comm, sizeof(ev.comm));
        ng_free_sandbox(pid);
        spin_unlock(&ng_table_lock);

        ev.pid = pid; ev.event_type = NG_EVENT_DETACH;
        ev.timestamp_ns = ktime_get_ns();
        ng_events_push(&ev);
        pr_info("neuroguard: ioctl DETACH PID %d\n", pid);
        break;

    case NG_SET_QUOTA:

        if (copy_from_user(&quota, (void __user *)arg, sizeof(quota))) {
            ret = -EFAULT; break;
        }

        pr_warn("neuroguard: NG_SET_QUOTA: use neuroguard-ctl for quota setting\n");
        ret = -ENOSYS;
        break;

    case NG_GET_SCORE:
        if (copy_from_user(&qr, (void __user *)arg, sizeof(qr))) {
            ret = -EFAULT; break;
        }
        spin_lock(&ng_table_lock);
        sb = ng_find_sandbox(qr.pid);
        if (!sb) {
            spin_unlock(&ng_table_lock);
            ret = -ENOENT; break;
        }
        qr.score = sb->ewma_score;
        spin_unlock(&ng_table_lock);
        if (copy_to_user((void __user *)arg, &qr, sizeof(qr)))
            ret = -EFAULT;
        break;

    case NG_SET_ACTION:

        pid    = (pid_t)((arg >> 16) & 0xFFFF);
        action = (unsigned int)(arg & 0xFFFF);
        if (action > NG_ACTION_KILL) { ret = -EINVAL; break; }
        spin_lock(&ng_table_lock);
        sb = ng_find_sandbox(pid);
        if (!sb) {
            spin_unlock(&ng_table_lock);
            ret = -ENOENT; break;
        }
        sb->breach_action = action;
        spin_unlock(&ng_table_lock);
        pr_info("neuroguard: PID %d action set to %u\n", pid, action);
        break;

    default:
        ret = -ENOTTY;
    }

    mutex_unlock(&ng_ioctl_mutex);
    return ret;
}

static __poll_t ng_poll(struct file *filp, poll_table *wait)
{
    ng_events_poll_wait(filp, wait);
    if (ng_events_available())
        return EPOLLIN | EPOLLRDNORM;
    return 0;
}

const struct file_operations ng_fops = {
    .owner          = THIS_MODULE,
    .open           = ng_open,
    .release        = ng_release,
    .read           = ng_read,
    .write          = ng_write,
    .unlocked_ioctl = ng_ioctl,
    .poll           = ng_poll,
    .llseek         = no_llseek,
};



#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/signal.h>
#include <linux/ktime.h>
#include "include/neuroguard.h"
#include "include/ng_internal.h"

#define FP_SHIFT    16
#define FP_ONE      (1u << FP_SHIFT)           

static inline __u32 fp_from_int(unsigned long v)
{
    return (v > 0xFFFFul) ? 0xFFFFFFFFu : ((__u32)v << FP_SHIFT);
}

static inline unsigned long fp_to_int(__u32 v)
{
    return v >> FP_SHIFT;
}

static inline __u32 fp_mul(__u32 a, __u32 b)
{
    return ((__u64)a * b) >> FP_SHIFT;
}

__u32 ng_ewma_update(__u32 prev_score, unsigned long delta_bytes)
{
    __u32 alpha, one_minus_alpha, delta_fp, new_score;
    unsigned int a = ng_alpha_pct;

    if (a > 100)
        a = 100;

    alpha          = (FP_ONE * a) / 100;
    one_minus_alpha = FP_ONE - alpha;

    if (delta_bytes > (1UL << 23))
        delta_bytes = (1UL << 23);

    delta_fp  = fp_from_int(delta_bytes >> 10); 
    new_score = fp_mul(alpha, delta_fp) + fp_mul(one_minus_alpha, prev_score);
    return new_score;
}

static unsigned long ng_get_rss(pid_t pid)
{
    struct task_struct *task;
    struct mm_struct *mm;
    unsigned long rss = 0;

    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task) {
        rcu_read_unlock();
        return 0;
    }

    mm = get_task_mm(task);
    rcu_read_unlock();

    if (!mm)
        return 0;

    rss = get_mm_rss(mm) << PAGE_SHIFT; 
    mmput(mm);
    return rss;
}

static void ng_apply_action(struct ng_sandbox *sb, struct neuroguard_event *ev)
{
    struct task_struct *task;

    ev->event_type = NG_EVENT_ALERT;

    if (sb->breach_action == NG_ACTION_ALERT)
        goto emit;

    rcu_read_lock();
    task = pid_task(find_vpid(sb->pid), PIDTYPE_PID);
    if (!task) {
        rcu_read_unlock();
        goto emit;
    }

    if (sb->breach_action == NG_ACTION_THROTTLE) {
        send_sig(SIGSTOP, task, 1);
        ev->event_type = NG_EVENT_THROTTLE;
        pr_info("neuroguard: SIGSTOP -> PID %d (%s) score=%u\n",
                sb->pid, sb->comm, fp_to_int(sb->ewma_score));
    } else if (sb->breach_action == NG_ACTION_KILL) {
        send_sig(SIGKILL, task, 1);
        ev->event_type = NG_EVENT_KILL;
        pr_warn("neuroguard: SIGKILL -> PID %d (%s) score=%u\n",
                sb->pid, sb->comm, fp_to_int(sb->ewma_score));
    }

    rcu_read_unlock();
emit:
    ng_events_push(ev);
}

void ng_check_sandbox(struct ng_sandbox *sb)
{
    unsigned long rss, delta;
    struct neuroguard_event ev = {};

    if (sb->pid < 0)
        return;

    rss = ng_get_rss(sb->pid);

    if (rss == 0) {

        pr_info("neuroguard: PID %d exited, auto-detaching\n", sb->pid);
        ev.pid        = sb->pid;
        ev.event_type = NG_EVENT_DETACH;
        ev.timestamp_ns = ktime_get_ns();
        strscpy(ev.comm, sb->comm, sizeof(ev.comm));
        ng_events_push(&ev);
        ng_free_sandbox(sb->pid);
        return;
    }

    delta = (rss > sb->prev_rss) ? (rss - sb->prev_rss) : (sb->prev_rss - rss);
    sb->prev_rss  = rss;

    sb->ewma_score = ng_ewma_update(sb->ewma_score, delta);

    if (sb->mem_quota > 0 && rss > sb->mem_quota) {
        ev.pid          = sb->pid;
        ev.timestamp_ns = ktime_get_ns();
        ev.anomaly_score = sb->ewma_score;
        ev.mem_delta_bytes = (long long)delta;
        strscpy(ev.comm, sb->comm, sizeof(ev.comm));
        ev.event_type = NG_EVENT_QUOTA;
        pr_warn("neuroguard: PID %d exceeded quota (%lu > %lu bytes)\n",
                sb->pid, rss, sb->mem_quota);
        ng_apply_action(sb, &ev);
        return;
    }

    if (sb->ewma_score > ng_threshold) {
        ev.pid           = sb->pid;
        ev.timestamp_ns  = ktime_get_ns();
        ev.anomaly_score = sb->ewma_score;
        ev.mem_delta_bytes = (long long)delta;
        strscpy(ev.comm, sb->comm, sizeof(ev.comm));
        ng_apply_action(sb, &ev);
    }
}

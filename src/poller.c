

#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include "include/neuroguard.h"
#include "include/ng_internal.h"

static struct workqueue_struct *ng_wq;
static struct delayed_work      ng_poll_work;

static void ng_poll_fn(struct work_struct *work)
{
    int i;

    for (i = 0; i < NEUROGUARD_MAX_PIDS; i++) {
        struct ng_sandbox *sb = &ng_sandboxes[i];
        spin_lock(&sb->lock);
        if (sb->pid > 0)
            ng_check_sandbox(sb);
        spin_unlock(&sb->lock);
    }

    queue_delayed_work(ng_wq, &ng_poll_work,
                       msecs_to_jiffies(ng_poll_ms));
}

int ng_poller_init(void)
{
    ng_wq = alloc_ordered_workqueue("neuroguard_poll", WQ_MEM_RECLAIM);
    if (!ng_wq) {
        pr_err("neuroguard: failed to create polling workqueue\n");
        return -ENOMEM;
    }

    INIT_DELAYED_WORK(&ng_poll_work, ng_poll_fn);
    queue_delayed_work(ng_wq, &ng_poll_work, msecs_to_jiffies(ng_poll_ms));
    pr_info("neuroguard: poller started (interval=%u ms)\n", ng_poll_ms);
    return 0;
}

void ng_poller_exit(void)
{
    cancel_delayed_work_sync(&ng_poll_work);
    destroy_workqueue(ng_wq);
    pr_info("neuroguard: poller stopped\n");
}

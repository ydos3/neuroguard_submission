

#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include "include/neuroguard.h"
#include "include/ng_internal.h"

static DEFINE_KFIFO(ng_ring, struct neuroguard_event, NEUROGUARD_RING_SIZE);
static DEFINE_SPINLOCK(ng_ring_lock);
static DECLARE_WAIT_QUEUE_HEAD(ng_wait_queue);

int ng_events_init(void)
{

    pr_debug("neuroguard: event ring ready (%u slots)\n", NEUROGUARD_RING_SIZE);
    return 0;
}

void ng_events_exit(void)
{
    spin_lock(&ng_ring_lock);
    kfifo_reset(&ng_ring);
    spin_unlock(&ng_ring_lock);
}

void ng_events_push(const struct neuroguard_event *ev)
{
    unsigned int dropped = 0;

    spin_lock(&ng_ring_lock);

    if (kfifo_is_full(&ng_ring)) {

        struct neuroguard_event tmp;
        kfifo_get(&ng_ring, &tmp);
        dropped = 1;
    }

    kfifo_put(&ng_ring, *ev);
    spin_unlock(&ng_ring_lock);

    if (dropped)
        pr_warn_ratelimited("neuroguard: ring full, oldest event dropped\n");

    wake_up_interruptible(&ng_wait_queue);
}

bool ng_events_available(void)
{
    return !kfifo_is_empty(&ng_ring);
}

int ng_events_wait(struct file *filp)
{
    return wait_event_interruptible(ng_wait_queue, ng_events_available());
}

int ng_events_pop(struct neuroguard_event *ev, unsigned int *copied)
{
    spin_lock(&ng_ring_lock);
    *copied = kfifo_get(&ng_ring, ev);
    spin_unlock(&ng_ring_lock);
    return (*copied) ? 0 : -EAGAIN;
}

void ng_events_poll_wait(struct file *filp, poll_table *wait)
{
    poll_wait(filp, &ng_wait_queue, wait);
}

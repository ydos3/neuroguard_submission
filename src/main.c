

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include "include/neuroguard.h"
#include "include/ng_internal.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yuval");
MODULE_DESCRIPTION("NeuroGuard: per-process memory isolation and EWMA anomaly detection");
MODULE_VERSION("0.1");

int              ng_major;
struct class    *ng_class;
struct device   *ng_device;
struct cdev      ng_cdev;

struct ng_sandbox ng_sandboxes[NEUROGUARD_MAX_PIDS];
DEFINE_SPINLOCK(ng_table_lock);

unsigned int ng_alpha_pct = 30;   
unsigned int ng_threshold  = 1000; 
unsigned int ng_poll_ms    = 500;  

struct ng_sandbox *ng_find_sandbox(pid_t pid)
{
    int i;
    for (i = 0; i < NEUROGUARD_MAX_PIDS; i++) {
        if (ng_sandboxes[i].pid == pid)
            return &ng_sandboxes[i];
    }
    return NULL;
}

struct ng_sandbox *ng_alloc_sandbox(pid_t pid)
{
    int i;
    for (i = 0; i < NEUROGUARD_MAX_PIDS; i++) {
        if (ng_sandboxes[i].pid == -1) {
            ng_sandboxes[i].pid = pid;
            return &ng_sandboxes[i];
        }
    }
    return NULL; 
}

void ng_free_sandbox(pid_t pid)
{
    struct ng_sandbox *sb = ng_find_sandbox(pid);
    if (sb) {
        sb->pid          = -1;
        sb->ewma_score   = 0;
        sb->prev_rss     = 0;
        sb->mem_quota    = 0;
        sb->breach_action = NG_ACTION_ALERT;
    }
}

static int __init neuroguard_init(void)
{
    dev_t dev;
    int ret, i;

    pr_info("neuroguard: initialising\n");

    for (i = 0; i < NEUROGUARD_MAX_PIDS; i++) {
        ng_sandboxes[i].pid = -1;
        spin_lock_init(&ng_sandboxes[i].lock);
        ng_sandboxes[i].breach_action = NG_ACTION_ALERT;
    }

    ret = alloc_chrdev_region(&dev, 0, 1, NEUROGUARD_DEVICE_NAME);
    if (ret < 0) {
        pr_err("neuroguard: alloc_chrdev_region failed (%d)\n", ret);
        return ret;
    }
    ng_major = MAJOR(dev);

    cdev_init(&ng_cdev, &ng_fops);
    ng_cdev.owner = THIS_MODULE;
    ret = cdev_add(&ng_cdev, dev, 1);
    if (ret < 0) {
        pr_err("neuroguard: cdev_add failed (%d)\n", ret);
        goto err_cdev;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    ng_class = class_create(NEUROGUARD_CLASS_NAME);
#else
    ng_class = class_create(THIS_MODULE, NEUROGUARD_CLASS_NAME);
#endif
    if (IS_ERR(ng_class)) {
        ret = PTR_ERR(ng_class);
        pr_err("neuroguard: class_create failed (%d)\n", ret);
        goto err_class;
    }

    ng_device = device_create(ng_class, NULL, dev, NULL, NEUROGUARD_DEVICE_NAME);
    if (IS_ERR(ng_device)) {
        ret = PTR_ERR(ng_device);
        pr_err("neuroguard: device_create failed (%d)\n", ret);
        goto err_device;
    }

    ret = ng_events_init();
    if (ret < 0) {
        pr_err("neuroguard: event ring init failed (%d)\n", ret);
        goto err_events;
    }

    ret = ng_sysfs_init();
    if (ret < 0) {
        pr_err("neuroguard: sysfs init failed (%d)\n", ret);
        goto err_sysfs;
    }

    ret = ng_procfs_init();
    if (ret < 0) {
        pr_err("neuroguard: procfs init failed (%d)\n", ret);
        goto err_procfs;
    }

    ret = ng_poller_init();
    if (ret < 0) {
        pr_err("neuroguard: poller init failed (%d)\n", ret);
        goto err_poller;
    }

    pr_info("neuroguard: ready — /dev/%s (major=%d), alpha=%u%%, threshold=%u\n",
            NEUROGUARD_DEVICE_NAME, ng_major, ng_alpha_pct, ng_threshold);
    return 0;

err_poller:
    ng_procfs_exit();
err_procfs:
    ng_sysfs_exit();
err_sysfs:
    ng_events_exit();
err_events:
    device_destroy(ng_class, MKDEV(ng_major, 0));
err_device:
    class_destroy(ng_class);
err_class:
    cdev_del(&ng_cdev);
err_cdev:
    unregister_chrdev_region(MKDEV(ng_major, 0), 1);
    return ret;
}

static void __exit neuroguard_exit(void)
{
    ng_poller_exit();
    ng_procfs_exit();
    ng_sysfs_exit();
    ng_events_exit();
    device_destroy(ng_class, MKDEV(ng_major, 0));
    class_destroy(ng_class);
    cdev_del(&ng_cdev);
    unregister_chrdev_region(MKDEV(ng_major, 0), 1);
    pr_info("neuroguard: unloaded\n");
}

module_init(neuroguard_init);
module_exit(neuroguard_exit);

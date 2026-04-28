

#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include "include/neuroguard.h"
#include "include/ng_internal.h"

static struct kobject *ng_kobj;

static ssize_t alpha_pct_show(struct kobject *kobj,
                               struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%u\n", ng_alpha_pct);
}

static ssize_t alpha_pct_store(struct kobject *kobj,
                                struct kobj_attribute *attr,
                                const char *buf, size_t count)
{
    unsigned int val;
    if (kstrtouint(buf, 10, &val) || val == 0 || val > 100)
        return -EINVAL;
    ng_alpha_pct = val;
    pr_info("neuroguard: alpha set to %u%%\n", val);
    return count;
}

static struct kobj_attribute ng_attr_alpha =
    __ATTR(alpha_pct, 0664, alpha_pct_show, alpha_pct_store);

static ssize_t threshold_show(struct kobject *kobj,
                               struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%u\n", ng_threshold);
}

static ssize_t threshold_store(struct kobject *kobj,
                                struct kobj_attribute *attr,
                                const char *buf, size_t count)
{
    unsigned int val;
    if (kstrtouint(buf, 10, &val))
        return -EINVAL;
    ng_threshold = val;
    pr_info("neuroguard: threshold set to %u\n", val);
    return count;
}

static struct kobj_attribute ng_attr_threshold =
    __ATTR(threshold, 0664, threshold_show, threshold_store);

static ssize_t poll_ms_show(struct kobject *kobj,
                              struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%u\n", ng_poll_ms);
}

static ssize_t poll_ms_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf, size_t count)
{
    unsigned int val;
    if (kstrtouint(buf, 10, &val) || val < 100 || val > 60000)
        return -EINVAL;
    ng_poll_ms = val;
    pr_info("neuroguard: poll interval set to %u ms\n", val);
    return count;
}

static struct kobj_attribute ng_attr_poll_ms =
    __ATTR(poll_ms, 0664, poll_ms_show, poll_ms_store);

static ssize_t active_pids_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
    int i, len = 0;
    spin_lock(&ng_table_lock);
    for (i = 0; i < NEUROGUARD_MAX_PIDS; i++) {
        if (ng_sandboxes[i].pid > 0 && len < PAGE_SIZE - 12)
            len += scnprintf(buf + len, PAGE_SIZE - len,
                             "%d(%s) ", ng_sandboxes[i].pid,
                             ng_sandboxes[i].comm);
    }
    spin_unlock(&ng_table_lock);
    if (len == 0)
        len = scnprintf(buf, PAGE_SIZE, "(none)");
    buf[len++] = '\n';
    return len;
}

static struct kobj_attribute ng_attr_pids =
    __ATTR(active_pids, 0444, active_pids_show, NULL);

static struct attribute *ng_attrs[] = {
    &ng_attr_alpha.attr,
    &ng_attr_threshold.attr,
    &ng_attr_poll_ms.attr,
    &ng_attr_pids.attr,
    NULL,
};

static struct attribute_group ng_attr_group = {
    .attrs = ng_attrs,
};

int ng_sysfs_init(void)
{
    int ret;
    ng_kobj = kobject_create_and_add("neuroguard", kernel_kobj);
    if (!ng_kobj)
        return -ENOMEM;

    ret = sysfs_create_group(ng_kobj, &ng_attr_group);
    if (ret)
        kobject_put(ng_kobj);

    pr_info("neuroguard: sysfs at /sys/kernel/neuroguard/\n");
    return ret;
}

void ng_sysfs_exit(void)
{
    if (ng_kobj) {
        sysfs_remove_group(ng_kobj, &ng_attr_group);
        kobject_put(ng_kobj);
    }
}



#ifndef _NEUROGUARD_H
#define _NEUROGUARD_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define NEUROGUARD_DEVICE_NAME  "neuroguard"
#define NEUROGUARD_CLASS_NAME   "neuroguard"
#define NEUROGUARD_MAX_PIDS     64      
#define NEUROGUARD_RING_SIZE    512     

#define NG_EVENT_ATTACH     0   
#define NG_EVENT_DETACH     1   
#define NG_EVENT_ALERT      2   
#define NG_EVENT_THROTTLE   3   
#define NG_EVENT_KILL       4   
#define NG_EVENT_QUOTA      5   

#define NG_ACTION_ALERT     0   
#define NG_ACTION_THROTTLE  1   
#define NG_ACTION_KILL      2   

#define NG_IOC_MAGIC        'N'

#define NG_ATTACH_PID       _IOW(NG_IOC_MAGIC, 1, pid_t)

#define NG_DETACH_PID       _IOW(NG_IOC_MAGIC, 2, pid_t)

#define NG_SET_QUOTA        _IOW(NG_IOC_MAGIC, 3, unsigned long)

#define NG_GET_SCORE        _IOR(NG_IOC_MAGIC, 4, unsigned int)

#define NG_SET_ACTION       _IOW(NG_IOC_MAGIC, 5, unsigned int)

struct neuroguard_event {
    __s32   pid;
    __u64   timestamp_ns;
    __u32   anomaly_score;
    __s64   mem_delta_bytes;
    __u32   event_type;
    char    comm[16];
};

struct neuroguard_query {
    __s32   pid;
    __u32   score;
};

#endif 

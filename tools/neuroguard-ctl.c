

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>


#include "../include/neuroguard.h"

#define DEVICE_PATH  "/dev/neuroguard"
#define PROC_SUMMARY "/proc/neuroguard/summary"
#define SYSFS_BASE   "/sys/kernel/neuroguard"

static const char *event_type_str(unsigned int t)
{
    switch (t) {
    case NG_EVENT_ATTACH:   return "ATTACH";
    case NG_EVENT_DETACH:   return "DETACH";
    case NG_EVENT_ALERT:    return "ALERT";
    case NG_EVENT_THROTTLE: return "THROTTLE";
    case NG_EVENT_KILL:     return "KILL";
    case NG_EVENT_QUOTA:    return "QUOTA";
    default:                return "UNKNOWN";
    }
}

static void print_event(const struct neuroguard_event *ev)
{

    char tstr[32];
    time_t sec = (time_t)(ev->timestamp_ns / 1000000000ULL);
    struct tm *tm_info = localtime(&sec);
    strftime(tstr, sizeof(tstr), "%H:%M:%S", tm_info);

    printf("[%s] %-8s  PID=%-6d  COMM=%-16s  SCORE=%-8u  DELTA=%+lld KB\n",
           tstr,
           event_type_str(ev->event_type),
           ev->pid,
           ev->comm,
           ev->anomaly_score >> 16,
           ev->mem_delta_bytes / 1024LL);
    fflush(stdout);
}

static int sysfs_write(const char *name, const char *val)
{
    char path[256];
    FILE *f;
    snprintf(path, sizeof(path), "%s/%s", SYSFS_BASE, name);
    f = fopen(path, "w");
    if (!f) { perror(path); return -1; }
    fputs(val, f);
    fclose(f);
    return 0;
}

static void cat_file(const char *path)
{
    FILE *f = fopen(path, "r");
    char buf[256];
    if (!f) { perror(path); return; }
    while (fgets(buf, sizeof(buf), f))
        fputs(buf, stdout);
    fclose(f);
}

static volatile int running = 1;
static void sig_handler(int s) { (void)s; running = 0; }

static int cmd_monitor(void)
{
    int fd;
    struct neuroguard_event ev;
    ssize_t n;

    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) { perror(DEVICE_PATH); return 1; }

    signal(SIGINT, sig_handler);
    printf("Monitoring %s — press Ctrl-C to stop\n\n", DEVICE_PATH);
    printf("%-10s %-8s  %-6s  %-16s  %-8s  %s\n",
           "TIME", "EVENT", "PID", "COMM", "SCORE", "DELTA");
    printf("%s\n", "----------------------------------------------------------------");

    while (running) {
        n = read(fd, &ev, sizeof(ev));
        if (n == sizeof(ev))
            print_event(&ev);
        else if (n < 0 && errno != EINTR)
            perror("read");
    }

    close(fd);
    printf("\nDone.\n");
    return 0;
}

static int cmd_attach(pid_t pid)
{
    int fd;
    ssize_t n;

    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) { perror(DEVICE_PATH); return 1; }

    n = write(fd, &pid, sizeof(pid));
    close(fd);

    if (n < 0) { perror("attach"); return 1; }
    printf("Attached PID %d\n", pid);
    return 0;
}

static int cmd_detach(pid_t pid)
{
    int fd;
    long ret;

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) { perror(DEVICE_PATH); return 1; }

    ret = ioctl(fd, NG_DETACH_PID, &pid);
    close(fd);

    if (ret < 0) { perror("detach"); return 1; }
    printf("Detached PID %d\n", pid);
    return 0;
}

static int cmd_score(pid_t pid)
{
    int fd;
    long ret;
    struct neuroguard_query qr = { .pid = pid, .score = 0 };

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) { perror(DEVICE_PATH); return 1; }

    ret = ioctl(fd, NG_GET_SCORE, &qr);
    close(fd);

    if (ret < 0) { perror("score"); return 1; }
    printf("PID %d — anomaly score: %u (raw Q16.16: %u)\n",
           pid, qr.score >> 16, qr.score);
    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <command> [args]\n\n"
        "Commands:\n"
        "  monitor                  Stream events from /dev/neuroguard\n"
        "  attach  <pid>            Attach a PID for monitoring\n"
        "  detach  <pid>            Detach a PID\n"
        "  score   <pid>            Print anomaly score for a PID\n"
        "  list                     Print /proc/neuroguard/summary\n"
        "  set alpha <1-100>        Set EWMA alpha %%\n"
        "  set threshold <n>        Set anomaly trigger threshold\n"
        "  set poll_ms <100-60000>  Set sampling interval\n\n"
        "Examples:\n"
        "  sudo %s attach 12345\n"
        "  sudo %s monitor\n"
        "  sudo %s set alpha 50\n\n",
        prog, prog, prog, prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2) { usage(argv[0]); return 1; }

    if (strcmp(argv[1], "monitor") == 0)
        return cmd_monitor();

    if (strcmp(argv[1], "attach") == 0 && argc >= 3)
        return cmd_attach((pid_t)atoi(argv[2]));

    if (strcmp(argv[1], "detach") == 0 && argc >= 3)
        return cmd_detach((pid_t)atoi(argv[2]));

    if (strcmp(argv[1], "score") == 0 && argc >= 3)
        return cmd_score((pid_t)atoi(argv[2]));

    if (strcmp(argv[1], "list") == 0) {
        cat_file(PROC_SUMMARY);
        return 0;
    }

    if (strcmp(argv[1], "set") == 0 && argc >= 4) {
        if (strcmp(argv[2], "alpha")     == 0) return sysfs_write("alpha_pct",  argv[3]);
        if (strcmp(argv[2], "threshold") == 0) return sysfs_write("threshold",  argv[3]);
        if (strcmp(argv[2], "poll_ms")   == 0) return sysfs_write("poll_ms",    argv[3]);
        fprintf(stderr, "Unknown parameter: %s\n", argv[2]);
        return 1;
    }

    usage(argv[0]);
    return 1;
}

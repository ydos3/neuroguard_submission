

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

static volatile int stop = 0;
static void on_signal(int s) { (void)s; stop = 1; }

static void msleep(unsigned int ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

int main(int argc, char *argv[])
{
    unsigned long burst_bytes  = 50UL * 1024 * 1024;   
    unsigned int  interval_ms  = 200;
    int burst = 0;

    if (argc >= 2) burst_bytes = (unsigned long)atol(argv[1]) * 1024 * 1024;
    if (argc >= 3) interval_ms = (unsigned int)atoi(argv[2]);

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    printf("NeuroGuard stress test | PID=%d | burst=%lu MB | interval=%u ms\n",
           getpid(), burst_bytes >> 20, interval_ms);
    printf("Attach with:  sudo ./neuroguard-ctl attach %d\n", getpid());
    printf("Monitor with: sudo ./neuroguard-ctl monitor\n\n");
    printf("Press Ctrl-C to stop.\n\n");

    while (!stop) {
        char *p = malloc(burst_bytes);
        if (!p) { perror("malloc"); break; }

        memset(p, (burst & 0xFF), burst_bytes);

        printf("[burst %3d] allocated %lu MB (press Ctrl-C to exit)\n",
               burst++, burst_bytes >> 20);

        msleep(interval_ms);
        free(p);
        msleep(interval_ms);
    }

    printf("Test ended after %d bursts.\n", burst);
    return 0;
}

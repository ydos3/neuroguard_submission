# NeuroGuard

A Linux kernel character device driver that monitors per-process memory
allocation behaviour and detects anomalies in real time using an EWMA
scoring engine.

---

## Features

- **`/dev/neuroguard`** — character device for attach/detach/event-stream
- **EWMA anomaly detector** — fixed-point Q16.16 arithmetic (no FPU)
- **Graduated response** — ALERT / SIGSTOP / SIGKILL per-process
- **sysfs knobs** — `alpha_pct`, `threshold`, `poll_ms` tuneable at runtime
- **procfs view** — `/proc/neuroguard/summary` and `/proc/neuroguard/config`
- **Workqueue poller** — zero busy-waiting, integrates with CPU hotplug
- **Userspace CLI** — `neuroguard-ctl monitor|attach|detach|score|list|set`

---

## Requirements

- Linux kernel 5.15+ (tested on 6.8)
- Kernel headers: `sudo apt install linux-headers-$(uname -r)`
- GCC (for userspace tools and tests)

---

## Build

```bash
# Build the kernel module
make

# Build the userspace CLI tool
make tools

# Build the stress test
make tests
```

---

## Quick Start

```bash
# 1. Load the module
sudo insmod neuroguard.ko

# Verify the device appeared
ls -l /dev/neuroguard

# 2. Open the event monitor in one terminal
sudo ./tools/neuroguard-ctl monitor

# 3. In a second terminal: run the memory-bomb stress test
./tests/test_anomaly 80 250
# prints: PID=XXXXX — copy this PID

# 4. In a third terminal: attach the bomb
sudo ./tools/neuroguard-ctl attach <PID>

# Watch ALERT events appear in the monitor terminal!

# 5. Switch from alert-only to throttle mode
sudo ./tools/neuroguard-ctl set threshold 500

# 6. Check the sandbox table
cat /proc/neuroguard/summary

# 7. Tune alpha live
echo 60 | sudo tee /sys/kernel/neuroguard/alpha_pct

# 8. Unload cleanly
sudo rmmod neuroguard
```

---

## ioctl Reference

| Command | Direction | Argument | Effect |
|---------|-----------|----------|--------|
| `NG_ATTACH_PID` | write | `pid_t` | Begin monitoring a PID |
| `NG_DETACH_PID` | write | `pid_t` | Stop monitoring a PID |
| `NG_GET_SCORE`  | read  | `struct neuroguard_query` | Read current EWMA score |
| `NG_SET_ACTION` | write | packed `(pid<<16)\|action` | Set breach action |

---

## sysfs Knobs (`/sys/kernel/neuroguard/`)

| File | R/W | Default | Description |
|------|-----|---------|-------------|
| `alpha_pct`  | RW | 30  | EWMA learning rate in percent (1–100) |
| `threshold`  | RW | 1000 | Score level that triggers action |
| `poll_ms`    | RW | 500 | Sampling interval in milliseconds |
| `active_pids`| R  | —   | Space-separated list of monitored PIDs |

---

## Source Layout

```
neuroguard/
├── Makefile
├── include/
│   ├── neuroguard.h       # public kernel+user structs and ioctl defs
│   └── ng_internal.h      # kernel-only types and lifecycle hooks
├── src/
│   ├── main.c             # module init/exit, sandbox table, globals
│   ├── chardev.c          # file_operations (open/read/write/ioctl/poll)
│   ├── events.c           # kfifo ring buffer + wait queue
│   ├── anomaly.c          # EWMA scoring, RSS sampling, breach dispatch
│   ├── poller.c           # delayed_work periodic scanner
│   ├── sysfs.c            # /sys/kernel/neuroguard/ attribute group
│   └── procfs.c           # /proc/neuroguard/ summary and config
├── tools/
│   └── neuroguard-ctl.c   # userspace CLI
├── tests/
│   ├── test_basic.sh      # smoke tests (requires root)
│   └── test_anomaly.c     # memory-bomb stress test
└── submission/
    └── proposal.html      # academic submission document (print → PDF)
```

---

## Run Smoke Tests

```bash
# After loading the module:
sudo bash tests/test_basic.sh
```

---

## License

GPL-2.0 — see `SPDX-License-Identifier` headers in each source file.

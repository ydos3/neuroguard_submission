#!/bin/bash
# test_basic.sh - Smoke tests for the NeuroGuard driver
# Must be run as root after loading the module: sudo insmod neuroguard.ko

set -e

PASS=0
FAIL=0

check() {
    local desc="$1"
    local result="$2"
    if [ "$result" = "0" ]; then
        echo "[PASS] $desc"
        PASS=$((PASS+1))
    else
        echo "[FAIL] $desc"
        FAIL=$((FAIL+1))
    fi
}

echo "=== NeuroGuard Basic Smoke Tests ==="
echo

# 1 - Device exists
check "device node /dev/neuroguard exists" $(test -c /dev/neuroguard; echo $?)

# 2 - sysfs knobs
check "sysfs alpha_pct readable" $(cat /sys/kernel/neuroguard/alpha_pct > /dev/null 2>&1; echo $?)
check "sysfs threshold readable" $(cat /sys/kernel/neuroguard/threshold  > /dev/null 2>&1; echo $?)
check "sysfs poll_ms readable"   $(cat /sys/kernel/neuroguard/poll_ms    > /dev/null 2>&1; echo $?)

# 3 - sysfs write
echo 40 > /sys/kernel/neuroguard/alpha_pct
VAL=$(cat /sys/kernel/neuroguard/alpha_pct)
check "sysfs alpha_pct write/read back ($VAL)" $(test "$VAL" = "40"; echo $?)

# 4 - procfs
check "procfs /proc/neuroguard/summary readable" $(cat /proc/neuroguard/summary > /dev/null 2>&1; echo $?)
check "procfs /proc/neuroguard/config readable"  $(cat /proc/neuroguard/config  > /dev/null 2>&1; echo $?)

# 5 - Attach current shell PID (non-blocking)
SELF=$$
./tools/neuroguard-ctl attach $SELF > /dev/null 2>&1
check "attach self PID ($SELF)" $?

# 6 - PID appears in active_pids
LISTED=$(grep -c "$SELF" /sys/kernel/neuroguard/active_pids 2>/dev/null || echo 0)
check "attached PID visible in active_pids" $(test "$LISTED" -ge 1; echo $?)

# 7 - Detach
./tools/neuroguard-ctl detach $SELF > /dev/null 2>&1
check "detach self PID ($SELF)" $?

# 8 - Reject re-attach after detach (should succeed again)
./tools/neuroguard-ctl attach $SELF > /dev/null 2>&1
check "re-attach after detach" $?
./tools/neuroguard-ctl detach $SELF > /dev/null 2>&1

echo
echo "Results: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ] && exit 0 || exit 1

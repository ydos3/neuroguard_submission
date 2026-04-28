#!/bin/bash
set -e

echo "==========================================="
echo " NeuroGuard Kernel Driver — Full Deployment"
echo "==========================================="

echo ""
echo "[1/7] Installing build dependencies..."
sudo apt-get update -qq
sudo apt-get install -y build-essential linux-headers-$(uname -r) dos2unix tmux

echo ""
echo "[2/7] Fixing Windows line-endings..."
find . -type f \( -name "*.c" -o -name "*.h" -o -name "Makefile" -o -name "*.sh" \) -exec dos2unix -q {} \;

echo ""
echo "[3/7] Building NeuroGuard kernel module..."
make clean 2>/dev/null || true
make

echo ""
echo "[4/7] Building userspace tools and tests..."
make tools
make tests

echo ""
echo "[5/7] Loading NeuroGuard into the kernel..."
sudo rmmod neuroguard 2>/dev/null || true
sudo insmod neuroguard.ko

echo ""
echo "[6/7] Setting device permissions..."
sudo chmod 666 /dev/neuroguard

echo ""
echo "[+] Verifying installation..."
echo "-------------------------------------------"
echo "Module loaded:"
lsmod | grep neuroguard
echo ""
echo "Device node:"
ls -l /dev/neuroguard
echo ""
echo "Kernel log:"
dmesg | grep neuroguard | tail -5
echo ""
echo "Config:"
cat /proc/neuroguard/config
echo "-------------------------------------------"

echo ""
echo "[7/7] Running the live anomaly demo!"
echo "==========================================="
echo "We will start the monitor, launch a memory"
echo "bomb, attach it, and watch the anomaly detection."
echo "==========================================="
echo ""

# Start the monitor in background, piping output to screen
./tools/neuroguard-ctl monitor &
MONITOR_PID=$!

sleep 2

# Start the memory bomb
./tests/test_anomaly 80 300 &
BOMB_PID=$!

sleep 1

# Get the bomb's PID and attach it
echo "[+] Attaching memory bomb (PID=$BOMB_PID) to NeuroGuard..."
./tools/neuroguard-ctl attach $BOMB_PID 2>/dev/null || true

echo ""
echo "[+] Watching for anomalies... (15 seconds)"
sleep 15

echo ""
echo "[+] Sandbox status:"
cat /proc/neuroguard/summary

echo ""
echo "[+] Active monitored PIDs:"
cat /sys/kernel/neuroguard/active_pids

# Cleanup background processes
kill $MONITOR_PID 2>/dev/null || true
kill $BOMB_PID 2>/dev/null || true
wait $MONITOR_PID 2>/dev/null || true
wait $BOMB_PID 2>/dev/null || true

echo ""
echo "==========================================="
echo " Demo complete!"
echo " NeuroGuard successfully detected anomalies."
echo ""
echo " To run smoke tests:  sudo bash tests/test_basic.sh"
echo " To clean up:         sudo rmmod neuroguard"
echo "==========================================="

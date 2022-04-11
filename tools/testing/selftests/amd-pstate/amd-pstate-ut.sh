#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

# amd-pstate-ut is a test module for testing the amd-pstate driver.
# (1) It can help all users to verify their processor support
# (SBIOS/Firmware or Hardware).
# (2) Kernel can have a basic function test to avoid the kernel
# regression during the update.
# (3) We can introduce more functional or performance tests to align
# the result together, it will benefit power and performance scale optimization.

# Kselftest framework requirement - SKIP code is 4.
ksft_skip=4

if ! uname -m | sed -e s/i.86/x86/ -e s/x86_64/x86/ | grep -q x86; then
	echo "$0 # Skipped: Test can only run on x86 architectures."
	exit $ksft_skip
fi

msg="Skip all tests:"
if [ ! -w /dev ]; then
    echo $msg please run this as root >&2
    exit $ksft_skip
fi

scaling_driver=$(cat /sys/devices/system/cpu/cpufreq/policy0/scaling_driver)

if [ "$scaling_driver" != "amd-pstate" ]; then
	echo "$0 # Skipped: Test can only run on amd-pstate driver."
	echo "$0 # Current cpufreq scaling drvier is $scaling_driver."
	exit $ksft_skip
fi

kernel_release=`uname -r`
sudo cp amd-pstate-ut.ko /lib/modules/$kernel_release/
if [ $? -eq 1 ];then
	echo $msg please check test module >&2
	exit $ksft_skip
fi
sudo depmod

$(dirname $0)/../kselftest/module.sh "amd-pstate-ut" amd-pstate-ut

sudo rm /lib/modules/$kernel_release/amd-pstate-ut.ko
sudo depmod

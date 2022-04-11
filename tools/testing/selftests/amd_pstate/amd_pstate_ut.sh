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

scaling_driver=$(cat /sys/devices/system/cpu/cpufreq/policy0/scaling_driver)
amd_pstate_driver=amd-pstate

if [ "$scaling_driver" != "$amd_pstate_driver" ]; then
	echo "$0 # Skipped: Test can only run on amd_pstate driver."
	exit $ksft_skip
fi

$(dirname $0)/../kselftest/module.sh "amd_pstate_ut" amd_pstate_ut

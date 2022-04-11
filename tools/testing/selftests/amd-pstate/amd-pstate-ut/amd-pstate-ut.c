// SPDX-License-Identifier: GPL-1.0-or-later
/*
 * AMD Processor P-state Frequency Driver Unit Test
 *
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 * Author: Meng Li <li.meng@amd.com>
 *
 * The AMD P-State Unit Test is a test module for testing the amd-pstate
 * driver. 1) It can help all users to verify their processor support
 * (SBIOS/Firmware or Hardware). 2) Kernel can have a basic function
 * test to avoid the kernel regression during the update. 3) We can
 * introduce more functional or performance tests to align the result
 * together, it will benefit power and performance scale optimization.
 *
 * At present, it only implements the basic framework and some simple
 * test cases. Next, 1) we will add a rst document. 2) we will add more
 * test cases to improve the depth and coverage of the test.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "../../kselftest_module.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/amd-pstate.h>

#include <acpi/cppc_acpi.h>

/*
 * Abbreviations:
 * aput: used as a shortform for AMD P-State unit test.
 * It helps to keep variable names smaller, simpler
 */

KSTM_MODULE_GLOBALS();

/*
 * Kernel module for testing the AMD P-State unit test
 */
enum aput_result {
	APUT_RESULT_PASS,
	APUT_RESULT_FAIL,
	MAX_APUT_RESULT,
};

struct aput_struct {
	const char *name;
	void (*func)(u32 index);
	enum aput_result result;
};

static void aput_acpi_cpc(u32 index);
static void aput_check_enabled(u32 index);
static void aput_check_perf(u32 index);
static void aput_check_freq(u32 index);

static struct aput_struct aput_cases[] = {
	{"acpi_cpc_valid",   aput_acpi_cpc         },
	{"check_enabled",    aput_check_enabled    },
	{"check_perf",       aput_check_perf       },
	{"check_freq",       aput_check_freq       }
};

static bool get_shared_mem(void)
{
	bool result = false;
	char buf[5] = {0};
	struct file *filp = NULL;
	loff_t pos = 0;
	ssize_t ret;

	if (!boot_cpu_has(X86_FEATURE_CPPC)) {
		filp = filp_open("/sys/module/amd_pstate/parameters/shared_mem", FMODE_PREAD, 0);
		if (IS_ERR(filp))
			pr_err("%s Open param file fail!\n", __func__);
		else {
			ret = kernel_read(filp, &buf, sizeof(buf), &pos);
			if (ret < 0)
				pr_err("%s ret=%ld unable to read from param file!\n",
					__func__, ret);
			filp_close(filp, NULL);
		}

		if ('Y' == *buf)
			result = true;
	}

	return result;
}

static void aput_acpi_cpc(u32 index)
{
	if (acpi_cpc_valid())
		aput_cases[index].result = APUT_RESULT_PASS;
	else
		aput_cases[index].result = APUT_RESULT_FAIL;
}

static void aput_pstate_enable(u32 index)
{
	int ret = 0;
	u64 cppc_enable = 0;

	ret = rdmsrl_safe(MSR_AMD_CPPC_ENABLE, &cppc_enable);
	if (ret) {
		aput_cases[index].result = APUT_RESULT_FAIL;
		pr_err("%s rdmsrl_safe MSR_AMD_CPPC_ENABLE ret=%d is error!\n", __func__, ret);
		return;
	}
	if (cppc_enable)
		aput_cases[index].result = APUT_RESULT_PASS;
	else
		aput_cases[index].result = APUT_RESULT_FAIL;
}

/*
 *Check if enabled amd pstate
 */
static void aput_check_enabled(u32 index)
{
	if (get_shared_mem())
		aput_cases[index].result = APUT_RESULT_PASS;
	else
		aput_pstate_enable(index);
}

/*
 * Check if the each performance values are reasonable.
 * highest_perf >= nominal_perf > lowest_nonlinear_perf > lowest_perf > 0
 */
static void aput_check_perf(u32 index)
{
	int cpu = 0, ret = 0;
	u32 highest_perf = 0, nominal_perf = 0, lowest_nonlinear_perf = 0, lowest_perf = 0;
	u64 cap1 = 0;
	struct cppc_perf_caps cppc_perf;
	struct cpufreq_policy *policy = NULL;
	struct amd_cpudata *cpudata = NULL;

	highest_perf = amd_get_highest_perf();

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;
		cpudata = policy->driver_data;

		if (get_shared_mem()) {
			ret = cppc_get_perf_caps(cpu, &cppc_perf);
			if (ret) {
				aput_cases[index].result = APUT_RESULT_FAIL;
				pr_err("%s cppc_get_perf_caps ret=%d is error!\n", __func__, ret);
				return;
			}

			nominal_perf = cppc_perf.nominal_perf;
			lowest_nonlinear_perf = cppc_perf.lowest_nonlinear_perf;
			lowest_perf = cppc_perf.lowest_perf;
		} else {
			ret = rdmsrl_safe_on_cpu(cpu, MSR_AMD_CPPC_CAP1, &cap1);
			if (ret) {
				aput_cases[index].result = APUT_RESULT_FAIL;
				pr_err("%s read CPPC_CAP1 ret=%d is error!\n", __func__, ret);
				return;
			}

			nominal_perf = AMD_CPPC_NOMINAL_PERF(cap1);
			lowest_nonlinear_perf = AMD_CPPC_LOWNONLIN_PERF(cap1);
			lowest_perf = AMD_CPPC_LOWEST_PERF(cap1);
		}

		if ((highest_perf != READ_ONCE(cpudata->highest_perf)) ||
			(nominal_perf != READ_ONCE(cpudata->nominal_perf)) ||
			(lowest_nonlinear_perf != READ_ONCE(cpudata->lowest_nonlinear_perf)) ||
			(lowest_perf != READ_ONCE(cpudata->lowest_perf))) {
			aput_cases[index].result = APUT_RESULT_FAIL;
			pr_err("%s cpu%d highest=%d %d nominal=%d %d lowest_nonlinear=%d %d lowest=%d %d are not equal!\n",
				__func__, cpu, highest_perf, cpudata->highest_perf,
				nominal_perf, cpudata->nominal_perf,
				lowest_nonlinear_perf, cpudata->lowest_nonlinear_perf,
				lowest_perf, cpudata->lowest_perf);
			return;
		}

		if (!((highest_perf >= nominal_perf) &&
			(nominal_perf > lowest_nonlinear_perf) &&
			(lowest_nonlinear_perf > lowest_perf) &&
			(lowest_perf > 0))) {
			aput_cases[index].result = APUT_RESULT_FAIL;
			pr_err("%s cpu%d highest=%d nominal=%d lowest_nonlinear=%d lowest=%d have error!\n",
				__func__, cpu, highest_perf, nominal_perf,
				lowest_nonlinear_perf, lowest_perf);
			return;
		}
	}

	aput_cases[index].result = APUT_RESULT_PASS;
}

/*
 * Check if the each frequency values are reasonable.
 * max_freq >= nominal_freq > lowest_nonlinear_freq > min_freq > 0
 * check max freq when set support boost mode.
 */
static void aput_check_freq(u32 index)
{
	int cpu = 0;
	struct cpufreq_policy *policy = NULL;
	struct amd_cpudata *cpudata = NULL;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;
		cpudata = policy->driver_data;

		if (!((cpudata->max_freq >= cpudata->nominal_freq) &&
			(cpudata->nominal_freq > cpudata->lowest_nonlinear_freq) &&
			(cpudata->lowest_nonlinear_freq > cpudata->min_freq) &&
			(cpudata->min_freq > 0))) {
			aput_cases[index].result = APUT_RESULT_FAIL;
			pr_err("%s cpu%d max=%d nominal=%d lowest_nonlinear=%d min=%d have error!\n",
				__func__, cpu, cpudata->max_freq, cpudata->nominal_freq,
				cpudata->lowest_nonlinear_freq, cpudata->min_freq);
			return;
		}

		if (cpudata->min_freq != policy->min) {
			aput_cases[index].result = APUT_RESULT_FAIL;
			pr_err("%s cpu%d cpudata_min_freq=%d policy_min=%d have error!\n",
				__func__, cpu, cpudata->min_freq, policy->min);
			return;
		}

		if (cpudata->boost_supported) {
			if ((policy->max == cpudata->max_freq) ||
					(policy->max == cpudata->nominal_freq))
				aput_cases[index].result = APUT_RESULT_PASS;
			else {
				aput_cases[index].result = APUT_RESULT_FAIL;
				pr_err("%s cpu%d policy_max=%d cpu_max=%d cpu_nominal=%d have error!\n",
					__func__, cpu, policy->max, cpudata->max_freq,
					cpudata->nominal_freq);
				return;
			}
		} else {
			aput_cases[index].result = APUT_RESULT_FAIL;
			pr_err("%s cpu%d not support boost!\n", __func__, cpu);
			return;
		}
	}
}

static void aput_do_test_case(void)
{
	u32 i = 0, arr_size = ARRAY_SIZE(aput_cases);

	for (i = 0; i < arr_size; i++) {
		pr_info("****** Begin %-5d\t %-20s\t ******\n", i+1, aput_cases[i].name);
		aput_cases[i].func(i);
		KSTM_CHECK_ZERO(aput_cases[i].result);
		pr_info("****** End   %-5d\t %-20s\t ******\n", i+1, aput_cases[i].name);
	}
}

static void __init selftest(void)
{
	aput_do_test_case();
}

KSTM_MODULE_LOADERS(amd_pstate_ut);
MODULE_AUTHOR("Meng Li <li.meng@amd.com>");
MODULE_DESCRIPTION("Unit test for AMD P-state driver");
MODULE_LICENSE("GPL");

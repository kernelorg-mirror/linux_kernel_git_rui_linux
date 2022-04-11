# SPDX-License-Identifier: GPL-2.0-only
# Makefile for amd-pstate/ function selftests

uname_M := $(shell uname -m 2>/dev/null || echo not)
ARCH ?= $(shell echo $(uname_M) | sed -e s/i.86/x86/ -e s/x86_64/x86/)

ifeq (x86,$(ARCH))
TEST_GEN_FILES := amd-pstate-ut/amd-pstate-ut.ko
endif

TEST_PROGS := amd-pstate-ut.sh

include ../lib.mk

$(TEST_GEN_FILES): $(HEADERS)

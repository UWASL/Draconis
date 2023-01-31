# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# binary name
APP = DraconisBackend.o
# APP = DraconisClient.o
# APP = DraconisDPDKServer.o

# all source are stored in SRCS-y
SRCS-y := /dpdk/Draconis_Combined/Draconis_Backend.cpp
# SRCS-y := /dpdk/Draconis_Combined/Draconis_Client.cpp
# SRCS-y := /dpdk/Draconis_Combined/Draconis_DPDK_Server.cpp

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

CFLAGS += -std=c++11
# CFLAGS += $(WERROR_FLAGS)

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_main.o += -Wno-return-type
endif

EXTRA_CFLAGS += -O3 -g -Wfatal-errors

include $(RTE_SDK)/mk/rte.extapp.mk

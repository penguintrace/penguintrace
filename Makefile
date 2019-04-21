#-----------------------------------------------------------------
# Copyright (C) 2019 Alex Beharrell
#
# This file is part of penguinTrace.
#
# penguinTrace is free software: you can redistribute it and/or
# modify it under the terms of the GNU Affero General Public
# License as published by the Free Software Foundation, either
# version 3 of the License, or any later version.
#
# penguinTrace is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public
# License along with penguinTrace.  If not, see
# <https://www.gnu.org/licenses/>.
#-----------------------------------------------------------------

include make.cfg

# Arch definitions
MACHINE_ARCH=$(shell uname -m)
KERNEL_VERSION=$(shell uname -r)

# Other conditional code
ifeq ($(USE_CAP),1)
	CAPSRC=capabilities/enabled
ifeq ($(findstring Microsoft,$(KERNEL_VERSION)), Microsoft)
$(error "WSL does not support user_namespaces, so cannot use isolation")
endif
else
	CAPSRC=capabilities/disabled
endif

trimslashes = $(if $(filter %/,$(1)),$(call trimslashes,$(patsubst %/,%,$(1))),$(1))

BUILDDIR = $(call trimslashes, $(BUILD))

CLI_EXES = step-elf run-elf dwarf-info compile session-id-gen
SRV_EXES = penguintrace
CLI_BUILT_EXES = $(addprefix $(BUILDDIR)/bin/,$(CLI_EXES))
SRV_BUILT_EXES = $(addprefix $(BUILDDIR)/bin/,$(SRV_EXES))
ALL_BUILT_EXES = $(CLI_BUILT_EXES) $(SRV_BUILT_EXES)

SRC_DIRS = common debug object dwarf $(MACHINE_ARCH) $(CAPSRC)
SRV_SRC_DIRS = server penguintrace
SRCS = $(wildcard $(addsuffix /*.cpp, $(SRC_DIRS)))
SRV_SRCS = $(wildcard $(addsuffix /*.cpp, $(SRV_SRC_DIRS))) server/static_files.cpp
ALL_SRCS = $(SRCS) $(SRV_SRCS)
OBJS = $(addprefix $(BUILDDIR)/,$(subst .cpp,.o,$(SRCS)))
SRV_OBJS = $(addprefix $(BUILDDIR)/,$(subst .cpp,.o,$(SRV_SRCS)))
ALL_OBJS = $(OBJS) $(SRV_OBJS)
DEPS = $(addprefix $(BUILDDIR)/,$(subst .cpp,.d,$(ALL_SRCS)))

DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.d
#CFLAGS += -Wall -Werror
CXXFLAGS += -I$(MACHINE_ARCH) -Wall -Werror -std=c++11 -pthread $(DEPFLAGS)
LDFLAGS = -lutil
ifeq ($(USE_CAP),1)
	LDFLAGS+= -lcap
endif

EXTRA_DEPS = dwarf/definitions.h server/static_files.h server/static_files.cpp debug/syscall.h

CXXFLAGS += -DMACHINE_ARCH='"$(MACHINE_ARCH)"'

ifeq ($(USE_ELF),1)
	CXXFLAGS += -DUSE_ELF=1
endif

# Use LLVM (will want to depend on LLVM for disassembly)
# Needs: llvm-dev libclang-dev
ifeq ($(USE_LLVM),1)
ifeq ($(PROF), 0)
	CC=clang
	CXX=clang++
endif
	CXXFLAGS += -I$(shell llvm-config --includedir)
	CXXFLAGS += -DUSE_LIBCLANG=1
	CXXFLAGS += -DUSE_LIBLLVM=1
	LDFLAGS += $(shell llvm-config --ldflags)
	LDFLAGS += -lclang
	LDFLAGS += -lLLVM
	CXXFLAGS += -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
endif

ifeq ($(DEBUG),1)
	CXXFLAGS += -g
endif

ifeq ($(OPT),1)
	CXXFLAGS += -O3
endif

ifeq ($(PROF), 1)
	CXXFLAGS += -pg
	LDFLAGS += -pg
endif

all:   $(ALL_BUILT_EXES)

.SECONDEXPANSION:

$(SRV_BUILT_EXES):	$(ALL_OBJS) $(BUILDDIR)/$$(@F).o
	@mkdir -p $(@D)
	@echo "  [LINK] $@ (server)"
	$(Q)$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(CLI_BUILT_EXES):	$(OBJS) $(BUILDDIR)/$$(@F).o
	@mkdir -p $(@D)
	@echo "  [LINK] $@ (cmdline)"
	$(Q)$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Don't want to include extra deps for all objects
#  so included as an order only rule
$(BUILDDIR)/%.o:	%.cpp %.d | $(EXTRA_DEPS)
	@mkdir -p $(@D)
	@echo "  [ CXX] $<"
	$(Q)$(CXX) -c $(CXXFLAGS) -o $@ $<

#$(BUILDDIR)/%.o:	%.c
#	@mkdir -p $(@D)
#	@echo "  [  CC] $<"
#	$(Q)$(CC) -c $(CFLAGS) -o $@ $<

clean:
	@echo "  [  RM] Cleaning $(BUILDDIR)"
	$(Q)rm -rf $(BUILDDIR)
	@echo "  [  RM] Cleaning generated files"
	$(Q)rm -rf $(EXTRA_DEPS)
	@echo "  [  RM] Cleaning source bundle"
	$(Q)rm -rf static/source.tar
	$(Q)rm -rf static/source.tar.gz

%.d: ;

dwarf/definitions.h : tools/generate_dwarf_defs
	@mkdir -p $(@D)
	@echo "  [ GEN] $@"
	$(Q)tools/generate_dwarf_defs > $@

debug/syscall.h : tools/generate_syscalls
	@mkdir -p $(@D)
	@echo "  [ GEN] $@"
	$(Q)tools/generate_syscalls > $@

STATIC_FILES = $(shell find static/ -type f -name '*')

ifeq ($(NO_UPDATE_BUNDLE),1)
	SRC_FILES =
else
ifeq (, $(shell which git))
	SRC_FILES = $(shell find . -type f -name '*')
else
	SRC_FILES = $(shell git ls-files)
endif
endif
static/source.tar: $(SRC_FILES)
	@echo "  [ TAR] $@"
	$(Q)tar -cf $@ $(SRC_FILES)

static/source.tar.gz: static/source.tar
	@echo "  [GZIP] $@"
	$(Q)gzip -k -f $<

server/static_files.cpp: tools/generate_static $(STATIC_FILES) static/source.tar.gz
	@echo "  [ GEN] $@"
	$(Q)tools/generate_static IMPL > $@

server/static_files.h: tools/generate_static $(STATIC_FILES) static/source.tar.gz
	@echo "  [ GEN] $@"
	$(Q)tools/generate_static HDR > $@

# Explicit dependencies
$(BUILDDIR)/server/FileResponseBuilder.o: server/static_files.h

$(BUILDDIR)/server/RouteTable.o: server/static_files.h

-include $(DEPS)

# Standalone syntax/compile guard for selected eigrpd files.
# This is not a replacement for the FRR build/link gate.

CC ?= gcc
PROJECT_ROOT := ..
EIGRPD_ROOT := $(PROJECT_ROOT)/eigrpd
FRR_ROOT ?= ../../frr
BUILD := obj
LOGS := logs

COMMON_CFLAGS := \
	-std=gnu11 \
	-fsyntax-only \
	-Wall -Wextra \
	-Wno-unused-parameter \
	-Wno-unused-function \
	-Wno-missing-field-initializers \
	-Werror=implicit-function-declaration \
	-Werror=incompatible-pointer-types \
	-Werror=int-conversion \
	-Werror=return-type \
	-Iinclude \
	-Iinclude/lib \
	-I$(PROJECT_ROOT) \
	-I$(EIGRPD_ROOT) \
	-I$(FRR_ROOT) \
	-DEIGRP_STANDALONE_BUILD

# Compile every real daemon translation unit. Generated *_clippy.c fragments are
# included by their owning source file and are not standalone translation units.
ALL_SOURCES := $(filter-out $(EIGRPD_ROOT)/%_clippy.c,$(sort $(wildcard $(EIGRPD_ROOT)/*.c)))
SMOKE_SOURCES := $(ALL_SOURCES)

all: all-sources

smoke: $(LOGS)
	@set -e; \
	for src in $(SMOKE_SOURCES); do \
	  echo "CC $$src"; \
	  $(CC) $(COMMON_CFLAGS) $$src >$(LOGS)/$$(basename $$src).log 2>&1 || { cat $(LOGS)/$$(basename $$src).log; exit 1; }; \
	done

all-sources: $(LOGS)
	@set -e; \
	for src in $(ALL_SOURCES); do \
	  echo "CC $$src"; \
	  $(CC) $(COMMON_CFLAGS) $$src >$(LOGS)/$$(basename $$src).log 2>&1 || { cat $(LOGS)/$$(basename $$src).log; exit 1; }; \
	done

cli: smoke

list:
	@printf '%s\n' $(SMOKE_SOURCES)

$(LOGS):
	@mkdir -p $(LOGS)

clean:
	rm -rf $(LOGS) $(BUILD)

.PHONY: all smoke all-sources cli list clean

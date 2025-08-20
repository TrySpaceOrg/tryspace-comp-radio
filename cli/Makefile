# Makefile for TrySpace radionstration component CLI development
.PHONY: all build clean debug runtime start stop

export BUILDDIR ?= $(CURDIR)/build
export BUILD_IMAGE ?= tryspaceorg/tryspace-lab:0.0.0
export RUNTIME_NAME ?= tryspace-comp-radio-cli
export TRYLABDIR ?= $(CURDIR)/../../..
export TGTNAME ?= cpu1

# Determine number of parallel jobs to avoid maxing out low-power systems (Raspberry Pi etc.).
# Use `nproc - 1` but ensure at least 1 job.
NPROC := $(shell nproc 2>/dev/null || echo 1)
JOBS := $(shell if [ $(NPROC) -le 1 ]; then echo 1; else expr $(NPROC) - 1; fi)

# Commands
all: build

build:
	docker run --rm -it -v $(TRYLABDIR):$(TRYLABDIR) --name $(RUNTIME_NAME) -w $(CURDIR) $(BUILD_IMAGE) make -j$(JOBS) build-cli

build-cli:
	mkdir -p $(BUILDDIR)
	cd $(BUILDDIR) && cmake .. -DTGTNAME=$(TGTNAME)
	$(MAKE) --no-print-directory -C $(BUILDDIR)

clean:
	docker run --rm -it -v $(CURDIR):$(CURDIR) --name $(RUNTIME_NAME) -w $(CURDIR) $(BUILD_IMAGE) rm -rf $(BUILDDIR)

debug:
	docker run --rm -it -v $(TRYLABDIR):$(TRYLABDIR) --name $(RUNTIME_NAME) -w $(CURDIR) $(BUILD_IMAGE) /bin/bash

runtime:
	$(MAKE) clean build && docker build -t $(RUNTIME_NAME) -f ../support/Dockerfile.cli .

start:
	docker run --rm -it -v $(TRYLABDIR):$(TRYLABDIR) --name "tryspace_comp_run" $(BUILD_IMAGE) ./$(BUILDDIR)/radio_cli

stop:
	docker ps --filter name=tryspace-* | xargs docker stop

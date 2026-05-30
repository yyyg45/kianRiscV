# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Hirosh Dabui <hirosh@dabui.de>

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        bsdextrautils \
        cmake \
        device-tree-compiler \
        file \
        g++ \
        gcc \
        gcc-riscv64-unknown-elf \
        git \
        make \
        ninja-build \
        picolibc-riscv64-unknown-elf \
        python3 \
        python3-pip \
        python3-venv \
        xz-utils && \
    rm -rf /var/lib/apt/lists/*

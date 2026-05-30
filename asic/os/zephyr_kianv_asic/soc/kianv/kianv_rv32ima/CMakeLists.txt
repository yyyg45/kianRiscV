# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Hirosh Dabui <hirosh@dabui.de>

set(SOC_LINKER_SCRIPT ${ZEPHYR_BASE}/include/zephyr/arch/riscv/common/linker.ld CACHE INTERNAL "")

zephyr_include_directories(.)
zephyr_sources(soc.c)

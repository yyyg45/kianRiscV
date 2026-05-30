# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Hirosh Dabui <hirosh@dabui.de>

cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND SOC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/Kconfig)
file(WRITE ${CMAKE_BINARY_DIR}/Kconfig/kconfig_module_dirs.cmake "set(kconfig_env_dirs)\n")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(kianv_asic_zephyr)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE drivers/serial/uart_kianv_poll.c)

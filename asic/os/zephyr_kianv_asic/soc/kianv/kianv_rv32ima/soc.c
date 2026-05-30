// SPDX-License-Identifier: Apache-2.0
/* Copyright (c) 2026 Hirosh Dabui <hirosh@dabui.de> */

#include <zephyr/arch/cpu.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/sys/sys_io.h>

#define KIANV_SYSCON_BASE 0x11100000u
#define KIANV_POWEROFF    0x5555u
#define KIANV_REBOOT      0x7777u

void sys_arch_reboot(int type)
{
	ARG_UNUSED(type);
	sys_write32(KIANV_REBOOT, KIANV_SYSCON_BASE);
}

void z_sys_poweroff(void)
{
	sys_write32(KIANV_POWEROFF, KIANV_SYSCON_BASE);
	CODE_UNREACHABLE;
}

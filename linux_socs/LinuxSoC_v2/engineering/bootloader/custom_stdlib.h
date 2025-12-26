/*
 *  kianv.v - RISC-V rv32ima
 *
 *  copyright (c) 2025 hirosh dabui <hirosh@dabui.de>
 *
 *  permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  the software is provided "as is" and the author disclaims all warranties
 *  with regard to this software including all implied warranties of
 *  merchantability and fitness. in no event shall the author be liable for
 *  any special, direct, indirect, or consequential damages or any damages
 *  whatsoever resulting from loss of use, data or profits, whether in an
 *  action of contract, negligence or other tortious action, arising out of
 *  or in connection with the use or performance of this software.
 *
 */
#pragma once
#include "kianv_io_utils.h"

/* ANSI Color Codes */
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_PURPLE "\x1b[35m"
#define COLOR_RESET "\x1b[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_CYAN_BOLD "\x1b[1;36m"
#define COLOR_CYAN_BRIGHT "\x1b[96m"
#define COLOR_RESET "\x1b[0m"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

char* malloc();
int printf(const char* format, ...);

void* memcpy(void* dest, const void* src, long n);
char* strcpy(char* dest, const char* src);
int strcmp(const char* s1, const char* s2);

/*
 * Rapid Analysis QEMU System Emulator
 *
 * Copyright (c) 2020 Cromulence LLC
 *
 * Distribution Statement A
 *
 * Approved for Public Release, Distribution Unlimited
 *
 * Authors:
 *  Joseph Walker
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 * 
 * The creation of this code was funded by the US Government.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qom/cpu.h"

void notify_exec_instruction(CPUState *cs, uint64_t vaddr);
void notify_read_memory(CPUState *cs, uint64_t paddr, uint8_t *value, int size);
void notify_write_memory(CPUState *cs, uint64_t paddr, const uint8_t *value, int size);
void notify_breakpoint_hit(CPUState *cs, OSBreakpoint* bp);
void notify_exception(int32_t exception);
void notify_syscall(uint64_t number, ...);
void notify_interrupt(int mask);

bool is_memread_instrumentation_enabled(void);
bool is_memwrite_instrumentation_enabled(void);
bool is_exec_instrumentation_enabled(void);
bool is_syscall_instrumentation_enabled(void);
bool is_interrupt_instrumentation_enabled(void);

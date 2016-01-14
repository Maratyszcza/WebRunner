#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

#include <cpuid.h>

#include "perfctr.h"

#define COUNT_OF(x) (sizeof(x) / sizeof(x[0]))

struct x86_cpu_info {
	uint32_t display_model;
	uint32_t display_family;
};

struct x86_cpu_info get_x86_cpu_info(void) {
	uint32_t eax, ebx, ecx, edx;
	__cpuid(1, eax, ebx, ecx, edx);
	const uint32_t model = (eax >> 4) & 0xF;
	const uint32_t family = (eax >> 8) & 0xF;
	const uint32_t extended_model = (eax >> 16) & 0xF;
	const uint32_t extended_family = (eax >> 20) & 0xFF;
	uint32_t display_family = family;
	if (family == 0xF) {
		display_family += extended_family;
	}
	uint32_t display_model = model;
	if ((family == 0x6) || (family == 0xF)) {
		display_model += extended_model << 4;
	}
	return (struct x86_cpu_info) {
		.display_model = display_model,
		.display_family = display_family
	};
}

struct performance_counter_specification {
	const char* name;
	uint8_t event;
	uint8_t umask;
	uint8_t inv;
	uint8_t cmask;
	uint8_t edge;
};

// DisplayFamily_DisplayModel == 06_3DH or 06_47H
const struct performance_counter_specification broadwell_specification[] = {
	{ .name = "LD_BLOCKS.STORE_FORWARD", .event = 0x03, .umask = 0x02 },
	{ .name = "LD_BLOCKS.NO_SR", .event = 0x03, .umask = 0x08 },
	{ .name = "MISALIGN_MEM_REF.LOADS", .event = 0x05, .umask = 0x01 },
	{ .name = "MISALIGN_MEM_REF.STORES", .event = 0x05, .umask = 0x02 },
	{ .name = "UOPS_ISSUED.ANY", .event = 0x0E, .umask = 0x01 },
	{ .name = "UOPS_ISSUED.FLAGS_MERGE", .event = 0x0E, .umask = 0x10 },
	{ .name = "UOPS_ISSUED.SLOW_LEA", .event = 0x0E, .umask = 0x20 },
	{ .name = "UOPS_ISSUED.SINGLE_MUL", .event = 0x0E, .umask = 0x40 },
	{ .name = "ARITH.FPU_DIV_ACTIVE", .event = 0x14, .umask = 0x01 },
	{ .name = "L1D.REPLACEMENT", .event = 0x51, .umask = 0x01 },
	{ .name = "MOVE_ELIMINATION.INT_NOT_ELIMINATED", .event = 0x58, .umask = 0x04 },
	{ .name = "MOVE_ELIMINATION.SIMD_NOT_ELIMINATED", .event = 0x58, .umask = 0x08 },
	{ .name = "MOVE_ELIMINATION.INT_ELIMINATED", .event = 0x58, .umask = 0x01 },
	{ .name = "MOVE_ELIMINATION.SIMD_ELIMINATED", .event = 0x58, .umask = 0x02 },
	{ .name = "RS_EVENTS.EMPTY_CYCLES", .event = 0x5E, .umask = 0x01 },
	{ .name = "IDQ.EMPTY", .event = 0x79, .umask = 0x02 },
	{ .name = "IDQ.MITE_UOPS", .event = 0x79, .umask = 0x04 },
	{ .name = "IDQ.DSB_UOPS", .event = 0x79, .umask = 0x08 },
	{ .name = "IDQ.MS_DSB_UOPS", .event = 0x79, .umask = 0x10 },
	{ .name = "IDQ.MS_MITE_UOPS", .event = 0x79, .umask = 0x20 },
	{ .name = "IDQ.MS_UOPS", .event = 0x79, .umask = 0x30 },
	{ .name = "IDQ.ALL_DSB_CYCLES_ANY_UOPS", .event = 0x79, .umask = 0x18, .cmask = 1 },
	{ .name = "IDQ.ALL_DSB_CYCLES_4_UOPS", .event = 0x79, .umask = 0x18, .cmask = 4 },
	{ .name = "IDQ.ALL_MITE_CYCLES_ANY_UOPS", .event = 0x79, .umask = 0x18, .cmask = 1 },
	{ .name = "IDQ.ALL_MITE_CYCLES_4_UOPS", .event = 0x79, .umask = 0x18, .cmask = 4 },
	{ .name = "IDQ.MITE_ALL_UOPS", .event = 0x79, .umask = 0x3C },
	{ .name = "ICACHE.MISSES", .event = 0x80, .umask = 0x02 },
	{ .name = "ILD_STALL.LCP", .event = 0x87, .umask = 0x01 },
	{ .name = "IDQ_UOPS_NOT_DELIVERED.CORE", .event = 0x9C, .umask = 0x01 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_0", .event = 0xA1, .umask = 0x01 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_1", .event = 0xA1, .umask = 0x02 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_2", .event = 0xA1, .umask = 0x04 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_3", .event = 0xA1, .umask = 0x08 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_4", .event = 0xA1, .umask = 0x10 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_5", .event = 0xA1, .umask = 0x20 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_6", .event = 0xA1, .umask = 0x40 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_7", .event = 0xA1, .umask = 0x80 },
	{ .name = "RESOURCE_STALLS.ANY", .event = 0xA2, .umask = 0x01 },
	{ .name = "RESOURCE_STALLS.RS", .event = 0xA2, .umask = 0x04 },
	{ .name = "RESOURCE_STALLS.SB", .event = 0xA2, .umask = 0x08 },
	{ .name = "RESOURCE_STALLS.ROB", .event = 0xA2, .umask = 0x10 },
	{ .name = "LSD.UOPS", .event = 0xA8, .umask = 0x01 },
	{ .name = "DSB2MITE_SWITCHES.PENALTY_CYCLES", .event = 0xAB, .umask = 0x02 },
	{ .name = "UOPS_EXECUTED.THREAD", .event = 0xB1, .umask = 0x01 },
	{ .name = "UOPS_EXECUTED.THREAD.STALLS", .event = 0xB1, .umask = 0x01, .cmask = 1 },
	{ .name = "INST_RETIRED.ANY_P", .event = 0xC0, .umask = 0x00 },
	{ .name = "INST_RETIRED.X87", .event = 0xC0, .umask = 0x02 },
	{ .name = "OTHER_ASSISTS.AVX_TO_SSE", .event = 0xC1, .umask = 0x08 },
	{ .name = "OTHER_ASSISTS.SSE_TO_AVX", .event = 0xC1, .umask = 0x10 },
	{ .name = "OTHER_ASSISTS.ANY_WB_ASSIST", .event = 0xC1, .umask = 0x40 },
	{ .name = "UOPS_RETIRED.ALL", .event = 0xC2, .umask = 0x01 },
	{ .name = "UOPS_RETIRED.ALL.STALLS", .event = 0xC2, .umask = 0x01, .cmask = 1, .inv = 1 },
	{ .name = "UOPS_RETIRED.RETIRE_SLOTS", .event = 0xC2, .umask = 0x02 },
	{ .name = "FP_ASSIST.X87_OUTPUT", .event = 0xCA, .umask = 0x02 },
	{ .name = "FP_ASSIST.X87_INPUT", .event = 0xCA, .umask = 0x04 },
	{ .name = "FP_ASSIST.SIMD_OUTPUT", .event = 0xCA, .umask = 0x08 },
	{ .name = "FP_ASSIST.SIMD_INPUT", .event = 0xCA, .umask = 0x10 },
	{ .name = "FP_ASSIST.ANY", .event = 0xCA, .umask = 0x1E },
	{ .name = "ROB_MISC_EVENTS.LBR_INSERTS", .event = 0xCC, .umask = 0x20 },
};

// DisplayFamily_DisplayModel == 06_3CH, 06_45H or 06_46H
const struct performance_counter_specification haswell_specification[] = {
	{ .name = "MOVE_ELIMINATION.INT_NOT_ELIMINATED", .event = 0x58, .umask = 0x04 },
	{ .name = "MOVE_ELIMINATION.SIMD_NOT_ELIMINATED", .event = 0x58, .umask = 0x08 },
	{ .name = "MOVE_ELIMINATION.INT_ELIMINATED", .event = 0x58, .umask = 0x01 },
	{ .name = "MOVE_ELIMINATION.SIMD_ELIMINATED", .event = 0x58, .umask = 0x02 },
	{ .name = "UOPS_EXECUTED_PORT.PORT_0", .event = 0xA1, .umask = 0x01 },
	{ .name = "UOPS_EXECUTED_PORT.PORT_1", .event = 0xA1, .umask = 0x02 },
	{ .name = "UOPS_EXECUTED_PORT.PORT_2", .event = 0xA1, .umask = 0x04 },
	{ .name = "UOPS_EXECUTED_PORT.PORT_3", .event = 0xA1, .umask = 0x08 },
	{ .name = "UOPS_EXECUTED_PORT.PORT_4", .event = 0xA1, .umask = 0x10 },
	{ .name = "UOPS_EXECUTED_PORT.PORT_5", .event = 0xA1, .umask = 0x20 },
	{ .name = "UOPS_EXECUTED_PORT.PORT_6", .event = 0xA1, .umask = 0x40 },
	{ .name = "UOPS_EXECUTED_PORT.PORT_7", .event = 0xA1, .umask = 0x80 },
	{ .name = "UOPS_EXECUTED.CORE", .event = 0xB1, .umask = 0x02 },
	{ .name = "RESOURCE_STALLS.RS", .event = 0xA2, .umask = 0x04 },
	{ .name = "RESOURCE_STALLS.SB", .event = 0xA2, .umask = 0x08 },
	{ .name = "RESOURCE_STALLS.ROB", .event = 0xA2, .umask = 0x10 },
	{ .name = "OTHER_ASSISTS.AVX_TO_SSE", .event = 0xC1, .umask = 0x08 },
	{ .name = "OTHER_ASSISTS.SSE_TO_AVX", .event = 0xC1, .umask = 0x10 },
	{ .name = "OTHER_ASSISTS.ANY_WB_ASSIST", .event = 0xC1, .umask = 0x40 },
	{ .name = "UOPS_RETIRED.ALL", .event = 0xC2, .umask = 0x01 },
	{ .name = "UOPS_RETIRED.ALL.STALLS", .event = 0xC2, .umask = 0x01, .cmask = 1, .inv = 1 },
	{ .name = "UOPS_RETIRED.ALL.ACTIVE", .event = 0xC2, .umask = 0x01, .cmask = 1 },
	{ .name = "UOPS_RETIRED.RETIRE_SLOTS", .event = 0xC2, .umask = 0x02 },
	{ .name = "LSD.UOPS", .event = 0xA8, .umask = 0x01 },
	{ .name = "UOPS_ISSUED.ANY", .event = 0x0E, .umask = 0x01 },
	{ .name = "UOPS_ISSUED.STALLS", .event = 0x0E, .umask = 0x01, .cmask = 1, .inv = 1 },
	{ .name = "UOPS_ISSUED.FLAGS_MERGE", .event = 0x0E, .umask = 0x10 },
	{ .name = "UOPS_ISSUED.SLOW_LEA", .event = 0x0E, .umask = 0x20 },
	{ .name = "UOPS_ISSUED.SINGLE_MUL", .event = 0x0E, .umask = 0x40 },
	{ .name = "IDQ.EMPTY", .event = 0x79, .umask = 0x02 },
	{ .name = "IDQ.MITE_UOPS", .event = 0x79, .umask = 0x04 },
	{ .name = "IDQ.DSB_UOPS", .event = 0x79, .umask = 0x08 },
	{ .name = "IDQ.MS_DSB_UOPS", .event = 0x79, .umask = 0x10 },
	{ .name = "IDQ.MS_MITE_UOPS", .event = 0x79, .umask = 0x20 },
	{ .name = "IDQ.MS_UOPS", .event = 0x79, .umask = 0x30 },
	{ .name = "IDQ.ALL_DSB_CYCLES_ANY_UOPS", .event = 0x79, .umask = 0x18, .cmask = 1 },
	{ .name = "IDQ.ALL_DSB_CYCLES_4_UOPS", .event = 0x79, .umask = 0x24, .cmask = 4 },
	{ .name = "IDQ.ALL_MITE_CYCLES_ANY_UOPS", .event = 0x79, .umask = 0x24, .cmask = 1 },
	{ .name = "IDQ.ALL_MITE_CYCLES_4_UOPS", .event = 0x79, .umask = 0x24, .cmask = 4 },
	{ .name = "IDQ.MITE_ALL_UOPS", .event = 0x79, .umask = 0x3C },
	{ .name = "ICACHE.MISSES", .event = 0x80, .umask = 0x02 },
	{ .name = "ILD_STALL.LCP", .event = 0x87, .umask = 0x01 },
	{ .name = "ILD_STALL.IQ_FULL", .event = 0x87, .umask = 0x04 },
	{ .name = "RS_EVENTS.EMPTY_CYCLES", .event = 0x5E, .umask = 0x01 },
};

// DisplayFamily_DisplayModel == 06_3AH
const struct performance_counter_specification ivybridge_specification[] = {
	{ .name = "UOPS_ISSUED.ANY", .event = 0x0E, .umask = 0x01 },
	{ .name = "UOPS_ISSUED.FLAGS_MERGE", .event = 0x0E, .umask = 0x10 },
	{ .name = "UOPS_ISSUED.SLOW_LEA", .event = 0x0E, .umask = 0x20 },
	{ .name = "UOPS_ISSUED.SINGLE_MUL", .event = 0x0E, .umask = 0x40 },
	{ .name = "FP_COMP_OPS_EXE.X87", .event = 0x10, .umask = 0x01 },
	{ .name = "FP_COMP_OPS_EXE.SSE_FP_PACKED_DOUBLE", .event = 0x10, .umask = 0x10 },
	{ .name = "FP_COMP_OPS_EXE.SSE_FP_SCALAR_SINGLE", .event = 0x10, .umask = 0x20 },
	{ .name = "FP_COMP_OPS_EXE.SSE_PACKED_SINGLE", .event = 0x10, .umask = 0x40 },
	{ .name = "FP_COMP_OPS_EXE.SSE_SCALAR_DOUBLE", .event = 0x10, .umask = 0x80 },
	{ .name = "SIMD_FP_256.PACKED_SINGLE", .event = 0x11, .umask = 0x01 },
	{ .name = "SIMD_FP_256.PACKED_DOUBLE", .event = 0x11, .umask = 0x01 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_0", .event = 0xA1, .umask = 0x01 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_1", .event = 0xA1, .umask = 0x02 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_2", .event = 0xA1, .umask = 0x0C },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_3", .event = 0xA1, .umask = 0x30 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_4", .event = 0xA1, .umask = 0x40 },
	{ .name = "UOPS_DISPATCHED_PORT.PORT_5", .event = 0xA1, .umask = 0x80 },
	{ .name = "IDQ.EMPTY", .event = 0x79, .umask = 0x02 },
	{ .name = "IDQ.MITE_UOPS", .event = 0x79, .umask = 0x04 },
	{ .name = "IDQ.DSB_UOPS", .event = 0x79, .umask = 0x08 },
	{ .name = "IDQ.MS_DSB_UOPS", .event = 0x79, .umask = 0x10 },
	{ .name = "IDQ.MS_MITE_UOPS", .event = 0x79, .umask = 0x20 },
	{ .name = "IDQ.MS_UOPS", .event = 0x79, .umask = 0x30 },
	{ .name = "IDQ.ALL_DSB_CYCLES_ANY_UOPS", .event = 0x79, .umask = 0x18, .cmask = 1 },
	{ .name = "IDQ.ALL_DSB_CYCLES_4_UOPS", .event = 0x79, .umask = 0x18, .cmask = 4 },
	{ .name = "IDQ.ALL_MITE_CYCLES_ANY_UOPS", .event = 0x79, .umask = 0x24, .cmask = 1 },
	{ .name = "IDQ.ALL_MITE_CYCLES_4_UOPS", .event = 0x79, .umask = 0x24, .cmask = 4 },
	{ .name = "IDQ.MITE_ALL_UOPS", .event = 0x79, .umask = 0x3C },
	{ .name = "ICACHE.IFETCH_STALL", .event = 0x80, .umask = 0x04 },
	{ .name = "ICACHE.MISSES", .event = 0x80, .umask = 0x02 },
	{ .name = "ILD_STALL.LCP", .event = 0x87, .umask = 0x01 },
	{ .name = "ILD_STALL.IQ_FULL", .event = 0x87, .umask = 0x04 },
	{ .name = "IDQ_UOPS_NOT_DELIVERED.CORE", .event = 0x9C, .umask = 0x01 },
	{ .name = "RESOURCE_STALLS.ANY", .event = 0xA2, .umask = 0x01 },
	{ .name = "RESOURCE_STALLS.RS", .event = 0xA2, .umask = 0x04 },
	{ .name = "RESOURCE_STALLS.SB", .event = 0xA2, .umask = 0x08 },
	{ .name = "RESOURCE_STALLS.ROB", .event = 0xA2, .umask = 0x10 },
	{ .name = "LSD.UOPS", .event = 0xA8, .umask = 0x01 },
	{ .name = "DSB2MITE_SWITCHES.COUNT", .event = 0xAB, .umask = 0x01 },
	{ .name = "DSB2MITE_SWITCHES.PENALTY_CYCLES", .event = 0xAB, .umask = 0x02 },
	{ .name = "DSB_FILL.EXCEED_DSB_LINES", .event = 0xAC, .umask = 0x08 },
	{ .name = "OTHER_ASSISTS.AVX_STORE", .event = 0xC1, .umask = 0x08 },
	{ .name = "OTHER_ASSISTS.AVX_TO_SSE", .event = 0xC1, .umask = 0x10 },
	{ .name = "OTHER_ASSISTS.SSE_TO_AVX", .event = 0xC1, .umask = 0x20 },
	{ .name = "OTHER_ASSISTS.WB", .event = 0xC1, .umask = 0x80 },
};

// DisplayFamily_DisplayModel == 06_1CH, 06_26H, 06_27H, 06_35H aor 06_36H
const struct performance_counter_specification atom_specification[] = {
	{ .name = "STORE_FORWARDS.GOOD", .event = 0x02, .umask = 0x81 },
	{ .name = "SEGMENT_REG_LOADS.ANY", .event = 0x06, .umask = 0x00 },
	{ .name = "PREFETCH.PREFETCHT0", .event = 0x07, .umask = 0x01 },
	{ .name = "PREFETCH.SW_L2", .event = 0x07, .umask = 0x06 },
	{ .name = "PREFETCH.PREFETCHNTA", .event = 0x07, .umask = 0x08 },
	{ .name = "DATA_TLB_MISSES.DTLB_MISS", .event = 0x08, .umask = 0x07 },
	{ .name = "DATA_TLB_MISSES.DTLB_MISS_LD", .event = 0x08, .umask = 0x05 },
	{ .name = "DATA_TLB_MISSES.L0_DTLB_MISS_LD", .event = 0x08, .umask = 0x09 },
	{ .name = "DATA_TLB_MISSES.DTLB_MISS_ST", .event = 0x08, .umask = 0x06 },
	{ .name = "PAGE_WALKS.WALKS", .event = 0x0C, .umask = 0x03, .edge = 1 },
	{ .name = "PAGE_WALKS.CYCLES", .event = 0x0C, .umask = 0x03 },
	{ .name = "X87_COMP_OPS_EXE.ANY.S", .event = 0x10, .umask = 0x01 },
	{ .name = "X87_COMP_OPS_EXE.ANY.AR", .event = 0x10, .umask = 0x81 },
	{ .name = "FP_ASSIST", .event = 0x11, .umask = 0x01 },
	{ .name = "FP_ASSIST.AR", .event = 0x11, .umask = 0x01 },
	{ .name = "MUL.S", .event = 0x12, .umask = 0x01 },
	{ .name = "MUL.AR", .event = 0x12, .umask = 0x81 },
	{ .name = "DIV.S", .event = 0x13, .umask = 0x01 },
	{ .name = "DIV.AR", .event = 0x13, .umask = 0x81 },
	{ .name = "CYCLES_DIV_BUSY", .event = 0x14, .umask = 0x01 },
	{ .name = "L1D_CACHE.LD", .event = 0x40, .umask = 0x21 },
	{ .name = "L1D_CACHE.ST", .event = 0x40, .umask = 0x22 },
	{ .name = "ICACHE.ACCESSES", .event = 0x80, .umask = 0x03 },
	{ .name = "ICACHE.MISSES", .event = 0x80, .umask = 0x02 },
	{ .name = "MACRO_INSTS.CISC_DECODED", .event = 0xAA, .umask = 0x02 },
	{ .name = "MACRO_INSTS.ALL_DECODED", .event = 0xAA, .umask = 0x03 },
	{ .name = "SIMD_UOPS_EXEC.S", .event = 0xB0, .umask = 0x00 },
	{ .name = "SIMD_UOPS_EXEC.AR", .event = 0xB0, .umask = 0x80 },
	{ .name = "SIMD_SAT_UOP_EXEC.S", .event = 0xB1, .umask = 0x00 },
	{ .name = "SIMD_SAT_UOP_EXEC.AR", .event = 0xB1, .umask = 0x80 },
	{ .name = "SIMD_UOP_TYPE_EXEC.MUL.S", .event = 0xB3, .umask = 0x01 },
	{ .name = "SIMD_UOP_TYPE_EXEC.MUL.AR", .event = 0xB3, .umask = 0x81 },
	{ .name = "SIMD_UOP_TYPE_EXEC.SHIFT.S", .event = 0xB3, .umask = 0x02 },
	{ .name = "SIMD_UOP_TYPE_EXEC.SHIFT.AR", .event = 0xB3, .umask = 0x82 },
	{ .name = "SIMD_UOP_TYPE_EXEC.PACK.S", .event = 0xB3, .umask = 0x04 },
	{ .name = "SIMD_UOP_TYPE_EXEC.PACK.AR", .event = 0xB3, .umask = 0x84 },
	{ .name = "SIMD_UOP_TYPE_EXEC.UNPACK.S", .event = 0xB3, .umask = 0x08 },
	{ .name = "SIMD_UOP_TYPE_EXEC.UNPACK.AR", .event = 0xB3, .umask = 0x88 },
	{ .name = "SIMD_UOP_TYPE_EXEC.LOGICAL.S", .event = 0xB3, .umask = 0x10 },
	{ .name = "SIMD_UOP_TYPE_EXEC.LOGICAL.AR", .event = 0xB3, .umask = 0x90 },
	{ .name = "SIMD_UOP_TYPE_EXEC.ARITHMETIC.S", .event = 0xB3, .umask = 0x20 },
	{ .name = "SIMD_UOP_TYPE_EXEC.ARITHMETIC.AR", .event = 0xB3, .umask = 0xA0 },
	{ .name = "INST_RETIRED.ANY_P", .event = 0xC0, .umask = 0x00 },
	{ .name = "UOPS_RETIRED.ANY", .event = 0xC2, .umask = 0x10 },
	{ .name = "SIMD_INST_RETIRED.PACKED_SINGLE", .event = 0xC7, .umask = 0x01 },
	{ .name = "SIMD_INST_RETIRED.SCALAR_SINGLE", .event = 0xC7, .umask = 0x02 },
	{ .name = "SIMD_INST_RETIRED.PACKED_DOUBLE", .event = 0xC7, .umask = 0x04 },
	{ .name = "SIMD_INST_RETIRED.SCALAR_DOUBLE", .event = 0xC7, .umask = 0x08 },
	{ .name = "SIMD_INST_RETIRED.VECTOR", .event = 0xC7, .umask = 0x10 },
	{ .name = "SIMD_COMP_INST_RETIRED.PACKED_SINGLE", .event = 0xCA, .umask = 0x01 },
	{ .name = "SIMD_COMP_INST_RETIRED.SCALAR_SINGLE", .event = 0xCA, .umask = 0x02 },
	{ .name = "SIMD_COMP_INST_RETIRED.PACKED_DOUBLE", .event = 0xCA, .umask = 0x04 },
	{ .name = "SIMD_COMP_INST_RETIRED.SCALAR_DOUBLE", .event = 0xCA, .umask = 0x08 },
	{ .name = "SIMD_ASSIST", .event = 0xCD, .umask = 0x00 },
	{ .name = "SIMD_INSTR_RETIRED", .event = 0xCE, .umask = 0x00 },
	{ .name = "SIMD_SAT_INSTR_RETIRED", .event = 0xCD, .umask = 0x00 },
};

const struct performance_counter_specification steamroller_specification[] = {
	{ .name = "DISPATCHED_FPU_OPS.PIPE_0", .event = 0x00, .umask = 0x01 },
	{ .name = "DISPATCHED_FPU_OPS.PIPE_1", .event = 0x00, .umask = 0x02 },
	{ .name = "DISPATCHED_FPU_OPS.PIPE_2", .event = 0x00, .umask = 0x04 },
	{ .name = "DISPATCHED_FPU_OPS.DUAL_PIPE.PIPE_0", .event = 0x00, .umask = 0x10 },
	{ .name = "DISPATCHED_FPU_OPS.DUAL_PIPE.PIPE_1", .event = 0x00, .umask = 0x20 },
	{ .name = "DISPATCHED_FPU_OPS.DUAL_PIPE.PIPE_2", .event = 0x00, .umask = 0x40 },
	{ .name = "FP_SCHEDULER.EMPTY", .event = 0x01 },
	{ .name = "FP_SCHEDULER.BUSY", .event = 0x01, .inv = 1 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_ADDSUB", .event = 0x03, .umask = 0x01 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_MUL", .event = 0x03, .umask = 0x02 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_DIVSQRT", .event = 0x03, .umask = 0x04 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_FMA", .event = 0x03, .umask = 0x08 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_ADDSUB", .event = 0x03, .umask = 0x10 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_MUL", .event = 0x03, .umask = 0x20 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_DIVSQRT", .event = 0x03, .umask = 0x40 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_FMA", .event = 0x03, .umask = 0x80 },
	// { .name = "RETIRED_X87_FLOPS.ADDSUB", .event = 0x1C0, .umask = 0x01 },
	// { .name = "RETIRED_X87_FLOPS.MUL", .event = 0x1C0, .umask = 0x02 },
	// { .name = "RETIRED_X87_FLOPS.DIVSQRT", .event = 0x1C0, .umask = 0x04 },
	{ .name = "MOVE_OPTIMIZATION.SSE_MOVE_OPS.ALL", .event = 0x04, .umask = 0x01 },
	{ .name = "MOVE_OPTIMIZATION.SSE_MOVE_OPS.ELIMINATED", .event = 0x04, .umask = 0x02 },
	{ .name = "MOVE_OPTIMIZATION.SCALAR_CANDIDATE_OPS.ALL", .event = 0x04, .umask = 0x04 },
	{ .name = "MOVE_OPTIMIZATION.SCALAR_CANDIDATE_OPS.OPTIMIZED", .event = 0x04, .umask = 0x08 },
	{ .name = "RETIRED_SERIALIZING_OPS.SSE_BOTTOM_EXECUTING_UOPS", .event = 0x05, .umask = 0x01 },
	{ .name = "RETIRED_SERIALIZING_OPS.SSE_MXCSR_MISPREDICTS", .event = 0x05, .umask = 0x02 },
	{ .name = "RETIRED_SERIALIZING_OPS.X87_BOTTOM_EXECUTING_UOPS", .event = 0x05, .umask = 0x04 },
	{ .name = "RETIRED_SERIALIZING_OPS.X87_CONTROL_WORD_MISPREDICTS", .event = 0x05, .umask = 0x08 },
	{ .name = "MEM_STALL_CYCLES.LDQ_FULL", .event = 0x23, .umask = 0x01 },
	{ .name = "MEM_STALL_CYCLES.STQ_FULL", .event = 0x23, .umask = 0x02 },
	{ .name = "LS_DISPATCH_OPS.LD", .event = 0x29, .umask = 0x01 },
	{ .name = "LS_DISPATCH_OPS.ST", .event = 0x29, .umask = 0x02 },
	{ .name = "LS_DISPATCH_OPS.LD_OP_ST", .event = 0x29, .umask = 0x04 },
	{ .name = "STLD_FORWARD_CANCELLED.LAYOUT_MISMATCH", .event = 0x2A, .umask = 0x01 },
	{ .name = "STLD_FORWARD_CANCELLED.TAG_MISMATCH", .event = 0x2A, .umask = 0x02 },
	{ .name = "RETIRED_INSTRUCTIONS.ALL", .event = 0xC0 },
	{ .name = "RETIRED_INSTRUCTIONS.SSE_AVX", .event = 0xCB, .umask = 0x04 },
	{ .name = "RETIRED_INSTRUCTIONS.MMX", .event = 0xCB, .umask = 0x02 },
	{ .name = "RETIRED_INSTRUCTIONS.X87", .event = 0xCB, .umask = 0x01 },
	{ .name = "DISPATCH_STALL.ALL", .event = 0xD1 },
	{ .name = "MICROSEQUENCER_STALL.SERIALIZATION", .event = 0xD3 },
	{ .name = "DISPATCH_STALL.RETIRE_QUEUE_FULL", .event = 0xD5 },
	{ .name = "DISPATCH_STALL.INT_SCHEDULER_QUEUE_FULL", .event = 0xD6 },
	{ .name = "DISPATCH_STALL.FP_SCHEDULER_QUEUE_FULL", .event = 0xD7 },
	{ .name = "DISPATCH_STALL.LDQ_FULL", .event = 0xD8 },
	{ .name = "MICROSEQUENCER_STALL.WAIT_ALL_QUIET", .event = 0xD9 },
};

const struct performance_counter_specification bulldozer_specification[] = {
	{ .name = "DISPATCHED_FPU_OPS.PIPE_0", .event = 0x00, .umask = 0x01 },
	{ .name = "DISPATCHED_FPU_OPS.PIPE_1", .event = 0x00, .umask = 0x02 },
	{ .name = "DISPATCHED_FPU_OPS.PIPE_2", .event = 0x00, .umask = 0x04 },
	{ .name = "DISPATCHED_FPU_OPS.PIPE_3", .event = 0x00, .umask = 0x08 },
	{ .name = "DISPATCHED_FPU_OPS.DUAL_PIPE.PIPE_0", .event = 0x00, .umask = 0x10 },
	{ .name = "DISPATCHED_FPU_OPS.DUAL_PIPE.PIPE_1", .event = 0x00, .umask = 0x20 },
	{ .name = "DISPATCHED_FPU_OPS.DUAL_PIPE.PIPE_2", .event = 0x00, .umask = 0x40 },
	{ .name = "DISPATCHED_FPU_OPS.DUAL_PIPE.PIPE_3", .event = 0x00, .umask = 0x80 },
	{ .name = "FP_SCHEDULER.EMPTY", .event = 0x01 },
	{ .name = "FP_SCHEDULER.BUSY", .event = 0x01, .inv = 1 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_ADDSUB", .event = 0x03, .umask = 0x01 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_MUL", .event = 0x03, .umask = 0x02 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_DIVSQRT", .event = 0x03, .umask = 0x04 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_FMA", .event = 0x03, .umask = 0x08 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_ADDSUB", .event = 0x03, .umask = 0x10 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_MUL", .event = 0x03, .umask = 0x20 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_DIVSQRT", .event = 0x03, .umask = 0x40 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_FMA", .event = 0x03, .umask = 0x80 },
	{ .name = "MOVE_OPTIMIZATION.SSE_MOVE_OPS.ALL", .event = 0x04, .umask = 0x01 },
	{ .name = "MOVE_OPTIMIZATION.SSE_MOVE_OPS.ELIMINATED", .event = 0x04, .umask = 0x02 },
	{ .name = "MOVE_OPTIMIZATION.SCALAR_CANDIDATE_OPS.ALL", .event = 0x04, .umask = 0x04 },
	{ .name = "MOVE_OPTIMIZATION.SCALAR_CANDIDATE_OPS.OPTIMIZED", .event = 0x04, .umask = 0x08 },
	{ .name = "RETIRED_SERIALIZING_OPS.SSE_BOTTOM_EXECUTING_UOPS", .event = 0x05, .umask = 0x01 },
	{ .name = "RETIRED_SERIALIZING_OPS.SSE_MXCSR_MISPREDICTS", .event = 0x05, .umask = 0x02 },
	{ .name = "RETIRED_SERIALIZING_OPS.X87_BOTTOM_EXECUTING_UOPS", .event = 0x05, .umask = 0x04 },
	{ .name = "RETIRED_SERIALIZING_OPS.X87_CONTROL_WORD_MISPREDICTS", .event = 0x05, .umask = 0x08 },
	{ .name = "MEM_STALL_CYCLES.LDQ_FULL", .event = 0x23, .umask = 0x01 },
	{ .name = "MEM_STALL_CYCLES.STQ_FULL", .event = 0x23, .umask = 0x02 },
	{ .name = "LS_DISPATCH_OPS.LD", .event = 0x29, .umask = 0x01 },
	{ .name = "LS_DISPATCH_OPS.ST", .event = 0x29, .umask = 0x02 },
	{ .name = "LS_DISPATCH_OPS.LD_OP_ST", .event = 0x29, .umask = 0x04 },
	{ .name = "STLD_FORWARD_CANCELLED.LAYOUT_MISMATCH", .event = 0x2A, .umask = 0x01 },
	{ .name = "INSTRUCTION_FETCH_STALL", .event = 0x87 },
	{ .name = "RETIRED_INSTRUCTIONS.ALL", .event = 0xC0 },
	{ .name = "RETIRED_UOPS", .event = 0xC1 },
	{ .name = "RETIRED_INSTRUCTIONS.SSE_AVX", .event = 0xCB, .umask = 0x04 },
	{ .name = "RETIRED_INSTRUCTIONS.MMX", .event = 0xCB, .umask = 0x02 },
	{ .name = "RETIRED_INSTRUCTIONS.X87", .event = 0xCB, .umask = 0x01 },
	{ .name = "DECODER_EMPTY", .event = 0xD0 },
	{ .name = "DISPATCH_STALL.ALL", .event = 0xD1 },
	{ .name = "MICROSEQUENCER_STALL.SERIALIZATION", .event = 0xD3 },
	{ .name = "DISPATCH_STALL.RETIRE_QUEUE_FULL", .event = 0xD5 },
	{ .name = "DISPATCH_STALL.INT_SCHEDULER_QUEUE_FULL", .event = 0xD6 },
	{ .name = "DISPATCH_STALL.FP_SCHEDULER_QUEUE_FULL", .event = 0xD7 },
	{ .name = "DISPATCH_STALL.LDQ_FULL", .event = 0xD8 },
	{ .name = "MICROSEQUENCER_STALL.WAIT_ALL_QUIET", .event = 0xD9 },
};

const struct performance_counter_specification bobcat_specification[] = {
	{ .name = "DISPATCHED_FPU_OPS.PIPE_0", .event = 0x00, .umask = 0x01 },
	{ .name = "DISPATCHED_FPU_OPS.PIPE_1", .event = 0x00, .umask = 0x02 },
	{ .name = "FP_SCHEDULER.EMPTY", .event = 0x01 },
	{ .name = "FP_SCHEDULER.BUSY", .event = 0x01, .inv = 1 },
	{ .name = "DISPATCHED_FAST_FLAG_FPU_OPS", .event = 0x02 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_ADDSUB", .event = 0x03, .umask = 0x41 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_MUL", .event = 0x03, .umask = 0x42 },
	{ .name = "RETIRED_SSEAVX_FLOPS.SP_DIVSQRT", .event = 0x03, .umask = 0x44 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_ADDSUB", .event = 0x03, .umask = 0x48 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_MUL", .event = 0x03, .umask = 0x50 },
	{ .name = "RETIRED_SSEAVX_FLOPS.DP_DIVSQRT", .event = 0x03, .umask = 0x60 },
	{ .name = "RETIRED_MOVE_OPS.MERGING_MOVE", .event = 0x04, .umask = 0x04 },
	{ .name = "RETIRED_MOVE_OPS.NON_MERGING_MOVE", .event = 0x04, .umask = 0x08 },
	{ .name = "RETIRED_SERIALIZING_OPS.SSE_BOTTOM_EXECUTING_UOPS", .event = 0x05, .umask = 0x01 },
	{ .name = "RETIRED_SERIALIZING_OPS.SSE_MXCSR_MISPREDICTS", .event = 0x05, .umask = 0x02 },
	{ .name = "RETIRED_SERIALIZING_OPS.X87_BOTTOM_EXECUTING_UOPS", .event = 0x05, .umask = 0x04 },
	{ .name = "RETIRED_SERIALIZING_OPS.X87_CONTROL_WORD_MISPREDICTS", .event = 0x05, .umask = 0x08 },
	{ .name = "RETIRED_X87_FLOPS.ADDSUB", .event = 0x11, .umask = 0x01 },
	{ .name = "RETIRED_X87_FLOPS.MUL", .event = 0x11, .umask = 0x02 },
	{ .name = "RETIRED_X87_FLOPS.DIVSQRT", .event = 0x11, .umask = 0x04 },
	{ .name = "MEM_STALL_CYCLES.RSQ_FULL", .event = 0x23 },
	{ .name = "STLD_FORWARD_CANCELLED.MISALIGNED", .event = 0x2A, .umask = 0x04 },
	{ .name = "STLD_FORWARD_CANCELLED.SIZE_MISMATCH", .event = 0x2A, .umask = 0x02 },
	{ .name = "STLD_FORWARD_CANCELLED.ADDRESS_MISMATCH", .event = 0x2A, .umask = 0x01 },
	{ .name = "PREFETCH_INSTRUCTIONS_DISPATCHED.LOAD", .event = 0x4B, .umask = 0x01 },
	{ .name = "PREFETCH_INSTRUCTIONS_DISPATCHED.STORE", .event = 0x4B, .umask = 0x02 },
	{ .name = "PREFETCH_INSTRUCTIONS_DISPATCHED.NTA", .event = 0x4B, .umask = 0x04 },
	{ .name = "INSTRUCTION_FETCH_STALL", .event = 0x87 },
	{ .name = "RETIRED_INSTRUCTIONS", .event = 0xC0 },
	{ .name = "RETIRED_UOPS", .event = 0xC1 },
	{ .name = "RETIRED_BRANCH_INSTRUCTIONS", .event = 0xC2 },
	{ .name = "RETIRED_MISPREDICTED_BRANCH_INSTRUCTIONS", .event = 0xC3 },
	{ .name = "RETIRED_TAKEN_BRANCH_INSTRUCTIONS", .event = 0xC4 },
	{ .name = "RETIRED_MISPREDICTED_TAKEN_BRANCH_INSTRUCTIONS", .event = 0xC5 },
	{ .name = "RETIRED_FP_INSTRUCTIONS.SSE", .event = 0xCB, .umask = 0x02 },
	{ .name = "RETIRED_FP_INSTRUCTIONS.MMX_X87", .event = 0xCB, .umask = 0x01 },
};

static int perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
	return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int open_performance_counter(enum perf_type_id type, uint64_t config) {
	struct perf_event_attr perf_event_attr;
	memset(&perf_event_attr, 0, sizeof(perf_event_attr));
	perf_event_attr.type = type;
	perf_event_attr.size = sizeof(perf_event_attr);
	perf_event_attr.config = config;
	perf_event_attr.disabled = 1;
	perf_event_attr.exclude_kernel = 1;
	perf_event_attr.exclude_hv = 1;
	return perf_event_open(&perf_event_attr, 0, -1, -1, 0);
}

struct performance_counters init_performance_counters(void) {
	size_t generic_count = 2;
	const struct performance_counter_specification* model_specification = NULL;
	size_t model_count = 0;
	struct x86_cpu_info cpu_info = get_x86_cpu_info();
	if (cpu_info.display_family == 0x06) {
		switch (cpu_info.display_model) {
			case 0x3D:
			case 0x47:
				/* Broadwell */
				model_specification = broadwell_specification;
				model_count = COUNT_OF(broadwell_specification);
				break;
			case 0x3C:
			case 0x45:
			case 0x46:
				/* Haswell */
				model_specification = haswell_specification;
				model_count = COUNT_OF(haswell_specification);
				break;
			case 0x3A:
				/* Ivy Bridge */
				model_specification = ivybridge_specification;
				model_count = COUNT_OF(ivybridge_specification);
				break;
			case 0x1C:
			case 0x26:
			case 0x27:
			case 0x35:
			case 0x36:
				/* Atom */
				model_specification = atom_specification;
				model_count = COUNT_OF(atom_specification);
				break;
		}
	}
	if (cpu_info.display_family == 0x15) {
		if ((cpu_info.display_model & ~0xF) == 0x00) {
			/* Bulldozer */
			model_specification = bulldozer_specification;
			model_count = COUNT_OF(bulldozer_specification);
		} else if ((cpu_info.display_model & ~0xF) == 0x30) {
			/* Steamroller */
			model_specification = steamroller_specification;
			model_count = COUNT_OF(steamroller_specification);
		}
	}
	if (cpu_info.display_family == 0x14) {
		if ((cpu_info.display_model & ~0xF) == 0x00) {
			/* Bobcat */
			model_specification = bobcat_specification;
			model_count = COUNT_OF(bobcat_specification);
		}
	}
	struct performance_counter* performance_counters =
		(struct performance_counter*) malloc((generic_count + model_count) * sizeof(struct performance_counter));
	performance_counters[0] = (struct performance_counter) {
		.name = "Cycles",
		.file_descriptor = open_performance_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES)
	};
	performance_counters[1] = (struct performance_counter) {
		.name = "Instructions",
		.file_descriptor = open_performance_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS)
	};
	for (size_t i = 0; i < model_count; i++) {
		const uint64_t config = ((uint32_t) model_specification[i].event) |
			(((uint32_t) model_specification[i].umask) << 8) |
			(((uint32_t) model_specification[i].edge) << 18) |
			(((uint32_t) model_specification[i].inv) << 23) |
			(((uint32_t) model_specification[i].cmask) << 24);

		performance_counters[generic_count + i] = (struct performance_counter) {
			.name = model_specification[i].name,
			.file_descriptor = open_performance_counter(PERF_TYPE_RAW, config)
		};
	}
	return (struct performance_counters) {
		.counters = performance_counters,
		.count = generic_count + model_count,
	};
}

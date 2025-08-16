#line 1 "../../src/sh4/sh4core.in"
/**
 * $Id$
 * 
 * SH4 emulation core, and parent module for all the SH4 peripheral
 * modules.
 *
 * Copyright (c) 2005 Nathan Keynes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define MODULE sh4_module
#include <assert.h>
#include <math.h>
#include "dream.h"
#include "dreamcast.h"
#include "eventq.h"
#include "mem.h"
#include "clock.h"
#include "syscall.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/sh4stat.h"
#include "sh4/mmu.h"

#define SH4_CALLTRACE 1

#define MAX_INT 0x7FFFFFFF
#define MIN_INT 0x80000000
#define MAX_INTF 2147483647.0
#define MIN_INTF -2147483648.0

/********************** SH4 Module Definition ****************************/

uint32_t sh4_emulate_run_slice( uint32_t nanosecs ) 
{
    int i;

    if( sh4_breakpoint_count == 0 ) {
	for( ; sh4r.slice_cycle < nanosecs; sh4r.slice_cycle += sh4_cpu_period ) {
	    if( SH4_EVENT_PENDING() ) {
	        sh4_handle_pending_events();
	    }
	    if( !sh4_execute_instruction() ) {
		break;
	    }
	}
    } else {
	for( ;sh4r.slice_cycle < nanosecs; sh4r.slice_cycle += sh4_cpu_period ) {
	    if( SH4_EVENT_PENDING() ) {
	        sh4_handle_pending_events();
	    }
                 
	    if( !sh4_execute_instruction() )
		break;
#ifdef ENABLE_DEBUG_MODE
	    for( i=0; i<sh4_breakpoint_count; i++ ) {
		if( sh4_breakpoints[i].address == sh4r.pc ) {
		    break;
		}
	    }
	    if( i != sh4_breakpoint_count ) {
	    	sh4_core_exit( CORE_EXIT_BREAKPOINT );
	    }
#endif	
	}
    }

    /* If we aborted early, but the cpu is still technically running,
     * we're doing a hard abort - cut the timeslice back to what we
     * actually executed
     */
    if( sh4r.slice_cycle != nanosecs && sh4r.sh4_state == SH4_STATE_RUNNING ) {
	nanosecs = sh4r.slice_cycle;
    }
    if( sh4r.sh4_state != SH4_STATE_STANDBY ) {
	TMU_run_slice( nanosecs );
	SCIF_run_slice( nanosecs );
    }
    return nanosecs;
}

/********************** SH4 emulation core  ****************************/

#if(SH4_CALLTRACE == 1)
#define MAX_CALLSTACK 32
static struct call_stack {
    sh4addr_t call_addr;
    sh4addr_t target_addr;
    sh4addr_t stack_pointer;
} call_stack[MAX_CALLSTACK];

static int call_stack_depth = 0;
int sh4_call_trace_on = 0;

static inline void trace_call( sh4addr_t source, sh4addr_t dest ) 
{
    if( call_stack_depth < MAX_CALLSTACK ) {
	call_stack[call_stack_depth].call_addr = source;
	call_stack[call_stack_depth].target_addr = dest;
	call_stack[call_stack_depth].stack_pointer = sh4r.r[15];
    }
    call_stack_depth++;
}

static inline void trace_return( sh4addr_t source, sh4addr_t dest )
{
    if( call_stack_depth > 0 ) {
	call_stack_depth--;
    }
}

void fprint_stack_trace( FILE *f )
{
    int i = call_stack_depth -1;
    if( i >= MAX_CALLSTACK )
	i = MAX_CALLSTACK - 1;
    for( ; i >= 0; i-- ) {
	fprintf( f, "%d. Call from %08X => %08X, SP=%08X\n", 
		 (call_stack_depth - i), call_stack[i].call_addr,
		 call_stack[i].target_addr, call_stack[i].stack_pointer );
    }
}

#define TRACE_CALL( source, dest ) trace_call(source, dest)
#define TRACE_RETURN( source, dest ) trace_return(source, dest)
#else
#define TRACE_CALL( dest, rts ) 
#define TRACE_RETURN( source, dest )
#endif

static gboolean FASTCALL sh4_raise_slot_exception( int normal_code, int slot_code ) {
    if( sh4r.in_delay_slot ) {
        sh4_raise_exception(slot_code);
    } else {
        sh4_raise_exception(normal_code);
    }
    return TRUE;
}


#define CHECKPRIV() if( !IS_SH4_PRIVMODE() ) { return sh4_raise_slot_exception( EXC_ILLEGAL, EXC_SLOT_ILLEGAL ); }
#define CHECKRALIGN16(addr) if( (addr)&0x01 ) { sh4_raise_exception( EXC_DATA_ADDR_READ ); return TRUE; }
#define CHECKRALIGN32(addr) if( (addr)&0x03 ) { sh4_raise_exception( EXC_DATA_ADDR_READ ); return TRUE; }
#define CHECKRALIGN64(addr) if( (addr)&0x07 ) { sh4_raise_exception( EXC_DATA_ADDR_READ ); return TRUE; }
#define CHECKWALIGN16(addr) if( (addr)&0x01 ) { sh4_raise_exception( EXC_DATA_ADDR_WRITE ); return TRUE; }
#define CHECKWALIGN32(addr) if( (addr)&0x03 ) { sh4_raise_exception( EXC_DATA_ADDR_WRITE ); return TRUE; }
#define CHECKWALIGN64(addr) if( (addr)&0x07 ) { sh4_raise_exception( EXC_DATA_ADDR_WRITE ); return TRUE; }

#define CHECKFPUEN() if( !IS_FPU_ENABLED() ) { if( ir == 0xFFFD ) { UNDEF(ir); } else { return sh4_raise_slot_exception( EXC_FPU_DISABLED, EXC_SLOT_FPU_DISABLED ); } }
#define CHECKDEST(p) if( (p) == 0 ) { ERROR( "%08X: Branch/jump to NULL, CPU halted", sh4r.pc ); sh4_core_exit(CORE_EXIT_HALT); return FALSE; }
#define CHECKSLOTILLEGAL() if(sh4r.in_delay_slot) { sh4_raise_exception(EXC_SLOT_ILLEGAL); return TRUE; }

#define ADDRSPACE (IS_SH4_PRIVMODE() ? sh4_address_space : sh4_user_address_space)
#define SQADDRSPACE (IS_SH4_PRIVMODE() ? storequeue_address_space : storequeue_user_address_space)

#define MEM_READ_BYTE( addr, val ) addrtmp = addr; if( (fntmp = mmu_get_region_for_vma_read(&addrtmp)) == NULL ) { sh4r.in_delay_slot = 0; return TRUE; } else { val = fntmp->read_byte(addrtmp); }
#define MEM_READ_BYTE_FOR_WRITE( addr, val ) addrtmp = addr; if( (fntmp = mmu_get_region_for_vma_write(&addrtmp)) == NULL ) { sh4r.in_delay_slot = 0; return TRUE; } else { val = fntmp->read_byte_for_write(addrtmp); }
#define MEM_READ_WORD( addr, val ) addrtmp = addr; if( (fntmp = mmu_get_region_for_vma_read(&addrtmp)) == NULL ) { sh4r.in_delay_slot = 0; return TRUE; } else { val = fntmp->read_word(addrtmp); }
#define MEM_READ_LONG( addr, val ) addrtmp = addr; if( (fntmp = mmu_get_region_for_vma_read(&addrtmp)) == NULL ) { sh4r.in_delay_slot = 0; return TRUE; } else { val = fntmp->read_long(addrtmp); }
#define MEM_WRITE_BYTE( addr, val ) addrtmp = addr; if( (fntmp = mmu_get_region_for_vma_write(&addrtmp)) == NULL ) { sh4r.in_delay_slot = 0; return TRUE; } else { fntmp->write_byte(addrtmp,val); }
#define MEM_WRITE_WORD( addr, val ) addrtmp = addr; if( (fntmp = mmu_get_region_for_vma_write(&addrtmp)) == NULL ) { sh4r.in_delay_slot = 0; return TRUE; } else { fntmp->write_word(addrtmp,val); }
#define MEM_WRITE_LONG( addr, val ) addrtmp = addr; if( (fntmp = mmu_get_region_for_vma_write(&addrtmp)) == NULL ) { sh4r.in_delay_slot = 0; return TRUE; } else { fntmp->write_long(addrtmp,val); }
#define MEM_PREFETCH( addr )  addrtmp = addr; if( (fntmp = mmu_get_region_for_vma_prefetch(&addrtmp)) == NULL ) { sh4r.in_delay_slot = 0; return TRUE; } else { fntmp->prefetch(addrtmp); }

#define FP_WIDTH (IS_FPU_DOUBLESIZE() ? 8 : 4)

#define MEM_FP_READ( addr, reg ) \
    if( IS_FPU_DOUBLESIZE() ) { \
	CHECKRALIGN64(addr); \
        if( reg & 1 ) { \
            MEM_READ_LONG( addr, *((uint32_t *)&XF((reg) & 0x0E)) ); \
            MEM_READ_LONG( addr+4, *((uint32_t *)&XF(reg)) ); \
        } else { \
            MEM_READ_LONG( addr, *((uint32_t *)&FR(reg)) ); \
            MEM_READ_LONG( addr+4, *((uint32_t *)&FR((reg)|0x01)) ); \
	} \
    } else { \
        CHECKRALIGN32(addr); \
        MEM_READ_LONG( addr, *((uint32_t *)&FR(reg)) ); \
    }
#define MEM_FP_WRITE( addr, reg ) \
    if( IS_FPU_DOUBLESIZE() ) { \
        CHECKWALIGN64(addr); \
        if( reg & 1 ) { \
	    MEM_WRITE_LONG( addr, *((uint32_t *)&XF((reg)&0x0E)) ); \
	    MEM_WRITE_LONG( addr+4, *((uint32_t *)&XF(reg)) ); \
        } else { \
	    MEM_WRITE_LONG( addr, *((uint32_t *)&FR(reg)) ); \
	    MEM_WRITE_LONG( addr+4, *((uint32_t *)&FR((reg)|0x01)) ); \
	} \
    } else { \
    	CHECKWALIGN32(addr); \
        MEM_WRITE_LONG(addr, *((uint32_t *)&FR((reg))) ); \
    }

#define UNDEF(ir)
#define UNIMP(ir)

/**
 * Perform instruction-completion following core exit of a partially completed
 * instruction. NOTE: This is only allowed on memory writes, operation is not
 * guaranteed in any other case.
 */
void sh4_finalize_instruction( void )
{
    unsigned short ir;
    uint32_t tmp;

    if( IS_SYSCALL(sh4r.pc) || !IS_IN_ICACHE(sh4r.pc) ) {
        return;
    }
    ir = *(uint16_t *)GET_ICACHE_PTR(sh4r.pc);
    
    /**
     * Note - we can't take an exit on a control transfer instruction itself,
     * which means the exit must have happened in the delay slot. So for these
     * cases, finalize the delay slot instruction, and re-execute the control transfer.
     *
     * For delay slots which modify the argument used in the branch instruction,
     * we pretty much just assume that that can't have already happened in an exit case.
     */
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
        switch( (ir&0xF000) >> 12 ) {
            case 0x0:
                switch( ir&0xF ) {
                    case 0x2:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        UNIMP(ir); /* STC SR, Rn */
                                        break;
                                    case 0x1:
                                        UNIMP(ir); /* STC GBR, Rn */
                                        break;
                                    case 0x2:
                                        UNIMP(ir); /* STC VBR, Rn */
                                        break;
                                    case 0x3:
                                        UNIMP(ir); /* STC SSR, Rn */
                                        break;
                                    case 0x4:
                                        UNIMP(ir); /* STC SPC, Rn */
                                        break;
                                    default:
                                        UNDEF(ir);
                                        break;
                                }
                                break;
                            case 0x1:
                                UNIMP(ir); /* STC Rm_BANK, Rn */
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* BSRF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 251 "../../src/sh4/sh4core.in"
                                /* Note: PR is already set */ 
                                sh4r.pc += 2;
                                tmp = sh4r.r[Rn];
                                sh4_finalize_instruction();
                                sh4r.pc += tmp;
                                }
                                break;
                            case 0x2:
                                { /* BRAF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 239 "../../src/sh4/sh4core.in"
                                sh4r.pc += 2; 
                                tmp = sh4r.r[Rn];
                                sh4_finalize_instruction(); 
                                sh4r.pc += tmp;
                                }
                                break;
                            case 0x8:
                                UNIMP(ir); /* PREF @Rn */
                                break;
                            case 0x9:
                                UNIMP(ir); /* OCBI @Rn */
                                break;
                            case 0xA:
                                UNIMP(ir); /* OCBP @Rn */
                                break;
                            case 0xB:
                                UNIMP(ir); /* OCBWB @Rn */
                                break;
                            case 0xC:
                                UNIMP(ir); /* MOVCA.L R0, @Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x4:
                        UNIMP(ir); /* MOV.B Rm, @(R0, Rn) */
                        break;
                    case 0x5:
                        UNIMP(ir); /* MOV.W Rm, @(R0, Rn) */
                        break;
                    case 0x6:
                        UNIMP(ir); /* MOV.L Rm, @(R0, Rn) */
                        break;
                    case 0x7:
                        UNIMP(ir); /* MUL.L Rm, Rn */
                        break;
                    case 0x8:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* CLRT */
                                break;
                            case 0x1:
                                UNIMP(ir); /* SETT */
                                break;
                            case 0x2:
                                UNIMP(ir); /* CLRMAC */
                                break;
                            case 0x3:
                                UNIMP(ir); /* LDTLB */
                                break;
                            case 0x4:
                                UNIMP(ir); /* CLRS */
                                break;
                            case 0x5:
                                UNIMP(ir); /* SETS */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x9:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* NOP */
                                break;
                            case 0x1:
                                UNIMP(ir); /* DIV0U */
                                break;
                            case 0x2:
                                UNIMP(ir); /* MOVT Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xA:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* STS MACH, Rn */
                                break;
                            case 0x1:
                                UNIMP(ir); /* STS MACL, Rn */
                                break;
                            case 0x2:
                                UNIMP(ir); /* STS PR, Rn */
                                break;
                            case 0x3:
                                UNIMP(ir); /* STC SGR, Rn */
                                break;
                            case 0x5:
                                UNIMP(ir); /* STS FPUL, Rn */
                                break;
                            case 0x6:
                                UNIMP(ir); /* STS FPSCR, Rn */
                                break;
                            case 0xF:
                                UNIMP(ir); /* STC DBR, Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xB:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                { /* RTS */
#line 291 "../../src/sh4/sh4core.in"
                                sh4r.pc += 2;
                                sh4_finalize_instruction();
                                sh4r.pc = sh4r.pr;
                                sh4r.new_pc = sh4r.pr + 2;
                                sh4r.slice_cycle += sh4_cpu_period;
                                return;
                                }
                                break;
                            case 0x1:
                                UNIMP(ir); /* SLEEP */
                                break;
                            case 0x2:
                                { /* RTE */
#line 299 "../../src/sh4/sh4core.in"
                                /* SR is already set */
                                sh4r.pc += 2;
                                sh4_finalize_instruction();
                                sh4r.pc = sh4r.spc;
                                sh4r.new_pc = sh4r.pr + 2;
                                sh4r.slice_cycle += sh4_cpu_period;
                                return;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xC:
                        UNIMP(ir); /* MOV.B @(R0, Rm), Rn */
                        break;
                    case 0xD:
                        UNIMP(ir); /* MOV.W @(R0, Rm), Rn */
                        break;
                    case 0xE:
                        UNIMP(ir); /* MOV.L @(R0, Rm), Rn */
                        break;
                    case 0xF:
                        UNIMP(ir); /* MAC.L @Rm+, @Rn+ */
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
            case 0x1:
                UNIMP(ir); /* MOV.L Rm, @(disp, Rn) */
                break;
            case 0x2:
                switch( ir&0xF ) {
                    case 0x0:
                        UNIMP(ir); /* MOV.B Rm, @Rn */
                        break;
                    case 0x1:
                        UNIMP(ir); /* MOV.W Rm, @Rn */
                        break;
                    case 0x2:
                        UNIMP(ir); /* MOV.L Rm, @Rn */
                        break;
                    case 0x4:
                        { /* MOV.B Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 307 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn]--;
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 308 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] -= 2;
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 309 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] -= 4;
                        }
                        break;
                    case 0x7:
                        UNIMP(ir); /* DIV0S Rm, Rn */
                        break;
                    case 0x8:
                        UNIMP(ir); /* TST Rm, Rn */
                        break;
                    case 0x9:
                        UNIMP(ir); /* AND Rm, Rn */
                        break;
                    case 0xA:
                        UNIMP(ir); /* XOR Rm, Rn */
                        break;
                    case 0xB:
                        UNIMP(ir); /* OR Rm, Rn */
                        break;
                    case 0xC:
                        UNIMP(ir); /* CMP/STR Rm, Rn */
                        break;
                    case 0xD:
                        UNIMP(ir); /* XTRCT Rm, Rn */
                        break;
                    case 0xE:
                        UNIMP(ir); /* MULU.W Rm, Rn */
                        break;
                    case 0xF:
                        UNIMP(ir); /* MULS.W Rm, Rn */
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
            case 0x3:
                switch( ir&0xF ) {
                    case 0x0:
                        UNIMP(ir); /* CMP/EQ Rm, Rn */
                        break;
                    case 0x2:
                        UNIMP(ir); /* CMP/HS Rm, Rn */
                        break;
                    case 0x3:
                        UNIMP(ir); /* CMP/GE Rm, Rn */
                        break;
                    case 0x4:
                        UNIMP(ir); /* DIV1 Rm, Rn */
                        break;
                    case 0x5:
                        UNIMP(ir); /* DMULU.L Rm, Rn */
                        break;
                    case 0x6:
                        UNIMP(ir); /* CMP/HI Rm, Rn */
                        break;
                    case 0x7:
                        UNIMP(ir); /* CMP/GT Rm, Rn */
                        break;
                    case 0x8:
                        UNIMP(ir); /* SUB Rm, Rn */
                        break;
                    case 0xA:
                        UNIMP(ir); /* SUBC Rm, Rn */
                        break;
                    case 0xB:
                        UNIMP(ir); /* SUBV Rm, Rn */
                        break;
                    case 0xC:
                        UNIMP(ir); /* ADD Rm, Rn */
                        break;
                    case 0xD:
                        UNIMP(ir); /* DMULS.L Rm, Rn */
                        break;
                    case 0xE:
                        UNIMP(ir); /* ADDC Rm, Rn */
                        break;
                    case 0xF:
                        UNIMP(ir); /* ADDV Rm, Rn */
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
            case 0x4:
                switch( ir&0xF ) {
                    case 0x0:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* SHLL Rn */
                                break;
                            case 0x1:
                                UNIMP(ir); /* DT Rn */
                                break;
                            case 0x2:
                                UNIMP(ir); /* SHAL Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x1:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* SHLR Rn */
                                break;
                            case 0x1:
                                UNIMP(ir); /* CMP/PZ Rn */
                                break;
                            case 0x2:
                                UNIMP(ir); /* SHAR Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x2:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* STS.L MACH, @-Rn */
                                break;
                            case 0x1:
                                UNIMP(ir); /* STS.L MACL, @-Rn */
                                break;
                            case 0x2:
                                UNIMP(ir); /* STS.L PR, @-Rn */
                                break;
                            case 0x3:
                                UNIMP(ir); /* STC.L SGR, @-Rn */
                                break;
                            case 0x5:
                                UNIMP(ir); /* STS.L FPUL, @-Rn */
                                break;
                            case 0x6:
                                UNIMP(ir); /* STS.L FPSCR, @-Rn */
                                break;
                            case 0xF:
                                UNIMP(ir); /* STC.L DBR, @-Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        UNIMP(ir); /* STC.L SR, @-Rn */
                                        break;
                                    case 0x1:
                                        UNIMP(ir); /* STC.L GBR, @-Rn */
                                        break;
                                    case 0x2:
                                        UNIMP(ir); /* STC.L VBR, @-Rn */
                                        break;
                                    case 0x3:
                                        UNIMP(ir); /* STC.L SSR, @-Rn */
                                        break;
                                    case 0x4:
                                        UNIMP(ir); /* STC.L SPC, @-Rn */
                                        break;
                                    default:
                                        UNDEF(ir);
                                        break;
                                }
                                break;
                            case 0x1:
                                UNIMP(ir); /* STC.L Rm_BANK, @-Rn */
                                break;
                        }
                        break;
                    case 0x4:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* ROTL Rn */
                                break;
                            case 0x2:
                                UNIMP(ir); /* ROTCL Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x5:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* ROTR Rn */
                                break;
                            case 0x1:
                                UNIMP(ir); /* CMP/PL Rn */
                                break;
                            case 0x2:
                                UNIMP(ir); /* ROTCR Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x6:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* LDS.L @Rm+, MACH */
                                break;
                            case 0x1:
                                UNIMP(ir); /* LDS.L @Rm+, MACL */
                                break;
                            case 0x2:
                                UNIMP(ir); /* LDS.L @Rm+, PR */
                                break;
                            case 0x3:
                                UNIMP(ir); /* LDC.L @Rm+, SGR */
                                break;
                            case 0x5:
                                UNIMP(ir); /* LDS.L @Rm+, FPUL */
                                break;
                            case 0x6:
                                UNIMP(ir); /* LDS.L @Rm+, FPSCR */
                                break;
                            case 0xF:
                                UNIMP(ir); /* LDC.L @Rm+, DBR */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x7:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        UNIMP(ir); /* LDC.L @Rm+, SR */
                                        break;
                                    case 0x1:
                                        UNIMP(ir); /* LDC.L @Rm+, GBR */
                                        break;
                                    case 0x2:
                                        UNIMP(ir); /* LDC.L @Rm+, VBR */
                                        break;
                                    case 0x3:
                                        UNIMP(ir); /* LDC.L @Rm+, SSR */
                                        break;
                                    case 0x4:
                                        UNIMP(ir); /* LDC.L @Rm+, SPC */
                                        break;
                                    default:
                                        UNDEF(ir);
                                        break;
                                }
                                break;
                            case 0x1:
                                UNIMP(ir); /* LDC.L @Rm+, Rn_BANK */
                                break;
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* SHLL2 Rn */
                                break;
                            case 0x1:
                                UNIMP(ir); /* SHLL8 Rn */
                                break;
                            case 0x2:
                                UNIMP(ir); /* SHLL16 Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x9:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* SHLR2 Rn */
                                break;
                            case 0x1:
                                UNIMP(ir); /* SHLR8 Rn */
                                break;
                            case 0x2:
                                UNIMP(ir); /* SHLR16 Rn */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xA:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* LDS Rm, MACH */
                                break;
                            case 0x1:
                                UNIMP(ir); /* LDS Rm, MACL */
                                break;
                            case 0x2:
                                UNIMP(ir); /* LDS Rm, PR */
                                break;
                            case 0x3:
                                UNIMP(ir); /* LDC Rm, SGR */
                                break;
                            case 0x5:
                                UNIMP(ir); /* LDS Rm, FPUL */
                                break;
                            case 0x6:
                                UNIMP(ir); /* LDS Rm, FPSCR */
                                break;
                            case 0xF:
                                UNIMP(ir); /* LDC Rm, DBR */
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xB:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* JSR @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 281 "../../src/sh4/sh4core.in"
                                /* Note: PR is already set */ 
                                sh4r.pc += 2;
                                tmp = sh4r.r[Rn];
                                sh4_finalize_instruction();
                                sh4r.pc = tmp;
                                sh4r.new_pc = tmp + 2;
                                sh4r.slice_cycle += sh4_cpu_period;
                                return;
                                }
                                break;
                            case 0x1:
                                UNIMP(ir); /* TAS.B @Rn */
                                break;
                            case 0x2:
                                { /* JMP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 272 "../../src/sh4/sh4core.in"
                                sh4r.pc += 2;
                                tmp = sh4r.r[Rn];
                                sh4_finalize_instruction();
                                sh4r.pc = tmp;
                                sh4r.new_pc = tmp + 2;
                                sh4r.slice_cycle += sh4_cpu_period;
                                return;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xC:
                        UNIMP(ir); /* SHAD Rm, Rn */
                        break;
                    case 0xD:
                        UNIMP(ir); /* SHLD Rm, Rn */
                        break;
                    case 0xE:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        UNIMP(ir); /* LDC Rm, SR */
                                        break;
                                    case 0x1:
                                        UNIMP(ir); /* LDC Rm, GBR */
                                        break;
                                    case 0x2:
                                        UNIMP(ir); /* LDC Rm, VBR */
                                        break;
                                    case 0x3:
                                        UNIMP(ir); /* LDC Rm, SSR */
                                        break;
                                    case 0x4:
                                        UNIMP(ir); /* LDC Rm, SPC */
                                        break;
                                    default:
                                        UNDEF(ir);
                                        break;
                                }
                                break;
                            case 0x1:
                                UNIMP(ir); /* LDC Rm, Rn_BANK */
                                break;
                        }
                        break;
                    case 0xF:
                        UNIMP(ir); /* MAC.W @Rm+, @Rn+ */
                        break;
                }
                break;
            case 0x5:
                UNIMP(ir); /* MOV.L @(disp, Rm), Rn */
                break;
            case 0x6:
                switch( ir&0xF ) {
                    case 0x0:
                        UNIMP(ir); /* MOV.B @Rm, Rn */
                        break;
                    case 0x1:
                        UNIMP(ir); /* MOV.W @Rm, Rn */
                        break;
                    case 0x2:
                        UNIMP(ir); /* MOV.L @Rm, Rn */
                        break;
                    case 0x3:
                        UNIMP(ir); /* MOV Rm, Rn */
                        break;
                    case 0x4:
                        { /* MOV.B @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 310 "../../src/sh4/sh4core.in"
                        if( Rm != Rn ) { sh4r.r[Rm] ++;  }
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 311 "../../src/sh4/sh4core.in"
                        if( Rm != Rn ) { sh4r.r[Rm] += 2; }
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 312 "../../src/sh4/sh4core.in"
                        if( Rm != Rn ) { sh4r.r[Rm] += 4; }
                        }
                        break;
                    case 0x7:
                        UNIMP(ir); /* NOT Rm, Rn */
                        break;
                    case 0x8:
                        UNIMP(ir); /* SWAP.B Rm, Rn */
                        break;
                    case 0x9:
                        UNIMP(ir); /* SWAP.W Rm, Rn */
                        break;
                    case 0xA:
                        UNIMP(ir); /* NEGC Rm, Rn */
                        break;
                    case 0xB:
                        UNIMP(ir); /* NEG Rm, Rn */
                        break;
                    case 0xC:
                        UNIMP(ir); /* EXTU.B Rm, Rn */
                        break;
                    case 0xD:
                        UNIMP(ir); /* EXTU.W Rm, Rn */
                        break;
                    case 0xE:
                        UNIMP(ir); /* EXTS.B Rm, Rn */
                        break;
                    case 0xF:
                        UNIMP(ir); /* EXTS.W Rm, Rn */
                        break;
                }
                break;
            case 0x7:
                UNIMP(ir); /* ADD #imm, Rn */
                break;
            case 0x8:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        UNIMP(ir); /* MOV.B R0, @(disp, Rn) */
                        break;
                    case 0x1:
                        UNIMP(ir); /* MOV.W R0, @(disp, Rn) */
                        break;
                    case 0x4:
                        UNIMP(ir); /* MOV.B @(disp, Rm), R0 */
                        break;
                    case 0x5:
                        UNIMP(ir); /* MOV.W @(disp, Rm), R0 */
                        break;
                    case 0x8:
                        UNIMP(ir); /* CMP/EQ #imm, R0 */
                        break;
                    case 0x9:
                        UNIMP(ir); /* BT disp */
                        break;
                    case 0xB:
                        UNIMP(ir); /* BF disp */
                        break;
                    case 0xD:
                        { /* BT/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 265 "../../src/sh4/sh4core.in"
                        sh4r.pc += 2;
                        sh4_finalize_instruction();
                        if( sh4r.t ) {
                            sh4r.pc += disp;
                        }
                        }
                        break;
                    case 0xF:
                        { /* BF/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 258 "../../src/sh4/sh4core.in"
                        sh4r.pc += 2;
                        sh4_finalize_instruction();
                        if( !sh4r.t ) {
                            sh4r.pc += disp;
                        }
                        }
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
            case 0x9:
                UNIMP(ir); /* MOV.W @(disp, PC), Rn */
                break;
            case 0xA:
                { /* BRA disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
#line 234 "../../src/sh4/sh4core.in"
                sh4r.pc += 2; 
                sh4_finalize_instruction(); 
                sh4r.pc += disp;
                }
                break;
            case 0xB:
                { /* BSR disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
#line 245 "../../src/sh4/sh4core.in"
                /* Note: PR is already set */ 
                sh4r.pc += 2;
                sh4_finalize_instruction();
                sh4r.pc += disp;
                }
                break;
            case 0xC:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        UNIMP(ir); /* MOV.B R0, @(disp, GBR) */
                        break;
                    case 0x1:
                        UNIMP(ir); /* MOV.W R0, @(disp, GBR) */
                        break;
                    case 0x2:
                        UNIMP(ir); /* MOV.L R0, @(disp, GBR) */
                        break;
                    case 0x3:
                        UNIMP(ir); /* TRAPA #imm */
                        break;
                    case 0x4:
                        UNIMP(ir); /* MOV.B @(disp, GBR), R0 */
                        break;
                    case 0x5:
                        UNIMP(ir); /* MOV.W @(disp, GBR), R0 */
                        break;
                    case 0x6:
                        UNIMP(ir); /* MOV.L @(disp, GBR), R0 */
                        break;
                    case 0x7:
                        UNIMP(ir); /* MOVA @(disp, PC), R0 */
                        break;
                    case 0x8:
                        UNIMP(ir); /* TST #imm, R0 */
                        break;
                    case 0x9:
                        UNIMP(ir); /* AND #imm, R0 */
                        break;
                    case 0xA:
                        UNIMP(ir); /* XOR #imm, R0 */
                        break;
                    case 0xB:
                        UNIMP(ir); /* OR #imm, R0 */
                        break;
                    case 0xC:
                        UNIMP(ir); /* TST.B #imm, @(R0, GBR) */
                        break;
                    case 0xD:
                        UNIMP(ir); /* AND.B #imm, @(R0, GBR) */
                        break;
                    case 0xE:
                        UNIMP(ir); /* XOR.B #imm, @(R0, GBR) */
                        break;
                    case 0xF:
                        UNIMP(ir); /* OR.B #imm, @(R0, GBR) */
                        break;
                }
                break;
            case 0xD:
                UNIMP(ir); /* MOV.L @(disp, PC), Rn */
                break;
            case 0xE:
                UNIMP(ir); /* MOV #imm, Rn */
                break;
            case 0xF:
                switch( ir&0xF ) {
                    case 0x0:
                        UNIMP(ir); /* FADD FRm, FRn */
                        break;
                    case 0x1:
                        UNIMP(ir); /* FSUB FRm, FRn */
                        break;
                    case 0x2:
                        UNIMP(ir); /* FMUL FRm, FRn */
                        break;
                    case 0x3:
                        UNIMP(ir); /* FDIV FRm, FRn */
                        break;
                    case 0x4:
                        UNIMP(ir); /* FCMP/EQ FRm, FRn */
                        break;
                    case 0x5:
                        UNIMP(ir); /* FCMP/GT FRm, FRn */
                        break;
                    case 0x6:
                        UNIMP(ir); /* FMOV @(R0, Rm), FRn */
                        break;
                    case 0x7:
                        UNIMP(ir); /* FMOV FRm, @(R0, Rn) */
                        break;
                    case 0x8:
                        UNIMP(ir); /* FMOV @Rm, FRn */
                        break;
                    case 0x9:
                        UNIMP(ir); /* FMOV @Rm+, FRn */
                        break;
                    case 0xA:
                        UNIMP(ir); /* FMOV FRm, @Rn */
                        break;
                    case 0xB:
                        UNIMP(ir); /* FMOV FRm, @-Rn */
                        break;
                    case 0xC:
                        UNIMP(ir); /* FMOV FRm, FRn */
                        break;
                    case 0xD:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                UNIMP(ir); /* FSTS FPUL, FRn */
                                break;
                            case 0x1:
                                UNIMP(ir); /* FLDS FRm, FPUL */
                                break;
                            case 0x2:
                                UNIMP(ir); /* FLOAT FPUL, FRn */
                                break;
                            case 0x3:
                                UNIMP(ir); /* FTRC FRm, FPUL */
                                break;
                            case 0x4:
                                UNIMP(ir); /* FNEG FRn */
                                break;
                            case 0x5:
                                UNIMP(ir); /* FABS FRn */
                                break;
                            case 0x6:
                                UNIMP(ir); /* FSQRT FRn */
                                break;
                            case 0x7:
                                UNIMP(ir); /* FSRRA FRn */
                                break;
                            case 0x8:
                                UNIMP(ir); /* FLDI0 FRn */
                                break;
                            case 0x9:
                                UNIMP(ir); /* FLDI1 FRn */
                                break;
                            case 0xA:
                                UNIMP(ir); /* FCNVSD FPUL, FRn */
                                break;
                            case 0xB:
                                UNIMP(ir); /* FCNVDS FRm, FPUL */
                                break;
                            case 0xE:
                                UNIMP(ir); /* FIPR FVm, FVn */
                                break;
                            case 0xF:
                                switch( (ir&0x100) >> 8 ) {
                                    case 0x0:
                                        UNIMP(ir); /* FSCA FPUL, FRn */
                                        break;
                                    case 0x1:
                                        switch( (ir&0x200) >> 9 ) {
                                            case 0x0:
                                                UNIMP(ir); /* FTRV XMTRX, FVn */
                                                break;
                                            case 0x1:
                                                switch( (ir&0xC00) >> 10 ) {
                                                    case 0x0:
                                                        UNIMP(ir); /* FSCHG */
                                                        break;
                                                    case 0x2:
                                                        UNIMP(ir); /* FRCHG */
                                                        break;
                                                    case 0x3:
                                                        UNIMP(ir); /* UNDEF */
                                                        break;
                                                    default:
                                                        UNDEF(ir);
                                                        break;
                                                }
                                                break;
                                        }
                                        break;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xE:
                        UNIMP(ir); /* FMAC FR0, FRm, FRn */
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
        }
#pragma clang diagnostic pop
#line 313 "../../src/sh4/sh4core.in"

    sh4r.in_delay_slot = 0;
    sh4r.pc += 2;
    sh4r.new_pc = sh4r.pc+2;
    sh4r.slice_cycle += sh4_cpu_period;
}

#undef UNDEF
#undef UNIMP

#define UNDEF(ir) return sh4_raise_slot_exception(EXC_ILLEGAL, EXC_SLOT_ILLEGAL)
#define UNIMP(ir) do{ ERROR( "Halted on unimplemented instruction at %08x, opcode = %04x", sh4r.pc, ir ); sh4_core_exit(CORE_EXIT_HALT); return FALSE; }while(0)


gboolean sh4_execute_instruction( void )
{
    uint32_t pc;
    unsigned short ir;
    uint32_t tmp;
    float ftmp;
    double dtmp;
    sh4addr_t addrtmp; // temporary holder for memory addresses
    mem_region_fn_t fntmp;
    

#define R0 sh4r.r[0]
    pc = sh4r.pc;
    if( pc > 0xFFFFFF00 ) {
	/* SYSCALL Magic */
        sh4r.in_delay_slot = 0;
        sh4r.pc = sh4r.pr;
        sh4r.new_pc = sh4r.pc + 2;
	syscall_invoke( pc );
        return TRUE;
    }
    CHECKRALIGN16(pc);

#ifdef ENABLE_SH4STATS
    sh4_stats_add_by_pc(sh4r.pc);
#endif

    /* Read instruction */
    if( !IS_IN_ICACHE(pc) ) {
        gboolean delay_slot = sh4r.in_delay_slot;
	if( !mmu_update_icache(pc) ) {
	    if( delay_slot ) {
	        sh4r.spc -= 2;
	    }
	    // Fault - look for the fault handler
	    if( !mmu_update_icache(sh4r.pc) ) {
		// double fault - halt
		ERROR( "Double fault - halting" );
		return FALSE;
	    }
	}
	pc = sh4r.pc;

        if( !IS_IN_ICACHE(pc) ) {
            ERROR( "Branch to unmapped address %08x", sh4r.pc );
            return FALSE;
        }
    }

    ir = *(uint16_t *)GET_ICACHE_PTR(sh4r.pc);
    
    /* FIXME: This is a bit of a hack, but the PC of the delay slot should not
     * be visible until after the instruction has executed (for exception 
     * correctness)
     */
    if( sh4r.in_delay_slot ) {
    	sh4r.pc -= 2;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
        switch( (ir&0xF000) >> 12 ) {
            case 0x0:
                switch( ir&0xF ) {
                    case 0x2:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* STC SR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 1079 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        sh4r.r[Rn] = sh4_read_sr();
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC GBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 1083 "../../src/sh4/sh4core.in"
                                        sh4r.r[Rn] = sh4r.gbr;
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC VBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 1086 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        sh4r.r[Rn] = sh4r.vbr;
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC SSR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 1090 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        sh4r.r[Rn] = sh4r.ssr;
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC SPC, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 1094 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        sh4r.r[Rn] = sh4r.spc;
                                        }
                                        break;
                                    default:
                                        UNDEF(ir);
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* STC Rm_BANK, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm_BANK = ((ir>>4)&0x7); 
#line 1098 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                sh4r.r[Rn] = sh4r.r_bank[Rm_BANK];
                                }
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* BSRF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 743 "../../src/sh4/sh4core.in"
                                CHECKSLOTILLEGAL();
                                CHECKDEST( pc + 4 + sh4r.r[Rn] );
                                sh4r.in_delay_slot = 1;
                                sh4r.pr = sh4r.pc + 4;
                                sh4r.pc = sh4r.new_pc;
                                sh4r.new_pc = pc + 4 + sh4r.r[Rn];
                                TRACE_CALL( pc, sh4r.new_pc );
                                return TRUE;
                                }
                                break;
                            case 0x2:
                                { /* BRAF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 735 "../../src/sh4/sh4core.in"
                                CHECKSLOTILLEGAL();
                                CHECKDEST( pc + 4 + sh4r.r[Rn] );
                                sh4r.in_delay_slot = 1;
                                sh4r.pc = sh4r.new_pc;
                                sh4r.new_pc = pc + 4 + sh4r.r[Rn];
                                return TRUE;
                                }
                                break;
                            case 0x8:
                                { /* PREF @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 476 "../../src/sh4/sh4core.in"
                                MEM_PREFETCH(sh4r.r[Rn]);
                                }
                                break;
                            case 0x9:
                                { /* OCBI @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xA:
                                { /* OCBP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xB:
                                { /* OCBWB @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xC:
                                { /* MOVCA.L R0, @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 482 "../../src/sh4/sh4core.in"
                                tmp = sh4r.r[Rn];
                                CHECKWALIGN32(tmp);
                                MEM_WRITE_LONG( tmp, R0 );
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x4:
                        { /* MOV.B Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 486 "../../src/sh4/sh4core.in"
                        MEM_WRITE_BYTE( R0 + sh4r.r[Rn], sh4r.r[Rm] );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 488 "../../src/sh4/sh4core.in"
                        CHECKWALIGN16( R0 + sh4r.r[Rn] );
                        MEM_WRITE_WORD( R0 + sh4r.r[Rn], sh4r.r[Rm] );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 492 "../../src/sh4/sh4core.in"
                        CHECKWALIGN32( R0 + sh4r.r[Rn] );
                        MEM_WRITE_LONG( R0 + sh4r.r[Rn], sh4r.r[Rm] );
                        }
                        break;
                    case 0x7:
                        { /* MUL.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 707 "../../src/sh4/sh4core.in"
                        sh4r.mac = (sh4r.mac&0xFFFFFFFF00000000LL) |
                                               (sh4r.r[Rm] * sh4r.r[Rn]);
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                { /* CLRT */
#line 466 "../../src/sh4/sh4core.in"
                                sh4r.t = 0;
                                }
                                break;
                            case 0x1:
                                { /* SETT */
#line 467 "../../src/sh4/sh4core.in"
                                sh4r.t = 1;
                                }
                                break;
                            case 0x2:
                                { /* CLRMAC */
#line 468 "../../src/sh4/sh4core.in"
                                sh4r.mac = 0;
                                }
                                break;
                            case 0x3:
                                { /* LDTLB */
#line 469 "../../src/sh4/sh4core.in"
                                MMU_ldtlb();
                                }
                                break;
                            case 0x4:
                                { /* CLRS */
#line 470 "../../src/sh4/sh4core.in"
                                sh4r.s = 0;
                                }
                                break;
                            case 0x5:
                                { /* SETS */
#line 471 "../../src/sh4/sh4core.in"
                                sh4r.s = 1;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x9:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* NOP */
#line 473 "../../src/sh4/sh4core.in"
                                /* NOP */
                                }
                                break;
                            case 0x1:
                                { /* DIV0U */
#line 615 "../../src/sh4/sh4core.in"
                                sh4r.m = sh4r.q = sh4r.t = 0;
                                }
                                break;
                            case 0x2:
                                { /* MOVT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 472 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] = sh4r.t;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xA:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* STS MACH, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 860 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] = (sh4r.mac>>32);
                                }
                                break;
                            case 0x1:
                                { /* STS MACL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 906 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] = (uint32_t)sh4r.mac;
                                }
                                break;
                            case 0x2:
                                { /* STS PR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 934 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] = sh4r.pr;
                                }
                                break;
                            case 0x3:
                                { /* STC SGR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 963 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                sh4r.r[Rn] = sh4r.sgr;
                                }
                                break;
                            case 0x5:
                                { /* STS FPUL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 1005 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                sh4r.r[Rn] = FPULi;
                                }
                                break;
                            case 0x6:
                                { /* STS FPSCR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 1025 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                sh4r.r[Rn] = sh4r.fpscr;
                                }
                                break;
                            case 0xF:
                                { /* STC DBR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 1045 "../../src/sh4/sh4core.in"
                                CHECKPRIV(); sh4r.r[Rn] = sh4r.dbr;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xB:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                { /* RTS */
#line 816 "../../src/sh4/sh4core.in"
                                CHECKSLOTILLEGAL();
                                CHECKDEST( sh4r.pr );
                                sh4r.in_delay_slot = 1;
                                sh4r.pc = sh4r.new_pc;
                                sh4r.new_pc = sh4r.pr;
                                TRACE_RETURN( pc, sh4r.new_pc );
                                return TRUE;
                                }
                                break;
                            case 0x1:
                                { /* SLEEP */
#line 825 "../../src/sh4/sh4core.in"
                                if( MMIO_READ( CPG, STBCR ) & 0x80 ) {
                            	sh4r.sh4_state = SH4_STATE_STANDBY;
                                } else {
                            	sh4r.sh4_state = SH4_STATE_SLEEP;
                                }
                                return FALSE; /* Halt CPU */
                                }
                                break;
                            case 0x2:
                                { /* RTE */
#line 833 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                CHECKDEST( sh4r.spc );
                                CHECKSLOTILLEGAL();
                                sh4r.in_delay_slot = 1;
                                sh4r.pc = sh4r.new_pc;
                                sh4r.new_pc = sh4r.spc;
                                sh4_write_sr( sh4r.ssr );
                                return TRUE;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xC:
                        { /* MOV.B @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 495 "../../src/sh4/sh4core.in"
                        MEM_READ_BYTE( R0 + sh4r.r[Rm], sh4r.r[Rn] );
                        }
                        break;
                    case 0xD:
                        { /* MOV.W @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 496 "../../src/sh4/sh4core.in"
                        CHECKRALIGN16( R0 + sh4r.r[Rm] );
                           MEM_READ_WORD( R0 + sh4r.r[Rm], sh4r.r[Rn] );
                        }
                        break;
                    case 0xE:
                        { /* MOV.L @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 499 "../../src/sh4/sh4core.in"
                        CHECKRALIGN32( R0 + sh4r.r[Rm] );
                           MEM_READ_LONG( R0 + sh4r.r[Rm], sh4r.r[Rn] );
                        }
                        break;
                    case 0xF:
                        { /* MAC.L @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 680 "../../src/sh4/sh4core.in"
                        int64_t tmpl;
                        if( Rm == Rn ) {
                    	CHECKRALIGN32( sh4r.r[Rn] );
                    	MEM_READ_LONG(sh4r.r[Rn], tmp);
                    	tmpl = SIGNEXT32(tmp);
                    	MEM_READ_LONG(sh4r.r[Rn]+4, tmp);
                    	tmpl = tmpl * SIGNEXT32(tmp) + sh4r.mac;
                    	sh4r.r[Rn] += 8;
                        } else {
                    	CHECKRALIGN32( sh4r.r[Rm] );
                    	CHECKRALIGN32( sh4r.r[Rn] );
                    	MEM_READ_LONG(sh4r.r[Rn], tmp);
                    	tmpl = SIGNEXT32(tmp);
                    	MEM_READ_LONG(sh4r.r[Rm], tmp);
                    	tmpl = tmpl * SIGNEXT32(tmp) + sh4r.mac;
                    	sh4r.r[Rn] += 4;
                    	sh4r.r[Rm] += 4;
                        }
                        if( sh4r.s ) {
                            /* 48-bit Saturation. Yuch */
                            if( tmpl < (int64_t)0xFFFF800000000000LL )
                                tmpl = 0xFFFF800000000000LL;
                            else if( tmpl > (int64_t)0x00007FFFFFFFFFFFLL )
                                tmpl = 0x00007FFFFFFFFFFFLL;
                        }
                        sh4r.mac = tmpl;
                        }
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
            case 0x1:
                { /* MOV.L Rm, @(disp, Rn) */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<2; 
#line 503 "../../src/sh4/sh4core.in"
                tmp = sh4r.r[Rn] + disp;
                CHECKWALIGN32( tmp );
                MEM_WRITE_LONG( tmp, sh4r.r[Rm] );
                }
                break;
            case 0x2:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 507 "../../src/sh4/sh4core.in"
                        MEM_WRITE_BYTE( sh4r.r[Rn], sh4r.r[Rm] );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 508 "../../src/sh4/sh4core.in"
                        CHECKWALIGN16( sh4r.r[Rn] ); MEM_WRITE_WORD( sh4r.r[Rn], sh4r.r[Rm] );
                        }
                        break;
                    case 0x2:
                        { /* MOV.L Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 509 "../../src/sh4/sh4core.in"
                        CHECKWALIGN32( sh4r.r[Rn] ); MEM_WRITE_LONG( sh4r.r[Rn], sh4r.r[Rm] );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 510 "../../src/sh4/sh4core.in"
                        MEM_WRITE_BYTE( sh4r.r[Rn]-1, sh4r.r[Rm] ); sh4r.r[Rn]--;
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 511 "../../src/sh4/sh4core.in"
                        CHECKWALIGN16( sh4r.r[Rn] ); MEM_WRITE_WORD( sh4r.r[Rn]-2, sh4r.r[Rm] ); sh4r.r[Rn] -= 2;
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 512 "../../src/sh4/sh4core.in"
                        CHECKWALIGN32( sh4r.r[Rn] ); MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.r[Rm] ); sh4r.r[Rn] -= 4;
                        }
                        break;
                    case 0x7:
                        { /* DIV0S Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 617 "../../src/sh4/sh4core.in"
                        sh4r.q = sh4r.r[Rn]>>31;
                        sh4r.m = sh4r.r[Rm]>>31;
                        sh4r.t = sh4r.q ^ sh4r.m;
                        }
                        break;
                    case 0x8:
                        { /* TST Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 398 "../../src/sh4/sh4core.in"
                        sh4r.t = (sh4r.r[Rn]&sh4r.r[Rm] ? 0 : 1);
                        }
                        break;
                    case 0x9:
                        { /* AND Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 386 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] &= sh4r.r[Rm];
                        }
                        break;
                    case 0xA:
                        { /* XOR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 401 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] ^= sh4r.r[Rm];
                        }
                        break;
                    case 0xB:
                        { /* OR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 390 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] |= sh4r.r[Rm];
                        }
                        break;
                    case 0xC:
                        { /* CMP/STR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 597 "../../src/sh4/sh4core.in"
                        /* set T = 1 if any byte in RM & RN is the same */
                        tmp = sh4r.r[Rm] ^ sh4r.r[Rn];
                        sh4r.t = ((tmp&0x000000FF)==0 || (tmp&0x0000FF00)==0 ||
                                 (tmp&0x00FF0000)==0 || (tmp&0xFF000000)==0)?1:0;
                        }
                        break;
                    case 0xD:
                        { /* XTRCT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 404 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = (sh4r.r[Rn]>>16) | (sh4r.r[Rm]<<16);
                        }
                        break;
                    case 0xE:
                        { /* MULU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 710 "../../src/sh4/sh4core.in"
                        sh4r.mac = (sh4r.mac&0xFFFFFFFF00000000LL) |
                                   (uint32_t)((sh4r.r[Rm]&0xFFFF) * (sh4r.r[Rn]&0xFFFF));
                        }
                        break;
                    case 0xF:
                        { /* MULS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 714 "../../src/sh4/sh4core.in"
                        sh4r.mac = (sh4r.mac&0xFFFFFFFF00000000LL) |
                                   (uint32_t)(SIGNEXT32((int16_t)(sh4r.r[Rm])) * SIGNEXT32((int16_t)(sh4r.r[Rn])));
                        }
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
            case 0x3:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* CMP/EQ Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 589 "../../src/sh4/sh4core.in"
                        sh4r.t = ( sh4r.r[Rm] == sh4r.r[Rn] ? 1 : 0 );
                        }
                        break;
                    case 0x2:
                        { /* CMP/HS Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 593 "../../src/sh4/sh4core.in"
                        sh4r.t = ( sh4r.r[Rn] >= sh4r.r[Rm] ? 1 : 0 );
                        }
                        break;
                    case 0x3:
                        { /* CMP/GE Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 590 "../../src/sh4/sh4core.in"
                        sh4r.t = ( ((int32_t)sh4r.r[Rn]) >= ((int32_t)sh4r.r[Rm]) ? 1 : 0 );
                        }
                        break;
                    case 0x4:
                        { /* DIV1 Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 622 "../../src/sh4/sh4core.in"
                        /* This is derived from the sh4 manual with some simplifications */
                        uint32_t tmp0, tmp1, tmp2, dir;
                    
                        dir = sh4r.q ^ sh4r.m;
                        sh4r.q = (sh4r.r[Rn] >> 31);
                        tmp2 = sh4r.r[Rm];
                        sh4r.r[Rn] = (sh4r.r[Rn] << 1) | sh4r.t;
                        tmp0 = sh4r.r[Rn];
                        if( dir ) {
                             sh4r.r[Rn] += tmp2;
                             tmp1 = (sh4r.r[Rn]<tmp0 ? 1 : 0 );
                        } else {
                             sh4r.r[Rn] -= tmp2;
                             tmp1 = (sh4r.r[Rn]>tmp0 ? 1 : 0 );
                        }
                        sh4r.q ^= sh4r.m ^ tmp1;
                        sh4r.t = ( sh4r.q == sh4r.m ? 1 : 0 );
                        }
                        break;
                    case 0x5:
                        { /* DMULU.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 641 "../../src/sh4/sh4core.in"
                        sh4r.mac = ((uint64_t)sh4r.r[Rm]) * ((uint64_t)sh4r.r[Rn]);
                        }
                        break;
                    case 0x6:
                        { /* CMP/HI Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 592 "../../src/sh4/sh4core.in"
                        sh4r.t = ( sh4r.r[Rn] > sh4r.r[Rm] ? 1 : 0 );
                        }
                        break;
                    case 0x7:
                        { /* CMP/GT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 591 "../../src/sh4/sh4core.in"
                        sh4r.t = ( ((int32_t)sh4r.r[Rn]) > ((int32_t)sh4r.r[Rm]) ? 1 : 0 );
                        }
                        break;
                    case 0x8:
                        { /* SUB Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 723 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] -= sh4r.r[Rm];
                        }
                        break;
                    case 0xA:
                        { /* SUBC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 725 "../../src/sh4/sh4core.in"
                        tmp = sh4r.r[Rn];
                        sh4r.r[Rn] = sh4r.r[Rn] - sh4r.r[Rm] - sh4r.t;
                        sh4r.t = (sh4r.r[Rn] > tmp || (sh4r.r[Rn] == tmp && sh4r.t == 1));
                        }
                        break;
                    case 0xB:
                        { /* SUBV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 730 "../../src/sh4/sh4core.in"
                        tmp = sh4r.r[Rn] - sh4r.r[Rm];
                        sh4r.t = ( (sh4r.r[Rn]>>31) != (sh4r.r[Rm]>>31) && ((sh4r.r[Rn]>>31) != (tmp>>31)) );
                        sh4r.r[Rn] = tmp;
                        }
                        break;
                    case 0xC:
                        { /* ADD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 603 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] += sh4r.r[Rm];
                        }
                        break;
                    case 0xD:
                        { /* DMULS.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 640 "../../src/sh4/sh4core.in"
                        sh4r.mac = SIGNEXT32(sh4r.r[Rm]) * SIGNEXT32(sh4r.r[Rn]);
                        }
                        break;
                    case 0xE:
                        { /* ADDC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 606 "../../src/sh4/sh4core.in"
                        tmp = sh4r.r[Rn];
                        sh4r.r[Rn] += sh4r.r[Rm] + sh4r.t;
                        sh4r.t = ( sh4r.r[Rn] < tmp || (sh4r.r[Rn] == tmp && sh4r.t != 0) ? 1 : 0 );
                        }
                        break;
                    case 0xF:
                        { /* ADDV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 611 "../../src/sh4/sh4core.in"
                        tmp = sh4r.r[Rn] + sh4r.r[Rm];
                        sh4r.t = ( (sh4r.r[Rn]>>31) == (sh4r.r[Rm]>>31) && ((sh4r.r[Rn]>>31) != (tmp>>31)) );
                        sh4r.r[Rn] = tmp;
                        }
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
            case 0x4:
                switch( ir&0xF ) {
                    case 0x0:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 450 "../../src/sh4/sh4core.in"
                                sh4r.t = sh4r.r[Rn] >> 31; sh4r.r[Rn] <<= 1;
                                }
                                break;
                            case 0x1:
                                { /* DT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 643 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] --;
                                sh4r.t = ( sh4r.r[Rn] == 0 ? 1 : 0 );
                                }
                                break;
                            case 0x2:
                                { /* SHAL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 443 "../../src/sh4/sh4core.in"
                                sh4r.t = sh4r.r[Rn] >> 31;
                                sh4r.r[Rn] <<= 1;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x1:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 451 "../../src/sh4/sh4core.in"
                                sh4r.t = sh4r.r[Rn] & 0x00000001; sh4r.r[Rn] >>= 1;
                                }
                                break;
                            case 0x1:
                                { /* CMP/PZ Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 595 "../../src/sh4/sh4core.in"
                                sh4r.t = ( ((int32_t)sh4r.r[Rn]) >= 0 ? 1 : 0 );
                                }
                                break;
                            case 0x2:
                                { /* SHAR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 447 "../../src/sh4/sh4core.in"
                                sh4r.t = sh4r.r[Rn] & 0x00000001;
                                sh4r.r[Rn] = ((int32_t)sh4r.r[Rn]) >> 1;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x2:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* STS.L MACH, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 862 "../../src/sh4/sh4core.in"
                                CHECKWALIGN32( sh4r.r[Rn] );
                                MEM_WRITE_LONG( sh4r.r[Rn]-4, (sh4r.mac>>32) );
                                sh4r.r[Rn] -= 4;
                                }
                                break;
                            case 0x1:
                                { /* STS.L MACL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 908 "../../src/sh4/sh4core.in"
                                CHECKWALIGN32( sh4r.r[Rn] );
                                MEM_WRITE_LONG( sh4r.r[Rn]-4, (uint32_t)sh4r.mac );
                                sh4r.r[Rn] -= 4;
                                }
                                break;
                            case 0x2:
                                { /* STS.L PR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 936 "../../src/sh4/sh4core.in"
                                CHECKWALIGN32( sh4r.r[Rn] );
                                MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.pr );
                                sh4r.r[Rn] -= 4;
                                }
                                break;
                            case 0x3:
                                { /* STC.L SGR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 967 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                CHECKWALIGN32( sh4r.r[Rn] );
                                MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.sgr );
                                sh4r.r[Rn] -= 4;
                                }
                                break;
                            case 0x5:
                                { /* STS.L FPUL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 1009 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                CHECKWALIGN32( sh4r.r[Rn] );
                                MEM_WRITE_LONG( sh4r.r[Rn]-4, FPULi );
                                sh4r.r[Rn] -= 4;
                                }
                                break;
                            case 0x6:
                                { /* STS.L FPSCR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 1029 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                CHECKWALIGN32( sh4r.r[Rn] );
                                MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.fpscr );
                                sh4r.r[Rn] -= 4;
                                }
                                break;
                            case 0xF:
                                { /* STC.L DBR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 1047 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                CHECKWALIGN32( sh4r.r[Rn] );
                                MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.dbr );
                                sh4r.r[Rn] -= 4;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* STC.L SR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 867 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        CHECKWALIGN32( sh4r.r[Rn] );
                                        MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4_read_sr() );
                                        sh4r.r[Rn] -= 4;
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC.L GBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 913 "../../src/sh4/sh4core.in"
                                        CHECKWALIGN32( sh4r.r[Rn] );
                                        MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.gbr );
                                        sh4r.r[Rn] -= 4;
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC.L VBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 941 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        CHECKWALIGN32( sh4r.r[Rn] );
                                        MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.vbr );
                                        sh4r.r[Rn] -= 4;
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC.L SSR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 973 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        CHECKWALIGN32( sh4r.r[Rn] );
                                        MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.ssr );
                                        sh4r.r[Rn] -= 4;
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC.L SPC, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 989 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        CHECKWALIGN32( sh4r.r[Rn] );
                                        MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.spc );
                                        sh4r.r[Rn] -= 4;
                                        }
                                        break;
                                    default:
                                        UNDEF(ir);
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* STC.L Rm_BANK, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm_BANK = ((ir>>4)&0x7); 
#line 1063 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                CHECKWALIGN32( sh4r.r[Rn] );
                                MEM_WRITE_LONG( sh4r.r[Rn]-4, sh4r.r_bank[Rm_BANK] );
                                sh4r.r[Rn] -= 4;
                                }
                                break;
                        }
                        break;
                    case 0x4:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* ROTL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 407 "../../src/sh4/sh4core.in"
                                sh4r.t = sh4r.r[Rn] >> 31;
                                sh4r.r[Rn] <<= 1;
                                sh4r.r[Rn] |= sh4r.t;
                                }
                                break;
                            case 0x2:
                                { /* ROTCL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 417 "../../src/sh4/sh4core.in"
                                tmp = sh4r.r[Rn] >> 31;
                                sh4r.r[Rn] <<= 1;
                                sh4r.r[Rn] |= sh4r.t;
                                sh4r.t = tmp;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x5:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* ROTR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 412 "../../src/sh4/sh4core.in"
                                sh4r.t = sh4r.r[Rn] & 0x00000001;
                                sh4r.r[Rn] >>= 1;
                                sh4r.r[Rn] |= (sh4r.t << 31);
                                }
                                break;
                            case 0x1:
                                { /* CMP/PL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 594 "../../src/sh4/sh4core.in"
                                sh4r.t = ( ((int32_t)sh4r.r[Rn]) > 0 ? 1 : 0 );
                                }
                                break;
                            case 0x2:
                                { /* ROTCR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 423 "../../src/sh4/sh4core.in"
                                tmp = sh4r.r[Rn] & 0x00000001;
                                sh4r.r[Rn] >>= 1;
                                sh4r.r[Rn] |= (sh4r.t << 31 );
                                sh4r.t = tmp;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x6:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* LDS.L @Rm+, MACH */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 873 "../../src/sh4/sh4core.in"
                                CHECKRALIGN32( sh4r.r[Rm] );
                                MEM_READ_LONG(sh4r.r[Rm], tmp);
                                sh4r.mac = (sh4r.mac & 0x00000000FFFFFFFF) |
                            	(((uint64_t)tmp)<<32);
                                sh4r.r[Rm] += 4;
                                }
                                break;
                            case 0x1:
                                { /* LDS.L @Rm+, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 918 "../../src/sh4/sh4core.in"
                                CHECKRALIGN32( sh4r.r[Rm] );
                                MEM_READ_LONG(sh4r.r[Rm], tmp);
                                sh4r.mac = (sh4r.mac & 0xFFFFFFFF00000000LL) |
                                           (uint64_t)((uint32_t)tmp);
                                sh4r.r[Rm] += 4;
                                }
                                break;
                            case 0x2:
                                { /* LDS.L @Rm+, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 947 "../../src/sh4/sh4core.in"
                                CHECKRALIGN32( sh4r.r[Rm] );
                                MEM_READ_LONG( sh4r.r[Rm], sh4r.pr );
                                sh4r.r[Rm] += 4;
                                }
                                break;
                            case 0x3:
                                { /* LDC.L @Rm+, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 901 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                CHECKRALIGN32( sh4r.r[Rm] );
                                MEM_READ_LONG(sh4r.r[Rm], sh4r.sgr);
                                sh4r.r[Rm] +=4;
                                }
                                break;
                            case 0x5:
                                { /* LDS.L @Rm+, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 1015 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                CHECKRALIGN32( sh4r.r[Rm] );
                                MEM_READ_LONG(sh4r.r[Rm], FPULi);
                                sh4r.r[Rm] +=4;
                                }
                                break;
                            case 0x6:
                                { /* LDS.L @Rm+, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 1035 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                CHECKRALIGN32( sh4r.r[Rm] );
                                MEM_READ_LONG(sh4r.r[Rm], tmp);
                                sh4r.r[Rm] +=4;
                                sh4_write_fpscr( tmp );
                                }
                                break;
                            case 0xF:
                                { /* LDC.L @Rm+, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 1053 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                CHECKRALIGN32( sh4r.r[Rm] );
                                MEM_READ_LONG(sh4r.r[Rm], sh4r.dbr);
                                sh4r.r[Rm] +=4;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x7:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* LDC.L @Rm+, SR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 880 "../../src/sh4/sh4core.in"
                                        CHECKSLOTILLEGAL();
                                        CHECKPRIV();
                                        CHECKWALIGN32( sh4r.r[Rm] );
                                        MEM_READ_LONG(sh4r.r[Rm], tmp);
                                        sh4_write_sr( tmp );
                                        sh4r.r[Rm] +=4;
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC.L @Rm+, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 925 "../../src/sh4/sh4core.in"
                                        CHECKRALIGN32( sh4r.r[Rm] );
                                        MEM_READ_LONG(sh4r.r[Rm], sh4r.gbr);
                                        sh4r.r[Rm] +=4;
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC.L @Rm+, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 952 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        CHECKRALIGN32( sh4r.r[Rm] );
                                        MEM_READ_LONG(sh4r.r[Rm], sh4r.vbr);
                                        sh4r.r[Rm] +=4;
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC.L @Rm+, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 979 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        CHECKRALIGN32( sh4r.r[Rm] );
                                        MEM_READ_LONG(sh4r.r[Rm], sh4r.ssr);
                                        sh4r.r[Rm] +=4;
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC.L @Rm+, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 995 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        CHECKRALIGN32( sh4r.r[Rm] );
                                        MEM_READ_LONG(sh4r.r[Rm], sh4r.spc);
                                        sh4r.r[Rm] +=4;
                                        }
                                        break;
                                    default:
                                        UNDEF(ir);
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* LDC.L @Rm+, Rn_BANK */
                                uint32_t Rm = ((ir>>8)&0xF); uint32_t Rn_BANK = ((ir>>4)&0x7); 
#line 1069 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                CHECKRALIGN32( sh4r.r[Rm] );
                                MEM_READ_LONG( sh4r.r[Rm], sh4r.r_bank[Rn_BANK] );
                                sh4r.r[Rm] += 4;
                                }
                                break;
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLL2 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 452 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] <<= 2;
                                }
                                break;
                            case 0x1:
                                { /* SHLL8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 454 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] <<= 8;
                                }
                                break;
                            case 0x2:
                                { /* SHLL16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 456 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] <<= 16;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0x9:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLR2 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 453 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] >>= 2;
                                }
                                break;
                            case 0x1:
                                { /* SHLR8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 455 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] >>= 8;
                                }
                                break;
                            case 0x2:
                                { /* SHLR16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 457 "../../src/sh4/sh4core.in"
                                sh4r.r[Rn] >>= 16;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xA:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* LDS Rm, MACH */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 888 "../../src/sh4/sh4core.in"
                                sh4r.mac = (sh4r.mac & 0x00000000FFFFFFFF) |
                                           (((uint64_t)sh4r.r[Rm])<<32);
                                }
                                break;
                            case 0x1:
                                { /* LDS Rm, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 930 "../../src/sh4/sh4core.in"
                                sh4r.mac = (sh4r.mac & 0xFFFFFFFF00000000LL) |
                                           (uint64_t)((uint32_t)(sh4r.r[Rm]));
                                }
                                break;
                            case 0x2:
                                { /* LDS Rm, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 957 "../../src/sh4/sh4core.in"
                                sh4r.pr = sh4r.r[Rm];
                                }
                                break;
                            case 0x3:
                                { /* LDC Rm, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 897 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                sh4r.sgr = sh4r.r[Rm];
                                }
                                break;
                            case 0x5:
                                { /* LDS Rm, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 1021 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                FPULi = sh4r.r[Rm];
                                }
                                break;
                            case 0x6:
                                { /* LDS Rm, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 1042 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                sh4_write_fpscr( sh4r.r[Rm] );
                                }
                                break;
                            case 0xF:
                                { /* LDC Rm, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 1059 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                sh4r.dbr = sh4r.r[Rm];
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xB:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* JSR @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 851 "../../src/sh4/sh4core.in"
                                CHECKDEST( sh4r.r[Rn] );
                                CHECKSLOTILLEGAL();
                                sh4r.in_delay_slot = 1;
                                sh4r.pc = sh4r.new_pc;
                                sh4r.new_pc = sh4r.r[Rn];
                                sh4r.pr = pc + 4;
                                TRACE_CALL( pc, sh4r.new_pc );
                                return TRUE;
                                }
                                break;
                            case 0x1:
                                { /* TAS.B @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 394 "../../src/sh4/sh4core.in"
                                MEM_READ_BYTE_FOR_WRITE( sh4r.r[Rn], tmp );
                                sh4r.t = ( tmp == 0 ? 1 : 0 );
                                MEM_WRITE_BYTE( sh4r.r[Rn], tmp | 0x80 );
                                }
                                break;
                            case 0x2:
                                { /* JMP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 843 "../../src/sh4/sh4core.in"
                                CHECKDEST( sh4r.r[Rn] );
                                CHECKSLOTILLEGAL();
                                sh4r.in_delay_slot = 1;
                                sh4r.pc = sh4r.new_pc;
                                sh4r.new_pc = sh4r.r[Rn];
                                return TRUE;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xC:
                        { /* SHAD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 429 "../../src/sh4/sh4core.in"
                        tmp = sh4r.r[Rm];
                        if( (tmp & 0x80000000) == 0 ) sh4r.r[Rn] <<= (tmp&0x1f);
                        else if( (tmp & 0x1F) == 0 )  
                            sh4r.r[Rn] = ((int32_t)sh4r.r[Rn]) >> 31;
                        else 
                    	sh4r.r[Rn] = ((int32_t)sh4r.r[Rn]) >> (((~sh4r.r[Rm]) & 0x1F)+1);
                        }
                        break;
                    case 0xD:
                        { /* SHLD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 437 "../../src/sh4/sh4core.in"
                        tmp = sh4r.r[Rm];
                        if( (tmp & 0x80000000) == 0 ) sh4r.r[Rn] <<= (tmp&0x1f);
                        else if( (tmp & 0x1F) == 0 ) sh4r.r[Rn] = 0;
                        else sh4r.r[Rn] >>= (((~tmp) & 0x1F)+1);
                        }
                        break;
                    case 0xE:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* LDC Rm, SR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 892 "../../src/sh4/sh4core.in"
                                        CHECKSLOTILLEGAL();
                                        CHECKPRIV();
                                        sh4_write_sr( sh4r.r[Rm] );
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC Rm, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 933 "../../src/sh4/sh4core.in"
                                        sh4r.gbr = sh4r.r[Rm];
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC Rm, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 959 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        sh4r.vbr = sh4r.r[Rm];
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC Rm, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 985 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        sh4r.ssr = sh4r.r[Rm];
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC Rm, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 1001 "../../src/sh4/sh4core.in"
                                        CHECKPRIV();
                                        sh4r.spc = sh4r.r[Rm];
                                        }
                                        break;
                                    default:
                                        UNDEF(ir);
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* LDC Rm, Rn_BANK */
                                uint32_t Rm = ((ir>>8)&0xF); uint32_t Rn_BANK = ((ir>>4)&0x7); 
#line 1075 "../../src/sh4/sh4core.in"
                                CHECKPRIV();
                                sh4r.r_bank[Rn_BANK] = sh4r.r[Rm];
                                }
                                break;
                        }
                        break;
                    case 0xF:
                        { /* MAC.W @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 647 "../../src/sh4/sh4core.in"
                        int32_t stmp;
                        if( Rm == Rn ) {
                    	CHECKRALIGN16(sh4r.r[Rn]);
                    	MEM_READ_WORD( sh4r.r[Rn], tmp );
                    	stmp = SIGNEXT16(tmp);
                    	MEM_READ_WORD( sh4r.r[Rn]+2, tmp );
                    	stmp *= SIGNEXT16(tmp);
                    	sh4r.r[Rn] += 4;
                        } else {
                    	CHECKRALIGN16( sh4r.r[Rn] );
                    	MEM_READ_WORD(sh4r.r[Rn], tmp);
                    	stmp = SIGNEXT16(tmp);
                    	CHECKRALIGN16( sh4r.r[Rm] );
                    	MEM_READ_WORD(sh4r.r[Rm], tmp);
                    	stmp = stmp * SIGNEXT16(tmp);
                    	sh4r.r[Rn] += 2;
                    	sh4r.r[Rm] += 2;
                        }
                        if( sh4r.s ) {
                    	int64_t tmpl = (int64_t)((int32_t)sh4r.mac) + (int64_t)stmp;
                    	if( tmpl > (int64_t)0x000000007FFFFFFFLL ) {
                    	    sh4r.mac = 0x000000017FFFFFFFLL;
                    	} else if( tmpl < (int64_t)0xFFFFFFFF80000000LL ) {
                    	    sh4r.mac = 0x0000000180000000LL;
                    	} else {
                    	    sh4r.mac = (sh4r.mac & 0xFFFFFFFF00000000LL) |
                    		((uint32_t)(sh4r.mac + stmp));
                    	}
                        } else {
                    	sh4r.mac += SIGNEXT32(stmp);
                        }
                        }
                        break;
                }
                break;
            case 0x5:
                { /* MOV.L @(disp, Rm), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<2; 
#line 514 "../../src/sh4/sh4core.in"
                tmp = sh4r.r[Rm] + disp;
                CHECKRALIGN32( tmp );
                MEM_READ_LONG( tmp, sh4r.r[Rn] );
                }
                break;
            case 0x6:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 518 "../../src/sh4/sh4core.in"
                        MEM_READ_BYTE( sh4r.r[Rm], sh4r.r[Rn] );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 519 "../../src/sh4/sh4core.in"
                        CHECKRALIGN16( sh4r.r[Rm] ); MEM_READ_WORD( sh4r.r[Rm], sh4r.r[Rn] );
                        }
                        break;
                    case 0x2:
                        { /* MOV.L @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 520 "../../src/sh4/sh4core.in"
                        CHECKRALIGN32( sh4r.r[Rm] ); MEM_READ_LONG( sh4r.r[Rm], sh4r.r[Rn] );
                        }
                        break;
                    case 0x3:
                        { /* MOV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 521 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = sh4r.r[Rm];
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 522 "../../src/sh4/sh4core.in"
                        MEM_READ_BYTE( sh4r.r[Rm], sh4r.r[Rn] ); if( Rm != Rn ) { sh4r.r[Rm] ++; }
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 523 "../../src/sh4/sh4core.in"
                        CHECKRALIGN16( sh4r.r[Rm] ); MEM_READ_WORD( sh4r.r[Rm], sh4r.r[Rn] ); if( Rm != Rn ) { sh4r.r[Rm] += 2; }
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 524 "../../src/sh4/sh4core.in"
                        CHECKRALIGN32( sh4r.r[Rm] ); MEM_READ_LONG( sh4r.r[Rm], sh4r.r[Rn] ); if( Rm != Rn ) { sh4r.r[Rm] += 4; }
                        }
                        break;
                    case 0x7:
                        { /* NOT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 389 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = ~sh4r.r[Rm];
                        }
                        break;
                    case 0x8:
                        { /* SWAP.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 463 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = (sh4r.r[Rm]&0xFFFF0000) | ((sh4r.r[Rm]&0x0000FF00)>>8) | ((sh4r.r[Rm]&0x000000FF)<<8);
                        }
                        break;
                    case 0x9:
                        { /* SWAP.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 464 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = (sh4r.r[Rm]>>16) | (sh4r.r[Rm]<<16);
                        }
                        break;
                    case 0xA:
                        { /* NEGC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 718 "../../src/sh4/sh4core.in"
                        tmp = 0 - sh4r.r[Rm];
                        sh4r.r[Rn] = tmp - sh4r.t;
                        sh4r.t = ( 0<tmp || tmp<sh4r.r[Rn] ? 1 : 0 );
                        }
                        break;
                    case 0xB:
                        { /* NEG Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 722 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = 0 - sh4r.r[Rm];
                        }
                        break;
                    case 0xC:
                        { /* EXTU.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 459 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = sh4r.r[Rm]&0x000000FF;
                        }
                        break;
                    case 0xD:
                        { /* EXTU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 460 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = sh4r.r[Rm]&0x0000FFFF;
                        }
                        break;
                    case 0xE:
                        { /* EXTS.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 461 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = SIGNEXT8( sh4r.r[Rm]&0x000000FF );
                        }
                        break;
                    case 0xF:
                        { /* EXTS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 462 "../../src/sh4/sh4core.in"
                        sh4r.r[Rn] = SIGNEXT16( sh4r.r[Rm]&0x0000FFFF );
                        }
                        break;
                }
                break;
            case 0x7:
                { /* ADD #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
#line 604 "../../src/sh4/sh4core.in"
                sh4r.r[Rn] += imm;
                }
                break;
            case 0x8:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
#line 552 "../../src/sh4/sh4core.in"
                        MEM_WRITE_BYTE( sh4r.r[Rn] + disp, R0 );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
#line 554 "../../src/sh4/sh4core.in"
                        tmp = sh4r.r[Rn] + disp;
                        CHECKWALIGN16( tmp );
                        MEM_WRITE_WORD( tmp, R0 );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
#line 558 "../../src/sh4/sh4core.in"
                        MEM_READ_BYTE( sh4r.r[Rm] + disp, R0 );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
#line 560 "../../src/sh4/sh4core.in"
                        tmp = sh4r.r[Rm] + disp;
                        CHECKRALIGN16( tmp );
                        MEM_READ_WORD( tmp, R0 );
                        }
                        break;
                    case 0x8:
                        { /* CMP/EQ #imm, R0 */
                        int32_t imm = SIGNEXT8(ir&0xFF); 
#line 588 "../../src/sh4/sh4core.in"
                        sh4r.t = ( R0 == imm ? 1 : 0 );
                        }
                        break;
                    case 0x9:
                        { /* BT disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 753 "../../src/sh4/sh4core.in"
                        CHECKSLOTILLEGAL();
                        if( sh4r.t ) {
                            CHECKDEST( sh4r.pc + disp + 4 )
                            sh4r.pc += disp + 4;
                            sh4r.new_pc = sh4r.pc + 2;
                            return TRUE;
                        }
                        }
                        break;
                    case 0xB:
                        { /* BF disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 762 "../../src/sh4/sh4core.in"
                        CHECKSLOTILLEGAL();
                        if( !sh4r.t ) {
                            CHECKDEST( sh4r.pc + disp + 4 )
                            sh4r.pc += disp + 4;
                            sh4r.new_pc = sh4r.pc + 2;
                            return TRUE;
                        }
                        }
                        break;
                    case 0xD:
                        { /* BT/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 771 "../../src/sh4/sh4core.in"
                        CHECKSLOTILLEGAL();
                        if( sh4r.t ) {
                            CHECKDEST( sh4r.pc + disp + 4 )
                            sh4r.in_delay_slot = 1;
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = pc + disp + 4;
                            sh4r.in_delay_slot = 1;
                            return TRUE;
                        }
                        }
                        break;
                    case 0xF:
                        { /* BF/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 782 "../../src/sh4/sh4core.in"
                        CHECKSLOTILLEGAL();
                        if( !sh4r.t ) {
                            CHECKDEST( sh4r.pc + disp + 4 )
                            sh4r.in_delay_slot = 1;
                            sh4r.pc = sh4r.new_pc;
                            sh4r.new_pc = pc + disp + 4;
                            return TRUE;
                        }
                        }
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
            case 0x9:
                { /* MOV.W @(disp, PC), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t disp = (ir&0xFF)<<1; 
#line 565 "../../src/sh4/sh4core.in"
                CHECKSLOTILLEGAL();
                tmp = pc + 4 + disp;
                MEM_READ_WORD( tmp, sh4r.r[Rn] );
                }
                break;
            case 0xA:
                { /* BRA disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
#line 792 "../../src/sh4/sh4core.in"
                CHECKSLOTILLEGAL();
                CHECKDEST( sh4r.pc + disp + 4 );
                sh4r.in_delay_slot = 1;
                sh4r.pc = sh4r.new_pc;
                sh4r.new_pc = pc + 4 + disp;
                return TRUE;
                }
                break;
            case 0xB:
                { /* BSR disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
#line 800 "../../src/sh4/sh4core.in"
                CHECKDEST( sh4r.pc + disp + 4 );
                CHECKSLOTILLEGAL();
                sh4r.in_delay_slot = 1;
                sh4r.pr = pc + 4;
                sh4r.pc = sh4r.new_pc;
                sh4r.new_pc = pc + 4 + disp;
                TRACE_CALL( pc, sh4r.new_pc );
                return TRUE;
                }
                break;
            case 0xC:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF); 
#line 530 "../../src/sh4/sh4core.in"
                        MEM_WRITE_BYTE( sh4r.gbr + disp, R0 );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<1; 
#line 532 "../../src/sh4/sh4core.in"
                        tmp = sh4r.gbr + disp;
                        CHECKWALIGN16( tmp );
                        MEM_WRITE_WORD( tmp, R0 );
                        }
                        break;
                    case 0x2:
                        { /* MOV.L R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<2; 
#line 537 "../../src/sh4/sh4core.in"
                        tmp = sh4r.gbr + disp;
                        CHECKWALIGN32( tmp );
                        MEM_WRITE_LONG( tmp, R0 );
                        }
                        break;
                    case 0x3:
                        { /* TRAPA #imm */
                        uint32_t imm = (ir&0xFF); 
#line 810 "../../src/sh4/sh4core.in"
                        CHECKSLOTILLEGAL();
                        sh4r.pc += 2;
                        sh4_raise_trap( imm );
                        return TRUE;
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF); 
#line 541 "../../src/sh4/sh4core.in"
                        MEM_READ_BYTE( sh4r.gbr + disp, R0 );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<1; 
#line 543 "../../src/sh4/sh4core.in"
                        tmp = sh4r.gbr + disp;
                        CHECKRALIGN16( tmp );
                        MEM_READ_WORD( tmp, R0 );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
#line 548 "../../src/sh4/sh4core.in"
                        tmp = sh4r.gbr + disp;
                        CHECKRALIGN32( tmp );
                        MEM_READ_LONG( tmp, R0 );
                        }
                        break;
                    case 0x7:
                        { /* MOVA @(disp, PC), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
#line 570 "../../src/sh4/sh4core.in"
                        CHECKSLOTILLEGAL();
                        R0 = (pc&0xFFFFFFFC) + disp + 4;
                        }
                        break;
                    case 0x8:
                        { /* TST #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
#line 399 "../../src/sh4/sh4core.in"
                        sh4r.t = (R0 & imm ? 0 : 1);
                        }
                        break;
                    case 0x9:
                        { /* AND #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
#line 387 "../../src/sh4/sh4core.in"
                        R0 &= imm;
                        }
                        break;
                    case 0xA:
                        { /* XOR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
#line 402 "../../src/sh4/sh4core.in"
                        R0 ^= imm;
                        }
                        break;
                    case 0xB:
                        { /* OR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
#line 391 "../../src/sh4/sh4core.in"
                        R0 |= imm;
                        }
                        break;
                    case 0xC:
                        { /* TST.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
#line 400 "../../src/sh4/sh4core.in"
                        MEM_READ_BYTE(R0+sh4r.gbr, tmp); sh4r.t = ( tmp & imm ? 0 : 1 );
                        }
                        break;
                    case 0xD:
                        { /* AND.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
#line 388 "../../src/sh4/sh4core.in"
                        MEM_READ_BYTE_FOR_WRITE(R0+sh4r.gbr, tmp); MEM_WRITE_BYTE( R0 + sh4r.gbr, imm & tmp );
                        }
                        break;
                    case 0xE:
                        { /* XOR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
#line 403 "../../src/sh4/sh4core.in"
                        MEM_READ_BYTE_FOR_WRITE(R0+sh4r.gbr, tmp); MEM_WRITE_BYTE( R0 + sh4r.gbr, imm ^ tmp );
                        }
                        break;
                    case 0xF:
                        { /* OR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
#line 392 "../../src/sh4/sh4core.in"
                        MEM_READ_BYTE_FOR_WRITE(R0+sh4r.gbr, tmp); MEM_WRITE_BYTE( R0 + sh4r.gbr, imm | tmp );
                        }
                        break;
                }
                break;
            case 0xD:
                { /* MOV.L @(disp, PC), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t disp = (ir&0xFF)<<2; 
#line 526 "../../src/sh4/sh4core.in"
                CHECKSLOTILLEGAL();
                tmp = (pc&0xFFFFFFFC) + disp + 4;
                MEM_READ_LONG( tmp, sh4r.r[Rn] );
                }
                break;
            case 0xE:
                { /* MOV #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
#line 573 "../../src/sh4/sh4core.in"
                sh4r.r[Rn] = imm;
                }
                break;
            case 0xF:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* FADD FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 1103 "../../src/sh4/sh4core.in"
                        CHECKFPUEN();
                        if( IS_FPU_DOUBLEPREC() ) {
                    	DR(FRn) += DR(FRm);
                        } else {
                    	FR(FRn) += FR(FRm);
                        }
                        }
                        break;
                    case 0x1:
                        { /* FSUB FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 1111 "../../src/sh4/sh4core.in"
                        CHECKFPUEN();
                        if( IS_FPU_DOUBLEPREC() ) {
                    	DR(FRn) -= DR(FRm);
                        } else {
                    	FR(FRn) -= FR(FRm);
                        }
                        }
                        break;
                    case 0x2:
                        { /* FMUL FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 1120 "../../src/sh4/sh4core.in"
                        CHECKFPUEN();
                        if( IS_FPU_DOUBLEPREC() ) {
                    	DR(FRn) *= DR(FRm);
                        } else {
                    	FR(FRn) *= FR(FRm);
                        }
                        }
                        break;
                    case 0x3:
                        { /* FDIV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 1129 "../../src/sh4/sh4core.in"
                        CHECKFPUEN();
                        if( IS_FPU_DOUBLEPREC() ) {
                    	DR(FRn) /= DR(FRm);
                        } else {
                    	FR(FRn) /= FR(FRm);
                        }
                        }
                        break;
                    case 0x4:
                        { /* FCMP/EQ FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 1138 "../../src/sh4/sh4core.in"
                        CHECKFPUEN();
                        if( IS_FPU_DOUBLEPREC() ) {
                    	sh4r.t = ( DR(FRn) == DR(FRm) ? 1 : 0 );
                        } else {
                    	sh4r.t = ( FR(FRn) == FR(FRm) ? 1 : 0 );
                        }
                        }
                        break;
                    case 0x5:
                        { /* FCMP/GT FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 1147 "../../src/sh4/sh4core.in"
                        CHECKFPUEN();
                        if( IS_FPU_DOUBLEPREC() ) {
                    	sh4r.t = ( DR(FRn) > DR(FRm) ? 1 : 0 );
                        } else {
                    	sh4r.t = ( FR(FRn) > FR(FRm) ? 1 : 0 );
                        }
                        }
                        break;
                    case 0x6:
                        { /* FMOV @(R0, Rm), FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 575 "../../src/sh4/sh4core.in"
                        MEM_FP_READ( sh4r.r[Rm] + R0, FRn );
                        }
                        break;
                    case 0x7:
                        { /* FMOV FRm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 576 "../../src/sh4/sh4core.in"
                        MEM_FP_WRITE( sh4r.r[Rn] + R0, FRm );
                        }
                        break;
                    case 0x8:
                        { /* FMOV @Rm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 577 "../../src/sh4/sh4core.in"
                        MEM_FP_READ( sh4r.r[Rm], FRn );
                        }
                        break;
                    case 0x9:
                        { /* FMOV @Rm+, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 578 "../../src/sh4/sh4core.in"
                        MEM_FP_READ( sh4r.r[Rm], FRn ); sh4r.r[Rm] += FP_WIDTH;
                        }
                        break;
                    case 0xA:
                        { /* FMOV FRm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 579 "../../src/sh4/sh4core.in"
                        MEM_FP_WRITE( sh4r.r[Rn], FRm );
                        }
                        break;
                    case 0xB:
                        { /* FMOV FRm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 580 "../../src/sh4/sh4core.in"
                        MEM_FP_WRITE( sh4r.r[Rn] - FP_WIDTH, FRm ); sh4r.r[Rn] -= FP_WIDTH;
                        }
                        break;
                    case 0xC:
                        { /* FMOV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 582 "../../src/sh4/sh4core.in"
                        if( IS_FPU_DOUBLESIZE() )
                    	DR(FRn) = DR(FRm);
                        else
                    	FR(FRn) = FR(FRm);
                        }
                        break;
                    case 0xD:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* FSTS FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 1155 "../../src/sh4/sh4core.in"
                                CHECKFPUEN(); FR(FRn) = FPULf;
                                }
                                break;
                            case 0x1:
                                { /* FLDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
#line 1156 "../../src/sh4/sh4core.in"
                                CHECKFPUEN(); FPULf = FR(FRm);
                                }
                                break;
                            case 0x2:
                                { /* FLOAT FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 1158 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( IS_FPU_DOUBLEPREC() ) {
                            	if( FRn&1 ) { // No, really...
                            	    dtmp = (double)FPULi;
                            	    FR(FRn) = *(((float *)&dtmp)+1);
                            	} else {
                            	    DRF(FRn>>1) = (double)FPULi;
                            	}
                                } else {
                            	FR(FRn) = (float)FPULi;
                                }
                                }
                                break;
                            case 0x3:
                                { /* FTRC FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
#line 1171 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( IS_FPU_DOUBLEPREC() ) {
                            	if( FRm&1 ) {
                            	    dtmp = 0;
                            	    *(((float *)&dtmp)+1) = FR(FRm);
                            	} else {
                            	    dtmp = DRF(FRm>>1);
                            	}
                                    if( dtmp >= MAX_INTF )
                                        FPULi = MAX_INT;
                                    else if( dtmp <= MIN_INTF )
                                        FPULi = MIN_INT;
                                    else 
                                        FPULi = (int32_t)dtmp;
                                } else {
                            	ftmp = FR(FRm);
                            	if( ftmp >= MAX_INTF )
                            	    FPULi = MAX_INT;
                            	else if( ftmp <= MIN_INTF )
                            	    FPULi = MIN_INT;
                            	else
                            	    FPULi = (int32_t)ftmp;
                                }
                                }
                                break;
                            case 0x4:
                                { /* FNEG FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 1196 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( IS_FPU_DOUBLEPREC() ) {
                            	DR(FRn) = -DR(FRn);
                                } else {
                                    FR(FRn) = -FR(FRn);
                                }
                                }
                                break;
                            case 0x5:
                                { /* FABS FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 1204 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( IS_FPU_DOUBLEPREC() ) {
                            	DR(FRn) = fabs(DR(FRn));
                                } else {
                                    FR(FRn) = fabsf(FR(FRn));
                                }
                                }
                                break;
                            case 0x6:
                                { /* FSQRT FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 1212 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( IS_FPU_DOUBLEPREC() ) {
                            	DR(FRn) = sqrt(DR(FRn));
                                } else {
                                    FR(FRn) = sqrtf(FR(FRn));
                                }
                                }
                                break;
                            case 0x7:
                                { /* FSRRA FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 1263 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( !IS_FPU_DOUBLEPREC() ) {
                            	FR(FRn) = 1.0/sqrt(FR(FRn));
                                }
                                }
                                break;
                            case 0x8:
                                { /* FLDI0 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 1220 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( IS_FPU_DOUBLEPREC() ) {
                            	DR(FRn) = 0.0;
                                } else {
                                    FR(FRn) = 0.0;
                                }
                                }
                                break;
                            case 0x9:
                                { /* FLDI1 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 1228 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( IS_FPU_DOUBLEPREC() ) {
                            	DR(FRn) = 1.0;
                                } else {
                                    FR(FRn) = 1.0;
                                }
                                }
                                break;
                            case 0xA:
                                { /* FCNVSD FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 1250 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( IS_FPU_DOUBLEPREC() && !IS_FPU_DOUBLESIZE() ) {
                            	DR(FRn) = (double)FPULf;
                                }
                                }
                                break;
                            case 0xB:
                                { /* FCNVDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
#line 1256 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( IS_FPU_DOUBLEPREC() && !IS_FPU_DOUBLESIZE() ) {
                            	FPULf = (float)DR(FRm);
                                }
                                }
                                break;
                            case 0xE:
                                { /* FIPR FVm, FVn */
                                uint32_t FVn = ((ir>>10)&0x3); uint32_t FVm = ((ir>>8)&0x3); 
#line 1269 "../../src/sh4/sh4core.in"
                                CHECKFPUEN();
                                if( !IS_FPU_DOUBLEPREC() ) {
                                    int tmp2 = FVn<<2;
                                    tmp = FVm<<2;
                                    FR(tmp2+3) = FR(tmp)*FR(tmp2) +
                                        FR(tmp+1)*FR(tmp2+1) +
                                        FR(tmp+2)*FR(tmp2+2) +
                                        FR(tmp+3)*FR(tmp2+3);
                                }
                                }
                                break;
                            case 0xF:
                                switch( (ir&0x100) >> 8 ) {
                                    case 0x0:
                                        { /* FSCA FPUL, FRn */
                                        uint32_t FRn = ((ir>>9)&0x7)<<1; 
#line 1280 "../../src/sh4/sh4core.in"
                                        CHECKFPUEN();
                                        if( !IS_FPU_DOUBLEPREC() ) {
                                    	sh4_fsca( FPULi, (float *)&(DRF(FRn>>1)) );
                                        }
                                        }
                                        break;
                                    case 0x1:
                                        switch( (ir&0x200) >> 9 ) {
                                            case 0x0:
                                                { /* FTRV XMTRX, FVn */
                                                uint32_t FVn = ((ir>>10)&0x3); 
#line 1286 "../../src/sh4/sh4core.in"
                                                CHECKFPUEN();
                                                if( !IS_FPU_DOUBLEPREC() ) {
                                            	sh4_ftrv((float *)&(DRF(FVn<<1)) );
                                                }
                                                }
                                                break;
                                            case 0x1:
                                                switch( (ir&0xC00) >> 10 ) {
                                                    case 0x0:
                                                        { /* FSCHG */
#line 1248 "../../src/sh4/sh4core.in"
                                                        CHECKFPUEN(); sh4r.fpscr ^= FPSCR_SZ;
                                                        }
                                                        break;
                                                    case 0x2:
                                                        { /* FRCHG */
#line 1244 "../../src/sh4/sh4core.in"
                                                        CHECKFPUEN(); 
                                                        sh4r.fpscr ^= FPSCR_FR; 
                                                        sh4_switch_fr_banks();
                                                        }
                                                        break;
                                                    case 0x3:
                                                        { /* UNDEF */
#line 1292 "../../src/sh4/sh4core.in"
                                                        UNDEF(ir);
                                                        }
                                                        break;
                                                    default:
                                                        UNDEF(ir);
                                                        break;
                                                }
                                                break;
                                        }
                                        break;
                                }
                                break;
                            default:
                                UNDEF(ir);
                                break;
                        }
                        break;
                    case 0xE:
                        { /* FMAC FR0, FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 1236 "../../src/sh4/sh4core.in"
                        CHECKFPUEN();
                        if( IS_FPU_DOUBLEPREC() ) {
                            DR(FRn) += DR(FRm)*DR(0);
                        } else {
                    	FR(FRn) += (double)FR(FRm)*(double)FR(0);
                        }
                        }
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
        }
#pragma clang diagnostic pop
#line 1294 "../../src/sh4/sh4core.in"

    sh4r.pc = sh4r.new_pc;
    sh4r.new_pc += 2;

    sh4r.in_delay_slot = 0;
    return TRUE;
}

#line 1 "../../src/sh4/sh4stat.in"
/**
 * $Id$
 * 
 * Support module for collecting instruction stats
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

#include "dream.h"
#include "sh4/sh4stat.h"
#include "sh4/sh4core.h"
#include "sh4/mmu.h"

static uint64_t sh4_stats[SH4_INSTRUCTION_COUNT+1];
static uint64_t sh4_stats_total;
static const char *sh4_stats_names[] = {
    "???",
"ADD Rm, Rn",
"ADD #imm, Rn",
"ADDC Rm, Rn",
"ADDV Rm, Rn",
"AND Rm, Rn",
"AND #imm, R0",
"AND.B #imm, @(R0, GBR)",
"BF disp",
"BF/S disp",
"BRA disp",
"BRAF Rn",
"BSR disp",
"BSRF Rn",
"BT disp",
"BT/S disp",
"CLRMAC",
"CLRS",
"CLRT",
"CMP/EQ Rm, Rn",
"CMP/EQ #imm, R0",
"CMP/GE Rm, Rn",
"CMP/GT Rm, Rn",
"CMP/HI Rm, Rn",
"CMP/HS Rm, Rn",
"CMP/PL Rn",
"CMP/PZ Rn",
"CMP/STR Rm, Rn",
"DIV0S Rm, Rn",
"DIV0U",
"DIV1 Rm, Rn",
"DMULS.L Rm, Rn",
"DMULU.L Rm, Rn",
"DT Rn",
"EXTS.B Rm, Rn",
"EXTS.W Rm, Rn",
"EXTU.B Rm, Rn",
"EXTU.W Rm, Rn",
"FABS FRn",
"FADD FRm, FRn",
"FCMP/EQ FRm, FRn",
"FCMP/GT FRm, FRn",
"FCNVDS FRm, FPUL",
"FCNVSD FPUL, FRn",
"FDIV FRm, FRn",
"FIPR FVm, FVn",
"FLDS FRm, FPUL",
"FLDI0 FRn",
"FLDI1 FRn",
"FLOAT FPUL, FRn",
"FMAC FR0, FRm, FRn",
"FMOV FRm, FRn",
"FMOV FRm, @Rn",
"FMOV FRm, @-Rn",
"FMOV FRm, @(R0, Rn)",
"FMOV @Rm, FRn",
"FMOV @Rm+, FRn",
"FMOV @(R0, Rm), FRn",
"FMUL FRm, FRn",
"FNEG FRn",
"FRCHG",
"FSCA FPUL, FRn",
"FSCHG",
"FSQRT FRn",
"FSRRA FRn",
"FSTS FPUL, FRn",
"FSUB FRm, FRn",
"FTRC FRm, FPUL",
"FTRV XMTRX, FVn",
"JMP @Rn",
"JSR @Rn",
"LDC Rm, SR",
"LDC Rm, *",
"LDC.L @Rm+, SR",
"LDC.L @Rm+, *",
"LDS Rm, FPSCR",
"LDS Rm, *",
"LDS.L @Rm+, FPSCR",
"LDS.L @Rm+, *",
"LDTLB",
"MAC.L @Rm+, @Rn+",
"MAC.W @Rm+, @Rn+",
"MOV Rm, Rn",
"MOV #imm, Rn",
"MOV.B ...",
"MOV.L ...",
"MOV.L @(disp, PC)",
"MOV.W ...",
"MOVA @(disp, PC), R0",
"MOVCA.L R0, @Rn",
"MOVT Rn",
"MUL.L Rm, Rn",
"MULS.W Rm, Rn",
"MULU.W Rm, Rn",
"NEG Rm, Rn",
"NEGC Rm, Rn",
"NOP",
"NOT Rm, Rn",
"OCBI @Rn",
"OCBP @Rn",
"OCBWB @Rn",
"OR Rm, Rn",
"OR #imm, R0",
"OR.B #imm, @(R0, GBR)",
"PREF @Rn",
"ROTCL Rn",
"ROTCR Rn",
"ROTL Rn",
"ROTR Rn",
"RTE",
"RTS",
"SETS",
"SETT",
"SHAD Rm, Rn",
"SHAL Rn",
"SHAR Rn",
"SHLD Rm, Rn",
"SHLL* Rn",
"SHLR* Rn",
"SLEEP",
"STC SR, Rn",
"STC *, Rn",
"STC.L SR, @-Rn",
"STC.L *, @-Rn",
"STS FPSCR, Rn",
"STS *, Rn",
"STS.L FPSCR, @-Rn",
"STS.L *, @-Rn",
"SUB Rm, Rn",
"SUBC Rm, Rn",
"SUBV Rm, Rn",
"SWAP.B Rm, Rn",
"SWAP.W Rm, Rn",
"TAS.B @Rn",
"TRAPA #imm",
"TST Rm, Rn",
"TST #imm, R0",
"TST.B #imm, @(R0, GBR)",
"XOR Rm, Rn",
"XOR #imm, R0",
"XOR.B #imm, @(R0, GBR)",
"XTRCT Rm, Rn",
"UNDEF"
};

void sh4_stats_reset( void )
{
    int i;
    for( i=0; i<= I_UNDEF; i++ ) {
	sh4_stats[i] = 0;
    }
    sh4_stats_total = 0;
}

void sh4_stats_print( FILE *out )
{
    int i;
    for( i=0; i<= I_UNDEF; i++ ) {
	fprintf( out, "%-20s\t%d\t%.2f%%\n", sh4_stats_names[i], (uint32_t)sh4_stats[i], ((double)sh4_stats[i])*100.0/(double)sh4_stats_total );
    }
    fprintf( out, "Total: %lld\n", (long long int)sh4_stats_total );
}

void FASTCALL sh4_stats_add( sh4_inst_id item )
{
    sh4_stats[item]++;
    sh4_stats_total++;
}

void sh4_stats_add_by_pc( uint32_t pc ) 
{
    sh4addr_t addr = mmu_vma_to_phys_disasm(pc);
    uint16_t ir = ext_address_space[addr>>12]->read_word(addr);
#define UNDEF(ir) sh4_stats[0]++
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
#line 373 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STCSR]++;
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC GBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 374 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STC]++;
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC VBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 375 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STC]++;
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC SSR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 376 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STC]++;
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC SPC, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 377 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STC]++;
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
#line 380 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STC]++;
                                }
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* BSRF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 214 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_BSRF]++;
                                }
                                break;
                            case 0x2:
                                { /* BRAF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 212 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_BRAF]++;
                                }
                                break;
                            case 0x8:
                                { /* PREF @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 351 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_PREF]++;
                                }
                                break;
                            case 0x9:
                                { /* OCBI @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 345 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_OCBI]++;
                                }
                                break;
                            case 0xA:
                                { /* OCBP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 346 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_OCBP]++;
                                }
                                break;
                            case 0xB:
                                { /* OCBWB @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 347 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_OCBWB]++;
                                }
                                break;
                            case 0xC:
                                { /* MOVCA.L R0, @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 336 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_MOVCA]++;
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
#line 305 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 326 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 315 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVL]++;
                        }
                        break;
                    case 0x7:
                        { /* MUL.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 338 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MULL]++;
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                { /* CLRT */
#line 219 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_CLRT]++;
                                }
                                break;
                            case 0x1:
                                { /* SETT */
#line 359 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SETT]++;
                                }
                                break;
                            case 0x2:
                                { /* CLRMAC */
#line 217 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_CLRMAC]++;
                                }
                                break;
                            case 0x3:
                                { /* LDTLB */
#line 298 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDTLB]++;
                                }
                                break;
                            case 0x4:
                                { /* CLRS */
#line 218 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_CLRS]++;
                                }
                                break;
                            case 0x5:
                                { /* SETS */
#line 358 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SETS]++;
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
#line 343 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_NOP]++;
                                }
                                break;
                            case 0x1:
                                { /* DIV0U */
#line 230 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_DIV0U]++;
                                }
                                break;
                            case 0x2:
                                { /* MOVT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 337 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_MOVT]++;
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
#line 393 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STS]++;
                                }
                                break;
                            case 0x1:
                                { /* STS MACL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 395 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STS]++;
                                }
                                break;
                            case 0x2:
                                { /* STS PR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 397 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STS]++;
                                }
                                break;
                            case 0x3:
                                { /* STC SGR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 378 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STC]++;
                                }
                                break;
                            case 0x5:
                                { /* STS FPUL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 391 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STS]++;
                                }
                                break;
                            case 0x6:
                                { /* STS FPSCR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 389 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STSFPSCR]++;
                                }
                                break;
                            case 0xF:
                                { /* STC DBR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 379 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STC]++;
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
#line 357 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_RTS]++;
                                }
                                break;
                            case 0x1:
                                { /* SLEEP */
#line 372 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SLEEP]++;
                                }
                                break;
                            case 0x2:
                                { /* RTE */
#line 356 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_RTE]++;
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
#line 310 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0xD:
                        { /* MOV.W @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 331 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0xE:
                        { /* MOV.L @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 320 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVL]++;
                        }
                        break;
                    case 0xF:
                        { /* MAC.L @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 299 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MACL]++;
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
#line 317 "../../src/sh4/sh4stat.in"
                sh4_stats[I_MOVL]++;
                }
                break;
            case 0x2:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 303 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0x1:
                        { /* MOV.W Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 324 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0x2:
                        { /* MOV.L Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 313 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVL]++;
                        }
                        break;
                    case 0x4:
                        { /* MOV.B Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 304 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 325 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 314 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVL]++;
                        }
                        break;
                    case 0x7:
                        { /* DIV0S Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 229 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_DIV0S]++;
                        }
                        break;
                    case 0x8:
                        { /* TST Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 406 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_TST]++;
                        }
                        break;
                    case 0x9:
                        { /* AND Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 206 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_AND]++;
                        }
                        break;
                    case 0xA:
                        { /* XOR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 409 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_XOR]++;
                        }
                        break;
                    case 0xB:
                        { /* OR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 348 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_OR]++;
                        }
                        break;
                    case 0xC:
                        { /* CMP/STR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 228 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_CMPSTR]++;
                        }
                        break;
                    case 0xD:
                        { /* XTRCT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 412 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_XTRCT]++;
                        }
                        break;
                    case 0xE:
                        { /* MULU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 340 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MULUW]++;
                        }
                        break;
                    case 0xF:
                        { /* MULS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 339 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MULSW]++;
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
#line 220 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_CMPEQ]++;
                        }
                        break;
                    case 0x2:
                        { /* CMP/HS Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 225 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_CMPHS]++;
                        }
                        break;
                    case 0x3:
                        { /* CMP/GE Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 222 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_CMPGE]++;
                        }
                        break;
                    case 0x4:
                        { /* DIV1 Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 231 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_DIV1]++;
                        }
                        break;
                    case 0x5:
                        { /* DMULU.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 233 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_DMULU]++;
                        }
                        break;
                    case 0x6:
                        { /* CMP/HI Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 224 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_CMPHI]++;
                        }
                        break;
                    case 0x7:
                        { /* CMP/GT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 223 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_CMPGT]++;
                        }
                        break;
                    case 0x8:
                        { /* SUB Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 399 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_SUB]++;
                        }
                        break;
                    case 0xA:
                        { /* SUBC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 400 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_SUBC]++;
                        }
                        break;
                    case 0xB:
                        { /* SUBV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 401 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_SUBV]++;
                        }
                        break;
                    case 0xC:
                        { /* ADD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 202 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_ADD]++;
                        }
                        break;
                    case 0xD:
                        { /* DMULS.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 232 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_DMULS]++;
                        }
                        break;
                    case 0xE:
                        { /* ADDC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 204 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_ADDC]++;
                        }
                        break;
                    case 0xF:
                        { /* ADDV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 205 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_ADDV]++;
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
#line 364 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHLL]++;
                                }
                                break;
                            case 0x1:
                                { /* DT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 234 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_DT]++;
                                }
                                break;
                            case 0x2:
                                { /* SHAL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 361 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHAL]++;
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
#line 368 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHLR]++;
                                }
                                break;
                            case 0x1:
                                { /* CMP/PZ Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 227 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_CMPPZ]++;
                                }
                                break;
                            case 0x2:
                                { /* SHAR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 362 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHAR]++;
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
#line 394 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STSM]++;
                                }
                                break;
                            case 0x1:
                                { /* STS.L MACL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 396 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STSM]++;
                                }
                                break;
                            case 0x2:
                                { /* STS.L PR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 398 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STSM]++;
                                }
                                break;
                            case 0x3:
                                { /* STC.L SGR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 385 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STCM]++;
                                }
                                break;
                            case 0x5:
                                { /* STS.L FPUL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 392 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STSM]++;
                                }
                                break;
                            case 0x6:
                                { /* STS.L FPSCR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 390 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STSFPSCRM]++;
                                }
                                break;
                            case 0xF:
                                { /* STC.L DBR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 386 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STCM]++;
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
#line 381 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STCSRM]++;
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC.L GBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 388 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STCM]++;
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC.L VBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 382 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STCM]++;
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC.L SSR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 383 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STCM]++;
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC.L SPC, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
#line 384 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_STCM]++;
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
#line 387 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_STCM]++;
                                }
                                break;
                        }
                        break;
                    case 0x4:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* ROTL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 354 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_ROTL]++;
                                }
                                break;
                            case 0x2:
                                { /* ROTCL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 352 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_ROTCL]++;
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
#line 355 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_ROTR]++;
                                }
                                break;
                            case 0x1:
                                { /* CMP/PL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 226 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_CMPPL]++;
                                }
                                break;
                            case 0x2:
                                { /* ROTCR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 353 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_ROTCR]++;
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
#line 293 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDSM]++;
                                }
                                break;
                            case 0x1:
                                { /* LDS.L @Rm+, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 295 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDSM]++;
                                }
                                break;
                            case 0x2:
                                { /* LDS.L @Rm+, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 297 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDSM]++;
                                }
                                break;
                            case 0x3:
                                { /* LDC.L @Rm+, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 284 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDCM]++;
                                }
                                break;
                            case 0x5:
                                { /* LDS.L @Rm+, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 291 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDSM]++;
                                }
                                break;
                            case 0x6:
                                { /* LDS.L @Rm+, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 289 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDSFPSCRM]++;
                                }
                                break;
                            case 0xF:
                                { /* LDC.L @Rm+, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 286 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDCM]++;
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
#line 281 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDCSRM]++;
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC.L @Rm+, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 280 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDCM]++;
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC.L @Rm+, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 282 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDCM]++;
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC.L @Rm+, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 283 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDCM]++;
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC.L @Rm+, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 285 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDCM]++;
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
#line 287 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDCM]++;
                                }
                                break;
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLL2 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 365 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHLL]++;
                                }
                                break;
                            case 0x1:
                                { /* SHLL8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 366 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHLL]++;
                                }
                                break;
                            case 0x2:
                                { /* SHLL16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 367 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHLL]++;
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
#line 369 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHLR]++;
                                }
                                break;
                            case 0x1:
                                { /* SHLR8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 370 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHLR]++;
                                }
                                break;
                            case 0x2:
                                { /* SHLR16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 371 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_SHLR]++;
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
#line 292 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDS]++;
                                }
                                break;
                            case 0x1:
                                { /* LDS Rm, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 294 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDS]++;
                                }
                                break;
                            case 0x2:
                                { /* LDS Rm, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 296 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDS]++;
                                }
                                break;
                            case 0x3:
                                { /* LDC Rm, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 276 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDC]++;
                                }
                                break;
                            case 0x5:
                                { /* LDS Rm, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 290 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDS]++;
                                }
                                break;
                            case 0x6:
                                { /* LDS Rm, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 288 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDSFPSCR]++;
                                }
                                break;
                            case 0xF:
                                { /* LDC Rm, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
#line 278 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDC]++;
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
#line 271 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_JSR]++;
                                }
                                break;
                            case 0x1:
                                { /* TAS.B @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 404 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_TASB]++;
                                }
                                break;
                            case 0x2:
                                { /* JMP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
#line 270 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_JMP]++;
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
#line 360 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_SHAD]++;
                        }
                        break;
                    case 0xD:
                        { /* SHLD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 363 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_SHLD]++;
                        }
                        break;
                    case 0xE:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* LDC Rm, SR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 273 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDCSR]++;
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC Rm, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 272 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDC]++;
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC Rm, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 274 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDC]++;
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC Rm, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 275 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDC]++;
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC Rm, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
#line 277 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_LDC]++;
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
#line 279 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_LDC]++;
                                }
                                break;
                        }
                        break;
                    case 0xF:
                        { /* MAC.W @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 300 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MACW]++;
                        }
                        break;
                }
                break;
            case 0x5:
                { /* MOV.L @(disp, Rm), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<2; 
#line 323 "../../src/sh4/sh4stat.in"
                sh4_stats[I_MOVL]++;
                }
                break;
            case 0x6:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 308 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0x1:
                        { /* MOV.W @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 329 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0x2:
                        { /* MOV.L @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 318 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVL]++;
                        }
                        break;
                    case 0x3:
                        { /* MOV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 301 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOV]++;
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 309 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 330 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 319 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVL]++;
                        }
                        break;
                    case 0x7:
                        { /* NOT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 344 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_NOT]++;
                        }
                        break;
                    case 0x8:
                        { /* SWAP.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 402 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_SWAPB]++;
                        }
                        break;
                    case 0x9:
                        { /* SWAP.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 403 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_SWAPW]++;
                        }
                        break;
                    case 0xA:
                        { /* NEGC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 342 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_NEGC]++;
                        }
                        break;
                    case 0xB:
                        { /* NEG Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 341 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_NEG]++;
                        }
                        break;
                    case 0xC:
                        { /* EXTU.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 237 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_EXTUB]++;
                        }
                        break;
                    case 0xD:
                        { /* EXTU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 238 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_EXTUW]++;
                        }
                        break;
                    case 0xE:
                        { /* EXTS.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 235 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_EXTSB]++;
                        }
                        break;
                    case 0xF:
                        { /* EXTS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 236 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_EXTSW]++;
                        }
                        break;
                }
                break;
            case 0x7:
                { /* ADD #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
#line 203 "../../src/sh4/sh4stat.in"
                sh4_stats[I_ADDI]++;
                }
                break;
            case 0x8:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
#line 307 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
#line 328 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
#line 312 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
#line 334 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0x8:
                        { /* CMP/EQ #imm, R0 */
                        int32_t imm = SIGNEXT8(ir&0xFF); 
#line 221 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_CMPEQI]++;
                        }
                        break;
                    case 0x9:
                        { /* BT disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 215 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_BT]++;
                        }
                        break;
                    case 0xB:
                        { /* BF disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 209 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_BF]++;
                        }
                        break;
                    case 0xD:
                        { /* BT/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 216 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_BTS]++;
                        }
                        break;
                    case 0xF:
                        { /* BF/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
#line 210 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_BFS]++;
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
#line 333 "../../src/sh4/sh4stat.in"
                sh4_stats[I_MOVW]++;
                }
                break;
            case 0xA:
                { /* BRA disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
#line 211 "../../src/sh4/sh4stat.in"
                sh4_stats[I_BRA]++;
                }
                break;
            case 0xB:
                { /* BSR disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
#line 213 "../../src/sh4/sh4stat.in"
                sh4_stats[I_BSR]++;
                }
                break;
            case 0xC:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF); 
#line 306 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<1; 
#line 327 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0x2:
                        { /* MOV.L R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<2; 
#line 316 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVL]++;
                        }
                        break;
                    case 0x3:
                        { /* TRAPA #imm */
                        uint32_t imm = (ir&0xFF); 
#line 405 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_TRAPA]++;
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF); 
#line 311 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVB]++;
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<1; 
#line 332 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVW]++;
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
#line 321 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVL]++;
                        }
                        break;
                    case 0x7:
                        { /* MOVA @(disp, PC), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
#line 335 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_MOVA]++;
                        }
                        break;
                    case 0x8:
                        { /* TST #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
#line 407 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_TSTI]++;
                        }
                        break;
                    case 0x9:
                        { /* AND #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
#line 207 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_ANDI]++;
                        }
                        break;
                    case 0xA:
                        { /* XOR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
#line 410 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_XORI]++;
                        }
                        break;
                    case 0xB:
                        { /* OR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
#line 349 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_ORI]++;
                        }
                        break;
                    case 0xC:
                        { /* TST.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
#line 408 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_TSTB]++;
                        }
                        break;
                    case 0xD:
                        { /* AND.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
#line 208 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_ANDB]++;
                        }
                        break;
                    case 0xE:
                        { /* XOR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
#line 411 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_XORB]++;
                        }
                        break;
                    case 0xF:
                        { /* OR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
#line 350 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_ORB]++;
                        }
                        break;
                }
                break;
            case 0xD:
                { /* MOV.L @(disp, PC), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t disp = (ir&0xFF)<<2; 
#line 322 "../../src/sh4/sh4stat.in"
                sh4_stats[I_MOVLPC]++;
                }
                break;
            case 0xE:
                { /* MOV #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
#line 302 "../../src/sh4/sh4stat.in"
                sh4_stats[I_MOVI]++;
                }
                break;
            case 0xF:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* FADD FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 240 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FADD]++;
                        }
                        break;
                    case 0x1:
                        { /* FSUB FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 267 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FSUB]++;
                        }
                        break;
                    case 0x2:
                        { /* FMUL FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 259 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FMUL]++;
                        }
                        break;
                    case 0x3:
                        { /* FDIV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 245 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FDIV]++;
                        }
                        break;
                    case 0x4:
                        { /* FCMP/EQ FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 241 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FCMPEQ]++;
                        }
                        break;
                    case 0x5:
                        { /* FCMP/GT FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 242 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FCMPGT]++;
                        }
                        break;
                    case 0x6:
                        { /* FMOV @(R0, Rm), FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 258 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FMOV7]++;
                        }
                        break;
                    case 0x7:
                        { /* FMOV FRm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 255 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FMOV4]++;
                        }
                        break;
                    case 0x8:
                        { /* FMOV @Rm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 256 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FMOV5]++;
                        }
                        break;
                    case 0x9:
                        { /* FMOV @Rm+, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
#line 257 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FMOV6]++;
                        }
                        break;
                    case 0xA:
                        { /* FMOV FRm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 253 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FMOV2]++;
                        }
                        break;
                    case 0xB:
                        { /* FMOV FRm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 254 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FMOV3]++;
                        }
                        break;
                    case 0xC:
                        { /* FMOV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
#line 252 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FMOV1]++;
                        }
                        break;
                    case 0xD:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* FSTS FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 266 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FSTS]++;
                                }
                                break;
                            case 0x1:
                                { /* FLDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
#line 247 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FLDS]++;
                                }
                                break;
                            case 0x2:
                                { /* FLOAT FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 250 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FLOAT]++;
                                }
                                break;
                            case 0x3:
                                { /* FTRC FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
#line 268 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FTRC]++;
                                }
                                break;
                            case 0x4:
                                { /* FNEG FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 260 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FNEG]++;
                                }
                                break;
                            case 0x5:
                                { /* FABS FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 239 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FABS]++;
                                }
                                break;
                            case 0x6:
                                { /* FSQRT FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 264 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FSQRT]++;
                                }
                                break;
                            case 0x7:
                                { /* FSRRA FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 265 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FSRRA]++;
                                }
                                break;
                            case 0x8:
                                { /* FLDI0 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 248 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FLDI0]++;
                                }
                                break;
                            case 0x9:
                                { /* FLDI1 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 249 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FLDI1]++;
                                }
                                break;
                            case 0xA:
                                { /* FCNVSD FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
#line 244 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FCNVSD]++;
                                }
                                break;
                            case 0xB:
                                { /* FCNVDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
#line 243 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FCNVDS]++;
                                }
                                break;
                            case 0xE:
                                { /* FIPR FVm, FVn */
                                uint32_t FVn = ((ir>>10)&0x3); uint32_t FVm = ((ir>>8)&0x3); 
#line 246 "../../src/sh4/sh4stat.in"
                                sh4_stats[I_FIPR]++;
                                }
                                break;
                            case 0xF:
                                switch( (ir&0x100) >> 8 ) {
                                    case 0x0:
                                        { /* FSCA FPUL, FRn */
                                        uint32_t FRn = ((ir>>9)&0x7)<<1; 
#line 262 "../../src/sh4/sh4stat.in"
                                        sh4_stats[I_FSCA]++;
                                        }
                                        break;
                                    case 0x1:
                                        switch( (ir&0x200) >> 9 ) {
                                            case 0x0:
                                                { /* FTRV XMTRX, FVn */
                                                uint32_t FVn = ((ir>>10)&0x3); 
#line 269 "../../src/sh4/sh4stat.in"
                                                sh4_stats[I_FTRV]++;
                                                }
                                                break;
                                            case 0x1:
                                                switch( (ir&0xC00) >> 10 ) {
                                                    case 0x0:
                                                        { /* FSCHG */
#line 263 "../../src/sh4/sh4stat.in"
                                                        sh4_stats[I_FSCHG]++;
                                                        }
                                                        break;
                                                    case 0x2:
                                                        { /* FRCHG */
#line 261 "../../src/sh4/sh4stat.in"
                                                        sh4_stats[I_FRCHG]++;
                                                        }
                                                        break;
                                                    case 0x3:
                                                        { /* UNDEF */
#line 413 "../../src/sh4/sh4stat.in"
                                                        sh4_stats[I_UNDEF]++;
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
#line 251 "../../src/sh4/sh4stat.in"
                        sh4_stats[I_FMAC]++;
                        }
                        break;
                    default:
                        UNDEF(ir);
                        break;
                }
                break;
        }
#pragma clang diagnostic pop
#line 414 "../../src/sh4/sh4stat.in"


sh4_stats_total++;
}

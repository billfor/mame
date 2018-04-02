// license:BSD-3-Clause
// copyright-holders:Steve Ellenoff,R. Belmont,Ryan Holtz
/*****************************************************************************
 *
 *   arm7core.h
 *   Portable ARM7TDMI Core Emulator
 *
 *   Copyright Steve Ellenoff, all rights reserved.
 *
 *  This work is based on:
 *  #1) 'Atmel Corporation ARM7TDMI (Thumb) Datasheet - January 1999'
 *  #2) Arm 2/3/6 emulator By Bryan McPhail (bmcphail@tendril.co.uk) and Phil Stroffolino (MAME CORE 0.76)
 *
 *****************************************************************************

 This file contains everything related to the arm7 core itself, and is presumed
 to be cpu implementation non-specific, ie, applies to only the core.

 ******************************************************************************/

#pragma once

#ifndef __ARM7CORE_H__
#define __ARM7CORE_H__

#define ARM7_DEBUG_CORE 0


/****************************************************************************************************
 *  INTERRUPT LINES/EXCEPTIONS
 ***************************************************************************************************/
enum
{
	ARM7_IRQ_LINE=0, ARM7_FIRQ_LINE,
	ARM7_ABORT_EXCEPTION, ARM7_ABORT_PREFETCH_EXCEPTION, ARM7_UNDEFINE_EXCEPTION,
	ARM7_NUM_LINES
};
// Really there's only 1 ABORT Line.. and cpu decides whether it's during data fetch or prefetch, but we let the user specify

/****************************************************************************************************
 *  ARM7 CORE REGISTERS
 ***************************************************************************************************/

enum
{
	ARM7_PC = 0,
	ARM7_R0, ARM7_R1, ARM7_R2, ARM7_R3, ARM7_R4, ARM7_R5, ARM7_R6, ARM7_R7,
	ARM7_R8, ARM7_R9, ARM7_R10, ARM7_R11, ARM7_R12, ARM7_R13, ARM7_R14, ARM7_R15, ARM7_SPSR,
	ARM7_USRR8, ARM7_USRR9, ARM7_USRR10, ARM7_USRR11, ARM7_USRR12, ARM7_USRR13, ARM7_USRR14, ARM7_USRSPSR,
	ARM7_FR8, ARM7_FR9, ARM7_FR10, ARM7_FR11, ARM7_FR12, ARM7_FR13, ARM7_FR14, ARM7_FSPSR,
	ARM7_IR13, ARM7_IR14, ARM7_ISPSR,
	ARM7_SR13, ARM7_SR14, ARM7_SSPSR,
	ARM7_AR13, ARM7_AR14, ARM7_ASPSR,
	ARM7_UR13, ARM7_UR14, ARM7_USPSR,
	ARM7_CPSR
};

/* Coprocessor-related macros */
#define COPRO_TLB_BASE                      m_tlbBase
#define COPRO_TLB_BASE_MASK                 0xffffc000
#define COPRO_TLB_VADDR_FLTI_MASK           0xfff00000
#define COPRO_TLB_VADDR_FLTI_MASK_SHIFT     20
#define COPRO_TLB_VADDR_CSLTI_MASK          0x000ff000
#define COPRO_TLB_VADDR_CSLTI_MASK_SHIFT    10
#define COPRO_TLB_VADDR_FSLTI_MASK          0x000ffc00
#define COPRO_TLB_VADDR_FSLTI_MASK_SHIFT    8
#define COPRO_TLB_CFLD_ADDR_MASK            0xfffffc00
#define COPRO_TLB_CFLD_ADDR_MASK_SHIFT      10
#define COPRO_TLB_FPTB_ADDR_MASK            0xfffff000
#define COPRO_TLB_FPTB_ADDR_MASK_SHIFT      12
#define COPRO_TLB_SECTION_PAGE_MASK         0xfff00000
#define COPRO_TLB_LARGE_PAGE_MASK           0xffff0000
#define COPRO_TLB_SMALL_PAGE_MASK           0xfffff000
#define COPRO_TLB_TINY_PAGE_MASK            0xfffffc00
#define COPRO_TLB_UNMAPPED                  0
#define COPRO_TLB_LARGE_PAGE                1
#define COPRO_TLB_SMALL_PAGE                2
#define COPRO_TLB_TINY_PAGE                 3
#define COPRO_TLB_COARSE_TABLE              1
#define COPRO_TLB_SECTION_TABLE             2
#define COPRO_TLB_FINE_TABLE                3

#define COPRO_CTRL                          m_control
#define COPRO_CTRL_MMU_EN                   0x00000001
#define COPRO_CTRL_ADDRFAULT_EN             0x00000002
#define COPRO_CTRL_DCACHE_EN                0x00000004
#define COPRO_CTRL_WRITEBUF_EN              0x00000008
#define COPRO_CTRL_ENDIAN                   0x00000080
#define COPRO_CTRL_SYSTEM                   0x00000100
#define COPRO_CTRL_ROM                      0x00000200
#define COPRO_CTRL_ICACHE_EN                0x00001000
#define COPRO_CTRL_INTVEC_ADJUST            0x00002000
#define COPRO_CTRL_ADDRFAULT_EN_SHIFT       1
#define COPRO_CTRL_DCACHE_EN_SHIFT          2
#define COPRO_CTRL_WRITEBUF_EN_SHIFT        3
#define COPRO_CTRL_ENDIAN_SHIFT             7
#define COPRO_CTRL_SYSTEM_SHIFT             8
#define COPRO_CTRL_ROM_SHIFT                9
#define COPRO_CTRL_ICACHE_EN_SHIFT          12
#define COPRO_CTRL_INTVEC_ADJUST_SHIFT      13
#define COPRO_CTRL_LITTLE_ENDIAN            0
#define COPRO_CTRL_BIG_ENDIAN               1
#define COPRO_CTRL_INTVEC_0                 0
#define COPRO_CTRL_INTVEC_F                 1
#define COPRO_CTRL_MASK                     0x0000338f

#define COPRO_DOMAIN_ACCESS_CONTROL         m_domainAccessControl

#define COPRO_FAULT_STATUS_D                m_faultStatus[0]
#define COPRO_FAULT_STATUS_P                m_faultStatus[1]

#define COPRO_FAULT_ADDRESS                 m_faultAddress

#define COPRO_FCSE_PID                      m_fcsePID

/****************************************************************************************************
 *  VARIOUS INTERNAL STRUCS/DEFINES/ETC..
 ***************************************************************************************************/
// Mode values come from bit 4-0 of CPSR, but we are ignoring bit 4 here, since bit 4 always = 1 for valid modes
enum
{
	eARM7_MODE_USER = 0x0,      // Bit: 4-0 = 10000
	eARM7_MODE_FIQ  = 0x1,      // Bit: 4-0 = 10001
	eARM7_MODE_IRQ  = 0x2,      // Bit: 4-0 = 10010
	eARM7_MODE_SVC  = 0x3,      // Bit: 4-0 = 10011
	eARM7_MODE_ABT  = 0x7,      // Bit: 4-0 = 10111
	eARM7_MODE_UND  = 0xb,      // Bit: 4-0 = 11011
	eARM7_MODE_SYS  = 0xf       // Bit: 4-0 = 11111
};

static const int thumbCycles[256] =
{
//  0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 1
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 2
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 3
	1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // 4
	2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // 5
	2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,  // 6
	2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,  // 7
	2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,  // 8
	2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,  // 9
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // a
	1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 4, 1, 1,  // b
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // c
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3,  // d
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // e
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2   // f
};

#define N_BIT   31
#define Z_BIT   30
#define C_BIT   29
#define V_BIT   28
#define Q_BIT   27
#define I_BIT   7
#define F_BIT   6
#define T_BIT   5   // Thumb mode

#define N_MASK  ((uint32_t)(1 << N_BIT)) /* Negative flag */
#define Z_MASK  ((uint32_t)(1 << Z_BIT)) /* Zero flag */
#define C_MASK  ((uint32_t)(1 << C_BIT)) /* Carry flag */
#define V_MASK  ((uint32_t)(1 << V_BIT)) /* oVerflow flag */
#define Q_MASK  ((uint32_t)(1 << Q_BIT)) /* signed overflow for QADD, MAC */
#define I_MASK  ((uint32_t)(1 << I_BIT)) /* Interrupt request disable */
#define F_MASK  ((uint32_t)(1 << F_BIT)) /* Fast interrupt request disable */
#define T_MASK  ((uint32_t)(1 << T_BIT)) /* Thumb Mode flag */

#define N_IS_SET(pc)    ((pc) & N_MASK)
#define Z_IS_SET(pc)    ((pc) & Z_MASK)
#define C_IS_SET(pc)    ((pc) & C_MASK)
#define V_IS_SET(pc)    ((pc) & V_MASK)
#define Q_IS_SET(pc)    ((pc) & Q_MASK)
#define I_IS_SET(pc)    ((pc) & I_MASK)
#define F_IS_SET(pc)    ((pc) & F_MASK)
#define T_IS_SET(pc)    ((pc) & T_MASK)

#define N_IS_CLEAR(pc)  (!N_IS_SET(pc))
#define Z_IS_CLEAR(pc)  (!Z_IS_SET(pc))
#define C_IS_CLEAR(pc)  (!C_IS_SET(pc))
#define V_IS_CLEAR(pc)  (!V_IS_SET(pc))
#define Q_IS_CLEAR(pc)  (!Q_IS_SET(pc))
#define I_IS_CLEAR(pc)  (!I_IS_SET(pc))
#define F_IS_CLEAR(pc)  (!F_IS_SET(pc))
#define T_IS_CLEAR(pc)  (!T_IS_SET(pc))

/* Deconstructing an instruction */
// todo: use these in all places (including dasm file)
#define INSN_COND           ((uint32_t)0xf0000000u)
#define INSN_SDT_L          ((uint32_t)0x00100000u)
#define INSN_BDT_L          ((uint32_t)0x00100000u)
#define INSN_BDT_REGS       ((uint32_t)0x0000ffffu)
#define INSN_SDT_IMM        ((uint32_t)0x00000fffu)
#define INSN_MUL_A          ((uint32_t)0x00200000u)
#define INSN_MUL_RM         ((uint32_t)0x0000000fu)
#define INSN_MUL_RS         ((uint32_t)0x00000f00u)
#define INSN_MUL_RN         ((uint32_t)0x0000f000u)
#define INSN_MUL_RD         ((uint32_t)0x000f0000u)
#define INSN_I              ((uint32_t)0x02000000u)
#define INSN_OPCODE         ((uint32_t)0x01e00000u)
#define INSN_S              ((uint32_t)0x00100000u)
#define INSN_BL             ((uint32_t)0x01000000u)
#define INSN_BRANCH         ((uint32_t)0x00ffffffu)
#define INSN_SWI            ((uint32_t)0x00ffffffu)
#define INSN_RN             ((uint32_t)0x000f0000u)
#define INSN_RD             ((uint32_t)0x0000f000u)
#define INSN_OP2            ((uint32_t)0x00000fffu)
#define INSN_OP2_SHIFT      ((uint32_t)0x00000f80u)
#define INSN_OP2_SHIFT_TYPE ((uint32_t)0x00000070u)
#define INSN_OP2_RM         ((uint32_t)0x0000000fu)
#define INSN_OP2_ROTATE     ((uint32_t)0x00000f00u)
#define INSN_OP2_IMM        ((uint32_t)0x000000ffu)
#define INSN_OP2_SHIFT_TYPE_SHIFT   4
#define INSN_OP2_SHIFT_SHIFT        7
#define INSN_OP2_ROTATE_SHIFT       7
#define INSN_MUL_RS_SHIFT           8
#define INSN_MUL_RN_SHIFT           12
#define INSN_MUL_RD_SHIFT           16
#define INSN_OPCODE_SHIFT           21
#define INSN_RN_SHIFT               16
#define INSN_RD_SHIFT               12
#define INSN_COND_SHIFT             28

#define INSN_COPRO_N        ((uint32_t) 0x00100000u)
#define INSN_COPRO_CREG     ((uint32_t) 0x000f0000u)
#define INSN_COPRO_AREG     ((uint32_t) 0x0000f000u)
#define INSN_COPRO_CPNUM    ((uint32_t) 0x00000f00u)
#define INSN_COPRO_OP2      ((uint32_t) 0x000000e0u)
#define INSN_COPRO_OP3      ((uint32_t) 0x0000000fu)
#define INSN_COPRO_N_SHIFT          20
#define INSN_COPRO_CREG_SHIFT       16
#define INSN_COPRO_AREG_SHIFT       12
#define INSN_COPRO_CPNUM_SHIFT      8
#define INSN_COPRO_OP2_SHIFT        5

#define THUMB_INSN_TYPE     ((uint16_t)0xf000)
#define THUMB_COND_TYPE     ((uint16_t)0x0f00)
#define THUMB_GROUP4_TYPE   ((uint16_t)0x0c00)
#define THUMB_GROUP5_TYPE   ((uint16_t)0x0e00)
#define THUMB_GROUP5_RM     ((uint16_t)0x01c0)
#define THUMB_GROUP5_RN     ((uint16_t)0x0038)
#define THUMB_GROUP5_RD     ((uint16_t)0x0007)
#define THUMB_ADDSUB_RNIMM  ((uint16_t)0x01c0)
#define THUMB_ADDSUB_RS     ((uint16_t)0x0038)
#define THUMB_ADDSUB_RD     ((uint16_t)0x0007)
#define THUMB_INSN_CMP      ((uint16_t)0x0800)
#define THUMB_INSN_SUB      ((uint16_t)0x0800)
#define THUMB_INSN_IMM_RD   ((uint16_t)0x0700)
#define THUMB_INSN_IMM_S    ((uint16_t)0x0080)
#define THUMB_INSN_IMM      ((uint16_t)0x00ff)
#define THUMB_INSN_ADDSUB   ((uint16_t)0x0800)
#define THUMB_ADDSUB_TYPE   ((uint16_t)0x0600)
#define THUMB_HIREG_OP      ((uint16_t)0x0300)
#define THUMB_HIREG_H       ((uint16_t)0x00c0)
#define THUMB_HIREG_RS      ((uint16_t)0x0038)
#define THUMB_HIREG_RD      ((uint16_t)0x0007)
#define THUMB_STACKOP_TYPE  ((uint16_t)0x0f00)
#define THUMB_STACKOP_L     ((uint16_t)0x0800)
#define THUMB_STACKOP_RD    ((uint16_t)0x0700)
#define THUMB_ALUOP_TYPE    ((uint16_t)0x03c0)
#define THUMB_BLOP_LO       ((uint16_t)0x0800)
#define THUMB_BLOP_OFFS     ((uint16_t)0x07ff)
#define THUMB_SHIFT_R       ((uint16_t)0x0800)
#define THUMB_SHIFT_AMT     ((uint16_t)0x07c0)
#define THUMB_HALFOP_L      ((uint16_t)0x0800)
#define THUMB_HALFOP_OFFS   ((uint16_t)0x07c0)
#define THUMB_BRANCH_OFFS   ((uint16_t)0x07ff)
#define THUMB_LSOP_L        ((uint16_t)0x0800)
#define THUMB_LSOP_OFFS     ((uint16_t)0x07c0)
#define THUMB_MULTLS        ((uint16_t)0x0800)
#define THUMB_MULTLS_BASE   ((uint16_t)0x0700)
#define THUMB_RELADDR_SP    ((uint16_t)0x0800)
#define THUMB_RELADDR_RD    ((uint16_t)0x0700)
#define THUMB_INSN_TYPE_SHIFT       12
#define THUMB_COND_TYPE_SHIFT       8
#define THUMB_GROUP4_TYPE_SHIFT     10
#define THUMB_GROUP5_TYPE_SHIFT     9
#define THUMB_ADDSUB_TYPE_SHIFT     9
#define THUMB_INSN_IMM_RD_SHIFT     8
#define THUMB_STACKOP_TYPE_SHIFT    8
#define THUMB_HIREG_OP_SHIFT        8
#define THUMB_STACKOP_RD_SHIFT      8
#define THUMB_MULTLS_BASE_SHIFT     8
#define THUMB_RELADDR_RD_SHIFT      8
#define THUMB_HIREG_H_SHIFT         6
#define THUMB_HIREG_RS_SHIFT        3
#define THUMB_ALUOP_TYPE_SHIFT      6
#define THUMB_SHIFT_AMT_SHIFT       6
#define THUMB_HALFOP_OFFS_SHIFT     6
#define THUMB_LSOP_OFFS_SHIFT       6
#define THUMB_GROUP5_RM_SHIFT       6
#define THUMB_GROUP5_RN_SHIFT       3
#define THUMB_GROUP5_RD_SHIFT       0
#define THUMB_ADDSUB_RNIMM_SHIFT    6
#define THUMB_ADDSUB_RS_SHIFT       3
#define THUMB_ADDSUB_RD_SHIFT       0

enum
{
	COND_EQ = 0,          /*  Z           equal                   */
	COND_NE,              /* ~Z           not equal               */
	COND_CS, COND_HS = 2, /*  C           unsigned higher or same */
	COND_CC, COND_LO = 3, /* ~C           unsigned lower          */
	COND_MI,              /*  N           negative                */
	COND_PL,              /* ~N           positive or zero        */
	COND_VS,              /*  V           overflow                */
	COND_VC,              /* ~V           no overflow             */
	COND_HI,              /*  C && ~Z     unsigned higher         */
	COND_LS,              /* ~C ||  Z     unsigned lower or same  */
	COND_GE,              /*  N == V      greater or equal        */
	COND_LT,              /*  N != V      less than               */
	COND_GT,              /* ~Z && N == V greater than            */
	COND_LE,              /*  Z || N != V less than or equal      */
	COND_AL,              /*  1           always                  */
	COND_NV               /*  0           never                   */
};

/* Convenience Macros */
#define R15                     m_r[eR15]
#define SPSR                    16                     // SPSR is always the 17th register in our 0 based array s_register_table[][17]
#define GET_CPSR                m_cpsr
#define MODE_FLAG               0xF                    // Mode bits are 4:0 of CPSR, but we ignore bit 4.
#define GET_MODE                m_mode
#define SIGN_BIT                (1 << 31)
#define SIGN_BITS_DIFFER(a, b)  (((a) ^ (b)) >> 31)
/* I really don't know why these were set to 16-bit, the thumb registers are still 32-bit ... */
#define THUMB_SIGN_BIT               ((uint32_t)(1 << 31))
#define THUMB_SIGN_BITS_DIFFER(a, b) (((a)^(b)) >> 31)

#define SR_MODE32               0x10

#define MODE32                  (GET_CPSR & SR_MODE32)
#define MODE26                  (!(GET_CPSR & SR_MODE32))
#define GET_PC                  (MODE32 ? R15 : R15 & 0x03FFFFFC)

#define ARM7_TLB_ABORT_D (1 << 0)
#define ARM7_TLB_ABORT_P (1 << 1)
#define ARM7_TLB_READ    (1 << 2)
#define ARM7_TLB_WRITE   (1 << 5)

/* ARM flavors */
enum arm_flavor
{
	/* ARM7 variants */
	ARM_TYPE_ARM7,
	ARM_TYPE_ARM7BE,
	ARM_TYPE_ARM7500,
	ARM_TYPE_PXA255,
	ARM_TYPE_SA1110,

	/* ARM9 variants */
	ARM_TYPE_ARM9,
	ARM_TYPE_ARM920T,
	ARM_TYPE_ARM946ES
};

#endif /* __ARM7CORE_H__ */

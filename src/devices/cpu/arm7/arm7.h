// license:BSD-3-Clause
// copyright-holders:Steve Ellenoff,R. Belmont,Ryan Holtz
/*****************************************************************************
 *
 *   arm7.h
 *   Portable ARM7TDMI CPU Emulator
 *
 *   Copyright Steve Ellenoff, all rights reserved.
 *
 *  This work is based on:
 *  #1) 'Atmel Corporation ARM7TDMI (Thumb) Datasheet - January 1999'
 *  #2) Arm 2/3/6 emulator By Bryan McPhail (bmcphail@tendril.co.uk) and Phil Stroffolino (MAME CORE 0.76)
 *
 *****************************************************************************

 This file contains everything related to the arm7 cpu specific implementation.
 Anything related to the arm7 core itself is defined in arm7core.h instead.

 ******************************************************************************/

#ifndef MAME_CPU_ARM7_ARM7_H
#define MAME_CPU_ARM7_ARM7_H

#pragma once

#include "arm7dasm.h"

#include "cpu/drcfe.h"
#include "cpu/drcuml.h"
#include "cpu/drcumlsh.h"


#define ARM7_MAX_FASTRAM       4
#define ARM7_MAX_HOTSPOTS      16

#define MCFG_ARM_HIGH_VECTORS() \
	downcast<arm7_cpu_device &>(*device).set_high_vectors();

#define MCFG_ARM_PREFETCH_ENABLE() \
	downcast<arm7_cpu_device &>(*device).set_prefetch_enabled();

/***************************************************************************
    COMPILER-SPECIFIC OPTIONS
***************************************************************************/

/* compilation boundaries -- how far back/forward does the analysis extend? */
#define COMPILE_BACKWARDS_BYTES         128
#define COMPILE_FORWARDS_BYTES          512
#define COMPILE_MAX_INSTRUCTIONS        ((COMPILE_BACKWARDS_BYTES/4) + (COMPILE_FORWARDS_BYTES/4))
#define COMPILE_MAX_SEQUENCE            64

#define SINGLE_INSTRUCTION_MODE         (0)

#define ARM7DRC_STRICT_VERIFY      0x0001          /* verify all instructions */
#define ARM7DRC_FLUSH_PC           0x0002          /* flush the PC value before each memory access */

#define ARM7DRC_COMPATIBLE_OPTIONS (ARM7DRC_STRICT_VERIFY | ARM7DRC_FLUSH_PC)
#define ARM7DRC_FASTEST_OPTIONS    (0)

/****************************************************************************************************
 *  CONSTANTS
 ***************************************************************************************************/

enum
{
	TLB_COARSE = 0,
	TLB_FINE
};

enum
{
	FAULT_NONE = 0,
	FAULT_DOMAIN,
	FAULT_PERMISSION
};

/* There are 36 Unique - 32 bit processor registers */
/* Each mode has 17 registers (except user & system, which have 16) */
/* This is a list of each *unique* register */
enum
{
	/* All modes have the following */
	eR0 = 0, eR1, eR2, eR3, eR4, eR5, eR6, eR7,
	eR8, eR9, eR10, eR11, eR12,
	eR13, /* Stack Pointer */
	eR14, /* Link Register (holds return address) */
	eR15, /* Program Counter */
	eSPSR,

	/* User - All possible bank switched registers */
	eR8_USR, eR9_USR, eR10_USR, eR11_USR, eR12_USR, eR13_USR, eR14_USR, eSPSR_USR,

	/* Fast Interrupt - Bank switched registers */
	eR8_FIQ, eR9_FIQ, eR10_FIQ, eR11_FIQ, eR12_FIQ, eR13_FIQ, eR14_FIQ, eSPSR_FIQ,

	/* IRQ - Bank switched registers */
	eR13_IRQ, eR14_IRQ, eSPSR_IRQ,

	/* Supervisor/Service Mode - Bank switched registers */
	eR13_SVC, eR14_SVC, eSPSR_SVC,

	/* Abort Mode - Bank switched registers */
	eR13_ABT, eR14_ABT, eSPSR_ABT,

	/* Undefined Mode - Bank switched registers */
	eR13_UND, eR14_UND, eSPSR_UND,

	NUM_REGS
};

#define ARM7_NUM_MODES 0x10

/****************************************************************************************************
 *  PUBLIC FUNCTIONS
 ***************************************************************************************************/

class arm7_frontend;

class arm7_cpu_device : public cpu_device, public arm7_disassembler::config
{
	friend class arm7_frontend;
	friend class arm9_frontend;

public:
	// construction/destruction
	arm7_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void set_high_vectors() { m_vectorbase = 0xffff0000; }
	void set_prefetch_enabled() { m_prefetch_enabled = true; }

protected:
	enum
	{
		ARCHFLAG_T    = 1,        // Thumb present
		ARCHFLAG_E    = 2,        // extended DSP operations present (only for v5+)
		ARCHFLAG_J    = 4,        // "Jazelle" (direct execution of Java bytecode)
		ARCHFLAG_MMU  = 8,        // has on-board MMU (traditional ARM style like the SA1110)
		ARCHFLAG_SA   = 16,       // StrongARM extensions (enhanced TLB)
		ARCHFLAG_XSCALE   = 32,   // XScale extensions (CP14, enhanced TLB)
		ARCHFLAG_MODE26   = 64    // supports 26-bit backwards compatibility mode
	};

	enum
	{
		ARM9_COPRO_ID_STEP_SA1110_A0 = 0,
		ARM9_COPRO_ID_STEP_SA1110_B0 = 4,
		ARM9_COPRO_ID_STEP_SA1110_B1 = 5,
		ARM9_COPRO_ID_STEP_SA1110_B2 = 6,
		ARM9_COPRO_ID_STEP_SA1110_B4 = 8,

		ARM9_COPRO_ID_STEP_PXA255_A0 = 6,

		ARM9_COPRO_ID_STEP_ARM946_A0 = 1,

		ARM9_COPRO_ID_PART_SA1110 = 0xB11 << 4,
		ARM9_COPRO_ID_PART_ARM946 = 0x946 << 4,
		ARM9_COPRO_ID_PART_ARM920 = 0x920 << 4,
		ARM9_COPRO_ID_PART_ARM710 = 0x710 << 4,
		ARM9_COPRO_ID_PART_GENERICARM7 = 0x700 << 4,

		ARM9_COPRO_ID_PXA255_CORE_REV_SHIFT = 10,
		ARM9_COPRO_ID_PXA255_CORE_GEN_XSCALE = 0x01 << 13,

		ARM9_COPRO_ID_ARCH_V4     = 0x01 << 16,
		ARM9_COPRO_ID_ARCH_V4T    = 0x02 << 16,
		ARM9_COPRO_ID_ARCH_V5     = 0x03 << 16,
		ARM9_COPRO_ID_ARCH_V5T    = 0x04 << 16,
		ARM9_COPRO_ID_ARCH_V5TE   = 0x05 << 16,

		ARM9_COPRO_ID_SPEC_REV0   = 0x00 << 20,
		ARM9_COPRO_ID_SPEC_REV1   = 0x01 << 20,

		ARM9_COPRO_ID_MFR_ARM = 0x41 << 24,
		ARM9_COPRO_ID_MFR_DEC = 0x44 << 24,
		ARM9_COPRO_ID_MFR_INTEL = 0x69 << 24
	};

	enum insn_mode
	{
		ARM_MODE = 0,
		THUMB_MODE = 1
	};

	enum copro_mode
	{
		MMU_OFF = 0,
		MMU_ON = 1
	};

	enum imm_mode
	{
		REG_OP2 = 0,
		IMM_OP2 = 1
	};

	enum prefetch_mode
	{
		PREFETCH_OFF = 0,
		PREFETCH_ON = 1
	};

	enum index_mode
	{
		POST_INDEXED = 0,
		PRE_INDEXED = 1
	};

	enum offset_mode
	{
		OFFSET_DOWN = 0,
		OFFSET_UP = 1
	};

	enum flags_mode
	{
		NO_FLAGS = 0,
		SET_FLAGS = 1
	};

	enum bdt_s_bit
	{
		NO_S_BIT = 0,
		S_BIT = 1
	};

	enum alu_bit
	{
		PSR_OP = 0,
		ALU_OP = 1
	};

	enum size_mode
	{
		SIZE_DWORD = 0,
		SIZE_BYTE = 1
	};

	enum writeback_mode
	{
		NO_WRITEBACK = 0,
		WRITEBACK = 1
	};

	enum check_mode
	{
		NO_FETCH = 0,
		FETCH = 1
	};

	enum tlb_rw_mode
	{
		TLB_READ = 0,
		TLB_WRITE = 1
	};

	enum link_mode
	{
		BRANCH = 0,
		BRANCH_LINK = 1
	};

	enum pid_mode
	{
		IGNORE_PID = 0,
		VALID_PID = 1
	};

	enum debug_mode
	{
		NO_HOOK = 0,
		CHECK_HOOK = 1
	};

	enum lmul_mode
	{
		MUL_WORD = 0,
		MUL_LONG = 1
	};

	enum smul_mode
	{
		MUL_UNSIGNED = 0,
		MUL_SIGNED = 1
	};

	enum accum_mode
	{
		MUL_ONLY = 0,
		MUL_ACCUM = 1
	};

	enum load_mode
	{
		IS_STORE = 0,
		IS_LOAD = 1
	};

	enum alu_mode
	{
		OPCODE_AND, /* 0000 */
		OPCODE_EOR, /* 0001 */
		OPCODE_SUB, /* 0010 */
		OPCODE_RSB, /* 0011 */
		OPCODE_ADD, /* 0100 */
		OPCODE_ADC, /* 0101 */
		OPCODE_SBC, /* 0110 */
		OPCODE_RSC, /* 0111 */
		OPCODE_TST, /* 1000 */
		OPCODE_TEQ, /* 1001 */
		OPCODE_CMP, /* 1010 */
		OPCODE_CMN, /* 1011 */
		OPCODE_ORR, /* 1100 */
		OPCODE_MOV, /* 1101 */
		OPCODE_BIC, /* 1110 */
		OPCODE_MVN  /* 1111 */
	};

	enum stldm_mode
	{
		DEFAULT_MODE = 0,
		USER_MODE = 1
	};

	arm7_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, uint8_t archRev, uint8_t archFlags, endianness_t endianness);

	void postload();

	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;

	// device_execute_interface overrides
	virtual uint32_t execute_min_cycles() const override { return 3; }
	virtual uint32_t execute_max_cycles() const override { return 4; }
	virtual uint32_t execute_input_lines() const override { return 4; } /* There are actually only 2 input lines: we use 3 variants of the ABORT line while there is only 1 real one */
	virtual void execute_run() override;
	virtual void execute_set_input(int inputnum, int state) override;

	template <insn_mode THUMB, copro_mode MMU_ENABLED, prefetch_mode PREFETCH, pid_mode CHECK_PID, debug_mode DEBUG> void execute_core();
	template <copro_mode MMU> inline ATTR_FORCE_INLINE void execute_arm7_insn(const uint32_t insn);
	template <copro_mode MMU> inline ATTR_FORCE_INLINE void execute_thumb_insn(const uint32_t op, uint32_t pc);
	void execute_arm9_insn(const uint32_t insn);

	// device_memory_interface overrides
	virtual space_config_vector memory_space_config() const override;
	virtual bool memory_translate(int spacenum, int intention, offs_t &address) override;

	// device_state_interface overrides
	virtual void state_export(const device_state_entry &entry) override;
	virtual void state_string_export(const device_state_entry &entry, std::string &str) const override;

	// device_disasm_interface overrides
	virtual util::disasm_interface *create_disassembler() override;
	virtual bool get_t_flag() const override;

	address_space_config m_program_config;

	struct internal_arm_state
	{
		uint32_t m_r[NUM_REGS];
		uint32_t m_cpsr;
		uint32_t m_nflag;
		uint32_t m_zflag;
		uint32_t m_cflag;
		uint32_t m_vflag;
		uint32_t m_tflag;

		uint32_t m_insn_prefetch_depth;
		uint32_t m_insn_prefetch_count;
		uint32_t m_insn_prefetch_index;
		uint32_t m_insn_prefetch_buffer[3];
		uint32_t m_insn_prefetch_address[3];
		uint32_t m_insn_prefetch_translated[3];
		uint32_t m_prefetch_word0_shift;
		uint32_t m_prefetch_word1_shift;

		uint32_t m_pendingIrq;
		uint32_t m_pendingFiq;
		uint32_t m_pendingAbtD;
		uint32_t m_pendingAbtP;
		uint32_t m_pendingUnd;
		uint32_t m_pendingSwi;
		uint32_t m_pending_interrupt;
		int m_icount;

		/* Coprocessor Registers */
		uint32_t m_control;
		uint32_t m_tlbBase;
		uint32_t m_tlb_base_mask;
		uint32_t m_faultStatus[2];
		uint32_t m_faultAddress;
		uint32_t m_fcsePID;
		uint32_t m_pid_offset;
		uint32_t m_domainAccessControl;
		uint32_t m_decoded_access_control[16];
		uint32_t m_mode;

		const int* m_reg_group;
	};

	uint32_t m_r[NUM_REGS];
	uint32_t m_cpsr;
	uint32_t m_nflag;
	uint32_t m_zflag;
	uint32_t m_cflag;
	uint32_t m_vflag;
	uint32_t m_tflag;
	uint32_t **m_rp;

	uint32_t m_insn_prefetch_depth;
	uint32_t m_insn_prefetch_count;
	uint32_t m_insn_prefetch_index;
	uint32_t m_insn_prefetch_buffer[3];
	uint32_t m_insn_prefetch_address[3];
	uint32_t m_insn_prefetch_translated[3];
	uint32_t m_prefetch_word0_shift;
	uint32_t m_prefetch_word1_shift;

	uint32_t m_pendingIrq;
	uint32_t m_pendingFiq;
	uint32_t m_pendingAbtD;
	uint32_t m_pendingAbtP;
	uint32_t m_pendingUnd;
	uint32_t m_pendingSwi;
	uint32_t m_pending_interrupt;
	int m_icount;

	/* Coprocessor Registers */
	uint32_t m_control;
	uint32_t m_tlbBase;
	uint32_t m_tlb_base_mask;
	uint32_t m_faultStatus[2];
	uint32_t m_faultAddress;
	uint32_t m_fcsePID;
	uint32_t m_pid_offset;
	uint32_t m_domainAccessControl;
	uint32_t m_decoded_access_control[16];
	uint32_t m_mode;

	uint32_t m_section_bits[0x1000];
	uint32_t m_early_faultless[0x1000];
	uint8_t m_lvl1_type[0x1000];
	uint8_t m_dac_index[0x1000];
	uint8_t m_lvl1_ap[0x1000];
	uint8_t m_section_read_fault[0x1000];
	uint8_t m_section_write_fault[0x1000];

	const int* m_reg_group;

	void update_insn_prefetch(uint32_t curr_pc);
	template <pid_mode CHECK_PID> void update_insn_prefetch_mmu(uint32_t curr_pc);
	uint32_t insn_fetch_thumb(uint32_t pc, bool& translated);
	uint32_t insn_fetch_arm(uint32_t pc, bool& translated);
	int get_insn_prefetch_index(uint32_t address);

	internal_arm_state *m_core;

	int m_stashed_icount;

	address_space *m_program;
	direct_read_data<0> *m_direct;

	endianness_t m_endian;

	uint8_t m_archRev;          // ARM architecture revision (3, 4, and 5 are valid)
	uint8_t m_archFlags;        // architecture flags

	uint32_t m_vectorbase;
	bool m_prefetch_enabled;

	uint32_t m_copro_id;

	bool m_enable_drc;

	// For debugger
	uint32_t m_pc;

	void update_fault_table();
	void calculate_nvc_flags();
	int64_t saturate_qbit_overflow(int64_t res);
	void SwitchMode(uint32_t cpsr_mode_val);
	inline ATTR_FORCE_INLINE uint32_t decodeShiftWithCarry(const uint32_t insn, uint32_t *pCarry);
	inline ATTR_FORCE_INLINE uint32_t decodeShift(const uint32_t insn);
	template <copro_mode MMU, bdt_s_bit S_BIT, stldm_mode USER> int loadInc(const uint32_t insn, uint32_t rbv);
	template <copro_mode MMU, bdt_s_bit S_BIT, stldm_mode USER> int loadDec(const uint32_t insn, uint32_t rbv);
	template <copro_mode MMU, stldm_mode USER> int storeInc(const uint32_t insn, uint32_t rbv);
	template <copro_mode MMU, stldm_mode USER> int storeDec(const uint32_t insn, uint32_t rbv);
	void HandleCoProcRT(const uint32_t insn);
	void HandleCoProcDT(const uint32_t insn);
	template <link_mode LINK> void HandleBranch(const uint32_t insn);
	void HandleBranchHBit(const uint32_t insn);
	template <copro_mode MMU, imm_mode IMMEDIATE, index_mode PRE_INDEX, offset_mode OFFSET_UP, size_mode SIZE_BYTE, writeback_mode WRITEBACK, load_mode LOAD> void HandleMemSingle(const uint32_t insn);
	template <copro_mode MMU, index_mode PRE_INDEX, offset_mode OFFSET_UP, writeback_mode WRITEBACK, load_mode LOAD> void HandleHalfWordDT(const uint32_t insn);
	template <copro_mode MMU> void HandleSwap(const uint32_t insn);
	void HandlePSRTransfer(const uint32_t insn);
	template <imm_mode IMMEDIATE, flags_mode SET_FLAGS, alu_mode OPCODE> inline ATTR_FORCE_INLINE void HandleALU(const uint32_t insn);
	template <flags_mode SET_FLAGS, accum_mode ACCUM> void HandleMul(const uint32_t insn);
	template <flags_mode SET_FLAGS, accum_mode ACCUM> void HandleSMulLong(const uint32_t insn);
	template <flags_mode SET_FLAGS, accum_mode ACCUM> void HandleUMulLong(const uint32_t insn);
	template <copro_mode MMU, index_mode PRE_INDEX, offset_mode OFFSET_UP, bdt_s_bit S_BIT, writeback_mode WRITEBACK> void HandleMemBlock(const uint32_t insn);

	template <copro_mode MMU, offset_mode OFFSET_MODE, flags_mode SET_FLAGS, writeback_mode WRITEBACK, lmul_mode LONG_MUL, smul_mode SIGNED_MUL, accum_mode ACCUM, load_mode LOAD, alu_mode OPCODE> void arm7ops_0(const uint32_t insn);
	template <copro_mode MMU, offset_mode OFFSET_MODE, flags_mode SET_FLAGS, writeback_mode WRITEBACK, load_mode LOAD, alu_bit ALU_BIT, alu_mode OPCODE> void arm7ops_1(const uint32_t insn);

	void arm9ops_undef(const uint32_t insn);
	void arm9ops_1(const uint32_t insn);
	void arm9ops_57(const uint32_t insn);
	void arm9ops_89(const uint32_t insn);
	void arm9ops_ab(const uint32_t insn);
	void arm9ops_c(const uint32_t insn);
	void arm9ops_e(const uint32_t insn);

	inline void set_mode_changed() { m_stashed_icount = m_icount; m_icount = -1; }
	virtual void set_cpsr(uint32_t val);
	inline void set_cpsr_nomode(uint32_t val) { m_cpsr = val; }
	inline uint32_t make_cpsr();
	inline void split_flags();
	template <tlb_rw_mode WRITE> inline ATTR_FORCE_INLINE bool arm7_tlb_translate(offs_t &addr);
	template <check_mode DO_FETCH, insn_mode THUMB, pid_mode CHECK_PID> inline ATTR_FORCE_INLINE uint32_t arm7_tlb_translate_check(offs_t addr);
	uint32_t arm7_tlb_get_second_level_descriptor(uint32_t granularity, uint32_t vaddr);
	inline int detect_read_fault(uint32_t desc_index, uint32_t ap);
	template <tlb_rw_mode WRITE> inline ATTR_FORCE_INLINE int detect_fault(uint32_t desc_index, uint32_t ap);
	int decode_fault(int mode, int ap, int access_control, int system, int rom, int write);
	inline ATTR_FORCE_INLINE void arm7_check_irq_state();
	void update_irq_state();
	virtual void arm7_cpu_write32(uint32_t addr, uint32_t data);
	virtual void arm7_cpu_write16(uint32_t addr, uint16_t data);
	virtual void arm7_cpu_write8(uint32_t addr, uint8_t data);
	virtual void arm7_cpu_write32_mmu(uint32_t addr, uint32_t data);
	virtual void arm7_cpu_write16_mmu(uint32_t addr, uint16_t data);
	virtual void arm7_cpu_write8_mmu(uint32_t addr, uint8_t data);
	virtual uint32_t arm7_cpu_read32(uint32_t addr);
	virtual uint32_t arm7_cpu_read16(uint32_t addr);
	virtual uint8_t arm7_cpu_read8(uint32_t addr);
	virtual uint32_t arm7_cpu_read32_mmu(uint32_t addr);
	virtual uint32_t arm7_cpu_read16_mmu(uint32_t addr);
	virtual uint8_t arm7_cpu_read8_mmu(uint32_t addr);

	// Coprocessor support
	DECLARE_WRITE32_MEMBER( arm7_do_callback );
	virtual DECLARE_READ32_MEMBER( arm7_rt_r_callback );
	virtual DECLARE_WRITE32_MEMBER( arm7_rt_w_callback );
	void arm7_dt_r_callback(const uint32_t insn, uint32_t *prn);
	void arm7_dt_w_callback(const uint32_t insn, uint32_t *prn);

	void tg00_0(uint32_t op, uint32_t pc);
	void tg00_1(uint32_t op, uint32_t pc);
	void tg01_0(uint32_t op, uint32_t pc);
	void tg01_10(uint32_t op, uint32_t pc);
	void tg01_11(uint32_t op, uint32_t pc);
	void tg01_12(uint32_t op, uint32_t pc);
	void tg01_13(uint32_t op, uint32_t pc);
	void tg02_0(uint32_t op, uint32_t pc);
	void tg02_1(uint32_t op, uint32_t pc);
	void tg03_0(uint32_t op, uint32_t pc);
	void tg03_1(uint32_t op, uint32_t pc);
	void tg04_00_00(uint32_t op, uint32_t pc);
	void tg04_00_01(uint32_t op, uint32_t pc);
	void tg04_00_02(uint32_t op, uint32_t pc);
	void tg04_00_03(uint32_t op, uint32_t pc);
	void tg04_00_04(uint32_t op, uint32_t pc);
	void tg04_00_05(uint32_t op, uint32_t pc);
	void tg04_00_06(uint32_t op, uint32_t pc);
	void tg04_00_07(uint32_t op, uint32_t pc);
	void tg04_00_08(uint32_t op, uint32_t pc);
	void tg04_00_09(uint32_t op, uint32_t pc);
	void tg04_00_0a(uint32_t op, uint32_t pc);
	void tg04_00_0b(uint32_t op, uint32_t pc);
	void tg04_00_0c(uint32_t op, uint32_t pc);
	void tg04_00_0d(uint32_t op, uint32_t pc);
	void tg04_00_0e(uint32_t op, uint32_t pc);
	void tg04_00_0f(uint32_t op, uint32_t pc);
	void tg04_01_00(uint32_t op, uint32_t pc);
	void tg04_01_01(uint32_t op, uint32_t pc);
	void tg04_01_02(uint32_t op, uint32_t pc);
	void tg04_01_03(uint32_t op, uint32_t pc);
	void tg04_01_10(uint32_t op, uint32_t pc);
	void tg04_01_11(uint32_t op, uint32_t pc);
	void tg04_01_12(uint32_t op, uint32_t pc);
	void tg04_01_13(uint32_t op, uint32_t pc);
	void tg04_01_20(uint32_t op, uint32_t pc);
	void tg04_01_21(uint32_t op, uint32_t pc);
	void tg04_01_22(uint32_t op, uint32_t pc);
	void tg04_01_23(uint32_t op, uint32_t pc);
	void tg04_01_30(uint32_t op, uint32_t pc);
	void tg04_01_31(uint32_t op, uint32_t pc);
	void tg04_01_32(uint32_t op, uint32_t pc);
	void tg04_01_33(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg04_0203(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg05_0(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg05_1(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg05_2(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg05_3(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg05_4(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg05_5(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg05_6(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg05_7(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg06_0(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg06_1(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg07_0(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg07_1(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg08_0(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg08_1(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg09_0(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg09_1(uint32_t op, uint32_t pc);
	void tg0a_0(uint32_t op, uint32_t pc);
	void tg0a_1(uint32_t op, uint32_t pc);
	void tg0b_0(uint32_t op, uint32_t pc);
	void tg0b_1(uint32_t op, uint32_t pc);
	void tg0b_2(uint32_t op, uint32_t pc);
	void tg0b_3(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg0b_4(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg0b_5(uint32_t op, uint32_t pc);
	void tg0b_6(uint32_t op, uint32_t pc);
	void tg0b_7(uint32_t op, uint32_t pc);
	void tg0b_8(uint32_t op, uint32_t pc);
	void tg0b_9(uint32_t op, uint32_t pc);
	void tg0b_a(uint32_t op, uint32_t pc);
	void tg0b_b(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg0b_c(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg0b_d(uint32_t op, uint32_t pc);
	void tg0b_e(uint32_t op, uint32_t pc);
	void tg0b_f(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg0c_0(uint32_t op, uint32_t pc);
	template <copro_mode MMU> void tg0c_1(uint32_t op, uint32_t pc);
	void tg0d_0(uint32_t op, uint32_t pc);
	void tg0d_1(uint32_t op, uint32_t pc);
	void tg0d_2(uint32_t op, uint32_t pc);
	void tg0d_3(uint32_t op, uint32_t pc);
	void tg0d_4(uint32_t op, uint32_t pc);
	void tg0d_5(uint32_t op, uint32_t pc);
	void tg0d_6(uint32_t op, uint32_t pc);
	void tg0d_7(uint32_t op, uint32_t pc);
	void tg0d_8(uint32_t op, uint32_t pc);
	void tg0d_9(uint32_t op, uint32_t pc);
	void tg0d_a(uint32_t op, uint32_t pc);
	void tg0d_b(uint32_t op, uint32_t pc);
	void tg0d_c(uint32_t op, uint32_t pc);
	void tg0d_d(uint32_t op, uint32_t pc);
	void tg0d_e(uint32_t op, uint32_t pc);
	void tg0d_f(uint32_t op, uint32_t pc);
	void tg0e_0(uint32_t op, uint32_t pc);
	void tg0e_1(uint32_t op, uint32_t pc);
	void tg0f_0(uint32_t op, uint32_t pc);
	void tg0f_1(uint32_t op, uint32_t pc);

	uint32_t *m_tlb_base;
	static int s_read_fault_table_user[16];
	static int s_read_fault_table_no_user[16];
	static int s_write_fault_table_user[16];
	static int s_write_fault_table_no_user[16];
	static uint32_t s_read_fault_word_user;
	static uint32_t s_read_fault_word_no_user;
	static uint32_t s_write_fault_word_user;
	static uint32_t s_write_fault_word_no_user;
	static uint32_t s_add_nvc_flags[8];
	static uint32_t s_sub_nvc_flags[8];
	static const int s_register_table[ARM7_NUM_MODES][17];
	uint32_t *m_register_pointers[ARM7_NUM_MODES][17];

	int *m_read_fault_table;
	int *m_write_fault_table;
	uint32_t m_read_fault_word;
	uint32_t m_write_fault_word;

	//
	// DRC
	//

	/* fast RAM info */
	struct fast_ram_info
	{
		offs_t              start;                      /* start of the RAM block */
		offs_t              end;                        /* end of the RAM block */
		bool                readonly;                   /* true if read-only */
		void *              base;                       /* base in memory where the RAM lives */
	};

	struct hotspot_info
	{
		uint32_t             pc;
		uint32_t             opcode;
		uint32_t             cycles;
	};

	/* internal compiler state */
	struct compiler_state
	{
		uint32_t              cycles;                     /* accumulated cycles */
		uint8_t               checkints;                  /* need to check interrupts before next instruction */
		uint8_t               checksoftints;              /* need to check software interrupts before next instruction */
		uml::code_label  labelnum;                   /* index for local labels */
	};

	/* ARM7 registers */
	struct arm7imp_state
	{
		/* internal stuff */
		uint32_t            jmpdest;                    /* destination jump target */

		/* parameters for subroutines */
		uint64_t            numcycles;                  /* return value from gettotalcycles */
		uint32_t            mode;                       /* current global mode */
		const char *        format;                     /* format string for print_debug */
		uint32_t            arg0;                       /* print_debug argument 1 */
		uint32_t            arg1;                       /* print_debug argument 2 */

		/* register mappings */
		uml::parameter      regmap[/*NUM_REGS*/45];     /* parameter to register mappings for all integer registers */

		/* subroutines */
		uml::code_handle *  entry;                      /* entry point */
		uml::code_handle *  nocode;                     /* nocode exception handler */
		uml::code_handle *  out_of_cycles;              /* out of cycles exception handler */
		uml::code_handle *  tlb_translate;              /* tlb translation handler */
		uml::code_handle *  detect_fault;               /* tlb fault detection handler */
		uml::code_handle *  check_irq;                  /* irq check handler */
		uml::code_handle *  read8;                      /* read byte */
		uml::code_handle *  write8;                     /* write byte */
		uml::code_handle *  read16;                     /* read half */
		uml::code_handle *  write16;                    /* write half */
		uml::code_handle *  read32;                     /* read word */
		uml::code_handle *  write32;                    /* write word */

		/* fast RAM */
		uint32_t            fastram_select;
		fast_ram_info       fastram[ARM7_MAX_FASTRAM];

		/* hotspots */
		uint32_t            hotspot_select;
		hotspot_info        hotspot[ARM7_MAX_HOTSPOTS];
	} m_impstate;

	/* core state */
	drc_cache           m_cache;                      /* pointer to the DRC code cache */
	std::unique_ptr<drcuml_state> m_drcuml;           /* DRC UML generator state */
	std::unique_ptr<arm7_frontend> m_drcfe;           /* pointer to the DRC front-end state */
	uint32_t            m_drcoptions;                 /* configurable DRC options */
	uint8_t             m_cache_dirty;                /* true if we need to flush the cache */

	typedef void ( arm7_cpu_device::*arm7thumb_drcophandler)(drcuml_block*, compiler_state*, const opcode_desc*);
	static const arm7thumb_drcophandler drcthumb_handler[0x40*0x10];

	void drctg00_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Shift left */
	void drctg00_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Shift right */
	void drctg01_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg01_10(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg01_11(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* SUB Rd, Rs, Rn */
	void drctg01_12(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ADD Rd, Rs, #imm */
	void drctg01_13(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* SUB Rd, Rs, #imm */
	void drctg02_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg02_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg03_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ADD Rd, #Offset8 */
	void drctg03_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* SUB Rd, #Offset8 */
	void drctg04_00_00(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* AND Rd, Rs */
	void drctg04_00_01(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* EOR Rd, Rs */
	void drctg04_00_02(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* LSL Rd, Rs */
	void drctg04_00_03(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* LSR Rd, Rs */
	void drctg04_00_04(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ASR Rd, Rs */
	void drctg04_00_05(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ADC Rd, Rs */
	void drctg04_00_06(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* SBC Rd, Rs */
	void drctg04_00_07(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ROR Rd, Rs */
	void drctg04_00_08(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* TST Rd, Rs */
	void drctg04_00_09(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* NEG Rd, Rs */
	void drctg04_00_0a(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* CMP Rd, Rs */
	void drctg04_00_0b(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* CMN Rd, Rs - check flags, add dasm */
	void drctg04_00_0c(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ORR Rd, Rs */
	void drctg04_00_0d(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* MUL Rd, Rs */
	void drctg04_00_0e(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* BIC Rd, Rs */
	void drctg04_00_0f(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* MVN Rd, Rs */
	void drctg04_01_00(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg04_01_01(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ADD Rd, HRs */
	void drctg04_01_02(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ADD HRd, Rs */
	void drctg04_01_03(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Add HRd, HRs */
	void drctg04_01_10(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* CMP Rd, Rs */
	void drctg04_01_11(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* CMP Rd, Hs */
	void drctg04_01_12(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* CMP Hd, Rs */
	void drctg04_01_13(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* CMP Hd, Hs */
	void drctg04_01_20(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* MOV Rd, Rs (undefined) */
	void drctg04_01_21(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* MOV Rd, Hs */
	void drctg04_01_22(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* MOV Hd, Rs */
	void drctg04_01_23(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* MOV Hd, Hs */
	void drctg04_01_30(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg04_01_31(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg04_01_32(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg04_01_33(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg04_0203(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg05_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* STR Rd, [Rn, Rm] */
	void drctg05_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* STRH Rd, [Rn, Rm] */
	void drctg05_2(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* STRB Rd, [Rn, Rm] */
	void drctg05_3(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* LDSB Rd, [Rn, Rm] todo, add dasm */
	void drctg05_4(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* LDR Rd, [Rn, Rm] */
	void drctg05_5(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* LDRH Rd, [Rn, Rm] */
	void drctg05_6(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* LDRB Rd, [Rn, Rm] */
	void drctg05_7(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* LDSH Rd, [Rn, Rm] */
	void drctg06_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Store */
	void drctg06_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Load */
	void drctg07_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Store */
	void drctg07_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* Load */
	void drctg08_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Store */
	void drctg08_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Load */
	void drctg09_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Store */
	void drctg09_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Load */
	void drctg0a_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);  /* ADD Rd, PC, #nn */
	void drctg0a_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ADD Rd, SP, #nn */
	void drctg0b_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* ADD SP, #imm */
	void drctg0b_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_2(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_3(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_4(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* PUSH {Rlist} */
	void drctg0b_5(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* PUSH {Rlist}{LR} */
	void drctg0b_6(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_7(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_8(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_9(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_a(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_b(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_c(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* POP {Rlist} */
	void drctg0b_d(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* POP {Rlist}{PC} */
	void drctg0b_e(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0b_f(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0c_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Store */
	void drctg0c_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* Load */
	void drctg0d_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_EQ:
	void drctg0d_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_NE:
	void drctg0d_2(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_CS:
	void drctg0d_3(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_CC:
	void drctg0d_4(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_MI:
	void drctg0d_5(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_PL:
	void drctg0d_6(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_VS:
	void drctg0d_7(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_VC:
	void drctg0d_8(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_HI:
	void drctg0d_9(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_LS:
	void drctg0d_a(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_GE:
	void drctg0d_b(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_LT:
	void drctg0d_c(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_GT:
	void drctg0d_d(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_LE:
	void drctg0d_e(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // COND_AL:
	void drctg0d_f(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); // SWI (this is sort of a "hole" in the opcode encoding)
	void drctg0e_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0e_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0f_0(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void drctg0f_1(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc); /* BL */

	inline void update_reg_ptr(const uint32_t old_mode);
	inline void init_reg_ptr();
	void load_fast_iregs(drcuml_block *block);
	void save_fast_iregs(drcuml_block *block);
	void execute_run_drc();
	void arm7drc_set_options(uint32_t options);
	void arm7drc_add_fastram(offs_t start, offs_t end, uint8_t readonly, void *base);
	void arm7drc_add_hotspot(offs_t pc, uint32_t opcode, uint32_t cycles);
	void code_flush_cache();
	void code_compile_block(uint8_t mode, offs_t pc);
	void cfunc_get_cycles();
	void cfunc_unimplemented();
	void static_generate_entry_point();
	void static_generate_check_irq();
	void static_generate_nocode_handler();
	void static_generate_out_of_cycles();
	void static_generate_detect_fault(uml::code_handle **handleptr);
	void static_generate_tlb_translate(uml::code_handle **handleptr);
	void static_generate_memory_accessor(int size, bool istlb, bool iswrite, const char *name, uml::code_handle **handleptr);
	void generate_update_cycles(drcuml_block *block, compiler_state *compiler, uml::parameter param);
	void generate_checksum_block(drcuml_block *block, compiler_state *compiler, const opcode_desc *seqhead, const opcode_desc *seqlast);
	void generate_sequence_instruction(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
	void generate_delay_slot_and_branch(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc, uint8_t linkreg);

	typedef bool ( arm7_cpu_device::*drcarm7ops_ophandler)(drcuml_block*, compiler_state*, const opcode_desc*, uint32_t);
	static const drcarm7ops_ophandler drcops_handler[0x10];

	void saturate_qbit_overflow(drcuml_block *block);
	bool drcarm7ops_0123(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc, uint32_t op);
	bool drcarm7ops_4567(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc, uint32_t op);
	bool drcarm7ops_89(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc, uint32_t op);
	bool drcarm7ops_ab(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc, uint32_t op);
	bool drcarm7ops_cd(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc, uint32_t op);
	bool drcarm7ops_e(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc, uint32_t op);
	bool drcarm7ops_f(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc, uint32_t op);
	bool generate_opcode(drcuml_block *block, compiler_state *compiler, const opcode_desc *desc);
};


class arm7_be_cpu_device : public arm7_cpu_device
{
public:
	// construction/destruction
	arm7_be_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};


class arm7500_cpu_device : public arm7_cpu_device
{
public:
	// construction/destruction
	arm7500_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	void set_cpsr(uint32_t val) override;
};


class arm9_cpu_device : public arm7_cpu_device
{
public:
	// construction/destruction
	arm9_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	arm9_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, uint8_t archRev, uint8_t archFlags, endianness_t endianness);
};


class arm920t_cpu_device : public arm9_cpu_device
{
public:
	// construction/destruction
	arm920t_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};


class arm946es_cpu_device : public arm9_cpu_device
{
public:
	// construction/destruction
	arm946es_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// 946E-S has Protection Unit instead of ARM MMU so CP15 is quite different
	virtual DECLARE_READ32_MEMBER( arm7_rt_r_callback ) override;
	virtual DECLARE_WRITE32_MEMBER( arm7_rt_w_callback ) override;

	virtual void arm7_cpu_write32(uint32_t addr, uint32_t data) override;
	virtual void arm7_cpu_write16(uint32_t addr, uint16_t data) override;
	virtual void arm7_cpu_write8(uint32_t addr, uint8_t data) override;
	virtual void arm7_cpu_write32_mmu(uint32_t addr, uint32_t data) override;
	virtual void arm7_cpu_write16_mmu(uint32_t addr, uint16_t data) override;
	virtual void arm7_cpu_write8_mmu(uint32_t addr, uint8_t data) override;
	virtual uint32_t arm7_cpu_read32(uint32_t addr) override;
	virtual uint32_t arm7_cpu_read16(uint32_t addr) override;
	virtual uint8_t arm7_cpu_read8(uint32_t addr) override;
	virtual uint32_t arm7_cpu_read32_mmu(uint32_t addr) override;
	virtual uint32_t arm7_cpu_read16_mmu(uint32_t addr) override;
	virtual uint8_t arm7_cpu_read8_mmu(uint32_t addr) override;

protected:
	arm946es_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	virtual void device_start() override;

private:
	uint32_t cp15_control, cp15_itcm_base, cp15_dtcm_base, cp15_itcm_size, cp15_dtcm_size;
	uint32_t cp15_itcm_end, cp15_dtcm_end, cp15_itcm_reg, cp15_dtcm_reg;
	uint8_t ITCM[0x8000], DTCM[0x4000];

	void RefreshITCM();
	void RefreshDTCM();
};

class igs036_cpu_device : public arm946es_cpu_device
{
public:
	// construction/destruction
	igs036_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};

class pxa255_cpu_device : public arm7_cpu_device
{
public:
	// construction/destruction
	pxa255_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};


class sa1110_cpu_device : public arm7_cpu_device
{
public:
	// construction/destruction
	sa1110_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};

DECLARE_DEVICE_TYPE(ARM7,     arm7_cpu_device)
DECLARE_DEVICE_TYPE(ARM7_BE,  arm7_be_cpu_device)
DECLARE_DEVICE_TYPE(ARM7500,  arm7500_cpu_device)
DECLARE_DEVICE_TYPE(ARM9,     arm9_cpu_device)
DECLARE_DEVICE_TYPE(ARM920T,  arm920t_cpu_device)
DECLARE_DEVICE_TYPE(ARM946ES, arm946es_cpu_device)
DECLARE_DEVICE_TYPE(PXA255,   pxa255_cpu_device)
DECLARE_DEVICE_TYPE(SA1110,   sa1110_cpu_device)
DECLARE_DEVICE_TYPE(IGS036,   igs036_cpu_device)

#endif // MAME_CPU_ARM7_ARM7_H

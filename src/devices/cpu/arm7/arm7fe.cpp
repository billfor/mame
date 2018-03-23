// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/***************************************************************************

    arm7fe.c

    Front-end for ARM7 DRC

***************************************************************************/

#include "emu.h"
#include "arm7fe.h"
#include "arm7core.h"

//**************************************************************************
//  ARM7 FRONTEND
//**************************************************************************

#define REGFLAG_R(n)	(1 << (n))
#define REGFLAG_LR		(1 << 14)
#define REGFLAG_PC		(1 << 15)
#define REGFLAG_CPSR	(1 << 16)
#define REGFLAG_SPSR	(1 << 17)

//-------------------------------------------------
//  arm7_frontend - constructor
//-------------------------------------------------

arm7_frontend::arm7_frontend(arm7_cpu_device *arm7, uint32_t window_start, uint32_t window_end, uint32_t max_sequence)
	: drc_frontend(*arm7, window_start, window_end, max_sequence)
	, m_cpu(arm7)
{
}


//-------------------------------------------------
//  arm9_frontend - constructor
//-------------------------------------------------

arm9_frontend::arm9_frontend(arm9_cpu_device *arm9, uint32_t window_start, uint32_t window_end, uint32_t max_sequence)
	: arm7_frontend(arm9, window_start, window_end, max_sequence)
	, m_cpu(arm9)
{
}


//-------------------------------------------------
//  describe_thumb - build a description of a
//  thumb instruction
//-------------------------------------------------

bool arm7_frontend::describe_thumb(opcode_desc &desc, const opcode_desc *prev)
{
	return false;
}

//-------------------------------------------------
//  describe_ops_* - build a description of
//  an ARM7 instruction
//-------------------------------------------------

bool arm7_frontend::describe_ops_0123(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	/* Branch and Exchange (BX) */
	const uint32_t masked_op = op & 0x0ff000f0;
	if ((op & 0x0ffffff0) == 0x012fff10) // BX
	{
		desc.regin[0] |= REGFLAG_R(op & 0x0f);
		desc.flags |= OPFLAG_CAN_CHANGE_MODES | OPFLAG_IS_UNCONDITIONAL_BRANCH;
		desc.targetpc = BRANCH_TARGET_DYNAMIC;
	}
	else if ((op & 0x0ff000f0) == 0x01200030) // BLX Rn - v5
	{
		desc.regin[0] |= REGFLAG_R(op & 0x0f) | REGFLAG_PC;
		desc.regout[0] |= REGFLAG_LR;
		desc.flags |= OPFLAG_CAN_CHANGE_MODES | OPFLAG_IS_UNCONDITIONAL_BRANCH;
		desc.targetpc = BRANCH_TARGET_DYNAMIC;
	}
	else if (masked_op == 0x01600010) // CLZ - v5
	{
		const uint32_t rm = op & 0xf;
		const uint32_t rd = (op >> 12) & 0xf;
		desc.regin[0] |= REGFLAG_R(rm);
		desc.regout[0] |= REGFLAG_R(rd);
	}
	else if (masked_op == 0x01000050 || // QADD - v5
			 masked_op == 0x01400050 || // QDADD - v5
			 masked_op == 0x01200050 || // QSUB - v5
			 masked_op == 0x01000080)   // QDSUB - v5
	{
		const uint32_t rn = op & 0xf;
		const uint32_t rm = (op >> 16) & 0xf;
		const uint32_t rd = (op >> 12) & 0xf;
		desc.regin[0] |= REGFLAG_R(rn) | REGFLAG_R(rm);
		desc.regout[0] |= REGFLAG_R(rd);
		desc.regout[0] |= REGFLAG_CPSR;
	}
	else if ((op & 0x0ff00090) == 0x01000080 || // SMLAxy - v5
			 (op & 0x0ff000b0) == 0x01200080)   // SMLAWy - v5
	{
		const uint32_t rn = op & 0xf;
		const uint32_t rm = (op >> 8) & 0xf;
		const uint32_t r3 = (op >> 12) & 0xf;
		const uint32_t rd = (op >> 16) & 0xf;
		desc.regin[0] |= REGFLAG_R(rn) | REGFLAG_R(rm) | REGFLAG_R(r3);
		desc.regout[0] |= REGFLAG_R(rd);
		desc.regout[0] |= REGFLAG_CPSR;
	}
	else if ((op & 0x0ff00090) == 0x01400080) // SMLALxy - v5
	{
		const uint32_t rn = op & 0xf;
		const uint32_t rm = (op >> 8) & 0xf;
		const uint32_t r3 = (op >> 12) & 0xf;
		const uint32_t rd = (op >> 16) & 0xf;
		desc.regin[0] |= REGFLAG_R(rn) | REGFLAG_R(rm) | REGFLAG_R(r3) | REGFLAG_R(rd);
		desc.regout[0] |= REGFLAG_R(rd) | REGFLAG_R(r3);
	}
	else if ((op & 0x0ff00090) == 0x01600080 || // SMULxy - v5
			 (op & 0x0ff000b0) == 0x012000a0)   // SMULWy - v5
	{
		const uint32_t rn = op & 0xf;
		const uint32_t rm = (op >> 8) & 0xf;
		const uint32_t rd = (op >> 16) & 0xf;
		desc.regin[0] |= REGFLAG_R(rn) | REGFLAG_R(rm);
		desc.regout[0] |= REGFLAG_R(rd);
	}
	else if ((op & 0x0e000000) == 0 && (op & 0x80) && (op & 0x10)) // Multiply OR Swap OR Half Word Data Transfer
	{
		if (op & 0x60) // Half Word Data Transfer
		{
			describe_halfword_transfer(desc, prev, op);
		}
		else if (op & 0x01000000) // Swap
		{
			describe_swap(desc, prev, op);
		}
		else // Multiply Or Multiply Long
		{
			if (op & 0x800000) // Multiply long
			{
				describe_mul_long(desc, prev, op);
			}
			else // Multiply
			{
				describe_mul(desc, prev, op);
			}
		}
	}
	else if ((op & 0x0c000000) == 0) // Data Processing OR PSR Transfer
	{
		if (((op & 0x00100000) == 0) && ((op & 0x01800000) == 0x01000000)) // PSR Transfer
		{
			describe_psr_transfer(desc, prev, op);
		}
		else // Data processing
		{
			describe_alu(desc, prev, op);
		}
	}
	return true;
}

void arm7_frontend::describe_alu(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	const uint32_t rn = (op & INSN_RN) >> INSN_RN_SHIFT;
	const uint32_t rd = (op & INSN_RD) >> INSN_RD_SHIFT;
	const uint32_t alu_op = (op & INSN_OPCODE) >> INSN_OPCODE_SHIFT;

	desc.regin[0] |= REGFLAG_R(rn);
	desc.regin[0] |= REGFLAG_CPSR;
	desc.regout[0] |= REGFLAG_CPSR;
	if (rd == eR15 || (alu_op & 0xc) != 0x8)
	{
		desc.regout[0] |= REGFLAG_R(rd);
	}

	if (!(op & INSN_I))
	{
		desc.cycles = 4;
	}
}

void arm7_frontend::describe_psr_transfer(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	desc.cycles = 1;

	desc.regin[0] |= REGFLAG_CPSR;
	if (op & 0x400000)
	{
		desc.regin[0] |= REGFLAG_SPSR;
	}

	if ((op & 0x00200000) && !(op & INSN_I))
	{
		desc.regin[0] |= REGFLAG_R(op & 0x0f);
		if (op & 0x400000)
		{
			desc.regout[0] |= REGFLAG_SPSR;
		}
		else
		{
			desc.regout[0] |= REGFLAG_CPSR;
		}
	}
	else if (!(op & 0x00200000))
	{
		desc.regout[0] |= REGFLAG_R((op >> 12) & 0x0f);
	}
}

void arm7_frontend::describe_mul_long(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	const uint32_t rm  = op & 0xf;
	const uint32_t rs  = (op >> 8) & 0xf;
	const uint32_t rhi = (op >> 16) & 0xf;
	const uint32_t rlo = (op >> 12) & 0xf;

	desc.regin[0] |= REGFLAG_R(rm);
	desc.regin[0] |= REGFLAG_R(rs);
	desc.regout[0] |= REGFLAG_R(rhi);
	desc.regout[0] |= REGFLAG_R(rlo);

	if (op & INSN_MUL_A)
	{
		desc.regin[0] |= REGFLAG_R(rhi);
		desc.regin[0] |= REGFLAG_R(rlo);
	}

	if (op & INSN_S)
	{
		desc.regin[0] |= REGFLAG_CPSR;
		desc.regout[0] |= REGFLAG_CPSR;
	}
}

void arm7_frontend::describe_mul(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	const uint32_t rm = op & INSN_MUL_RM;
	const uint32_t rs = (op & INSN_MUL_RS) >> INSN_MUL_RS_SHIFT;
	const uint32_t rd = (op & INSN_MUL_RD) >> INSN_MUL_RD_SHIFT;

	desc.regin[0] |= REGFLAG_R(rm);
	desc.regin[0] |= REGFLAG_R(rs);
	desc.regout[0] |= REGFLAG_R(rd);

	if (op & INSN_MUL_A)
	{
		const uint32_t rn = (op & INSN_MUL_RN) >> INSN_MUL_RN_SHIFT;
		desc.regin[0] |= REGFLAG_R(rn);
	}

	if (op & INSN_S)
	{
		desc.regin[0] |= REGFLAG_CPSR;
		desc.regout[0] |= REGFLAG_CPSR;
	}
}

void arm7_frontend::describe_swap(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	const uint32_t rm = op & 0xf;          // reg. w/write address
	const uint32_t rn = (op >> 16) & 0xf;  // reg. w/read address
	const uint32_t rd = (op >> 12) & 0xf;  // dest reg

	desc.regin[0] |= REGFLAG_R(rn);
	desc.regin[0] |= REGFLAG_R(rm);
	desc.regout[0] |= REGFLAG_R(rd);
	desc.flags |= OPFLAG_READS_MEMORY | OPFLAG_WRITES_MEMORY;
	desc.cycles = 4;
}

void arm7_frontend::describe_halfword_transfer(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	if (!(op & 0x400000))
	{
		desc.regin[0] |= REGFLAG_R(op & 0x0f);
	}

	const uint32_t rn = (op & INSN_RN) >> INSN_RN_SHIFT;
	desc.regin[0] |= REGFLAG_R(rn);

	if (op & INSN_SDT_P)
	{
		if (op & INSN_SDT_W)
		{
			desc.regout[0] |= REGFLAG_R(rn);
		}
	}
	else
	{
		desc.regout[0] |= REGFLAG_R(rn);
	}

	const uint32_t rd = (op & INSN_RD) >> INSN_RD_SHIFT;
	if (op & INSN_SDT_L)
	{
		desc.flags |= OPFLAG_READS_MEMORY;
		desc.regout[0] |= REGFLAG_R(rd);
		if (rd == eR15)
		{
			desc.cycles = 5;
		}
	}
	else
	{
		if ((op & 0x60) == 0x40)  // LDRD
		{
			desc.regout[0] |= REGFLAG_R(rd);
		}
		else if ((op & 0x60) == 0x60) // STRD
		{
			desc.regin[0] |= REGFLAG_R(rd);
			desc.flags |= OPFLAG_WRITES_MEMORY;
		}
		else
		{
			desc.regout[0] |= REGFLAG_R(rd);
			if (rd == eR15)
			{
				desc.cycles = 4;
			}
		}
	}
}

bool arm7_frontend::describe_ops_4567(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	const uint32_t rn = (op & INSN_RN) >> INSN_RN_SHIFT;
	desc.regin[0] |= REGFLAG_R(rn);
	if ((op & INSN_SDT_P) && (op & INSN_SDT_W))
	{
		desc.regout[0] |= REGFLAG_R(rn);
	}

	const uint32_t rd = (op & INSN_RD) >> INSN_RD_SHIFT;
	if (op & INSN_SDT_L)
	{
		// Load
		desc.regout[0] |= REGFLAG_R(rd);
		desc.flags |= OPFLAG_READS_MEMORY;
		if (!(op & INSN_SDT_B) && rd == eR15)
		{
			desc.cycles = 5;
		}
	}
	else
	{
		// Store
		desc.flags |= OPFLAG_WRITES_MEMORY;
		desc.regin[0] |= REGFLAG_R(rd);
		desc.cycles = 2;
	}

	if (!(op & INSN_SDT_P))
	{
		desc.regout[0] |= REGFLAG_R(rn);
	}
	return true;
}

bool arm7_frontend::describe_ops_89(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	if ((op & 0x005f0f00) == 0x004d0500)
	{
		// unsupported (armv6 onwards only)
	}
	else if ((op & 0x00500f00) == 0x00100a00) /* Return From Exception (RFE) */
	{
		// unsupported (armv6 onwards only)
	}
	return false;
}

bool arm7_frontend::describe_ops_ab(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	desc.regout[0] |= REGFLAG_CPSR;
	desc.regin[0] |= REGFLAG_CPSR;
	// BLX
	//HandleBranch(op, true);
	//set_cpsr_nomode(GET_CPSR|T_MASK);
	return true;
}

bool arm7_frontend::describe_ops_cd(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	/* Additional coprocessor double register transfer */
	if ((op & 0x00e00000) == 0x00400000)
	{
		// unsupported
	}
	return false;
}

bool arm7_frontend::describe_ops_e(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	/* Additional coprocessor register transfer */
	// unsupported
	return false;
}

bool arm7_frontend::describe_ops_f(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	return false;
}

//-------------------------------------------------
//  describe_arm9_ops_* - build a description of
//  an ARM9 instruction
//-------------------------------------------------

bool arm9_frontend::describe_arm9_ops_1(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	/* Change processor state (CPS) */
	if ((op & 0x00f10020) == 0x00000000)
	{
		// unsupported (armv6 onwards only)
	}
	else if ((op & 0x00ff00f0) == 0x00010000) /* set endianness (SETEND) */
	{
		// unsupported (armv6 onwards only)
	}
	return false;
}

bool arm9_frontend::describe_arm9_ops_57(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	/* Cache Preload (PLD) */
	if ((op & 0x0070f000) == 0x0050f000)
	{
		// unsupported (armv6 onwards only)
	}
	return false;
}

bool arm9_frontend::describe_arm9_ops_89(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	return false;
}

bool arm9_frontend::describe_arm9_ops_ab(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	return false;
}

bool arm9_frontend::describe_arm9_ops_c(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	return false;
}

bool arm9_frontend::describe_arm9_ops_e(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	return false;
}


uint32_t arm7_frontend::get_cpsr()
{
	return m_cpu->m_core->m_r[eCPSR];
}

bool arm7_frontend::get_mode32()
{
	return m_cpu->m_core->m_r[eCPSR] & SR_MODE32;
}

//-------------------------------------------------
//  describe - build a description of a single
//  instruction
//-------------------------------------------------

bool arm7_frontend::describe(opcode_desc &desc, const opcode_desc *prev)
{
	// compute the physical PC
	const uint32_t cpsr = get_cpsr();
	assert((desc.physpc & (T_IS_SET(cpsr) ? 1 : 3)) == 0);
	if (!m_cpu->arm7_tlb_translate(desc.physpc, ARM7_TLB_ABORT_P | ARM7_TLB_READ))
	{
		// uh-oh: a page fault; leave the description empty and just if this is the first instruction, leave it empty and
		// mark as needing to validate; otherwise, just end the sequence here
		desc.flags |= OPFLAG_VALIDATE_TLB | OPFLAG_CAN_CAUSE_EXCEPTION | OPFLAG_COMPILER_PAGE_FAULT | OPFLAG_VIRTUAL_NOOP | OPFLAG_END_SEQUENCE;
		return true;
	}

	if (T_IS_SET(cpsr))
	{
		return describe_thumb(desc, prev);
	}

	// fetch the opcode
	uint32_t op = desc.opptr.l[0] = m_cpu->m_direct->read_dword(desc.physpc);

	// all non-THUMB instructions are 4 bytes and default to 3 cycles each
	desc.length = 4;
	desc.cycles = 3;

	// parse the instruction
	return parse(desc, prev, op);
}

bool arm7_frontend::parse(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	int op_offset = 0;
	if ((op >> INSN_COND_SHIFT) == COND_NV && m_cpu->m_archRev >= 5)
	{
		op_offset = 0x10;
	}

	switch (((op & 0xF000000) >> 24) + op_offset)
	{
		case 0x0: case 0x1: case 0x2: case 0x3:
			return describe_ops_0123(desc, prev, op);

		case 0x4: case 0x5: case 0x6: case 0x7:
			return describe_ops_4567(desc, prev, op);

		case 0x8: case 0x9:
			return describe_ops_89(desc, prev, op);

		case 0xa: case 0xb:
			return describe_ops_ab(desc, prev, op);

		case 0xc: case 0xd:
			return describe_ops_cd(desc, prev, op);

		case 0xe:
			return describe_ops_e(desc, prev, op);

		case 0xf:
			return describe_ops_f(desc, prev, op);

		default:
			return false;
	}
}

bool arm9_frontend::parse(opcode_desc &desc, const opcode_desc *prev, uint32_t op)
{
	bool described = arm7_frontend::parse(desc, prev, op);

	if (!described)
	{
		uint32_t op_index = (op >> 24) & 0xf;
		if ((op >> INSN_COND_SHIFT) == COND_NV && m_cpu->m_archRev >= 5)
		{
			op_index += 0x10;
		}

		if (op_index < 0x10)
		{
			return false;
		}

		switch (op_index)
		{
			case 0x11:
				return describe_arm9_ops_1(desc, prev, op);

			case 0x15: case 0x17:
				return describe_arm9_ops_57(desc, prev, op);

			case 0x18: case 0x19:
				return describe_arm9_ops_89(desc, prev, op);

			case 0x1a: case 0x1b:
				return describe_arm9_ops_ab(desc, prev, op);

			case 0x1c:
				return describe_arm9_ops_c(desc, prev, op);

			case 0x1e:
				return describe_arm9_ops_e(desc, prev, op);

			default:
				return false;
		}
	}

	return described;
}

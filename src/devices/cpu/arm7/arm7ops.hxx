// license:BSD-3-Clause
// copyright-holders:Steve Ellenoff,R. Belmont,Ryan Holtz

int64_t arm7_cpu_device::saturate_qbit_overflow(int64_t res)
{
	if (res > 2147483647)   // INT32_MAX
	{   // overflow high? saturate and set Q
		res = 2147483647;
		set_cpsr_nomode(GET_CPSR | Q_MASK);
	}
	else if (res < (-2147483647-1)) // INT32_MIN
	{   // overflow low? saturate and set Q
		res = (-2147483647-1);
		set_cpsr_nomode(GET_CPSR | Q_MASK);
	}

	return res;
}


void arm7_cpu_device::SwitchMode(uint32_t cpsr_mode_val)
{
	uint32_t cpsr = m_cpsr & ~MODE_FLAG;
	set_cpsr(cpsr | cpsr_mode_val);
}


/* Decodes an Op2-style shifted-register form.  If @carry@ is non-zero the
 * shifter carry output will manifest itself as @*carry == 0@ for carry clear
 * and @*carry != 0@ for carry set.

   SJE: Rules:
   IF RC = 256, Result = no shift.
   LSL   0   = Result = RM, Carry = Old Contents of CPSR C Bit
   LSL(0,31) = Result shifted, least significant bit is in carry out
   LSL  32   = Result of 0, Carry = Bit 0 of RM
   LSL >32   = Result of 0, Carry out 0
   LSR   0   = LSR 32 (see below)
   LSR  32   = Result of 0, Carry = Bit 31 of RM
   LSR >32   = Result of 0, Carry out 0
   ASR >=32  = ENTIRE Result = bit 31 of RM
   ROR  32   = Result = RM, Carry = Bit 31 of RM
   ROR >32   = Same result as ROR n-32 until amount in range of 1-32 then follow rules
*/

uint32_t arm7_cpu_device::decodeShiftWithCarry(const uint32_t insn, uint32_t *pCarry)
{
	uint32_t k  = (insn & INSN_OP2_SHIFT) >> INSN_OP2_SHIFT_SHIFT;  // Bits 11-7
	uint32_t rm = GetRegister(insn & INSN_OP2_RM);
	uint32_t t  = (insn & INSN_OP2_SHIFT_TYPE) >> INSN_OP2_SHIFT_TYPE_SHIFT;

	if ((insn & INSN_OP2_RM) == 0xf) {
		// "If a register is used to specify the shift amount the PC will be 12 bytes ahead." (instead of 8)
		rm += t & 1 ? 12 : 8;
	}

	/* All shift types ending in 1 are Rk, not #k */
	if (t & 1)
	{
//      LOG(("%08x:  RegShift %02x %02x\n", R15, k >> 1, GetRegister(k >> 1)));
#if ARM7_DEBUG_CORE
			if ((insn & 0x80) == 0x80)
				LOG(("%08x:  RegShift ERROR (p36)\n", R15));
#endif
		// Keep only the bottom 8 bits for a Register Shift
		k = GetRegister(k >> 1) & 0xff;

		if (k == 0) /* Register shift by 0 is a no-op */
		{
//          LOG(("%08x:  NO-OP Regshift\n", R15));
			*pCarry = m_cflag;
			return rm;
		}
	}
	/* Decode the shift type and perform the shift */
	switch (t >> 1)
	{
	case 0:                     /* LSL */
		// LSL  32   = Result of 0, Carry = Bit 0 of RM
		// LSL >32   = Result of 0, Carry out 0
		if (k >= 32)
		{
			*pCarry = (k == 32) ? rm & 1 : 0;
			return 0;
		}
		else
		{
			// LSL      0   = Result = RM, Carry = Old Contents of CPSR C Bit
			// LSL (0,31)   = Result shifted, least significant bit is in carry out
			*pCarry = k ? (rm & (1 << (32 - k))) : m_cflag;
			return LSL(rm, k);
		}

	case 1:                         /* LSR */
		if (k == 0 || k == 32)
		{
			*pCarry = rm & SIGN_BIT;
			return 0;
		}
		else if (k > 32)
		{
			*pCarry = 0;
			return 0;
		}
		else
		{
			*pCarry = (rm & (1 << (k - 1)));
			return LSR(rm, k);
		}

	case 2:                     /* ASR */
		if (k == 0 || k > 32)
			k = 32;

		*pCarry = (rm & (1 << (k - 1)));
		if (k >= 32)
			return rm & SIGN_BIT ? 0xffffffffu : 0;
		else
		{
			if (rm & SIGN_BIT)
				return LSR(rm, k) | (0xffffffffu << (32 - k));
			else
				return LSR(rm, k);
		}

	case 3:                     /* ROR and RRX */
		if (k)
		{
			k &= 31;
			if (k)
			{
				*pCarry = rm & (1 << (k - 1));
				return ROR(rm, k);
			}
			else
			{
				*pCarry = rm & SIGN_BIT;
				return rm;
			}
		}
		else
		{
			/* RRX */
			*pCarry = (rm & 1);
			return LSR(rm, 1) | (m_cflag << 31);
		}
	}

	LOG(("%08x: Decodeshift error\n", R15));
	return 0;
}

uint32_t arm7_cpu_device::decodeShift(const uint32_t insn)
{
	const uint32_t rmi = insn & INSN_OP2_RM;
	const uint32_t t  = (insn & INSN_OP2_SHIFT_TYPE) >> INSN_OP2_SHIFT_TYPE_SHIFT;

#if 1
	/* Decode the shift type and perform the shift */
	switch (t)
	{
	case 0: // LSL #k
	{
		const uint32_t rm = (rmi == 0xf) ? GetRegister(rmi) + 8 : (GetRegister(rmi));
		return rm << ((insn & INSN_OP2_SHIFT) >> INSN_OP2_SHIFT_SHIFT);
	}

	case 1: // LSL Rk
	{
		// LSL  32   = Result of 0, Carry = Bit 0 of RM
		// LSL >32   = Result of 0, Carry out 0
		const uint32_t rm = (rmi == 0xf) ? GetRegister(rmi) + 12 : (GetRegister(rmi));
		const uint32_t k = GetRegister((insn >> 8) & 0xf) & 0xff;
		if (k >= 32)
			return 0;
		else
			return rm << k;
		break;
	}

	case 2: // LSR #k
	{
		const uint32_t rm = (rmi == 0xf) ? GetRegister(rmi) + 8 : (GetRegister(rmi));
		const uint32_t k = (insn & INSN_OP2_SHIFT) >> INSN_OP2_SHIFT_SHIFT;
		if (k == 0)
			return 0;
		else
			return rm >> k;
		break;
	}

	case 3: // LSR Rk
	{
		const uint32_t rm = (rmi == 0xf) ? GetRegister(rmi) + 12 : (GetRegister(rmi));
		const uint32_t k  = GetRegister((insn >> 8) & 0xf) & 0xff;
		if (k >= 32)
			return 0;
		else
			return rm >> k;
		break;
	}

	case 4: // ASR #k
	{
		const uint32_t rm = (rmi == 0xf) ? GetRegister(rmi) + 8 : (GetRegister(rmi));
		const uint32_t k = (insn & INSN_OP2_SHIFT) >> INSN_OP2_SHIFT_SHIFT;
		if (k == 0)
			return rm & SIGN_BIT ? 0xffffffffu : 0;
		else
			return (int32_t)rm >> k;
		break;
	}

	case 5: // ASR Rk
	{
		const uint32_t rm = (rmi == 0xf) ? GetRegister(rmi) + 12 : (GetRegister(rmi));
		const uint32_t k  = GetRegister((insn >> 8) & 0xf) & 0xff;
		if (k == 0)
			return rm;
		else if (k >= 32)
			return rm & SIGN_BIT ? 0xffffffffu : 0;
		else
			return (int32_t)rm >> k;
		break;
	}

	case 6: // ROR #k and RRX #k
	{
		const uint32_t rm = (rmi == 0xf) ? GetRegister(rmi) + 8 : (GetRegister(rmi));
		const uint32_t k = (insn & INSN_OP2_SHIFT) >> INSN_OP2_SHIFT_SHIFT;
		if (k)
			return ROR(rm, k);
		else
			return LSR(rm, 1) | (m_cflag << 31); // RRX
		break;
	}

	case 7: // ROR Rk and RRX Rk
	{
		const uint32_t rm = (rmi == 0xf) ? GetRegister(rmi) + 12 : (GetRegister(rmi));
		uint32_t k  = GetRegister((insn >> 8) & 0xf) & 0xff;
		if (k)
		{
			// ROR
			k &= 0x1f;
			if (k)
				return ROR(rm, k);
		}
		return rm;
	}
	}
#else
	uint32_t k  = (insn & INSN_OP2_SHIFT) >> INSN_OP2_SHIFT_SHIFT;  // Bits 11-7

	/* All shift types ending in 1 are Rk, not #k */
	if (t & 1)
	{
		// Keep only the bottom 8 bits for a Register Shift
		k = GetRegister(k >> 1) & 0xff;

		if (k == 0) /* Register shift by 0 is a no-op */
			return rm;
	}
	/* Decode the shift type and perform the shift */
	switch (t >> 1)
	{
	case 0:                     /* LSL */
		// LSL  32   = Result of 0, Carry = Bit 0 of RM
		// LSL >32   = Result of 0, Carry out 0
		if (k >= 32)
			return 0;
		else
			return LSL(rm, k);
		break;

	case 1:                         /* LSR */
		if (k == 0 || k >= 32)
			return 0;
		else if (k > 32)
			return 0;
		else
			return LSR(rm, k);
		break;

	case 2:                     /* ASR */
		if (k == 0 || k >= 32)
		{
			return rm & SIGN_BIT ? 0xffffffffu : 0;
		}
		else
		{
			if (rm & SIGN_BIT)
				return LSR(rm, k) | (0xffffffffu << (32 - k));
			else
				return LSR(rm, k);
		}
		break;

	case 3:                     /* ROR and RRX */
		if (k)
		{
			k &= 0x1f;
			if (k)
				return ROR(rm, k);
			else
				return rm;
		}
		else
		{
			/* RRX */
			return LSR(rm, 1) | (m_cflag << 31);
		}
		break;
	}

#endif
	return 0;
}

template <arm7_cpu_device::copro_mode MMU, arm7_cpu_device::bdt_s_bit S_BIT, arm7_cpu_device::stldm_mode USER>
int arm7_cpu_device::loadInc(const uint32_t insn, uint32_t rbv)
{
	int result = 0;
	rbv &= ~3;
	int i;
	for (i = 0; i < 16; i++)
	{
		if ((insn >> i) & 1)
		{
			uint32_t data;
			if (MMU)
			{
				data = arm7_cpu_read32_mmu(rbv += 4);
				if (m_pendingAbtD) // "Overwriting of registers stops when the abort happens."
				{
					result++;
					break;
				}
			}
			else
			{
				data = arm7_cpu_read32(rbv += 4);
			}
			if (i == 15)
			{
				if (USER)
				{
					if (S_BIT) /* Pull full contents from stack */
						SetModeRegister(eARM7_MODE_USER, 15, data);
					else //if (MODE32) /* Pull only address, preserve mode & status flags */
						SetModeRegister(eARM7_MODE_USER, 15, data);
					//else
					//{
					//	SetModeRegister(mode, 15, (GetModeRegister(mode, 15) & ~0x03FFFFFC) | (data & 0x03FFFFFC));
					//}
				}
				else
				{
					if (S_BIT) /* Pull full contents from stack */
						SetRegister(15, data);
					else //if (MODE32) /* Pull only address, preserve mode & status flags */
						SetRegister(15, data);
					//else
					//{
					//	SetModeRegister(mode, 15, (GetModeRegister(mode, 15) & ~0x03FFFFFC) | (data & 0x03FFFFFC));
					//}
				}
			}
			else
			{
				if (USER)
					SetModeRegister(eARM7_MODE_USER, i, data);
				else
					SetRegister(i, data);
			}
			result++;
		}
	}

	if (m_pendingAbtD)
	{
		for (; i < 16; i++)
		{
			if ((insn >> i) & 1)
			{
				result++;
			}
		}
	}

	return result;
}

template <arm7_cpu_device::copro_mode MMU, arm7_cpu_device::bdt_s_bit S_BIT, arm7_cpu_device::stldm_mode USER>
int arm7_cpu_device::loadDec(const uint32_t insn, uint32_t rbv)
{
	int result = 0;
	rbv &= ~3;
	int i;
	for (i = 15; i >= 0; i--)
	{
		if ((insn >> i) & 1)
		{
			uint32_t data;
			if (MMU)
			{
				data = arm7_cpu_read32_mmu(rbv -= 4);
				if (m_pendingAbtD) // "Overwriting of registers stops when the abort happens."
				{
					result++;
					break;
				}
			}
			else
			{
				data = arm7_cpu_read32(rbv -= 4);
			}
			if (i == 15)
			{
				if (USER)
				{
					if (S_BIT) /* Pull full contents from stack */
						SetModeRegister(eARM7_MODE_USER, 15, data);
					else //if (MODE32) /* Pull only address, preserve mode & status flags */
						SetModeRegister(eARM7_MODE_USER, 15, data);
					//else
					//{
					//	SetModeRegister(mode, 15, (GetModeRegister(mode, 15) & ~0x03FFFFFC) | (data & 0x03FFFFFC));
					//}
				}
				else
				{
					if (S_BIT) /* Pull full contents from stack */
						SetRegister(15, data);
					else //if (MODE32) /* Pull only address, preserve mode & status flags */
						SetRegister(15, data);
					//else
					//{
					//	SetModeRegister(mode, 15, (GetModeRegister(mode, 15) & ~0x03FFFFFC) | (data & 0x03FFFFFC));
					//}
				}
			}
			else
			{
				if (USER)
					SetModeRegister(eARM7_MODE_USER, i, data);
				else
					SetRegister(i, data);
			}
			result++;
		}
	}

	if (m_pendingAbtD)
	{
		for (; i >= 0; i--)
		{
			if ((insn >> i) & 1)
			{
				result++;
			}
		}
	}

	return result;
}

template <arm7_cpu_device::copro_mode MMU, arm7_cpu_device::stldm_mode USER>
int arm7_cpu_device::storeInc(const uint32_t insn, uint32_t rbv)
{
	int result = 0;
	for (int i = 0; i < 16; i++)
	{
		if ((insn >> i) & 1)
		{
#if ARM7_DEBUG_CORE
			if (i == 15) /* R15 is plus 12 from address of STM */
				LOG(("%08x: StoreInc on R15\n", R15));
#endif
			if (USER)
				if (MMU)
					arm7_cpu_write32_mmu(rbv += 4, GetModeRegister(eARM7_MODE_USER, i));
				else
					arm7_cpu_write32(rbv += 4, GetModeRegister(eARM7_MODE_USER, i));
			else
				if (MMU)
					arm7_cpu_write32_mmu(rbv += 4, GetRegister(i));
				else
					arm7_cpu_write32(rbv += 4, GetRegister(i));
			result++;
		}
	}
	return result;
} /* storeInc */

template <arm7_cpu_device::copro_mode MMU, arm7_cpu_device::stldm_mode USER>
int arm7_cpu_device::storeDec(const uint32_t insn, uint32_t rbv)
{
	// pre-count the # of registers being stored
	int const result = population_count_32(insn & 0x0000ffff);

	// adjust starting address
	rbv -= (result << 2);

	for (int i = 0; i <= 15; i++)
	{
		if ((insn >> i) & 1)
		{
#if ARM7_DEBUG_CORE
			if (i == 15) /* R15 is plus 12 from address of STM */
				LOG(("%08x: StoreDec on R15\n", R15));
#endif
			if (USER)
				if (MMU)
					arm7_cpu_write32_mmu(rbv, GetModeRegister(eARM7_MODE_USER, i));
				else
					arm7_cpu_write32(rbv, GetModeRegister(eARM7_MODE_USER, i));
			else
				if (MMU)
					arm7_cpu_write32_mmu(rbv, GetRegister(i));
				else
					arm7_cpu_write32(rbv, GetRegister(i));
			rbv += 4;
		}
	}
	return result;
} /* storeDec */


/***************************************************************************
 *                            OPCODE HANDLING
 ***************************************************************************/

// Co-Processor Register Transfer - To/From Arm to Co-Proc
void arm7_cpu_device::HandleCoProcRT(const uint32_t insn)
{
	/* xxxx 1110 oooL nnnn dddd cccc ppp1 mmmm */

	// Load (MRC) data from Co-Proc to ARM7 register
	if (insn & 0x00100000)       // Bit 20 = Load or Store
	{
		uint32_t res = arm7_rt_r_callback(*m_program, insn, 0);   // RT Read handler must parse opcode & return appropriate result
		if (!m_pendingUnd)
		{
			SetRegister((insn >> 12) & 0xf, res);
		}
	}
	// Store (MCR) data from ARM7 to Co-Proc register
	else
	{
		arm7_rt_w_callback(*m_program, insn, GetRegister((insn >> 12) & 0xf), 0);
	}
}

/* Data Transfer - To/From Arm to Co-Proc
   Loading or Storing, the co-proc function is responsible to read/write from the base register supplied + offset
   8 bit immediate value Base Offset address is << 2 to get the actual #

  issues - #1 - the co-proc function, needs direct access to memory reads or writes (ie, so we must send a pointer to a func)
         - #2 - the co-proc may adjust the base address (especially if it reads more than 1 word), so a pointer to the register must be used
                but the old value of the register must be restored if write back is not set..
         - #3 - when post incrementing is used, it's up to the co-proc func. to add the offset, since the transfer
                address supplied in that case, is simply the base. I suppose this is irrelevant if write back not set
                but if co-proc reads multiple address, it must handle the offset adjustment itself.
*/
// todo: test with valid instructions
void arm7_cpu_device::HandleCoProcDT(const uint32_t insn)
{
	uint32_t rn = (insn >> 16) & 0xf;
	uint32_t rnv = GetRegister(rn);    // Get Address Value stored from Rn
	uint32_t ornv = rnv;                // Keep value of Rn
	uint32_t off = (insn & 0xff) << 2;  // Offset is << 2 according to manual
	uint32_t *prn = &m_r[s_register_table[m_mode][rn]];       // Pointer to our register, so it can be changed in the callback

#if ARM7_DEBUG_CORE
	if (((insn >> 16) & 0xf) == 15 && (insn & 0x200000))
		LOG(("%08x: Illegal use of R15 as base for write back value!\n", R15));
#endif

	// Pre-Increment base address (IF POST INCREMENT - CALL BACK FUNCTION MUST DO IT)
	if ((insn & 0x1000000) && off)
	{
		// Up - Down bit
		if (insn & 0x800000)
			rnv += off;
		else
			rnv -= off;
	}

	// Load (LDC) data from ARM7 memory to Co-Proc memory
	if (insn & 0x00100000)
	{
		arm7_dt_r_callback(insn, prn);
	}
	// Store (STC) data from Co-Proc to ARM7 memory
	else
	{
		arm7_dt_w_callback(insn, prn);
	}

	if (m_pendingUnd != 0) return;

	// If writeback not used - ensure the original value of RN is restored in case co-proc callback changed value
	if ((insn & 0x200000) == 0)
		SetRegister(rn, ornv);

	ARM7_ICOUNT -= 3;
	R15 += 4;
}

template <arm7_cpu_device::link_mode LINK>
void arm7_cpu_device::HandleBranch(const uint32_t insn)
{
	const uint32_t off = (insn & INSN_BRANCH) << 2;

	/* Save PC into LR if this is a branch with link or a BLX */
	if (LINK)
		SetRegister(14, R15 + 4);

	/* Sign-extend the 24-bit offset in our calculations */
	if (off & 0x2000000u)
	{
		//if (MODE32)
			R15 -= ((~(off | 0xfc000000u)) + 1) - 8;
		//else
		//	R15 = ((R15 - (((~(off | 0xfc000000u)) + 1) - 8)) & 0x03FFFFFC) | (R15 & ~0x03FFFFFC);
	}
	else
	{
		//if (MODE32)
			R15 += off + 8;
		//else
		//	R15 = ((R15 + (off + 8)) & 0x03FFFFFC) | (R15 & ~0x03FFFFFC);
	}

	ARM7_ICOUNT -= 3;
}

void arm7_cpu_device::HandleBranchHBit(const uint32_t insn)
{
	uint32_t off = (insn & INSN_BRANCH) << 2;

	// H goes to bit1
	off |= (insn & 0x01000000) >> 23;

	/* Save PC into LR if this is a branch with link or a BLX */
	if ((insn & INSN_BL) || ((insn & 0xfe000000) == 0xfa000000))
	{
		SetRegister(14, R15 + 4);
	}

	/* Sign-extend the 24-bit offset in our calculations */
	if (off & 0x2000000u)
	{
		//if (MODE32)
			R15 -= ((~(off | 0xfc000000u)) + 1) - 4;
		//else
		//	R15 = ((R15 - (((~(off | 0xfc000000u)) + 1) - 8)) & 0x03FFFFFC) | (R15 & ~0x03FFFFFC);
	}
	else
	{
		//if (MODE32)
			R15 += off + 4;
		//else
		//	R15 = ((R15 + (off + 8)) & 0x03FFFFFC) | (R15 & ~0x03FFFFFC);
	}

	if (!m_tflag)
		set_mode_changed();
	m_tflag = 1;

	ARM7_ICOUNT -= 2;
}

template <arm7_cpu_device::copro_mode MMU,
		  arm7_cpu_device::imm_mode IMMEDIATE,
		  arm7_cpu_device::index_mode PRE_INDEX,
		  arm7_cpu_device::offset_mode OFFSET_UP,
		  arm7_cpu_device::size_mode SIZE_BYTE,
		  arm7_cpu_device::writeback_mode WRITEBACK,
		  arm7_cpu_device::load_mode LOAD>
void arm7_cpu_device::HandleMemSingle(const uint32_t insn)
{
	uint32_t rnv, off, rd, rnv_old = 0;

	/* Fetch the offset */
	if (IMMEDIATE)
	{
		/* Register Shift */
		off = decodeShift(insn);
	}
	else
	{
		/* Immediate Value */
		off = insn & INSN_SDT_IMM;
	}

	/* Calculate Rn, accounting for PC */
	uint32_t rn = (insn & INSN_RN) >> INSN_RN_SHIFT;
	uint32_t cycles = 3;
	if (PRE_INDEX)
	{
		/* Pre-indexed addressing */
		if (OFFSET_UP)
		{
			//if ((MODE32) || (rn != eR15))
				rnv = (GetRegister(rn) + off);
			//else
			//	rnv = (GET_PC + off);
		}
		else
		{
			//if ((MODE32) || (rn != eR15))
				rnv = (GetRegister(rn) - off);
			//else
			//	rnv = (GET_PC - off);
		}

		if (WRITEBACK)
		{
			rnv_old = GetRegister(rn);
			SetRegister(rn, rnv);

	// check writeback???
		}
		else if (rn == eR15)
		{
			rnv = rnv + 8;
		}
	}
	else
	{
		/* Post-indexed addressing */
		if (rn == eR15)
		{
			//if (MODE32)
				rnv = R15 + 8;
			//else
			//	rnv = GET_PC + 8;
		}
		else
		{
			rnv = GetRegister(rn);
		}
	}

	/* Do the transfer */
	rd = (insn & INSN_RD) >> INSN_RD_SHIFT;
	if (LOAD)
	{
		/* Load */
		if (SIZE_BYTE)
		{
			if (MMU)
			{
				const uint32_t data = arm7_cpu_read8_mmu(rnv);
				if (!m_pendingAbtD)
				{
					SetRegister(rd, data);
				}
			}
			else
			{
				SetRegister(rd, arm7_cpu_read8(rnv));
			}
		}
		else
		{
			uint32_t data;
			if (MMU)
				data = arm7_cpu_read32_mmu(rnv);
			else
				data = arm7_cpu_read32(rnv);
			if (!MMU || !m_pendingAbtD)
			{
				if (rd == eR15)
				{
					//if (MODE32)
						R15 = data - 4;
					//else
					//	R15 = (R15 & ~0x03FFFFFC) /* N Z C V I F M1 M0 */ | ((data - 4) & 0x03FFFFFC);
					// LDR, PC takes 2S + 2N + 1I (5 total cycles)
					cycles = 5;
					if ((data & 1) && m_archRev >= 5)
					{
						if (!m_tflag)
							set_mode_changed();
						m_tflag = 1;
						R15--;
					}
				}
				else
				{
					SetRegister(rd, data);
				}
			}
		}
	}
	else
	{
		/* Store */
		if (SIZE_BYTE)
		{
#if ARM7_DEBUG_CORE
				if (rd == eR15)
					LOG(("Wrote R15 in byte mode\n"));
#endif
			if (MMU)
				arm7_cpu_write8_mmu(rnv, (uint8_t) GetRegister(rd) & 0xffu);
			else
				arm7_cpu_write8(rnv, (uint8_t) GetRegister(rd) & 0xffu);
		}
		else
		{
#if ARM7_DEBUG_CORE
				if (rd == eR15)
					LOG(("Wrote R15 in 32bit mode\n"));
#endif

			//WRITE32(rnv, rd == eR15 ? R15 + 8 : GetRegister(rd));
			if (MMU)
				arm7_cpu_write32_mmu(rnv, rd == eR15 ? R15 + 8 + 4 : GetRegister(rd)); // manual says STR rd = PC, +12
			else
				arm7_cpu_write32(rnv, rd == eR15 ? R15 + 8 + 4 : GetRegister(rd)); // manual says STR rd = PC, +12
		}
		// Store takes only 2 N Cycles, so add + 1
		cycles = 2;
	}

	if (m_pendingAbtD)
	{
		if (PRE_INDEX && WRITEBACK)
		{
			SetRegister(rn, rnv_old);
		}
	}
	else
	{
		/* Do post-indexing writeback */
		if (!PRE_INDEX/* && WRITEBACK*/)
		{
			if (OFFSET_UP)
			{
				/* Writeback is applied in pipeline, before value is read from mem,
				    so writeback is effectively ignored */
				if (rd == rn)
				{
					SetRegister(rn, GetRegister(rd));
					// todo: check for offs... ?
				}
				else
				{
					SetRegister(rn, (rnv + off));
				}
			}
			else
			{
				/* Writeback is applied in pipeline, before value is read from mem,
			    	so writeback is effectively ignored */
				if (rd == rn)
				{
					SetRegister(rn, GetRegister(rd));
				}
				else
				{
					SetRegister(rn, (rnv - off));
				}
			}
		}
	}

	//  arm7_check_irq_state();
	ARM7_ICOUNT -= cycles;

	R15 += 4;
}

template <arm7_cpu_device::copro_mode MMU,
		  arm7_cpu_device::index_mode PRE_INDEX,
          arm7_cpu_device::offset_mode OFFSET_UP,
          arm7_cpu_device::writeback_mode WRITEBACK,
          arm7_cpu_device::load_mode LOAD>
void arm7_cpu_device::HandleHalfWordDT(const uint32_t insn)
{
	uint32_t rn, rnv, off, rd, rnv_old = 0;

	// Immediate or Register Offset?
	if (insn & 0x400000) {               // Bit 22 - 1 = immediate, 0 = register
		// imm. value in high nibble (bits 8-11) and lo nibble (bit 0-3)
		off = (((insn >> 8) & 0x0f) << 4) | (insn & 0x0f);
	}
	else {
		// register
		off = GetRegister(insn & 0x0f);
	}

	/* Calculate Rn, accounting for PC */
	rn = (insn & INSN_RN) >> INSN_RN_SHIFT;

	uint32_t cycles = 3;

	if (PRE_INDEX)
	{
		/* Pre-indexed addressing */
		if (OFFSET_UP)
		{
			rnv = (GetRegister(rn) + off);
		}
		else
		{
			rnv = (GetRegister(rn) - off);
		}

		if (WRITEBACK)
		{
			rnv_old = GetRegister(rn);
			SetRegister(rn, rnv);

		// check writeback???
		}
		else if (rn == eR15)
		{
			rnv = (rnv) + 8;
		}
	}
	else
	{
		/* Post-indexed addressing */
		if (rn == eR15)
		{
			rnv = R15 + 8;
		}
		else
		{
			rnv = GetRegister(rn);
		}
	}

	/* Do the transfer */
	rd = (insn & INSN_RD) >> INSN_RD_SHIFT;

	/* Load */
	if (LOAD)
	{
		// Signed?
		if (insn & 0x40)
		{
			uint32_t newval;

			if (insn & 0x20)
			{
				// Signed Half Word
				int32_t data;
				if (MMU)
					data = (int32_t)(int16_t)(uint16_t)arm7_cpu_read16_mmu(rnv & ~1);
				else
					data = (int32_t)(int16_t)(uint16_t)arm7_cpu_read16(rnv & ~1);
				if ((rnv & 1) && m_archRev < 5)
					data >>= 8;
				newval = (uint32_t)data;
			}
			else
			{
				// Signed Byte
				if (MMU)
				{
					uint8_t databyte = arm7_cpu_read8_mmu(rnv) & 0xff;
					newval = (uint32_t)(int32_t)(int8_t)databyte;
				}
				else
				{
					uint8_t databyte = arm7_cpu_read8(rnv) & 0xff;
					newval = (uint32_t)(int32_t)(int8_t)databyte;
				}
			}

			if (!m_pendingAbtD)
			{
			// PC?
			if (rd == eR15)
			{
				R15 = newval + 8;
				// LDR(H,SH,SB) PC takes 2S + 2N + 1I (5 total cycles)
				cycles = 5;

			}
			else
			{
				SetRegister(rd, newval);
				R15 += 4;
			}

			}
			else
			{
				R15 += 4;
			}

		}
		// Unsigned Half Word
		else
		{
			uint32_t newval;
			if (MMU)
				newval = arm7_cpu_read16_mmu(rnv);
			else
				newval = arm7_cpu_read16(rnv);

			if (!MMU || !m_pendingAbtD)
			{
				if (rd == eR15)
				{
					R15 = newval + 8;
					// extra cycles for LDR(H,SH,SB) PC (5 total cycles)
					cycles = 5;
				}
				else
				{
					SetRegister(rd, newval);
					R15 += 4;
				}
			}
			else
			{
				R15 += 4;
			}

		}


	}
	/* Store or ARMv5+ dword insns */
	else
	{
		if ((insn & 0x60) == 0x40)  // LDRD
		{
			if (MMU)
			{
				SetRegister(rd, arm7_cpu_read32_mmu(rnv));
				SetRegister(rd+1, arm7_cpu_read32_mmu(rnv+4));
			}
			else
			{
				SetRegister(rd, arm7_cpu_read32(rnv));
				SetRegister(rd+1, arm7_cpu_read32(rnv+4));
			}
			R15 += 4;
		}
		else if ((insn & 0x60) == 0x60) // STRD
		{
			if (MMU)
			{
				arm7_cpu_write32_mmu(rnv, GetRegister(rd));
				arm7_cpu_write32_mmu(rnv+4, GetRegister(rd+1));
			}
			else
			{
				arm7_cpu_write32(rnv, GetRegister(rd));
				arm7_cpu_write32(rnv+4, GetRegister(rd+1));
			}
			R15 += 4;
		}
		else
		{
			// WRITE16(rnv, rd == eR15 ? R15 + 8 : GetRegister(rd));
			if (MMU)
				arm7_cpu_write16_mmu(rnv, rd == eR15 ? R15 + 8 + 4 : GetRegister(rd)); // manual says STR RD=PC, +12 of address
			else
				arm7_cpu_write16(rnv, rd == eR15 ? R15 + 8 + 4 : GetRegister(rd)); // manual says STR RD=PC, +12 of address

// if R15 is not increased then e.g. "STRH R10, [R15,#$10]" will be executed over and over again
#if 0
			if (rn != eR15)
#endif
			R15 += 4;

			// STRH takes 2 cycles, so we add + 1
			cycles = 2;
		}
	}

	if (m_pendingAbtD)
	{
		if (PRE_INDEX && WRITEBACK)
		{
			SetRegister(rn, rnv_old);
		}
	}
	else
	{
		// SJE: No idea if this writeback code works or makes sense here..
		/* Do post-indexing writeback */
		if (!PRE_INDEX/* && WRITEBACK*/)
		{
			if (OFFSET_UP)
			{
				/* Writeback is applied in pipeline, before value is read from mem,
					so writeback is effectively ignored */
				if (rd == rn)
				{
					SetRegister(rn, GetRegister(rd));
					// todo: check for offs... ?
				}
				else
				{
					SetRegister(rn, (rnv + off));
				}
			}
			else
			{
				/* Writeback is applied in pipeline, before value is read from mem,
					so writeback is effectively ignored */
				if (rd == rn)
				{
					SetRegister(rn, GetRegister(rd));
				}
				else
				{
					SetRegister(rn, (rnv - off));
				}
			}
		}
	}
	ARM7_ICOUNT -= cycles;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::HandleSwap(const uint32_t insn)
{
	const uint32_t rn = GetRegister((insn >> 16) & 0xf);  // reg. w/read address
	const uint32_t rm = GetRegister(insn & 0xf);          // reg. w/write address
	const uint32_t rd = (insn >> 12) & 0xf;                // dest reg

#if ARM7_DEBUG_CORE
	if (rn == 15 || rm == 15 || rd == 15)
		LOG(("%08x: Illegal use of R15 in Swap Instruction\n", R15));
#endif

	// can be byte or word
	if (insn & 0x400000)
	{
		if (MMU)
		{
			const uint32_t tmp = arm7_cpu_read8_mmu(rn);
			arm7_cpu_write8_mmu(rn, rm);
			SetRegister(rd, tmp);
		}
		else
		{
			const uint32_t tmp = arm7_cpu_read8(rn);
			arm7_cpu_write8(rn, rm);
			SetRegister(rd, tmp);
		}
	}
	else
	{
		if (MMU)
		{
			const uint32_t tmp = arm7_cpu_read32_mmu(rn);
			arm7_cpu_write32_mmu(rn, rm);
			SetRegister(rd, tmp);
		}
		else
		{
			const uint32_t tmp = arm7_cpu_read32(rn);
			arm7_cpu_write32(rn, rm);
			SetRegister(rd, tmp);
		}
	}

	R15 += 4;
	// Instruction takes 1S+2N+1I cycles - so we subtract one more..
	ARM7_ICOUNT -= 4;
}

void arm7_cpu_device::HandlePSRTransfer(const uint32_t insn)
{
	uint32_t val;
	const uint32_t oldmode = m_mode;

	// get old value of CPSR/SPSR
	uint32_t newval = (insn & 0x400000) ? GetRegister(SPSR) : make_cpsr();

	ARM7_ICOUNT -= 1;

	// MSR (bit 21 set) - Copy value to CPSR/SPSR
	if (insn & 0x00200000)
	{
		// Immediate Value?
		if (insn & INSN_I) {
			// Value can be specified for a Right Rotate, 2x the value specified.
			uint32_t by = (insn & INSN_OP2_ROTATE) >> INSN_OP2_ROTATE_SHIFT;
			if (by)
				val = ROR(insn & INSN_OP2_IMM, by);
			else
				val = insn & INSN_OP2_IMM;
		}
		// Value from Register
		else
		{
			val = GetRegister(insn & 0x0f);
		}

		// apply field code bits
		if (!(insn & 0x400000))
		{
			if (oldmode != eARM7_MODE_USER)
			{
				if (insn & 0x00010000)
				{
					newval = (newval & 0xffffff00) | (val & 0x000000ff);
				}
				if (insn & 0x00020000)
				{
					newval = (newval & 0xffff00ff) | (val & 0x0000ff00);
				}
				if (insn & 0x00040000)
				{
					newval = (newval & 0xff00ffff) | (val & 0x00ff0000);
				}
			}

			// status flags can be modified regardless of mode
			if (insn & 0x00080000)
			{
				// TODO for non ARMv5E mask should be 0xf0000000 (ie mask Q bit)
				newval = (newval & 0x00ffffff) | (val & 0xf8000000);
			}
		}
		else    // SPSR has stricter requirements
		{
			if (((GET_CPSR & 0x1f) > 0x10) && ((GET_CPSR & 0x1f) < 0x1f))
			{
				if (insn & 0x00010000)
				{
					newval = (newval & 0xffffff00) | (val & 0xff);
				}
				if (insn & 0x00020000)
				{
					newval = (newval & 0xffff00ff) | (val & 0xff00);
				}
				if (insn & 0x00040000)
				{
					newval = (newval & 0xff00ffff) | (val & 0xff0000);
				}
				if (insn & 0x00080000)
				{
					// TODO for non ARMv5E mask should be 0xf0000000 (ie mask Q bit)
					newval = (newval & 0x00ffffff) | (val & 0xf8000000);
				}
			}
		}

		// Update the Register
		if (!(insn & 0x400000))
		{
			const uint32_t old_t = m_tflag;
			set_cpsr(newval);
			if (m_tflag != old_t)
				set_mode_changed();
		}
		else
		{
			SetRegister(SPSR, newval);
		}

		// Switch to new mode if changed
		//if ((newval & MODE_FLAG) != oldmode)
		//{
		//	SwitchMode(GET_MODE);
		//}
	}
	// MRS (bit 21 clear) - Copy CPSR or SPSR to specified Register
	else
	{
		SetRegister((insn >> 12)& 0x0f, (insn & 0x400000) ? GetRegister(SPSR) : make_cpsr());
	}

	R15 += 4;
}

template <arm7_cpu_device::imm_mode IMMEDIATE, arm7_cpu_device::flags_mode SET_FLAGS, arm7_cpu_device::alu_mode OPCODE>
void arm7_cpu_device::HandleALU(const uint32_t insn)
{
	uint32_t op2, sc = 0;

	// Normal Data Processing : 1S
	// Data Processing with register specified shift : 1S + 1I
	// Data Processing with PC written : 2S + 1N
	// Data Processing with register specified shift and PC written : 2S + 1N + 1I

	const uint32_t opcode = (insn & INSN_OPCODE) >> INSN_OPCODE_SHIFT;

	uint32_t rd = 0;
	uint32_t rn = 0;

	uint32_t cycles = 1;

	/* --------------*/
	/* Construct Op2 */
	/* --------------*/

	/* Immediate constant */
	if (IMMEDIATE)
	{
		uint32_t by = (insn & INSN_OP2_ROTATE) >> INSN_OP2_ROTATE_SHIFT;
		if (by)
		{
			op2 = ROR(insn & INSN_OP2_IMM, by);
			sc = op2 & SIGN_BIT;
		}
		else
		{
			op2 = insn & INSN_OP2;      // SJE: Shouldn't this be INSN_OP2_IMM?
			sc = m_cflag;
		}
	}
	/* Op2 = Register Value */
	else
	{
		if (SET_FLAGS)
			op2 = decodeShiftWithCarry(insn, &sc);
		else
			op2 = decodeShift(insn);

		// extra cycle (register specified shift)
		cycles++;
	}

	// LD TODO this comment is wrong
	/* Calculate Rn to account for pipelining */
	if ((opcode & 0xd) != 0xd) /* No Rn in MOV */
	{
		rn = (insn & INSN_RN) >> INSN_RN_SHIFT;
		if (rn == eR15)
		{
#if ARM7_DEBUG_CORE
			LOG(("%08x:  Pipelined R15 (Shift %d)\n", R15, (insn & INSN_I ? 8 : insn & 0x10u ? 12 : 12)));
#endif
			//if (MODE32)
				rn = R15 + 8;
			//else
			//	rn = GET_PC + 8;
		}
		else
		{
			rn = GetRegister(rn);
		}
	}

	/* Perform the operation */
	if (OPCODE == OPCODE_AND) // 0
	{
		rd = rn & op2;
		HandleALULogicalFlags(rd, sc);
	}
	else if (OPCODE == OPCODE_EOR) // 1
	{
		rd = rn ^ op2;
		HandleALULogicalFlags(rd, sc);
	}
	else if (OPCODE == OPCODE_SUB) // 2
	{
		rd = (rn - op2);
		HandleALUSubFlags(rd, rn, op2);
	}
	else if (OPCODE == OPCODE_RSB) // 3
	{
		rd = (op2 - rn);
		HandleALUSubFlags(rd, op2, rn);
	}
	else if (OPCODE == OPCODE_ADD) // 4
	{
		rd = (rn + op2);
		HandleALUAddFlags(rd, rn, op2);
	}
	else if (OPCODE == OPCODE_ADC) // 5
	{
		rd = (rn + op2 + m_cflag);
		HandleALUAddFlags(rd, rn, op2);
	}
	else if (OPCODE == OPCODE_SBC) // 6
	{
		rd = (rn - op2 - (1 - m_cflag));
		HandleALUSubFlags(rd, rn, op2);
	}
	else if (OPCODE == OPCODE_RSC) // 7
	{
		rd = (op2 - rn - (1 - m_cflag));
		HandleALUSubFlags(rd, op2, rn);
	}
	else if (OPCODE == OPCODE_TST) // 8
	{
		rd = rn & op2;
		HandleALULogicalFlags(rd, sc);
	}
	else if (OPCODE == OPCODE_TEQ) // 9
	{
		rd = rn ^ op2;
		HandleALULogicalFlags(rd, sc);
	}
	else if (OPCODE == OPCODE_CMP) // A
	{
		rd = (rn - op2);
		HandleALUSubFlags(rd, rn, op2);
	}
	else if (OPCODE == OPCODE_CMN) // B
	{
		rd = (rn + op2);
		HandleALUAddFlags(rd, rn, op2);
	}
	else if (OPCODE == OPCODE_ORR) // C
	{
		rd = rn | op2;
		HandleALULogicalFlags(rd, sc);
	}
	else if (OPCODE == OPCODE_MOV) // D
	{
		rd = op2;
		HandleALULogicalFlags(rd, sc);
	}
	else if (OPCODE == OPCODE_BIC) // E
	{
		rd = rn & ~op2;
		HandleALULogicalFlags(rd, sc);
	}
	else if (OPCODE == OPCODE_MVN) // F
	{
		rd = (~op2);
		HandleALULogicalFlags(rd, sc);
	}

	/* Put the result in its register if not one of the test only opcodes (TST,TEQ,CMP,CMN) */
	uint32_t rdn = (insn & INSN_RD) >> INSN_RD_SHIFT;
	if ((opcode & 0xc) != 0x8)
	{
		if (rdn == eR15)
		{
			if (!SET_FLAGS)
			{
				//if (MODE32)
					R15 = rd;
				//else
				//	R15 = (R15 & ~0x03FFFFFC) | (rd & 0x03FFFFFC);
				cycles += 2;
			}
			else
			{
				//if (MODE32)
				//{
					if (GET_MODE != eARM7_MODE_USER)
					{
						uint32_t old_t = m_tflag;
						set_cpsr(GetRegister(SPSR));
						if (m_tflag != old_t)
							set_mode_changed();
						//SwitchMode(GET_MODE);
					}

					R15 = rd;
				//}
				//else
				//{
				//	uint32_t temp;
				//	R15 = rd; //(R15 & 0x03FFFFFC) | (rd & 0xFC000003);
				//	temp = (GET_CPSR & 0x0FFFFF20) | (rd & 0xF0000000) /* N Z C V */ | ((rd & 0x0C000000) >> (26 - 6)) /* I F */ | (rd & 0x00000003) /* M1 M0 */;
				//	set_cpsr(temp);
				//	SwitchMode(temp & 3);
				//}

				// extra cycles (PC written)
				cycles += 2;

				/* IRQ masks may have changed in this instruction */
//              arm7_check_irq_state();
			}
		}
		else
		{
			/* Rd != R15 - Write results to register & update CPSR (which was already handled using HandleALU flag macros) */
			SetRegister(rdn, rd);
		}
	}
	// SJE: Don't think this applies any more.. (see page 44 at bottom)
	/* TST & TEQ can affect R15 (the condition code register) with the S bit set */
	else if (rdn == eR15)
	{
		if (SET_FLAGS)
		{
			//if (MODE32)
			//{
				R15 = rd;
			//}
			//else
			//{
			//	uint32_t temp;
			//	R15 = (R15 & 0x03FFFFFC) | (rd & ~0x03FFFFFC);
			//	temp = (GET_CPSR & 0x0FFFFF20) | (rd & 0xF0000000) /* N Z C V */ | ((rd & 0x0C000000) >> (26 - 6)) /* I F */ | (rd & 0x00000003) /* M1 M0 */;
			//	set_cpsr(temp);
			//	SwitchMode(temp & 3);
			//}

			/* IRQ masks may have changed in this instruction */
//          arm7_check_irq_state();
		}
		// extra cycles (PC written)
		cycles += 2;
	}

	ARM7_ICOUNT -= cycles;
}

template <arm7_cpu_device::flags_mode SET_FLAGS, arm7_cpu_device::accum_mode ACCUM>
void arm7_cpu_device::HandleMul(const uint32_t insn)
{
	// MUL takes 1S + mI and MLA 1S + (m+1)I cycles to execute, where S and I are as
	// defined in 6.2 Cycle Types on page 6-2.
	// m is the number of 8 bit multiplier array cycles required to complete the
	// multiply, which is controlled by the value of the multiplier operand
	// specified by Rs.

	const uint32_t rm = GetRegister(insn & INSN_MUL_RM);
	uint32_t rs = GetRegister((insn & INSN_MUL_RS) >> INSN_MUL_RS_SHIFT);
	const uint32_t rd = (insn & INSN_MUL_RD) >> INSN_MUL_RD_SHIFT;

	/* Do the basic multiply of Rm and Rs */
	uint32_t r = rm * rs;

#if ARM7_DEBUG_CORE
	if ((insn & INSN_MUL_RM) == 0xf ||
		((insn & INSN_MUL_RS) >> INSN_MUL_RS_SHIFT) == 0xf ||
		((insn & INSN_MUL_RN) >> INSN_MUL_RN_SHIFT) == 0xf)
		LOG(("%08x:  R15 used in mult\n", R15));
#endif

	/* Add on Rn if this is a MLA */
	if (ACCUM)
	{
		const uint32_t rn = (insn & INSN_MUL_RN) >> INSN_MUL_RN_SHIFT;
		r += GetRegister(rn);
		// extra cycle for MLA
		ARM7_ICOUNT -= 1;
	}

	/* Write the result */
	SetRegister(rd, r);

	/* Set N and Z if asked */
	if (SET_FLAGS)
	{
		HandleALUNZFlags(r);
	}

	if (rs & SIGN_BIT) rs = -rs;
	if (rs < 0x00000100) ARM7_ICOUNT -= 1 + 1;
	else if (rs < 0x00010000) ARM7_ICOUNT -= 1 + 2;
	else if (rs < 0x01000000) ARM7_ICOUNT -= 1 + 3;
	else ARM7_ICOUNT -= 1 + 4;

	R15 += 4;
}

// todo: add proper cycle counts
template <arm7_cpu_device::flags_mode SET_FLAGS, arm7_cpu_device::accum_mode ACCUM>
void arm7_cpu_device::HandleSMulLong(const uint32_t insn)
{
	// MULL takes 1S + (m+1)I and MLAL 1S + (m+2)I cycles to execute, where m is the
	// number of 8 bit multiplier array cycles required to complete the multiply, which is
	// controlled by the value of the multiplier operand specified by Rs.

	const int32_t rm  = (int32_t)GetRegister(insn & 0xf);
	int32_t rs  = (int32_t)GetRegister(((insn >> 8) & 0xf));
	const uint32_t rhi = (insn >> 16) & 0xf;
	const uint32_t rlo = (insn >> 12) & 0xf;

#if ARM7_DEBUG_CORE
		if ((insn & 0xf) == 15 || ((insn >> 8) & 0xf) == 15 || ((insn >> 16) & 0xf) == 15 || ((insn >> 12) & 0xf) == 15)
			LOG(("%08x: Illegal use of PC as a register in SMULL opcode\n", R15));
#endif

	/* Perform the multiplication */
	int64_t res = (int64_t)rm * rs;

	/* Add on Rn if this is a MLA */
	if (ACCUM)
	{
		int64_t acum = (int64_t)((((int64_t)(GetRegister(rhi))) << 32) | GetRegister(rlo));
		res += acum;
		// extra cycle for MLA
		ARM7_ICOUNT -= 1;
	}

	/* Write the result (upper dword goes to RHi, lower to RLo) */
	SetRegister(rhi, res >> 32);
	SetRegister(rlo, res & 0xFFFFFFFF);

	/* Set N and Z if asked */
	if (SET_FLAGS)
	{
		HandleLongALUNZFlags(res);
	}

	if (rs < 0) rs = -rs;
	if (rs < 0x00000100) ARM7_ICOUNT -= 1 + 1 + 1;
	else if (rs < 0x00010000) ARM7_ICOUNT -= 1 + 2 + 1;
	else if (rs < 0x01000000) ARM7_ICOUNT -= 1 + 3 + 1;
	else ARM7_ICOUNT -= 1 + 4 + 1;

	R15 += 4;
}

// todo: add proper cycle counts
template <arm7_cpu_device::flags_mode SET_FLAGS, arm7_cpu_device::accum_mode ACCUM>
void arm7_cpu_device::HandleUMulLong(const uint32_t insn)
{
	// MULL takes 1S + (m+1)I and MLAL 1S + (m+2)I cycles to execute, where m is the
	// number of 8 bit multiplier array cycles required to complete the multiply, which is
	// controlled by the value of the multiplier operand specified by Rs.

	const uint32_t rm  = (int32_t)GetRegister(insn & 0xf);
	const uint32_t rs  = (int32_t)GetRegister(((insn >> 8) & 0xf));
	const uint32_t rhi = (insn >> 16) & 0xf;
	const uint32_t rlo = (insn >> 12) & 0xf;

#if ARM7_DEBUG_CORE
		if (((insn & 0xf) == 15) || (((insn >> 8) & 0xf) == 15) || (((insn >> 16) & 0xf) == 15) || (((insn >> 12) & 0xf) == 15))
			LOG(("%08x: Illegal use of PC as a register in MULL opcode\n", R15));
#endif

	/* Perform the multiplication */
	uint64_t res = (uint64_t)rm * rs;

	/* Add on Rn if this is a MLA */
	if (ACCUM)
	{
		uint64_t acum = (uint64_t)((((uint64_t)(GetRegister(rhi))) << 32) | GetRegister(rlo));
		res += acum;
		// extra cycle for MLA
		ARM7_ICOUNT -= 1;
	}

	/* Write the result (upper dword goes to RHi, lower to RLo) */
	SetRegister(rhi, res >> 32);
	SetRegister(rlo, res & 0xFFFFFFFF);

	/* Set N and Z if asked */
	if (SET_FLAGS)
	{
		HandleLongALUNZFlags(res);
	}

	if (rs < 0x00000100) ARM7_ICOUNT -= 1 + 1 + 1;
	else if (rs < 0x00010000) ARM7_ICOUNT -= 1 + 2 + 1;
	else if (rs < 0x01000000) ARM7_ICOUNT -= 1 + 3 + 1;
	else ARM7_ICOUNT -= 1 + 4 + 1;

	R15 += 4;
}

template <arm7_cpu_device::copro_mode MMU,
          arm7_cpu_device::index_mode PRE_INDEX,
          arm7_cpu_device::offset_mode OFFSET_UP,
          arm7_cpu_device::bdt_s_bit S_BIT,
          arm7_cpu_device::writeback_mode WRITEBACK>
void arm7_cpu_device::HandleMemBlock(const uint32_t insn)
{
	uint32_t rb = (insn & INSN_RN) >> INSN_RN_SHIFT;
	uint32_t rbp = GetRegister(rb);
	int result;

#if ARM7_DEBUG_CORE
	if (rbp & 3)
		LOG(("%08x: Unaligned Mem Transfer @ %08x\n", R15, rbp));
#endif

	// Normal LDM instructions take nS + 1N + 1I and LDM PC takes (n+1)S + 2N + 1I
	// incremental cycles, where S,N and I are as defined in 6.2 Cycle Types on page 6-2.
	// STM instructions take (n-1)S + 2N incremental cycles to execute, where n is the
	// number of words transferred.

	if (insn & INSN_BDT_L)
	{
		/* Loading */
		if (OFFSET_UP)
		{
			/* Incrementing */
			if (!PRE_INDEX)
			{
				rbp = rbp + (- 4);
			}

			// S Flag Set, but R15 not in list = User Bank Transfer
			if (S_BIT && (insn & 0x8000) == 0)
			{
				// !! actually switching to user mode triggers a section permission fault in Happy Fish 302-in-1 (BP C0030DF4, press F5 ~16 times) !!
				// set to user mode - then do the transfer, and set back
				//int curmode = GET_MODE;
				//SwitchMode(eARM7_MODE_USER);
				LOG(("%08x: User Bank Transfer not fully tested - please check if working properly!\n", R15));
				result = loadInc<MMU, S_BIT, USER_MODE>(insn, rbp);
				// todo - not sure if Writeback occurs on User registers also..
				//SwitchMode(curmode);
			}
			else
				result = loadInc<MMU, S_BIT, DEFAULT_MODE>(insn, rbp);

			if (WRITEBACK && !m_pendingAbtD)
			{
#if ARM7_DEBUG_CORE
					if (rb == 15)
						LOG(("%08x:  Illegal LDRM writeback to r15\n", R15));
#endif
				// "A LDM will always overwrite the updated base if the base is in the list." (also for a user bank transfer?)
				// GBA "V-Rally 3" expects R0 not to be overwritten with the updated base value [BP 8077B0C]
				if (((insn >> rb) & 1) == 0)
				{
					SetRegister(rb, GetRegister(rb) + result * 4);
				}
			}

			// R15 included? (NOTE: CPSR restore must occur LAST otherwise wrong registers restored!)
			if ((insn & 0x8000) && !m_pendingAbtD)
			{
				R15 -= 4;     // SJE: I forget why i did this?
				// S - Flag Set? Signals transfer of current mode SPSR->CPSR
				if (S_BIT)
				{
					//if (MODE32)
					//{
						const uint32_t old_t = m_tflag;
						set_cpsr(GetRegister(SPSR));
						if (m_tflag != old_t)
							set_mode_changed();
						//SwitchMode(GET_MODE);
					//}
					//else
					//{
					//	uint32_t temp;
//                      LOG(("LDM + S | R15 %08X CPSR %08X\n", R15, GET_CPSR));
					//	temp = (GET_CPSR & 0x0FFFFF20) | (R15 & 0xF0000000) /* N Z C V */ | ((R15 & 0x0C000000) >> (26 - 6)) /* I F */ | (R15 & 0x00000003) /* M1 M0 */;
					//	set_cpsr( temp);
					//	SwitchMode(temp & 3);
					//}
				}
				else
				{
					if ((R15 & 1) && m_archRev >= 5)
					{
						if (!m_tflag)
							set_mode_changed();
						m_tflag = 1;
						R15--;
					}
				}
				// LDM PC - takes 2 extra cycles
				result += 2;
			}
		}
		else
		{
			/* Decrementing */
			if (!PRE_INDEX)
			{
				rbp = rbp - (- 4);
			}

			// S Flag Set, but R15 not in list = User Bank Transfer
			if (S_BIT && ((insn & 0x8000) == 0))
			{
				// set to user mode - then do the transfer, and set back
				//int curmode = GET_MODE;
				//SwitchMode(eARM7_MODE_USER);
				LOG(("%08x: User Bank Transfer not fully tested - please check if working properly!\n", R15));
				result = loadDec<MMU, S_BIT, USER_MODE>(insn, rbp);
				// todo - not sure if Writeback occurs on User registers also..
				//SwitchMode(curmode);
			}
			else
			{
				result = loadDec<MMU, S_BIT, DEFAULT_MODE>(insn, rbp);
			}

			if (WRITEBACK && !m_pendingAbtD)
			{
				if (rb == 0xf)
				{
					LOG(("%08x:  Illegal LDRM writeback to r15\n", R15));
				}
				// "A LDM will always overwrite the updated base if the base is in the list." (also for a user bank transfer?)
				if (((insn >> rb) & 1) == 0)
				{
					SetRegister(rb, GetRegister(rb) - result * 4);
				}
			}

			// R15 included? (NOTE: CPSR restore must occur LAST otherwise wrong registers restored!)
			if ((insn & 0x8000) && !m_pendingAbtD)
			{
				R15 -= 4;     // SJE: I forget why i did this?
				// S - Flag Set? Signals transfer of current mode SPSR->CPSR
				if (S_BIT)
				{
					//if (MODE32)
					//{
						const uint32_t old_t = m_tflag;
						set_cpsr(GetRegister(SPSR));
						if (m_tflag != old_t)
							set_mode_changed();
						//SwitchMode(GET_MODE);
					//}
					//else
					//{
					//	uint32_t temp;
//                      LOG(("LDM + S | R15 %08X CPSR %08X\n", R15, GET_CPSR));
					//	temp = (GET_CPSR & 0x0FFFFF20) /* N Z C V I F M4 M3 M2 M1 M0 */ | (R15 & 0xF0000000) /* N Z C V */ | ((R15 & 0x0C000000) >> (26 - 6)) /* I F */ | (R15 & 0x00000003) /* M1 M0 */;
					//	set_cpsr(temp);
					//	SwitchMode(temp & 3);
					//}
				}
				else
				{
					if ((R15 & 1) && m_archRev >= 5)
					{
						if (!m_tflag)
							set_mode_changed();
						m_tflag = 1;
						R15--;
					}
				}
				// LDM PC - takes 2 extra cycles
				result += 2;
			}
		}
		// LDM (NO PC) takes (n)S + 1N + 1I cycles (n = # of register transfers)
		ARM7_ICOUNT -= result + 1 + 1;
	} /* Loading */
	else
	{
		/* Storing - STM */
		if (insn & (1 << eR15))
		{
#if ARM7_DEBUG_CORE
				LOG(("%08x: Writing R15 in strm\n", R15));
#endif
			/* special case handling if writing to PC */
			R15 += 12;
		}
		if (OFFSET_UP)
		{
			/* Incrementing */
			if (!PRE_INDEX)
			{
				rbp = rbp + (- 4);
			}

			// S Flag Set = User Bank Transfer
			if (S_BIT)
			{
				// todo: needs to be tested..

				// set to user mode - then do the transfer, and set back
				//int curmode = GET_MODE;
				//SwitchMode(eARM7_MODE_USER);
				LOG(("%08x: User Bank Transfer not fully tested - please check if working properly!\n", R15));
				result = storeInc<MMU, USER_MODE>(insn, rbp);
				// todo - not sure if Writeback occurs on User registers also..
				//SwitchMode(curmode);
			}
			else
			{
				result = storeInc<MMU, DEFAULT_MODE>(insn, rbp);
			}

			if (WRITEBACK && !m_pendingAbtD)
			{
				SetRegister(rb, GetRegister(rb) + result * 4);
			}
		}
		else
		{
			/* Decrementing - but real CPU writes in incrementing order */
			if (!PRE_INDEX)
			{
				rbp = rbp - (-4);
			}

			// S Flag Set = User Bank Transfer
			if (S_BIT)
			{
				// set to user mode - then do the transfer, and set back
				//int curmode = GET_MODE;
				//SwitchMode(eARM7_MODE_USER);
				LOG(("%08x: User Bank Transfer not fully tested - please check if working properly!\n", R15));
				result = storeDec<MMU, USER_MODE>(insn, rbp);
				// todo - not sure if Writeback occurs on User registers also..
				//SwitchMode(curmode);
			}
			else
			{
				result = storeDec<MMU, DEFAULT_MODE>(insn, rbp);
			}

			if (WRITEBACK && !m_pendingAbtD)
			{
				SetRegister(rb, GetRegister(rb) - result * 4);
			}
		}
		if (insn & (1 << eR15))
		{
			R15 -= 12;
		}

		// STM takes (n-1)S + 2N cycles (n = # of register transfers)
		ARM7_ICOUNT -= (result - 1) + 2;
	}

	R15 += 4;
} /* HandleMemBlock */


void arm7_cpu_device::arm9ops_undef(const uint32_t insn)
{
	// unsupported instruction
	LOG(("ARM7: Instruction %08X unsupported\n", insn));
}

void arm7_cpu_device::arm9ops_1(const uint32_t insn)
{
#if 0
	/* Change processor state (CPS) */
	if ((insn & 0x00f10020) == 0x00000000)
	{
		// unsupported (armv6 onwards only)
	}
	else if ((insn & 0x00ff00f0) == 0x00010000) /* set endianness (SETEND) */
	{
		// unsupported (armv6 onwards only)
	}
	else
	{
	}
#endif
	arm9ops_undef(insn);
}

void arm7_cpu_device::arm9ops_57(const uint32_t insn)
{
#if 0
	/* Cache Preload (PLD) */
	if ((insn & 0x0070f000) == 0x0050f000)
	{
		// unsupported (armv6 onwards only)
	}
	else
	{
	}
#endif
	arm9ops_undef(insn);
}

void arm7_cpu_device::arm9ops_89(const uint32_t insn)
{
#if 0
	/* Save Return State (SRS) */
	if ((insn & 0x005f0f00) == 0x004d0500)
	{
		// unsupported (armv6 onwards only)
	}
	else if ((insn & 0x00500f00) == 0x00100a00) /* Return From Exception (RFE) */
	{
		// unsupported (armv6 onwards only)
	}
	else
	{
	}
#endif
	arm9ops_undef(insn);
}

void arm7_cpu_device::arm9ops_c(const uint32_t insn)
{
#if 0
	/* Additional coprocessor double register transfer */
	if ((insn & 0x00e00000) == 0x00400000)
	{
		// unsupported
	}
	else
	{
	}
#endif
	arm9ops_undef(insn);
}

void arm7_cpu_device::arm9ops_e(const uint32_t insn)
{
	/* Additional coprocessor register transfer */
	// unsupported
	arm9ops_undef(insn);
}

template <arm7_cpu_device::copro_mode MMU,
		  arm7_cpu_device::offset_mode OFFSET_MODE,
          arm7_cpu_device::flags_mode SET_FLAGS,
          arm7_cpu_device::writeback_mode WRITEBACK,
          arm7_cpu_device::lmul_mode LONG_MUL,
          arm7_cpu_device::smul_mode SIGNED_MUL,
          arm7_cpu_device::accum_mode ACCUM,
          arm7_cpu_device::load_mode LOAD,
          arm7_cpu_device::alu_mode OPCODE>
void arm7_cpu_device::arm7ops_0(const uint32_t insn)
{
	if ((insn & 0x90) == 0x90)
		if (insn & 0x60)         // bits = 6-5 != 00
			HandleHalfWordDT<MMU, POST_INDEXED, OFFSET_MODE, WRITEBACK, LOAD>(insn);
		else
			if (LONG_MUL)
				if (SIGNED_MUL)
					HandleSMulLong<SET_FLAGS, ACCUM>(insn);
				else
					HandleUMulLong<SET_FLAGS, ACCUM>(insn);
			else
				HandleMul<SET_FLAGS, ACCUM>(insn);
	else
		HandleALU<REG_OP2, SET_FLAGS, OPCODE>(insn);
}

template <arm7_cpu_device::copro_mode MMU,
		  arm7_cpu_device::offset_mode OFFSET_MODE,
          arm7_cpu_device::flags_mode SET_FLAGS,
          arm7_cpu_device::writeback_mode WRITEBACK,
          arm7_cpu_device::load_mode LOAD,
          arm7_cpu_device::alu_bit ALU_BIT,
          arm7_cpu_device::alu_mode OPCODE>
void arm7_cpu_device::arm7ops_1(const uint32_t insn)
{
	/* Branch and Exchange (BX) */
	if ((insn & 0x00fffff0) == 0x002fff10)     // bits 27-4 == 000100101111111111110001
	{
		R15 = GetRegister(insn & 0x0f);
		// If new PC address has A0 set, switch to Thumb mode
		if (R15 & 1)
		{
			if (!m_tflag)
				set_mode_changed();
			m_tflag = 1;
			R15--;
		}
		ARM7_ICOUNT -= 3;
	}
	else if ((insn & 0x00f000f0) == 0x01200030) // BLX Rn - v5
	{
		// save link address
		SetRegister(14, R15 + 4);

		R15 = GetRegister(insn & 0x0f);
		// If new PC address has A0 set, switch to Thumb mode
		if (R15 & 1)
		{
			if (!m_tflag)
				set_mode_changed();
			m_tflag = 1;
			R15--;
		}
		ARM7_ICOUNT -= 3;
	}
	else if ((insn & 0x00f000f0) == 0x00600010) // CLZ - v5
	{
		uint32_t rm = insn & 0xf;
		uint32_t rd = (insn >> 12) & 0xf;

		SetRegister(rd, count_leading_zeros(GetRegister(rm)));

		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x00f000f0) == 0x00000050) // QADD - v5
	{
		int32_t src1 = GetRegister(insn & 0xf);
		int32_t src2 = GetRegister((insn >> 16) & 0xf);
		int64_t res;

		res = saturate_qbit_overflow((int64_t)src1 + (int64_t)src2);

		SetRegister((insn >> 12) & 0xf, (int32_t)res);
		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x00f000f0) == 0x00400050) // QDADD - v5
	{
		int32_t src1 = GetRegister(insn & 0xf);
		int32_t src2 = GetRegister((insn >> 16) & 0xf);

		// check if doubling operation will overflow
		int64_t res = (int64_t)src2 * 2;
		saturate_qbit_overflow(res);

		src2 *= 2;
		res = saturate_qbit_overflow((int64_t)src1 + (int64_t)src2);

		SetRegister((insn >> 12) & 0xf, (int32_t)res);
		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x00f000f0) == 0x00200050) // QSUB - v5
	{
		int32_t src1 = GetRegister(insn & 0xf);
		int32_t src2 = GetRegister((insn >> 16) & 0xf);
		int64_t res = saturate_qbit_overflow((int64_t)src1 - (int64_t)src2);

		SetRegister((insn >> 12) & 0xf, (int32_t)res);
		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x00f000f0) == 0x00600050) // QDSUB - v5
	{
		int32_t src1 = GetRegister(insn & 0xf);
		int32_t src2 = GetRegister((insn >> 16) & 0xf);

		// check if doubling operation will overflow
		int64_t res = (int64_t)src2 * 2;
		saturate_qbit_overflow(res);

		src2 *= 2;
		res = saturate_qbit_overflow((int64_t)src1 - (int64_t)src2);

		SetRegister((insn >> 12) & 0xf, (int32_t)res);
		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x00f00090) == 0x00000080) // SMLAxy - v5
	{
		int32_t src1 = GetRegister(insn & 0xf);
		int32_t src2 = GetRegister((insn >> 8) & 0xf);

		// select top and bottom halves of src1/src2 and sign extend if necessary
		if (insn & 0x20)
		{
			src1 >>= 16;
		}

		src1 &= 0xffff;
		if (src1 & 0x8000)
		{
			src1 |= 0xffff0000;
		}

		if (insn & 0x40)
		{
			src2 >>= 16;
		}

		src2 &= 0xffff;
		if (src2 & 0x8000)
		{
			src2 |= 0xffff0000;
		}

		// do the signed multiply
		int32_t res1 = src1 * src2;
		// and the accumulate.  NOTE: only the accumulate can cause an overflow, which is why we do it this way.
		saturate_qbit_overflow((int64_t)res1 + (int64_t)GetRegister((insn >> 12) & 0xf));

		SetRegister((insn >> 16) & 0xf, res1 + GetRegister((insn >> 12) & 0xf));
		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x00f00090) == 0x00400080) // SMLALxy - v5
	{
		int32_t src1 = GetRegister(insn & 0xf);
		int32_t src2 = GetRegister((insn >> 8) & 0xf);

		int64_t dst = (int64_t)GetRegister((insn >> 12) & 0xf);
		dst |= (int64_t)GetRegister((insn >> 16) & 0xf) << 32;

		// do the multiply and accumulate
		dst += (int64_t)src1 * (int64_t)src2;

		// write back the result
		SetRegister((insn >> 12) & 0xf, (uint32_t)dst);
		SetRegister((insn >> 16) & 0xf, (uint32_t)(dst >> 32));
		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x00f00090) == 0x00600080) // SMULxy - v5
	{
		int32_t src1 = GetRegister(insn & 0xf);
		int32_t src2 = GetRegister((insn >> 8) & 0xf);

		// select top and bottom halves of src1/src2 and sign extend if necessary
		if (insn & 0x20)
		{
			src1 >>= 16;
		}

		src1 &= 0xffff;
		if (src1 & 0x8000)
		{
			src1 |= 0xffff0000;
		}

		if (insn & 0x40)
		{
			src2 >>= 16;
		}

		src2 &= 0xffff;
		if (src2 & 0x8000)
		{
			src2 |= 0xffff0000;
		}

		int32_t res = src1 * src2;
		SetRegister((insn >> 16) & 0xf, res);
		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x00f000b0) == 0x002000a0) // SMULWy - v5
	{
		int32_t src1 = GetRegister(insn & 0xf);
		int32_t src2 = GetRegister((insn >> 8) & 0xf);

		if (insn & 0x40)
		{
			src2 >>= 16;
		}

		src2 &= 0xffff;
		if (src2 & 0x8000)
		{
			src2 |= 0xffff0000;
		}

		int64_t res = (int64_t)src1 * (int64_t)src2;
		res >>= 16;
		SetRegister((insn >> 16) & 0xf, (uint32_t)res);
		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x00f000b0) == 0x00200080) // SMLAWy - v5
	{
		int32_t src1 = GetRegister(insn & 0xf);
		int32_t src2 = GetRegister((insn >> 8) & 0xf);
		int32_t src3 = GetRegister((insn >> 12) & 0xf);

		if (insn & 0x40)
		{
			src2 >>= 16;
		}

		src2 &= 0xffff;
		if (src2 & 0x8000)
		{
			src2 |= 0xffff0000;
		}

		int64_t res = (int64_t)src1 * (int64_t)src2;
		res >>= 16;

		// check for overflow and set the Q bit
		saturate_qbit_overflow((int64_t)src3 + res);

		// do the real accumulate
		src3 += (int32_t)res;

		// write the result back
		SetRegister((insn >> 16) & 0xf, (uint32_t)res);
		ARM7_ICOUNT -= 3;
		R15 += 4;
	}
	else if ((insn & 0x90) == 0x90)  // bits 27-25=000 bit 7=1 bit 4=1
	{
		if (insn & 0x60)         // bits = 6-5 != 00
			HandleHalfWordDT<MMU, PRE_INDEXED, OFFSET_MODE, WRITEBACK, LOAD>(insn);
		else
			HandleSwap<MMU>(insn);
	}
	else
	{
		if (ALU_BIT == PSR_OP && OFFSET_MODE == OFFSET_DOWN) /* PSR Transfer (MRS & MSR) */
			HandlePSRTransfer(insn);
		else
			HandleALU<REG_OP2, SET_FLAGS, OPCODE>(insn); /* Data Processing */
	}
}

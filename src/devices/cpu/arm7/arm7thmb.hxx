// license:BSD-3-Clause
// copyright-holders:Steve Ellenoff,R. Belmont,Ryan Holtz

/* Shift operations */
void arm7_cpu_device::tg00_0(uint32_t insn, uint32_t pc) /* Shift left */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rrs = GetRegister(rs);
	int32_t offs = (insn & THUMB_SHIFT_AMT) >> THUMB_SHIFT_AMT_SHIFT;
	if (offs != 0)
	{
		SetRegister(rd, rrs << offs);
		if (rrs & (1 << (31 - (offs - 1))))
			m_cflag = 1;
		else
			m_cflag = 0;
	}
	else
	{
		SetRegister(rd, rrs);
	}
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg00_1(uint32_t insn, uint32_t pc) /* Shift right */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rrs = GetRegister(rs);
	int32_t offs = (insn & THUMB_SHIFT_AMT) >> THUMB_SHIFT_AMT_SHIFT;
	if (offs != 0)
	{
		SetRegister(rd, rrs >> offs);
		m_cflag = (rrs >> (offs - 1)) & 1;
	}
	else
	{
		SetRegister(rd, 0);
		m_cflag = rrs >> 31;
	}
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

	/* Arithmetic */

void arm7_cpu_device::tg01_0(uint32_t insn, uint32_t pc)
{
	/* ASR */
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rrs = GetRegister(rs);
	int32_t offs = (insn & THUMB_SHIFT_AMT) >> THUMB_SHIFT_AMT_SHIFT;
	if (offs == 0)
	{
		offs = 32;
	}
	if (offs >= 32)
	{
		m_cflag = rrs >> 31;
		SetRegister(rd, (rrs & 0x80000000) ? 0xFFFFFFFF : 0x00000000);
	}
	else
	{
		m_cflag = (rrs >> (offs - 1)) & 1;
		SetRegister(rd, (rrs & 0x80000000) ? ((0xFFFFFFFF << (32 - offs)) | (rrs >> offs)) : (rrs >> offs));
	}
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg01_10(uint32_t insn, uint32_t pc)  /* ADD Rd, Rs, Rn */
{
	uint32_t rn = GetRegister((insn & THUMB_ADDSUB_RNIMM) >> THUMB_ADDSUB_RNIMM_SHIFT);
	uint32_t rs = GetRegister((insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT);
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	SetRegister(rd, rs + rn);
	HandleThumbALUAddFlags(GetRegister(rd), rs, rn);

}

void arm7_cpu_device::tg01_11(uint32_t insn, uint32_t pc) /* SUB Rd, Rs, Rn */
{
	uint32_t rn = GetRegister((insn & THUMB_ADDSUB_RNIMM) >> THUMB_ADDSUB_RNIMM_SHIFT);
	uint32_t rs = GetRegister((insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT);
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	SetRegister(rd, rs - rn);
	HandleThumbALUSubFlags(GetRegister(rd), rs, rn);

}

void arm7_cpu_device::tg01_12(uint32_t insn, uint32_t pc) /* ADD Rd, Rs, #imm */
{
	uint32_t imm = (insn & THUMB_ADDSUB_RNIMM) >> THUMB_ADDSUB_RNIMM_SHIFT;
	uint32_t rs = GetRegister((insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT);
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	SetRegister(rd, rs + imm);
	HandleThumbALUAddFlags(GetRegister(rd), rs, imm);

}

void arm7_cpu_device::tg01_13(uint32_t insn, uint32_t pc) /* SUB Rd, Rs, #imm */
{
	uint32_t imm = (insn & THUMB_ADDSUB_RNIMM) >> THUMB_ADDSUB_RNIMM_SHIFT;
	uint32_t rs = GetRegister((insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT);
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	SetRegister(rd, rs - imm);
	HandleThumbALUSubFlags(GetRegister(rd), rs,imm);

}

	/* CMP / MOV */

void arm7_cpu_device::tg02_0(uint32_t insn, uint32_t pc)
{
	uint32_t rd = (insn & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT;
	uint32_t op2 = (insn & THUMB_INSN_IMM);
	SetRegister(rd, op2);
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg02_1(uint32_t insn, uint32_t pc)
{
	uint32_t rn = GetRegister((insn & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT);
	uint32_t op2 = insn & THUMB_INSN_IMM;
	uint32_t rd = rn - op2;
	HandleThumbALUSubFlags(rd, rn, op2);
}

	/* ADD/SUB immediate */

void arm7_cpu_device::tg03_0(uint32_t insn, uint32_t pc) /* ADD Rd, #Offset8 */
{
	uint32_t rn = GetRegister((insn & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT);
	uint32_t op2 = insn & THUMB_INSN_IMM;
	uint32_t rd = rn + op2;
	SetRegister((insn & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT, rd);
	HandleThumbALUAddFlags(rd, rn, op2);
}

void arm7_cpu_device::tg03_1(uint32_t insn, uint32_t pc) /* SUB Rd, #Offset8 */
{
	uint32_t rn = GetRegister((insn & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT);
	uint32_t op2 = insn & THUMB_INSN_IMM;
	uint32_t rd = rn - op2;
	SetRegister((insn & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT, rd);
	HandleThumbALUSubFlags(rd, rn, op2);
}

	/* Rd & Rm instructions */

void arm7_cpu_device::tg04_00_00(uint32_t insn, uint32_t pc) /* AND Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	SetRegister(rd, GetRegister(rd) & GetRegister(rs));
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg04_00_01(uint32_t insn, uint32_t pc) /* EOR Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	SetRegister(rd, GetRegister(rd) ^ GetRegister(rs));
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg04_00_02(uint32_t insn, uint32_t pc) /* LSL Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rrd = GetRegister(rd);
	int32_t offs = GetRegister(rs) & 0x000000ff;
	if (offs > 0)
	{
		if (offs < 32)
		{
			SetRegister(rd, rrd << offs);
			m_cflag = (rrd >> (31 - (offs - 1))) & 1;
		}
		else if (offs == 32)
		{
			SetRegister(rd, 0);
			m_cflag = rrd & 1;
		}
		else
		{
			SetRegister(rd, 0);
			m_cflag = 0;
		}
	}
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg04_00_03(uint32_t insn, uint32_t pc) /* LSR Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rrd = GetRegister(rd);
	int32_t offs = GetRegister(rs) & 0x000000ff;
	if (offs >  0)
	{
		if (offs < 32)
		{
			SetRegister(rd, rrd >> offs);
			m_cflag = (rrd >> (offs - 1)) & 1;
		}
		else if (offs == 32)
		{
			SetRegister(rd, 0);
			m_cflag = rrd >> 31;
		}
		else
		{
			SetRegister(rd, 0);
			m_cflag = 0;
		}
	}
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg04_00_04(uint32_t insn, uint32_t pc) /* ASR Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rrs = GetRegister(rs)&0xff;
	uint32_t rrd = GetRegister(rd);
	if (rrs != 0)
	{
		if (rrs >= 32)
		{
			m_cflag = rrd >> 31;
			SetRegister(rd, (GetRegister(rd) & 0x80000000) ? 0xFFFFFFFF : 0x00000000);
		}
		else
		{
			m_cflag = (rrd >> (rrs-1)) & 1;
			SetRegister(rd, (rrd & 0x80000000) ? ((0xFFFFFFFF << (32 - rrs)) | (rrd >> rrs)) : (rrd >> rrs));
		}
	}
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg04_00_05(uint32_t insn, uint32_t pc) /* ADC Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t op2 = m_cflag;
	uint32_t rn = GetRegister(rd) + GetRegister(rs) + op2;
	HandleThumbALUAddFlags(rn, GetRegister(rd), (GetRegister(rs))); // ?
	SetRegister(rd, rn);
}

void arm7_cpu_device::tg04_00_06(uint32_t insn, uint32_t pc)  /* SBC Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t op2 = 1 - m_cflag;
	uint32_t rn = GetRegister(rd) - GetRegister(rs) - op2;
	HandleThumbALUSubFlags(rn, GetRegister(rd), (GetRegister(rs))); //?
	SetRegister(rd, rn);
}

void arm7_cpu_device::tg04_00_07(uint32_t insn, uint32_t pc) /* ROR Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rrd = GetRegister(rd);
	uint32_t imm = GetRegister(rs) & 0x0000001f;
	SetRegister(rd, (rrd >> imm) | (rrd << (32 - imm)));
	m_cflag = (rrd >> (imm - 1)) & 1;
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg04_00_08(uint32_t insn, uint32_t pc) /* TST Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	HandleALUNZFlags(GetRegister(rd) & GetRegister(rs));
	R15 += 2;
}

void arm7_cpu_device::tg04_00_09(uint32_t insn, uint32_t pc) /* NEG Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rrs = GetRegister(rs);
	SetRegister(rd, 0 - rrs);
	HandleThumbALUSubFlags(GetRegister(rd), 0, rrs);
}

void arm7_cpu_device::tg04_00_0a(uint32_t insn, uint32_t pc) /* CMP Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rn = GetRegister(rd) - GetRegister(rs);
	HandleThumbALUSubFlags(rn, GetRegister(rd), GetRegister(rs));
}

void arm7_cpu_device::tg04_00_0b(uint32_t insn, uint32_t pc) /* CMN Rd, Rs - check flags, add dasm */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rn = GetRegister(rd) + GetRegister(rs);
	HandleThumbALUAddFlags(rn, GetRegister(rd), GetRegister(rs));
}

void arm7_cpu_device::tg04_00_0c(uint32_t insn, uint32_t pc) /* ORR Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	SetRegister(rd, GetRegister(rd) | GetRegister(rs));
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg04_00_0d(uint32_t insn, uint32_t pc) /* MUL Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t rn = GetRegister(rd) * GetRegister(rs);
	SetRegister(rd, rn);
	HandleALUNZFlags(rn);
	R15 += 2;
}

void arm7_cpu_device::tg04_00_0e(uint32_t insn, uint32_t pc) /* BIC Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	SetRegister(rd, GetRegister(rd) & (~GetRegister(rs)));
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

void arm7_cpu_device::tg04_00_0f(uint32_t insn, uint32_t pc) /* MVN Rd, Rs */
{
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	uint32_t op2 = GetRegister(rs);
	SetRegister(rd, ~op2);
	HandleALUNZFlags(GetRegister(rd));
	R15 += 2;
}

/* ADD Rd, Rs group */

void arm7_cpu_device::tg04_01_00(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: G4-1-0 Undefined Thumb instruction: %04x %x\n", pc, insn, (insn & THUMB_HIREG_H) >> THUMB_HIREG_H_SHIFT);
}

void arm7_cpu_device::tg04_01_01(uint32_t insn, uint32_t pc) /* ADD Rd, HRs */
{
	uint32_t rs = (insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
	uint32_t rd = insn & THUMB_HIREG_RD;
	SetRegister(rd, GetRegister(rd) + GetRegister(rs+8));
	// emulate the effects of pre-fetch
	if (rs == 7)
	{
		SetRegister(rd, GetRegister(rd) + 4);
	}

	R15 += 2;
}

void arm7_cpu_device::tg04_01_02(uint32_t insn, uint32_t pc) /* ADD HRd, Rs */
{
	uint32_t rs = (insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
	uint32_t rd = insn & THUMB_HIREG_RD;
	SetRegister(rd+8, GetRegister(rd+8) + GetRegister(rs));
	if (rd == 7)
	{
		R15 += 2;
	}

	R15 += 2;
}

void arm7_cpu_device::tg04_01_03(uint32_t insn, uint32_t pc) /* Add HRd, HRs */
{
	uint32_t rs = (insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
	uint32_t rd = insn & THUMB_HIREG_RD;
	SetRegister(rd+8, GetRegister(rd+8) + GetRegister(rs+8));
	// emulate the effects of pre-fetch
	if (rs == 7)
	{
		SetRegister(rd+8, GetRegister(rd+8) + 4);
	}
	if (rd == 7)
	{
		R15 += 2;
	}

	R15 += 2;
}

void arm7_cpu_device::tg04_01_10(uint32_t insn, uint32_t pc)  /* CMP Rd, Rs */
{
	uint32_t rs = GetRegister(((insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT));
	uint32_t rd = GetRegister(insn & THUMB_HIREG_RD);
	uint32_t rn = rd - rs;
	HandleThumbALUSubFlags(rn, rd, rs);
}

void arm7_cpu_device::tg04_01_11(uint32_t insn, uint32_t pc) /* CMP Rd, Hs */
{
	uint32_t rs = GetRegister(((insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT) + 8);
	uint32_t rd = GetRegister(insn & THUMB_HIREG_RD);
	uint32_t rn = rd - rs;
	HandleThumbALUSubFlags(rn, rd, rs);
}

void arm7_cpu_device::tg04_01_12(uint32_t insn, uint32_t pc) /* CMP Hd, Rs */
{
	uint32_t rs = GetRegister(((insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT));
	uint32_t rd = GetRegister((insn & THUMB_HIREG_RD) + 8);
	uint32_t rn = rd - rs;
	HandleThumbALUSubFlags(rn, rd, rs);
}

void arm7_cpu_device::tg04_01_13(uint32_t insn, uint32_t pc) /* CMP Hd, Hs */
{
	uint32_t rs = GetRegister(((insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT) + 8);
	uint32_t rd = GetRegister((insn & THUMB_HIREG_RD) + 8);
	uint32_t rn = rd - rs;
	HandleThumbALUSubFlags(rn, rd, rs);
}

/* MOV group */

// "The action of H1 = 0, H2 = 0 for Op = 00 (ADD), Op = 01 (CMP) and Op = 10 (MOV) is undefined, and should not be used."
void arm7_cpu_device::tg04_01_20(uint32_t insn, uint32_t pc) /* MOV Rd, Rs (undefined) */
{
	uint32_t rs = (insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
	uint32_t rd = insn & THUMB_HIREG_RD;
	SetRegister(rd, GetRegister(rs));
	R15 += 2;
}

void arm7_cpu_device::tg04_01_21(uint32_t insn, uint32_t pc) /* MOV Rd, Hs */
{
	uint32_t rs = (insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
	uint32_t rd = insn & THUMB_HIREG_RD;
	SetRegister(rd, GetRegister(rs + 8));
	if (rs == 7)
	{
		SetRegister(rd, GetRegister(rd) + 4);
	}
	R15 += 2;
}

void arm7_cpu_device::tg04_01_22(uint32_t insn, uint32_t pc) /* MOV Hd, Rs */
{
	uint32_t rs = (insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
	uint32_t rd = insn & THUMB_HIREG_RD;
	SetRegister(rd + 8, GetRegister(rs));
	if (rd != 7)
	{
		R15 += 2;
	}
	else
	{
		R15 &= ~1;
	}
}

void arm7_cpu_device::tg04_01_23(uint32_t insn, uint32_t pc) /* MOV Hd, Hs */
{
	uint32_t rs = (insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
	uint32_t rd = insn & THUMB_HIREG_RD;
	if (rs == 7)
	{
		SetRegister(rd + 8, GetRegister(rs+8)+4);
	}
	else
	{
		SetRegister(rd + 8, GetRegister(rs+8));
	}
	if (rd != 7)
	{
		R15 += 2;
	}
	else
	{
		R15 &= ~1;
	}
}

void arm7_cpu_device::tg04_01_30(uint32_t insn, uint32_t pc)
{
	uint32_t rd = (insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
	uint32_t addr = GetRegister(rd);
	if (addr & 1)
	{
		addr &= ~1;
	}
	else
	{
		if (m_tflag)
			set_mode_changed();
		m_tflag = 0;
		if (addr & 2)
		{
			addr += 2;
		}
	}
	R15 = addr;
}

void arm7_cpu_device::tg04_01_31(uint32_t insn, uint32_t pc)
{
	uint32_t rs = (insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
	uint32_t addr = GetRegister(rs+8);
	if (rs == 7)
	{
		addr += 2;
	}
	if (addr & 1)
	{
		addr &= ~1;
	}
	else
	{
		if (m_tflag)
			set_mode_changed();
		m_tflag = 0;
		if (addr & 2)
		{
			addr += 2;
		}
	}
	R15 = addr;
}

/* BLX */
void arm7_cpu_device::tg04_01_32(uint32_t insn, uint32_t pc)
{
	uint32_t addr = GetRegister((insn & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT);
	SetRegister(14, (R15 + 2) | 1);

	// are we also switching to ARM mode?
	if (!(addr & 1))
	{
		if (m_tflag)
			set_mode_changed();
		m_tflag = 0;
		if (addr & 2)
		{
			addr += 2;
		}
	}
	else
	{
		addr &= ~1;
	}

	R15 = addr;
}

void arm7_cpu_device::tg04_01_33(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: G4-3 Undefined Thumb instruction: %04x\n", pc, insn);
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg04_0203(uint32_t insn, uint32_t pc)
{
	if (MMU)
	{
		uint32_t readword = arm7_cpu_read32_mmu((R15 & ~2) + 4 + ((insn & THUMB_INSN_IMM) << 2));
		SetRegister((insn & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT, readword);
	}
	else
	{
		uint32_t readword = arm7_cpu_read32((R15 & ~2) + 4 + ((insn & THUMB_INSN_IMM) << 2));
		SetRegister((insn & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT, readword);
	}
	R15 += 2;
}

/* LDR* STR* group */
template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg05_0(uint32_t insn, uint32_t pc)  /* STR Rd, [Rn, Rm] */
{
	uint32_t rm = (insn & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
	uint32_t rn = (insn & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
	uint32_t rd = (insn & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
	uint32_t addr = GetRegister(rn) + GetRegister(rm);
	if (MMU)
		arm7_cpu_write32_mmu(addr, GetRegister(rd));
	else
		arm7_cpu_write32(addr, GetRegister(rd));
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg05_1(uint32_t insn, uint32_t pc)  /* STRH Rd, [Rn, Rm] */
{
	uint32_t rm = (insn & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
	uint32_t rn = (insn & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
	uint32_t rd = (insn & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
	uint32_t addr = GetRegister(rn) + GetRegister(rm);
	if (MMU)
		arm7_cpu_write16_mmu(addr, GetRegister(rd));
	else
		arm7_cpu_write16(addr, GetRegister(rd));
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg05_2(uint32_t insn, uint32_t pc)  /* STRB Rd, [Rn, Rm] */
{
	uint32_t rm = (insn & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
	uint32_t rn = (insn & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
	uint32_t rd = (insn & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
	uint32_t addr = GetRegister(rn) + GetRegister(rm);
	if (MMU)
		arm7_cpu_write8_mmu(addr, GetRegister(rd));
	else
		arm7_cpu_write8(addr, GetRegister(rd));
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg05_3(uint32_t insn, uint32_t pc)  /* LDSB Rd, [Rn, Rm] todo, add dasm */
{
	uint32_t rm = (insn & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
	uint32_t rn = (insn & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
	uint32_t rd = (insn & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
	uint32_t addr = GetRegister(rn) + GetRegister(rm);
	uint32_t op2;
	if (MMU)
		op2 = arm7_cpu_read8_mmu(addr);
	else
		op2 = arm7_cpu_read8(addr);
	if (op2 & 0x00000080)
	{
		op2 |= 0xffffff00;
	}
	SetRegister(rd, op2);
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg05_4(uint32_t insn, uint32_t pc)  /* LDR Rd, [Rn, Rm] */
{
	uint32_t rm = (insn & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
	uint32_t rn = (insn & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
	uint32_t rd = (insn & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
	uint32_t addr = GetRegister(rn) + GetRegister(rm);
	uint32_t op2;
	if (MMU)
		op2 = arm7_cpu_read32_mmu(addr);
	else
		op2 = arm7_cpu_read32(addr);
	SetRegister(rd, op2);
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg05_5(uint32_t insn, uint32_t pc)  /* LDRH Rd, [Rn, Rm] */
{
	uint32_t rm = (insn & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
	uint32_t rn = (insn & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
	uint32_t rd = (insn & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
	uint32_t addr = GetRegister(rn) + GetRegister(rm);
	uint32_t op2;
	if (MMU)
		op2 = arm7_cpu_read16_mmu(addr);
	else
		op2 = arm7_cpu_read16(addr);
	SetRegister(rd, op2);
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg05_6(uint32_t insn, uint32_t pc)  /* LDRB Rd, [Rn, Rm] */
{
	uint32_t rm = (insn & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
	uint32_t rn = (insn & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
	uint32_t rd = (insn & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
	uint32_t addr = GetRegister(rn) + GetRegister(rm);
	uint32_t op2;
	if (MMU)
		op2 = arm7_cpu_read8_mmu(addr);
	else
		op2 = arm7_cpu_read8(addr);
	SetRegister(rd, op2);
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg05_7(uint32_t insn, uint32_t pc)  /* LDSH Rd, [Rn, Rm] */
{
	uint32_t rm = (insn & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
	uint32_t rn = (insn & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
	uint32_t rd = (insn & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
	uint32_t addr = GetRegister(rn) + GetRegister(rm);
	int32_t op2;
	if (MMU)
		op2 = (int32_t)(int16_t)(uint16_t)arm7_cpu_read16_mmu(addr & ~1);
	else
		op2 = (int32_t)(int16_t)(uint16_t)arm7_cpu_read16(addr & ~1);
	if ((addr & 1) && m_archRev < 5)
		op2 >>= 8;
	SetRegister(rd, op2);
	R15 += 2;
}

/* Word Store w/ Immediate Offset */
template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg06_0(uint32_t insn, uint32_t pc) /* Store */
{
	uint32_t rn = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = insn & THUMB_ADDSUB_RD;
	int32_t offs = ((insn & THUMB_LSOP_OFFS) >> THUMB_LSOP_OFFS_SHIFT) << 2;
	if (MMU)
		arm7_cpu_write32_mmu(GetRegister(rn) + offs, GetRegister(rd));
	else
		arm7_cpu_write32(GetRegister(rn) + offs, GetRegister(rd));
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg06_1(uint32_t insn, uint32_t pc) /* Load */
{
	uint32_t rn = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = insn & THUMB_ADDSUB_RD;
	int32_t offs = ((insn & THUMB_LSOP_OFFS) >> THUMB_LSOP_OFFS_SHIFT) << 2;
	if (MMU)
		SetRegister(rd, arm7_cpu_read32_mmu(GetRegister(rn) + offs)); // fix
	else
		SetRegister(rd, arm7_cpu_read32(GetRegister(rn) + offs)); // fix
	R15 += 2;
}

/* Byte Store w/ Immeidate Offset */
template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg07_0(uint32_t insn, uint32_t pc) /* Store */
{
	uint32_t rn = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = insn & THUMB_ADDSUB_RD;
	int32_t offs = (insn & THUMB_LSOP_OFFS) >> THUMB_LSOP_OFFS_SHIFT;
	if (MMU)
		arm7_cpu_write8_mmu(GetRegister(rn) + offs, GetRegister(rd));
	else
		arm7_cpu_write8(GetRegister(rn) + offs, GetRegister(rd));
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg07_1(uint32_t insn, uint32_t pc)  /* Load */
{
	uint32_t rn = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = insn & THUMB_ADDSUB_RD;
	int32_t offs = (insn & THUMB_LSOP_OFFS) >> THUMB_LSOP_OFFS_SHIFT;
	if (MMU)
		SetRegister(rd, arm7_cpu_read8_mmu(GetRegister(rn) + offs));
	else
		SetRegister(rd, arm7_cpu_read8(GetRegister(rn) + offs));
	R15 += 2;
}

/* Load/Store Halfword */
template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg08_0(uint32_t insn, uint32_t pc) /* Store */
{
	uint32_t imm = (insn & THUMB_HALFOP_OFFS) >> THUMB_HALFOP_OFFS_SHIFT;
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	if (MMU)
		arm7_cpu_write16_mmu(GetRegister(rs) + (imm << 1), GetRegister(rd));
	else
		arm7_cpu_write16(GetRegister(rs) + (imm << 1), GetRegister(rd));
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg08_1(uint32_t insn, uint32_t pc) /* Load */
{
	uint32_t imm = (insn & THUMB_HALFOP_OFFS) >> THUMB_HALFOP_OFFS_SHIFT;
	uint32_t rs = (insn & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
	uint32_t rd = (insn & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
	if (MMU)
		SetRegister(rd, arm7_cpu_read16_mmu(GetRegister(rs) + (imm << 1)));
	else
		SetRegister(rd, arm7_cpu_read16(GetRegister(rs) + (imm << 1)));
	R15 += 2;
}

/* Stack-Relative Load/Store */
template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg09_0(uint32_t insn, uint32_t pc) /* Store */
{
	uint32_t rd = (insn & THUMB_STACKOP_RD) >> THUMB_STACKOP_RD_SHIFT;
	int32_t offs = (uint8_t)(insn & THUMB_INSN_IMM);
	if (MMU)
		arm7_cpu_write32_mmu(GetRegister(13) + ((uint32_t)offs << 2), GetRegister(rd));
	else
		arm7_cpu_write32(GetRegister(13) + ((uint32_t)offs << 2), GetRegister(rd));
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg09_1(uint32_t insn, uint32_t pc) /* Load */
{
	uint32_t rd = (insn & THUMB_STACKOP_RD) >> THUMB_STACKOP_RD_SHIFT;
	int32_t offs = (uint8_t)(insn & THUMB_INSN_IMM);
	uint32_t readword;
	if (MMU)
		readword = arm7_cpu_read32_mmu((GetRegister(13) + ((uint32_t)offs << 2)) & ~3);
	else
		readword = arm7_cpu_read32((GetRegister(13) + ((uint32_t)offs << 2)) & ~3);
	SetRegister(rd, readword);
	R15 += 2;
}

/* Get relative address */

void arm7_cpu_device::tg0a_0(uint32_t insn, uint32_t pc)  /* ADD Rd, PC, #nn */
{
	uint32_t rd = (insn & THUMB_RELADDR_RD) >> THUMB_RELADDR_RD_SHIFT;
	int32_t offs = (uint8_t)(insn & THUMB_INSN_IMM) << 2;
	SetRegister(rd, ((R15 + 4) & ~2) + offs);
	R15 += 2;
}

void arm7_cpu_device::tg0a_1(uint32_t insn, uint32_t pc) /* ADD Rd, SP, #nn */
{
	uint32_t rd = (insn & THUMB_RELADDR_RD) >> THUMB_RELADDR_RD_SHIFT;
	int32_t offs = (uint8_t)(insn & THUMB_INSN_IMM) << 2;
	SetRegister(rd, GetRegister(13) + offs);
	R15 += 2;
}

	/* Stack-Related Opcodes */

void arm7_cpu_device::tg0b_0(uint32_t insn, uint32_t pc) /* ADD SP, #imm */
{
	uint32_t addr = (insn & THUMB_INSN_IMM);
	addr &= ~THUMB_INSN_IMM_S;
	SetRegister(13, GetRegister(13) + ((insn & THUMB_INSN_IMM_S) ? -(addr << 2) : (addr << 2)));
	R15 += 2;
}

void arm7_cpu_device::tg0b_1(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

void arm7_cpu_device::tg0b_2(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

void arm7_cpu_device::tg0b_3(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg0b_4(uint32_t insn, uint32_t pc) /* PUSH {Rlist} */
{
	for (int32_t offs = 7; offs >= 0; offs--)
	{
		if (insn & (1 << offs))
		{
			SetRegister(13, GetRegister(13) - 4);
			if (MMU)
				arm7_cpu_write32_mmu(GetRegister(13), GetRegister(offs));
			else
				arm7_cpu_write32(GetRegister(13), GetRegister(offs));
		}
	}
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg0b_5(uint32_t insn, uint32_t pc) /* PUSH {Rlist}{LR} */
{
	SetRegister(13, GetRegister(13) - 4);
	if (MMU)
		arm7_cpu_write32_mmu(GetRegister(13), GetRegister(14));
	else
		arm7_cpu_write32(GetRegister(13), GetRegister(14));
	for (int32_t offs = 7; offs >= 0; offs--)
	{
		if (insn & (1 << offs))
		{
			SetRegister(13, GetRegister(13) - 4);
			if (MMU)
				arm7_cpu_write32(GetRegister(13), GetRegister(offs));
			else
				arm7_cpu_write32(GetRegister(13), GetRegister(offs));
		}
	}
	R15 += 2;
}

void arm7_cpu_device::tg0b_6(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

void arm7_cpu_device::tg0b_7(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

void arm7_cpu_device::tg0b_8(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

void arm7_cpu_device::tg0b_9(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

void arm7_cpu_device::tg0b_a(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

void arm7_cpu_device::tg0b_b(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg0b_c(uint32_t insn, uint32_t pc) /* POP {Rlist} */
{
	for (int32_t offs = 0; offs < 8; offs++)
	{
		if (insn & (1 << offs))
		{
			if (MMU)
				SetRegister(offs, arm7_cpu_read32_mmu(GetRegister(13) & ~3));
			else
				SetRegister(offs, arm7_cpu_read32(GetRegister(13) & ~3));
			SetRegister(13, GetRegister(13) + 4);
		}
	}
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg0b_d(uint32_t insn, uint32_t pc) /* POP {Rlist}{PC} */
{
	for (int32_t offs = 0; offs < 8; offs++)
	{
		if (insn & (1 << offs))
		{
			if (MMU)
				SetRegister(offs, arm7_cpu_read32_mmu(GetRegister(13) & ~3));
			else
				SetRegister(offs, arm7_cpu_read32(GetRegister(13) & ~3));
			SetRegister(13, GetRegister(13) + 4);
		}
	}
	uint32_t addr;
	if (MMU)
		addr = arm7_cpu_read32_mmu(GetRegister(13) & ~3);
	else
		addr = arm7_cpu_read32(GetRegister(13) & ~3);
	if (m_archRev < 5)
	{
		R15 = addr & ~1;
	}
	else
	{
		if (addr & 1)
		{
			addr &= ~1;
		}
		else
		{
			if (m_tflag)
				set_mode_changed();
			m_tflag = 0;
			if (addr & 2)
			{
				addr += 2;
			}
		}

		R15 = addr;
	}
	SetRegister(13, GetRegister(13) + 4);
}

void arm7_cpu_device::tg0b_e(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

void arm7_cpu_device::tg0b_f(uint32_t insn, uint32_t pc)
{
	fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, insn);
}

/* Multiple Load/Store */

// "The address should normally be a word aligned quantity and non-word aligned addresses do not affect the instruction."
// "However, the bottom 2 bits of the address will appear on A[1:0] and might be interpreted by the memory system."

// Endrift says LDMIA/STMIA ignore the low 2 bits and GBA Test Suite assumes it.
template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg0c_0(uint32_t insn, uint32_t pc) /* Store */
{
	uint32_t rd = (insn & THUMB_MULTLS_BASE) >> THUMB_MULTLS_BASE_SHIFT;
	uint32_t ld_st_address = GetRegister(rd);
	for (int32_t offs = 0; offs < 8; offs++)
	{
		if (insn & (1 << offs))
		{
			if (MMU)
				arm7_cpu_write32_mmu(ld_st_address & ~3, GetRegister(offs));
			else
				arm7_cpu_write32(ld_st_address & ~3, GetRegister(offs));
			ld_st_address += 4;
		}
	}
	SetRegister(rd, ld_st_address);
	R15 += 2;
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::tg0c_1(uint32_t insn, uint32_t pc) /* Load */
{
	uint32_t rd = (insn & THUMB_MULTLS_BASE) >> THUMB_MULTLS_BASE_SHIFT;
	int rd_in_list = insn & (1 << rd);
	uint32_t ld_st_address = GetRegister(rd);
	for (int32_t offs = 0; offs < 8; offs++)
	{
		if (insn & (1 << offs))
		{
			if (MMU)
				SetRegister(offs, arm7_cpu_read32_mmu(ld_st_address & ~3));
			else
				SetRegister(offs, arm7_cpu_read32(ld_st_address & ~3));
			ld_st_address += 4;
		}
	}
	if (!rd_in_list)
	{
		SetRegister(rd, ld_st_address);
	}
	R15 += 2;
}

/* Conditional Branch */

void arm7_cpu_device::tg0d_0(uint32_t insn, uint32_t pc) // COND_EQ:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_zflag)
		R15 += 4 + (offs << 1);
	else
		R15 += 2;

}

void arm7_cpu_device::tg0d_1(uint32_t insn, uint32_t pc) // COND_NE:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_zflag)
		R15 += 2;
	else
		R15 += 4 + (offs << 1);
}

void arm7_cpu_device::tg0d_2(uint32_t insn, uint32_t pc) // COND_CS:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_cflag)
		R15 += 4 + (offs << 1);
	else
		R15 += 2;
}

void arm7_cpu_device::tg0d_3(uint32_t insn, uint32_t pc) // COND_CC:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_cflag)
		R15 += 2;
	else
		R15 += 4 + (offs << 1);
}

void arm7_cpu_device::tg0d_4(uint32_t insn, uint32_t pc) // COND_MI:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_nflag)
		R15 += 4 + (offs << 1);
	else
		R15 += 2;
}

void arm7_cpu_device::tg0d_5(uint32_t insn, uint32_t pc) // COND_PL:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_nflag)
		R15 += 2;
	else
		R15 += 4 + (offs << 1);
}

void arm7_cpu_device::tg0d_6(uint32_t insn, uint32_t pc) // COND_VS:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_vflag)
		R15 += 4 + (offs << 1);
	else
		R15 += 2;
}

void arm7_cpu_device::tg0d_7(uint32_t insn, uint32_t pc) // COND_VC:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_vflag)
		R15 += 2;
	else
		R15 += 4 + (offs << 1);
}

void arm7_cpu_device::tg0d_8(uint32_t insn, uint32_t pc) // COND_HI:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_cflag && !m_zflag)
		R15 += 4 + (offs << 1);
	else
		R15 += 2;
}

void arm7_cpu_device::tg0d_9(uint32_t insn, uint32_t pc) // COND_LS:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (!m_cflag || m_zflag)
		R15 += 4 + (offs << 1);
	else
		R15 += 2;
}

void arm7_cpu_device::tg0d_a(uint32_t insn, uint32_t pc) // COND_GE:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_nflag == m_vflag)
		R15 += 4 + (offs << 1);
	else
		R15 += 2;
}

void arm7_cpu_device::tg0d_b(uint32_t insn, uint32_t pc) // COND_LT:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_nflag != m_vflag)
		R15 += 4 + (offs << 1);
	else
		R15 += 2;
}

void arm7_cpu_device::tg0d_c(uint32_t insn, uint32_t pc) // COND_GT:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (!m_zflag && (m_nflag == m_vflag))
	{
		R15 += 4 + (offs << 1);
	}
	else
	{
		R15 += 2;
	}
}

void arm7_cpu_device::tg0d_d(uint32_t insn, uint32_t pc) // COND_LE:
{
	int32_t offs = (int8_t)(insn & THUMB_INSN_IMM);
	if (m_zflag || (m_nflag != m_vflag))
	{
		R15 += 4 + (offs << 1);
	}
	else
	{
		R15 += 2;
	}
}

void arm7_cpu_device::tg0d_e(uint32_t insn, uint32_t pc) // COND_AL:
{
	fatalerror("%08x: Undefined Thumb instruction: %04x (ARM9 reserved)\n", pc, insn);
}

void arm7_cpu_device::tg0d_f(uint32_t insn, uint32_t pc) // COND_NV:   // SWI (this is sort of a "hole" in the opcode encoding)
{
	m_pendingSwi = true;
	m_pending_interrupt = true;
	//arm7_check_irq_state();
}

/* B #offs */

void arm7_cpu_device::tg0e_0(uint32_t insn, uint32_t pc)
{
	int32_t offs = (insn & THUMB_BRANCH_OFFS) << 1;
	if (offs & 0x00000800)
	{
		offs |= 0xfffff800;
	}
	R15 += 4 + offs;
}

void arm7_cpu_device::tg0e_1(uint32_t insn, uint32_t pc)
{
	/* BLX (LO) */

	uint32_t addr = GetRegister(14);
	addr += (insn & THUMB_BLOP_OFFS) << 1;
	addr &= 0xfffffffc;
	SetRegister(14, (R15 + 2) | 1);
	R15 = addr;
	if (m_tflag)
		set_mode_changed();
	m_tflag = 0;
}


void arm7_cpu_device::tg0f_0(uint32_t insn, uint32_t pc)
{
	/* BL (HI) */

	uint32_t addr = (insn & THUMB_BLOP_OFFS) << 12;
	if (addr & (1 << 22))
	{
		addr |= 0xff800000;
	}
	addr += R15 + 4;
	SetRegister(14, addr);
	R15 += 2;
}

void arm7_cpu_device::tg0f_1(uint32_t insn, uint32_t pc) /* BL */
{
	/* BL (LO) */

	uint32_t addr = GetRegister(14) & ~1;
	addr += (insn & THUMB_BLOP_OFFS) << 1;
	SetRegister(14, (R15 + 2) | 1);
	R15 = addr;
	//R15 += 2;
}

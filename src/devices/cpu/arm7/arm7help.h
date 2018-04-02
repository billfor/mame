// license:BSD-3-Clause
// copyright-holders:Steve Ellenoff,R. Belmont,Ryan Holtz
/* ARM7 core helper Macros / Functions */

/* Macros that need to be defined according to the cpu implementation specific need */
#define ARM7REG(reg)        m_r[reg]
#define ARM7_ICOUNT         m_icount


#if 0
#define LOG(x) osd_printf_debug x
#else
#define LOG(x) logerror x
#endif

/***************
 * helper funcs
 ***************/

// TODO LD:
//  - SIGN_BITS_DIFFER = THUMB_SIGN_BITS_DIFFER
//  - do while (0)
//  - HandleALUAddFlags = HandleThumbALUAddFlags except for PC incr
//  - HandleALUSubFlags = HandleThumbALUSubFlags except for PC incr

#define IsNeg(i) ((i) >> 31)
#define IsPos(i) ((~(i)) >> 31)

/* Set NZCV flags for ADDS / SUBS */
#define HandleALUAddFlags(rd, rn, op2)												\
	if (SET_FLAGS)																	\
	{																				\
		if (rd)																		\
		{																			\
			m_zflag = 0;															\
			m_nflag = rd >> 31;														\
		}																			\
		else																		\
		{																			\
			m_zflag = 1;															\
			m_nflag = 0;															\
		}																			\
		m_vflag = (~(rn ^ op2) & (rn ^ rd)) >> 31;									\
		m_cflag = ((rn & op2) | (rn & ~rd) | (op2 & ~rd)) >> 31;					\
	}																				\
	R15 += 4;

#define HandleThumbALUAddFlags(rd, rn, op2)										\
	if (rd)																		\
	{																			\
		m_zflag = 0;															\
		m_nflag = rd >> 31;														\
	}																			\
	else																		\
	{																			\
		m_zflag = 1;															\
		m_nflag = 0;															\
	}																			\
	m_vflag = (~(rn ^ op2) & (rn ^ rd)) >> 31;									\
	m_cflag = ((rn & op2) | (rn & ~rd) | (op2 & ~rd)) >> 31;					\
	R15 += 2;

#define DRCHandleThumbALUAddFlags(rd, rn, op2)                                                  \
	UML_AND(block, DRC_CPSR, DRC_CPSR, ~(N_MASK | Z_MASK | V_MASK | C_MASK));                   \
	DRCHandleALUNZFlags(rd);                                                                    \
	UML_XOR(block, uml::I1, rn, ~0);                                                            \
	UML_CMP(block, uml::I1, op2);                                                               \
	UML_MOVc(block, uml::COND_B, uml::I1, C_BIT);                                               \
	UML_MOVc(block, uml::COND_AE, uml::I1, 0);                                                  \
	UML_OR(block, uml::I0, uml::I0, uml::I1);                                                   \
	UML_XOR(block, uml::I1, rn, op2);                                                           \
	UML_XOR(block, uml::I2, rn, rd);                                                            \
	UML_AND(block, uml::I1, uml::I1, uml::I2);                                                  \
	UML_TEST(block, uml::I1, 1 << 31);                                                          \
	UML_MOVc(block, uml::COND_NZ, uml::I1, V_BIT);                                              \
	UML_MOVc(block, uml::COND_Z, uml::I1, 0);                                                   \
	UML_OR(block, uml::I0, uml::I0, uml::I1);                                                   \
	UML_OR(block, DRC_CPSR, DRC_CPSR, uml::I0);                                                  \
	UML_ADD(block, DRC_PC, DRC_PC, 2);

#define HandleALUSubFlags(rd, rn, op2)												\
	if (SET_FLAGS)																	\
	{																				\
		if (rd)																		\
		{																			\
			m_zflag = 0;															\
			m_nflag = rd >> 31;														\
		}																			\
		else																		\
		{																			\
			m_zflag = 1;															\
			m_nflag = 0;															\
		}																			\
		m_vflag = ((rn ^ op2) & (rn ^ rd)) >> 31;									\
		m_cflag = ((rn & ~op2) | (rn & ~rd) | (~op2 & ~rd)) >> 31;					\
	}																				\
	R15 += 4;

#define HandleThumbALUSubFlags(rd, rn, op2)											\
	if (rd)																		\
	{																			\
		m_zflag = 0;															\
		m_nflag = rd >> 31;														\
	}																			\
	else																		\
	{																			\
		m_zflag = 1;															\
		m_nflag = 0;															\
	}																			\
	m_vflag = ((rn ^ op2) & (rn ^ rd)) >> 31;									\
	m_cflag = ((rn & ~op2) | (rn & ~rd) | (~op2 & ~rd)) >> 31;					\
	R15 += 2;

#define DRCHandleThumbALUSubFlags(rd, rn, op2)                                                  \
	UML_AND(block, DRC_CPSR, DRC_CPSR, ~(N_MASK | Z_MASK | V_MASK | C_MASK));                   \
	DRCHandleALUNZFlags(rd);                                                                    \
	UML_XOR(block, uml::I1, rn, op2);                                                           \
	UML_XOR(block, uml::I2, rn, rd);                                                            \
	UML_AND(block, uml::I1, uml::I1, uml::I2);                                                  \
	UML_TEST(block, uml::I1, 1 << 31);                                                          \
	UML_MOVc(block, uml::COND_NZ, uml::I1, V_BIT);                                              \
	UML_MOVc(block, uml::COND_Z, uml::I1, 0);                                                   \
	UML_OR(block, uml::I0, uml::I0, uml::I1);                                                   \
	UML_OR(block, DRC_CPSR, DRC_CPSR, uml::I0);                                                 \
	UML_AND(block, uml::I0, rd, 1 << 31);                                                       \
	UML_AND(block, uml::I1, op2, 1 << 31);                                                      \
	UML_AND(block, uml::I2, rn, 1 << 31);                                                       \
	UML_XOR(block, uml::I2, uml::I2, ~0);                                                       \
	UML_AND(block, uml::I1, uml::I1, uml::I2);                                                  \
	UML_AND(block, uml::I2, uml::I2, uml::I0);                                                  \
	UML_OR(block, uml::I1, uml::I1, uml::I2);                                                   \
	UML_AND(block, uml::I2, op2, 1 << 31);                                                      \
	UML_AND(block, uml::I2, uml::I2, uml::I0);                                                  \
	UML_OR(block, uml::I1, uml::I1, uml::I2);                                                   \
	UML_TEST(block, uml::I1, 1 << 31);                                                          \
	UML_MOVc(block, uml::COND_NZ, uml::I0, C_MASK);                                             \
	UML_MOVc(block, uml::COND_Z, uml::I0, 0);                                                   \
	UML_OR(block, DRC_CPSR, DRC_CPSR, uml::I0);                                                 \
	UML_ADD(block, DRC_PC, DRC_PC, 2);

/* Set NZC flags for logical operations. */

// This macro (which I didn't write) - doesn't make it obvious that the SIGN BIT = 31, just as the N Bit does,
// therefore, N is set by default
#define HandleALUNZFlags(rd)		\
	if (rd)							\
	{								\
		m_zflag = 0;				\
		m_nflag = (rd >> 31) & 1;	\
	}								\
	else							\
	{								\
		m_zflag = 1;				\
		m_nflag = 0;				\
	}

#define DRCHandleALUNZFlags(rd)                            \
	UML_AND(block, uml::I0, rd, SIGN_BIT);                 \
	UML_CMP(block, rd, 0);                                 \
	UML_MOVc(block, uml::COND_E, uml::I1, 1);              \
	UML_MOVc(block, uml::COND_NE, uml::I1, 0);             \
	UML_ROLINS(block, uml::I0, uml::I1, Z_BIT, 1 << Z_BIT);

// Long ALU Functions use bit 63
#define HandleLongALUNZFlags(rd)	\
	if (rd)							\
	{								\
		m_zflag = 0;				\
		m_nflag = (rd >> 63) & 1;	\
	}								\
	else							\
	{								\
		m_zflag = 1;				\
		m_nflag = 0;				\
	}

#define HandleALULogicalFlags(rd, sc)	\
	if (SET_FLAGS)						\
	{									\
		HandleALUNZFlags(rd);			\
		m_cflag = (sc) != 0 ? 1 : 0;	\
	}									\
	R15 += 4;

#define DRC_RD      uml::mem(&GetRegister(rd))
#define DRC_RS      uml::mem(&GetRegister(rs))
#define DRC_CPSR    uml::mem(&GET_CPSR)
#define DRC_PC      uml::mem(&R15)
#define DRC_REG(i)  uml::mem(&GetRegister(i))

#define DRCHandleALULogicalFlags(rd, sc)                                \
	if (insn & INSN_S)                                                  \
	{                                                                   \
		UML_AND(block, DRC_CPSR, DRC_CPSR, ~(N_MASK | Z_MASK | C_MASK); \
		DRCHandleALUNZFlags(rd);                                        \
		UML_TEST(block, sc, ~0);                                        \
		UML_MOVc(block, uml::COND_Z, uml::I1, C_BIT);                   \
		UML_MOVc(block, uml::COND_NZ, uml::I1, 0);                      \
		UML_OR(block, uml::I0, uml::I0, uml::I1);                       \
		UML_OR(block, DRC_CPSR, DRC_CPSR, uml::I0);                     \
	}                                                                   \
	UML_ADD(block, DRC_PC, DRC_PC, 4);


// used to be functions, but no longer a need, so we'll use define for better speed.
#define GetRegister(rIndex)        m_r[rIndex]
#define SetRegister(rIndex, value) m_r[rIndex] = value

#define GetModeRegister(mode, rIndex)        m_r[s_register_table[mode][rIndex]]
#define SetModeRegister(mode, rIndex, value) m_r[s_register_table[mode][rIndex]] = value


/* Macros that can be re-defined for custom cpu implementations - The core expects these to be defined */
/* In this case, we are using the default arm7 handlers (supplied by the core)
   - but simply changes these and define your own if needed for cpu implementation specific needs */
#define PTR_READ32          &arm7_cpu_read32
#define PTR_WRITE32         &arm7_cpu_write32

#define LSL(v, s) ((v) << (s))
#define LSR(v, s) ((v) >> (s))
#define ROL(v, s) (LSL((v), (s)) | (LSR((v), 32u - (s))))
#define ROR(v, s) (LSR((v), (s)) | (LSL((v), 32u - (s))))

#define ALUStoreTest(rdn, rd)		\
	if (rdn == eR15)				\
	{								\
		if (SET_FLAGS)				\
			SetRegister(rdn, rd);	\
		cycles += 2;				\
	}

#define ALUStoreDest(rdn, rd)					\
	if (rdn == eR15)							\
	{											\
		if (SET_FLAGS)							\
		{										\
			if (m_mode != eARM7_MODE_USER)		\
			{									\
				uint32_t old_t = m_tflag;		\
				set_cpsr(GetRegister(SPSR));	\
				if (m_tflag != old_t)			\
					set_mode_changed();			\
			}									\
		}										\
		SetRegister(rdn, rd);					\
		cycles += 2;							\
	}											\
	else										\
	{											\
		SetRegister(rdn, rd);					\
	}

// license:BSD-3-Clause
// copyright-holders:Steve Ellenoff,R. Belmont,Ryan Holtz
/*****************************************************************************
 *
 *   arm7.c
 *   Portable CPU Emulator for 32-bit ARM v3/4/5/6
 *
 *   Copyright Steve Ellenoff, all rights reserved.
 *   Thumb, DSP, and MMU support and many bugfixes by R. Belmont and Ryan Holtz.
 *
 *  This work is based on:
 *  #1) 'Atmel Corporation ARM7TDMI (Thumb) Datasheet - January 1999'
 *  #2) Arm 2/3/6 emulator By Bryan McPhail (bmcphail@tendril.co.uk) and Phil Stroffolino (MAME CORE 0.76)
 *
 *****************************************************************************/

/******************************************************************************
 *  Notes:

    ** This is a plain vanilla implementation of an ARM7 cpu which incorporates my ARM7 core.
       It can be used as is, or used to demonstrate how to utilize the arm7 core to create a cpu
       that uses the core, since there are numerous different mcu packages that incorporate an arm7 core.

       See the notes in the arm7core.inc file itself regarding issues/limitations of the arm7 core.
    **

TODO:
- Cleanups
- Fix and finish the DRC code, or remove it entirely

*****************************************************************************/
#include "emu.h"
#include "debugger.h"
#include "arm7.h"
#include "arm7core.h"   //include arm7 core
#include "arm7help.h"
#include "arm7fe.h"

/* size of the execution code cache */
#define CACHE_SIZE                      (32 * 1024 * 1024)

DEFINE_DEVICE_TYPE(ARM7,     arm7_cpu_device,     "arm7_le",  "ARM7 (little)")
DEFINE_DEVICE_TYPE(ARM7_BE,  arm7_be_cpu_device,  "arm7_be",  "ARM7 (big)")
DEFINE_DEVICE_TYPE(ARM7500,  arm7500_cpu_device,  "arm7500",  "ARM7500")
DEFINE_DEVICE_TYPE(ARM9,     arm9_cpu_device,     "arm9",     "ARM9")
DEFINE_DEVICE_TYPE(ARM920T,  arm920t_cpu_device,  "arm920t",  "ARM920T")
DEFINE_DEVICE_TYPE(ARM946ES, arm946es_cpu_device, "arm946es", "ARM946ES")
DEFINE_DEVICE_TYPE(PXA255,   pxa255_cpu_device,   "pxa255",   "Intel XScale PXA255")
DEFINE_DEVICE_TYPE(SA1110,   sa1110_cpu_device,   "sa1110",   "Intel StrongARM SA-1110")
DEFINE_DEVICE_TYPE(IGS036,   igs036_cpu_device,   "igs036",   "IGS036")

int arm7_cpu_device::s_read_fault_table_user[16];
int arm7_cpu_device::s_read_fault_table_no_user[16];
int arm7_cpu_device::s_write_fault_table_user[16];
int arm7_cpu_device::s_write_fault_table_no_user[16];
uint32_t arm7_cpu_device::s_read_fault_word_user;
uint32_t arm7_cpu_device::s_read_fault_word_no_user;
uint32_t arm7_cpu_device::s_write_fault_word_user;
uint32_t arm7_cpu_device::s_write_fault_word_no_user;
uint32_t arm7_cpu_device::s_add_nvc_flags[8];
uint32_t arm7_cpu_device::s_sub_nvc_flags[8];

/* 17 processor registers are visible at any given time,
 * banked depending on processor mode.
 */

const int arm7_cpu_device::s_register_table[ARM7_NUM_MODES][17] =
{
	{ /* USR */
		eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7,
		eR8, eR9, eR10, eR11, eR12,
		eR13, eR14,
		eR15  // No SPSR in this mode
	},
	{ /* FIQ */
		eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7,
		eR8_FIQ, eR9_FIQ, eR10_FIQ, eR11_FIQ, eR12_FIQ,
		eR13_FIQ, eR14_FIQ,
		eR15, eSPSR_FIQ
	},
	{ /* IRQ */
		eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7,
		eR8, eR9, eR10, eR11, eR12,
		eR13_IRQ, eR14_IRQ,
		eR15, eSPSR_IRQ
	},
	{ /* SVC */
		eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7,
		eR8, eR9, eR10, eR11, eR12,
		eR13_SVC, eR14_SVC,
		eR15, eSPSR_SVC
	},
	{0}, {0}, {0},        // values for modes 4,5,6 are not valid
	{ /* ABT */
		eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7,
		eR8, eR9, eR10, eR11, eR12,
		eR13_ABT, eR14_ABT,
		eR15, eSPSR_ABT
	},
	{0}, {0}, {0},        // values for modes 8,9,a are not valid!
	{ /* UND */
		eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7,
		eR8, eR9, eR10, eR11, eR12,
		eR13_UND, eR14_UND,
		eR15, eSPSR_UND
	},
	{0}, {0}, {0},        // values for modes c,d, e are not valid!
	{ /* SYS */
		eR0, eR1, eR2, eR3, eR4, eR5, eR6, eR7,
		eR8, eR9, eR10, eR11, eR12,
		eR13, eR14,
		eR15  // No SPSR in this mode
	}
};

arm7_cpu_device::arm7_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: arm7_cpu_device(mconfig, ARM7, tag, owner, clock, 4, ARCHFLAG_T, ENDIANNESS_LITTLE)
{
}

arm7_cpu_device::arm7_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, uint8_t archRev, uint8_t archFlags, endianness_t endianness)
	: cpu_device(mconfig, type, tag, owner, clock)
	, m_program_config("program", endianness, 32, 32, 0)
	, m_core(nullptr)
	, m_stashed_icount(-1)
	, m_program(nullptr)
	, m_direct(nullptr)
	, m_endian(endianness)
	, m_archRev(archRev)
	, m_archFlags(archFlags)
	, m_vectorbase(0)
	, m_prefetch_enabled(false)
	, m_enable_drc(false)
	, m_pc(0)
	, m_cache(CACHE_SIZE + sizeof(arm7_cpu_device))
	, m_drcuml(nullptr)
	, m_drcfe(nullptr)
	, m_drcoptions(0)
	, m_cache_dirty(0)
{
	uint32_t arch = ARM9_COPRO_ID_ARCH_V4;
	if (m_archFlags & ARCHFLAG_T)
		arch = ARM9_COPRO_ID_ARCH_V4T;

	m_copro_id = ARM9_COPRO_ID_MFR_ARM | arch | ARM9_COPRO_ID_PART_GENERICARM7;
}


arm7_be_cpu_device::arm7_be_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: arm7_cpu_device(mconfig, ARM7_BE, tag, owner, clock, 4, ARCHFLAG_T, ENDIANNESS_BIG)
{
}


arm7500_cpu_device::arm7500_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: arm7_cpu_device(mconfig, ARM7500, tag, owner, clock, 4, ARCHFLAG_MODE26, ENDIANNESS_LITTLE)
{
	m_copro_id = ARM9_COPRO_ID_MFR_ARM
			   | ARM9_COPRO_ID_ARCH_V4
			   | ARM9_COPRO_ID_PART_ARM710;
}


arm9_cpu_device::arm9_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: arm9_cpu_device(mconfig, ARM9, tag, owner, clock, 5, ARCHFLAG_T | ARCHFLAG_E, ENDIANNESS_LITTLE)
{
}


arm9_cpu_device::arm9_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, uint8_t archRev, uint8_t archFlags, endianness_t endianness)
	: arm7_cpu_device(mconfig, type, tag, owner, clock, archRev, archFlags, endianness)
{
	uint32_t arch = ARM9_COPRO_ID_ARCH_V4;
	switch (archRev)
	{
		case 4:
			if (archFlags & ARCHFLAG_T)
				arch = ARM9_COPRO_ID_ARCH_V4T;
			break;
		case 5:
			arch = ARM9_COPRO_ID_ARCH_V5;
			if (archFlags & ARCHFLAG_T)
			{
				arch = ARM9_COPRO_ID_ARCH_V5T;
				if (archFlags & ARCHFLAG_E)
				{
					arch = ARM9_COPRO_ID_ARCH_V5TE;
				}
			}
			break;
		default: break;
	}

	m_copro_id = ARM9_COPRO_ID_MFR_ARM | arch | (0x900 << 4);
}


arm920t_cpu_device::arm920t_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: arm9_cpu_device(mconfig, ARM920T, tag, owner, clock, 4, ARCHFLAG_T, ENDIANNESS_LITTLE)
{
	m_copro_id = ARM9_COPRO_ID_MFR_ARM
			   | ARM9_COPRO_ID_SPEC_REV1
			   | ARM9_COPRO_ID_ARCH_V4T
			   | ARM9_COPRO_ID_PART_ARM920
			   | 0; // Stepping
}




arm946es_cpu_device::arm946es_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: arm9_cpu_device(mconfig, type, tag, owner, clock, 5, ARCHFLAG_T | ARCHFLAG_E, ENDIANNESS_LITTLE),
	cp15_control(0x78)
{
	m_copro_id = ARM9_COPRO_ID_MFR_ARM
		| ARM9_COPRO_ID_ARCH_V5TE
		| ARM9_COPRO_ID_PART_ARM946
		| ARM9_COPRO_ID_STEP_ARM946_A0;

	memset(ITCM, 0, 0x8000);
	memset(DTCM, 0, 0x4000);

	cp15_itcm_base = 0xffffffff;
	cp15_itcm_size = 0;
	cp15_itcm_end = 0;
	cp15_dtcm_base = 0xffffffff;
	cp15_dtcm_size = 0;
	cp15_dtcm_end = 0;
	cp15_itcm_reg = cp15_dtcm_reg = 0;
}

arm946es_cpu_device::arm946es_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: arm946es_cpu_device(mconfig, ARM946ES, tag, owner, clock)
{
}

// unknown configuration, but uses MPU not MMU, so closer to ARM946ES
igs036_cpu_device::igs036_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: arm946es_cpu_device(mconfig, IGS036, tag, owner, clock)
{
}

pxa255_cpu_device::pxa255_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: arm7_cpu_device(mconfig, PXA255, tag, owner, clock, 5, ARCHFLAG_T | ARCHFLAG_E | ARCHFLAG_XSCALE, ENDIANNESS_LITTLE)
{
	m_copro_id = ARM9_COPRO_ID_MFR_INTEL
			   | ARM9_COPRO_ID_ARCH_V5TE
			   | ARM9_COPRO_ID_PXA255_CORE_GEN_XSCALE
			   | (3 << ARM9_COPRO_ID_PXA255_CORE_REV_SHIFT)
			   | ARM9_COPRO_ID_STEP_PXA255_A0;
}

sa1110_cpu_device::sa1110_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: arm7_cpu_device(mconfig, SA1110, tag, owner, clock, 4, ARCHFLAG_SA, ENDIANNESS_LITTLE)
	// has StrongARM, no Thumb, no Enhanced DSP
{
	m_copro_id = ARM9_COPRO_ID_MFR_INTEL
			   | ARM9_COPRO_ID_ARCH_V4
			   | ARM9_COPRO_ID_PART_SA1110
			   | ARM9_COPRO_ID_STEP_SA1110_A0;
}

device_memory_interface::space_config_vector arm7_cpu_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(AS_PROGRAM, &m_program_config)
	};
}

void arm7_cpu_device::update_reg_ptr()
{
	m_reg_group = s_register_table[m_mode];
	m_rp = m_register_pointers[m_mode];
}

void arm7_cpu_device::split_flags()
{
	m_nflag = BIT(m_cpsr, N_BIT);
	m_zflag = BIT(m_cpsr, Z_BIT);
	m_cflag = BIT(m_cpsr, C_BIT);
	m_vflag = BIT(m_cpsr, V_BIT);
	m_tflag = BIT(m_cpsr, T_BIT);
}

uint32_t arm7_cpu_device::make_cpsr()
{
	return (m_cpsr &~ (N_MASK | Z_MASK | C_MASK | V_MASK | T_MASK)) | (m_nflag << N_BIT) | (m_zflag << Z_BIT) | (m_cflag << C_BIT) | (m_vflag << V_BIT) | (m_tflag << T_BIT);
}

void arm7_cpu_device::set_cpsr(uint32_t val)
{
	m_cpsr = val | 0x10;
	split_flags();
	const uint32_t mode = m_cpsr & MODE_FLAG;
	if (mode != m_mode)
	{
		m_mode = mode;
		m_read_fault_table = ((m_mode == eARM7_MODE_USER) ? s_read_fault_table_user : s_read_fault_table_no_user);
		m_write_fault_table = ((m_mode == eARM7_MODE_USER) ? s_write_fault_table_user : s_write_fault_table_no_user);
		//m_read_fault_word = ((m_mode == eARM7_MODE_USER) ? s_read_fault_word_user : s_read_fault_word_no_user);
		//m_write_fault_word = ((m_mode == eARM7_MODE_USER) ? s_write_fault_word_user : s_write_fault_word_no_user);
		update_reg_ptr();
	}
}

void arm7500_cpu_device::set_cpsr(uint32_t val)
{
	if ((val & 0x10) != (m_cpsr & 0x10))
	{
		if (val & 0x10)
		{
			// 26 -> 32
			val = (val & 0x0FFFFF3F) | (m_r[eR15] & 0xF0000000) /* N Z C V */ | ((m_r[eR15] & 0x0C000000) >> (26 - 6)) /* I F */;
			m_r[eR15] = m_r[eR15] & 0x03FFFFFC;
		}
		else
		{
			// 32 -> 26
			m_r[eR15] = (m_r[eR15] & 0x03FFFFFC) /* PC */ | (val & 0xF0000000) /* N Z C V */ | ((val & 0x000000C0) << (26 - 6)) /* I F */ | (val & 0x00000003) /* M1 M0 */;
		}
	}
	else
	{
		if (!(val & 0x10))
		{
			// mirror bits in pc
			m_r[eR15] = (m_r[eR15] & 0x03FFFFFF) | (val & 0xF0000000) /* N Z C V */ | ((val & 0x000000C0) << (26 - 6)) /* I F */;
		}
	}
	m_cpsr = val;
	split_flags();
	const uint32_t mode = m_cpsr & MODE_FLAG;
	if (mode != m_mode)
	{
		m_mode = mode;
		m_read_fault_table = ((m_mode == eARM7_MODE_USER) ? s_read_fault_table_user : s_read_fault_table_no_user);
		m_write_fault_table = ((m_mode == eARM7_MODE_USER) ? s_write_fault_table_user : s_write_fault_table_no_user);
		//m_read_fault_word = ((m_mode == eARM7_MODE_USER) ? s_read_fault_word_user : s_read_fault_word_no_user);
		//m_write_fault_word = ((m_mode == eARM7_MODE_USER) ? s_write_fault_word_user : s_write_fault_word_no_user);
		update_reg_ptr();
	}
}


/**************************************************************************
 * ARM TLB IMPLEMENTATION
 **************************************************************************/

// COARSE, desc_level1, vaddr
uint32_t arm7_cpu_device::arm7_tlb_get_second_level_descriptor(uint32_t granularity, uint32_t vaddr)
{
	uint32_t first_desc = m_tlb_base[vaddr >> COPRO_TLB_VADDR_FLTI_MASK_SHIFT];
	switch (granularity)
	{
		case TLB_COARSE:
			return m_program->read_dword((first_desc & COPRO_TLB_CFLD_ADDR_MASK) | ((vaddr & COPRO_TLB_VADDR_CSLTI_MASK) >> COPRO_TLB_VADDR_CSLTI_MASK_SHIFT));
		case TLB_FINE:
			return m_program->read_dword((first_desc & COPRO_TLB_FPTB_ADDR_MASK) | ((vaddr & COPRO_TLB_VADDR_FSLTI_MASK) >> COPRO_TLB_VADDR_FSLTI_MASK_SHIFT));
		default:
			// We shouldn't be here
			LOG( ( "ARM7: Attempting to get second-level TLB descriptor of invalid granularity (%d)\n", granularity ) );
			return 0;
	}
}

int arm7_cpu_device::decode_fault(int user_mode, int ap, int access_control, int system, int rom, int write)
{
	switch (access_control & 3)
	{
		case 0 : // "No access - Any access generates a domain fault"
			return FAULT_DOMAIN;

		case 1 : // "Client - Accesses are checked against the access permission bits in the section or page descriptor"
			switch (ap & 3)
			{
				case 0:
					if (system)
					{
						if (rom) // "Reserved" -> assume same behaviour as S=0/R=0 case
						{
							return FAULT_PERMISSION;
						}
						else // "Only Supervisor read permitted"
						{
							if (user_mode != 0 || write != 0)
							{
								return FAULT_PERMISSION;
							}
						}
					}
					else
					{
						if (rom) // "Any write generates a permission fault"
						{
							if (write != 0)
							{
								return FAULT_PERMISSION;
							}
						}
						else // "Any access generates a permission fault"
						{
							return FAULT_PERMISSION;
						}
					}
					return FAULT_NONE;
				case 1:
					if (user_mode != 0)
					{
						return FAULT_PERMISSION;
					}
					break;
				case 2:
					if (user_mode != 0 && write != 0)
					{
						return FAULT_PERMISSION;
					}
					break;
				case 3:
					return FAULT_NONE;
			}
			return FAULT_NONE;

		case 2 : // "Reserved - Reserved. Currently behaves like the no access mode"
			return FAULT_DOMAIN;

		case 3 : // "Manager - Accesses are not checked against the access permission bits so a permission fault cannot be generated"
			return FAULT_NONE;
	}
	return FAULT_NONE;
}

int arm7_cpu_device::detect_read_fault(uint32_t desc_index, uint32_t ap)
{
#if 1
	const uint32_t index = ap | m_decoded_access_control[desc_index];
	return m_read_fault_table[index];//(m_read_fault_word >> index) & 3;
#endif
#if 0
	switch (m_decoded_access_control[desc_index])
	{
		case 0 : // "No access - Any access generates a domain fault"
			return FAULT_DOMAIN;

		case 1 : // "Client - Accesses are checked against the access permission bits in the section or page descriptor"
			switch (ap)
			{
				case 0:
					if (m_control & COPRO_CTRL_SYSTEM)
					{
						if (m_control & COPRO_CTRL_ROM) // "Reserved" -> assume same behaviour as S=0/R=0 case
						{
							return FAULT_PERMISSION;
						}
						else // "Only Supervisor read permitted"
						{
							if (m_user_mode)
							{
								return FAULT_PERMISSION;
							}
						}
					}
					else
					{
						if (m_control & COPRO_CTRL_ROM) // "Any write generates a permission fault"
							return FAULT_NONE;
						else // "Any access generates a permission fault"
							return FAULT_PERMISSION;
					}
					return FAULT_NONE;
				case 1:
					if (m_user_mode)
					{
						return FAULT_PERMISSION;
					}
					break;
				case 2:
					return FAULT_NONE;
				case 3:
					return FAULT_NONE;
			}
			return FAULT_NONE;

		case 2 : // "Reserved - Reserved. Currently behaves like the no access mode"
			return FAULT_DOMAIN;

		case 3 : // "Manager - Accesses are not checked against the access permission bits so a permission fault cannot be generated"
			return FAULT_NONE;
	}
	return FAULT_NONE;
#endif
}

// Bits:
// 5: ARM7_TLB_WRITE
// 4..3: ap
//    2: User mode
// 1..0: decoded access control
template <arm7_cpu_device::tlb_rw_mode WRITE>
int arm7_cpu_device::detect_fault(uint32_t desc_index, uint32_t ap)
{
#if 1
	const uint32_t index = ap | m_decoded_access_control[desc_index];
	if (WRITE)
		return m_write_fault_table[index];//(m_write_fault_word >> index) & 3;
	else
		return m_read_fault_table[index];//(m_read_fault_word >> index) & 3;
#else
	switch (m_decoded_access_control[desc_index])
	{
		case 0 : // "No access - Any access generates a domain fault"
			return FAULT_DOMAIN;

		case 1 : // "Client - Accesses are checked against the access permission bits in the section or page descriptor"
			switch (ap & 3)
			{
				case 0:
					if (m_control & COPRO_CTRL_SYSTEM)
					{
						if (m_control & COPRO_CTRL_ROM) // "Reserved" -> assume same behaviour as S=0/R=0 case
						{
							return FAULT_PERMISSION;
						}
						else // "Only Supervisor read permitted"
						{
							if (WRITE || m_user_mode)
							{
								return FAULT_PERMISSION;
							}
						}
					}
					else
					{
						if (m_control & COPRO_CTRL_ROM) // "Any write generates a permission fault"
						{
							if (WRITE)
							{
								return FAULT_PERMISSION;
							}
						}
						else // "Any access generates a permission fault"
						{
							return FAULT_PERMISSION;
						}
					}
					return FAULT_NONE;
				case 1:
					if (m_user_mode)
					{
						return FAULT_PERMISSION;
					}
					break;
				case 2:
					if (WRITE && m_user_mode)
					{
						return FAULT_PERMISSION;
					}
					break;
				case 3:
					return FAULT_NONE;
			}
			return FAULT_NONE;

		case 2 : // "Reserved - Reserved. Currently behaves like the no access mode"
			return FAULT_DOMAIN;

		case 3 : // "Manager - Accesses are not checked against the access permission bits so a permission fault cannot be generated"
			return FAULT_NONE;
	}
	return FAULT_NONE;
#endif
}

template <arm7_cpu_device::check_mode DO_FETCH, arm7_cpu_device::insn_mode THUMB, arm7_cpu_device::pid_mode CHECK_PID>
uint32_t arm7_cpu_device::arm7_tlb_translate_check(offs_t addr)
{
	if (CHECK_PID && addr < 0x2000000)
	{
		addr += m_pid_offset;
	}

	const uint32_t entry_index = addr >> COPRO_TLB_VADDR_FLTI_MASK_SHIFT;//
	//const translation_entry &entry = m_translated_table[addr >> COPRO_TLB_VADDR_FLTI_MASK_SHIFT];

	if (m_lvl1_type[entry_index] == COPRO_TLB_SECTION_TABLE)
	{
		// Entry is a section
		//if (s_read_fault_table[m_decoded_access_control[(desc_lvl1 >> 5) & 0xf] | ((desc_lvl1 >> 8) & 0xc)] == FAULT_NONE)
		if (m_section_read_fault[entry_index] == FAULT_NONE)
		{
			if (THUMB)
				return m_direct->read_word(m_section_bits[entry_index] | (addr & ~COPRO_TLB_SECTION_PAGE_MASK));
			else
				return m_direct->read_dword(m_section_bits[entry_index] | (addr & ~COPRO_TLB_SECTION_PAGE_MASK));
		}
		return 0;
	}
	else if (m_lvl1_type[entry_index] == COPRO_TLB_UNMAPPED)
	{
		return 0;
	}
	else
	{
		// Entry is the physical address of a coarse second-level table
		const uint8_t permission = (m_domainAccessControl >> (m_dac_index[entry_index] << 1)) & 3;
		const uint32_t desc_lvl2 = arm7_tlb_get_second_level_descriptor(m_lvl1_type[entry_index] == COPRO_TLB_COARSE_TABLE ? TLB_COARSE : TLB_FINE, addr);
		if ((permission != 1) && (permission != 3))
		{
			uint8_t domain = m_dac_index[entry_index];
			fatalerror("ARM7: Not Yet Implemented: Coarse Table, Section Domain fault on virtual address, vaddr = %08x, domain = %08x, PC = %08x\n", addr, domain, m_r[eR15]);
		}

		switch (desc_lvl2 & 3)
		{
			case COPRO_TLB_UNMAPPED:
				return 0;
			case COPRO_TLB_LARGE_PAGE:
				// Large page descriptor
				if (THUMB)
					return m_direct->read_word((desc_lvl2 & COPRO_TLB_LARGE_PAGE_MASK) | (addr & ~COPRO_TLB_LARGE_PAGE_MASK));
				else
					return m_direct->read_dword((desc_lvl2 & COPRO_TLB_LARGE_PAGE_MASK) | (addr & ~COPRO_TLB_LARGE_PAGE_MASK));

			case COPRO_TLB_SMALL_PAGE:
			{
				// Small page descriptor
				uint8_t ap = ((((desc_lvl2 >> 4) & 0xFF) >> (((addr >> 10) & 3) << 1)) & 3) << 2;
				if (detect_read_fault(m_dac_index[entry_index], ap) == FAULT_NONE)
				{
					if (THUMB)
						return m_direct->read_word((desc_lvl2 & COPRO_TLB_SMALL_PAGE_MASK) | (addr & ~COPRO_TLB_SMALL_PAGE_MASK));
					else
						return m_direct->read_dword((desc_lvl2 & COPRO_TLB_SMALL_PAGE_MASK) | (addr & ~COPRO_TLB_SMALL_PAGE_MASK));
				}
				return 0;
			}
			case COPRO_TLB_TINY_PAGE:
				// Tiny page descriptor
				if (m_lvl1_type[entry_index] == 1)
				{
					LOG( ( "ARM7: It would appear that we're looking up a tiny page from a coarse TLB lookup.  This is bad. vaddr = %08x\n", addr ) );
				}
				if (THUMB)
					return m_direct->read_word((desc_lvl2 & COPRO_TLB_TINY_PAGE_MASK) | (addr & ~COPRO_TLB_TINY_PAGE_MASK));
				else
					return m_direct->read_dword((desc_lvl2 & COPRO_TLB_TINY_PAGE_MASK) | (addr & ~COPRO_TLB_TINY_PAGE_MASK));
		}
	}
#if 0
	switch (m_lvl1_type[entry_index])
	{
		case COPRO_TLB_UNMAPPED:
			return false;

		case COPRO_TLB_SECTION_TABLE:
		{
			// Entry is a section
			int fault = detect_read_fault(m_dac_index[entry_index], m_lvl1_ap[entry_index]);
			addr = (desc_lvl1 & COPRO_TLB_SECTION_PAGE_MASK) | (addr & ~COPRO_TLB_SECTION_PAGE_MASK);
			return (fault == FAULT_NONE)
		}

		case COPRO_TLB_COARSE_TABLE:
		case COPRO_TLB_FINE_TABLE:
		{
			// Entry is the physical address of a coarse second-level table
			const uint8_t permission = (m_domainAccessControl >> (m_dac_index[entry_index] << 1)) & 3;
			const uint32_t desc_lvl2 = arm7_tlb_get_second_level_descriptor(m_lvl1_type[entry_index] == COPRO_TLB_COARSE_TABLE ? TLB_COARSE : TLB_FINE, addr );
			if ((permission != 1) && (permission != 3))
			{
				uint8_t domain = m_dac_index[entry_index];
				fatalerror("ARM7: Not Yet Implemented: Coarse Table, Section Domain fault on virtual address, vaddr = %08x, domain = %08x, PC = %08x\n", addr, domain, m_r[eR15]);
			}

			switch (desc_lvl2 & 3)
			{
				case COPRO_TLB_UNMAPPED:
					return false;
				case COPRO_TLB_LARGE_PAGE:
					// Large page descriptor
					addr = (desc_lvl2 & COPRO_TLB_LARGE_PAGE_MASK) | (addr & ~COPRO_TLB_LARGE_PAGE_MASK);
					return true;
				case COPRO_TLB_SMALL_PAGE:
				{
					// Small page descriptor
					const uint32_t ap = ((((desc_lvl2 >> 4) & 0xFF) >> (((addr >> 10) & 3) << 1)) & 3) << 2;
					int fault = detect_read_fault(m_dac_index[entry_index], ap);
					addr = (desc_lvl2 & COPRO_TLB_SMALL_PAGE_MASK) | (addr & ~COPRO_TLB_SMALL_PAGE_MASK);
					return fault == FAULT_NONE;
				}
				case COPRO_TLB_TINY_PAGE:
					// Tiny page descriptor
					if ((desc_lvl1 & 3) == 1)
					{
						LOG( ( "ARM7: It would appear that we're looking up a tiny page from a coarse TLB lookup.  This is bad. vaddr = %08x\n", addr ) );
					}
					addr = (desc_lvl2 & COPRO_TLB_TINY_PAGE_MASK) | (addr & ~COPRO_TLB_TINY_PAGE_MASK);
					return true;
			}
		}
	}
#endif
	return true;
}

template <arm7_cpu_device::tlb_rw_mode WRITE>
bool arm7_cpu_device::arm7_tlb_translate(offs_t &addr)
{
	if (m_pid_offset != 0 && addr < 0x2000000)
	{
		addr += m_pid_offset;
	}

	const uint32_t entry_index = addr >> COPRO_TLB_VADDR_FLTI_MASK_SHIFT;
	//const translation_entry &entry = m_translated_table[addr >> COPRO_TLB_VADDR_FLTI_MASK_SHIFT];

	if (!WRITE && m_early_faultless[entry_index])
	{
		addr = m_section_bits[entry_index] | (addr & ~COPRO_TLB_SECTION_PAGE_MASK);
		return true;
	}

	if (m_lvl1_type[entry_index] == COPRO_TLB_SECTION_TABLE)
	{
		// Entry is a section
		const uint8_t fault = WRITE ? m_section_write_fault[entry_index] : m_section_read_fault[entry_index];
		if (fault == FAULT_NONE)
		{
			addr = m_section_bits[entry_index] | (addr & ~COPRO_TLB_SECTION_PAGE_MASK);
			return true;
		}
		else
		{
			uint8_t domain = m_dac_index[entry_index];
			m_faultStatus[0] = ((fault == FAULT_DOMAIN) ? (9 << 0) : (13 << 0)) | (domain << 4); // 9 = section domain fault, 13 = section permission fault
			m_faultAddress = addr;
			m_pendingAbtD = true;
			m_pending_interrupt = true;
			return false;
		}
	}
	else if (m_lvl1_type[entry_index] == COPRO_TLB_UNMAPPED)
	{
		// Unmapped, generate a translation fault
		LOG( ( "ARM7: Translation fault on unmapped virtual address, PC = %08x, vaddr = %08x\n", m_r[eR15], addr ) );
		m_faultStatus[0] = 5; // 5 = section translation fault
		m_faultAddress = addr;
		m_pendingAbtD = true;
		m_pending_interrupt = true;
		return false;
	}
	else
	{
		// Entry is the physical address of a coarse second-level table
		const uint8_t permission = (m_domainAccessControl >> (m_dac_index[entry_index] << 1)) & 3;
		const uint32_t desc_lvl2 = arm7_tlb_get_second_level_descriptor(m_lvl1_type[entry_index] == COPRO_TLB_COARSE_TABLE ? TLB_COARSE : TLB_FINE, addr);
		if ((permission != 1) && (permission != 3))
		{
			uint8_t domain = m_dac_index[entry_index];
			fatalerror("ARM7: Not Yet Implemented: Coarse Table, Section Domain fault on virtual address, vaddr = %08x, domain = %08x, PC = %08x\n", addr, domain, m_r[eR15]);
		}

		switch (desc_lvl2 & 3)
		{
			case COPRO_TLB_UNMAPPED:
			{
				// Unmapped, generate a translation fault
				uint8_t domain = m_dac_index[entry_index];
				LOG( ( "ARM7: Translation fault on unmapped virtual address, vaddr = %08x, PC %08X\n", addr, m_r[eR15] ) );
				m_faultStatus[0] = (7 << 0) | (domain << 4); // 7 = page translation fault
				m_faultAddress = addr;
				m_pendingAbtD = true;
				m_pending_interrupt = true;
				return false;
			}
			case COPRO_TLB_LARGE_PAGE:
				// Large page descriptor
				addr = (desc_lvl2 & COPRO_TLB_LARGE_PAGE_MASK) | (addr & ~COPRO_TLB_LARGE_PAGE_MASK);
				break;
			case COPRO_TLB_SMALL_PAGE:
				// Small page descriptor
				{
					uint8_t ap = ((((desc_lvl2 >> 4) & 0xFF) >> (((addr >> 10) & 3) << 1)) & 3) << 2;
					const uint8_t fault = detect_fault<WRITE>(m_dac_index[entry_index], ap);
					if (fault == FAULT_NONE)
					{
						addr = (desc_lvl2 & COPRO_TLB_SMALL_PAGE_MASK) | (addr & ~COPRO_TLB_SMALL_PAGE_MASK);
					}
					else
					{
						uint8_t domain = m_dac_index[entry_index];
						// hapyfish expects a data abort when something tries to write to a read-only memory location from user mode
						m_faultStatus[0] = ((fault == FAULT_DOMAIN) ? (11 << 0) : (15 << 0)) | (domain << 4); // 11 = page domain fault, 15 = page permission fault
						m_faultAddress = addr;
						m_pendingAbtD = true;
						m_pending_interrupt = true;
						return false;
					}
				}
				break;
			case COPRO_TLB_TINY_PAGE:
				// Tiny page descriptor
				if (m_lvl1_type[entry_index] == 1)
				{
					LOG( ( "ARM7: It would appear that we're looking up a tiny page from a coarse TLB lookup.  This is bad. vaddr = %08x\n", addr ) );
				}
				addr = (desc_lvl2 & COPRO_TLB_TINY_PAGE_MASK) | (addr & ~COPRO_TLB_TINY_PAGE_MASK);
				break;
		}
		return true;
	}

#if 0
	switch (desc_lvl1 & 3)
	{
		case COPRO_TLB_UNMAPPED:
			// Unmapped, generate a translation fault
			LOG( ( "ARM7: Translation fault on unmapped virtual address, PC = %08x, vaddr = %08x\n", m_r[eR15], addr ) );
			m_faultStatus[0] = (5 << 0); // 5 = section translation fault
			m_faultAddress = addr;
			m_pendingAbtD = true;
			m_pending_interrupt = true;
			return false;

		case COPRO_TLB_SECTION_TABLE:
		{
			// Entry is a section
			const uint8_t = detect_fault<WRITE>((desc_lvl1 >> 5) & 0xf, (desc_lvl1 >> 8) & 0xc);
			if (fault == FAULT_NONE)
			{
				addr = ( desc_lvl1 & COPRO_TLB_SECTION_PAGE_MASK ) | ( addr & ~COPRO_TLB_SECTION_PAGE_MASK );
			}
			else
			{
				uint8_t domain = (desc_lvl1 >> 5) & 0xF;
				m_faultStatus[0] = ((fault == FAULT_DOMAIN) ? (9 << 0) : (13 << 0)) | (domain << 4); // 9 = section domain fault, 13 = section permission fault
				m_faultAddress = addr;
				m_pendingAbtD = true;
				m_pending_interrupt = true;
				return false;
			}
			return true;
		}

		case COPRO_TLB_COARSE_TABLE:
		case COPRO_TLB_FINE_TABLE:
		{
			// Entry is the physical address of a coarse second-level table
			const uint8_t permission = (m_domainAccessControl >> (m_dac_index[entry_index] << 1)) & 3;
			const uint32_t desc_lvl2 = arm7_tlb_get_second_level_descriptor(m_lvl1_type[entry_index] == COPRO_TLB_COARSE_TABLE ? TLB_COARSE : TLB_FINE, addr);
			if ((permission != 1) && (permission != 3))
			{
				uint8_t domain = (desc_lvl1 >> 5) & 0xF;
				fatalerror("ARM7: Not Yet Implemented: Coarse Table, Section Domain fault on virtual address, vaddr = %08x, domain = %08x, PC = %08x\n", addr, domain, m_r[eR15]);
			}

			switch (desc_lvl2 & 3)
			{
				case COPRO_TLB_UNMAPPED:
					// Unmapped, generate a translation fault
					uint8_t domain = (desc_lvl1 >> 5) & 0xF;
					LOG( ( "ARM7: Translation fault on unmapped virtual address, vaddr = %08x, PC %08X\n", addr, m_r[eR15] ) );
					m_faultStatus[0] = (7 << 0) | (domain << 4); // 7 = page translation fault
					m_faultAddress = addr;
					m_pendingAbtD = true;
					m_pending_interrupt = true;
					return false;
				case COPRO_TLB_LARGE_PAGE:
					// Large page descriptor
					addr = (desc_lvl2 & COPRO_TLB_LARGE_PAGE_MASK) | (addr & ~COPRO_TLB_LARGE_PAGE_MASK);
					break;
				case COPRO_TLB_SMALL_PAGE:
					// Small page descriptor
					{
						uint8_t ap = ((((desc_lvl2 >> 4) & 0xFF) >> (((addr >> 10) & 3) << 1)) & 3) << 2;
						const uint8_t = detect_fault<WRITE>((desc_lvl1 >> 5) & 0xf, ap);
						if (fault == FAULT_NONE)
						{
							addr = (desc_lvl2 & COPRO_TLB_SMALL_PAGE_MASK) | (addr & ~COPRO_TLB_SMALL_PAGE_MASK);
						}
						else
						{
							uint8_t domain = (desc_lvl1 >> 5) & 0xF;
							// hapyfish expects a data abort when something tries to write to a read-only memory location from user mode
							m_faultStatus[0] = ((fault == FAULT_DOMAIN) ? (11 << 0) : (15 << 0)) | (domain << 4); // 11 = page domain fault, 15 = page permission fault
							m_faultAddress = addr;
							m_pendingAbtD = true;
							m_pending_interrupt = true;
							return false;
						}
					}
					break;
				case COPRO_TLB_TINY_PAGE:
					// Tiny page descriptor
					if ((desc_lvl1 & 3) == 1)
					{
						LOG( ( "ARM7: It would appear that we're looking up a tiny page from a coarse TLB lookup.  This is bad. vaddr = %08x\n", addr ) );
					}
					addr = (desc_lvl2 & COPRO_TLB_TINY_PAGE_MASK) | (addr & ~COPRO_TLB_TINY_PAGE_MASK);
					break;
			}
			return true;
		}
	}
#endif
	return true;
}


bool arm7_cpu_device::memory_translate(int spacenum, int intention, offs_t &address)
{
	/* only applies to the program address space and only does something if the MMU's enabled */
	if( spacenum == AS_PROGRAM && ( m_control & COPRO_CTRL_MMU_EN ) )
	{
		return arm7_tlb_translate<TLB_READ>(address);
	}
	return true;
}

#include "arm7core.hxx"
#include "arm7thmb.hxx"

/***************************************************************************
 * CPU SPECIFIC IMPLEMENTATIONS
 **************************************************************************/

void arm7_cpu_device::postload()
{
	update_reg_ptr();
}

void arm7_cpu_device::device_start()
{
	//m_core = (internal_arm_state *)m_cache.alloc_near(sizeof(internal_arm_state));
	//memset(m_core, 0, sizeof(internal_arm_state));

	m_enable_drc = false;//allow_drc();

	m_prefetch_word0_shift = (m_endian == ENDIANNESS_LITTLE ? 0 : 16);
	m_prefetch_word1_shift = (m_endian == ENDIANNESS_LITTLE ? 16 : 0);

	// TODO[RH]: Default to 3-instruction prefetch for unknown ARM variants. Derived cores should set the appropriate value in their constructors.
	m_insn_prefetch_depth = 3;

	memset(m_insn_prefetch_buffer, 0, sizeof(uint32_t) * 3);
	memset(m_insn_prefetch_address, 0, sizeof(uint32_t) * 3);
	memset(m_insn_prefetch_translated, 0, sizeof(uint32_t) * 3);
	m_insn_prefetch_count = 0;
	m_insn_prefetch_index = 0;

	m_program = &space(AS_PROGRAM);
	m_direct = m_program->direct<0>();
	m_tlb_base = (uint32_t*)m_direct->read_ptr(0);

	save_item(NAME(m_insn_prefetch_depth));
	save_item(NAME(m_insn_prefetch_count));
	save_item(NAME(m_insn_prefetch_index));
	save_item(NAME(m_insn_prefetch_buffer));
	save_item(NAME(m_insn_prefetch_address));
	save_item(NAME(m_r));
	save_item(NAME(m_pendingIrq));
	save_item(NAME(m_pendingFiq));
	save_item(NAME(m_pendingAbtD));
	save_item(NAME(m_pendingAbtP));
	save_item(NAME(m_pendingUnd));
	save_item(NAME(m_pendingSwi));
	save_item(NAME(m_pending_interrupt));
	save_item(NAME(m_control));
	save_item(NAME(m_tlbBase));
	save_item(NAME(m_tlb_base_mask));
	save_item(NAME(m_faultStatus));
	save_item(NAME(m_faultAddress));
	save_item(NAME(m_fcsePID));
	save_item(NAME(m_pid_offset));
	save_item(NAME(m_domainAccessControl));
	save_item(NAME(m_decoded_access_control));
	machine().save().register_postload(save_prepost_delegate(FUNC(arm7_cpu_device::postload), this));

	m_icountptr = &m_icount;

	/*
	uint32_t umlflags = 0;
	m_drcuml = std::make_unique<drcuml_state>(*this, m_cache, umlflags, 1, 32, 1);

	// add UML symbols
	m_drcuml->symbol_add(&m_core->m_r[eR15], sizeof(uint32_t), "pc");
	char buf[4];
	for (int i = 0; i < 16; i++)
	{
		sprintf(buf, "r%d", i);
		m_drcuml->symbol_add(&m_core->m_r[i], sizeof(uint32_t), buf);
	}
	m_drcuml->symbol_add(&m_core->m_cpsr, sizeof(uint32_t), "sr");
	m_drcuml->symbol_add(&m_core->m_r[eR8_FIQ], sizeof(uint32_t), "r8_fiq");
	m_drcuml->symbol_add(&m_core->m_r[eR9_FIQ], sizeof(uint32_t), "r9_fiq");
	m_drcuml->symbol_add(&m_core->m_r[eR10_FIQ], sizeof(uint32_t), "r10_fiq");
	m_drcuml->symbol_add(&m_core->m_r[eR11_FIQ], sizeof(uint32_t), "r11_fiq");
	m_drcuml->symbol_add(&m_core->m_r[eR12_FIQ], sizeof(uint32_t), "r12_fiq");
	m_drcuml->symbol_add(&m_core->m_r[eR13_FIQ], sizeof(uint32_t), "r13_fiq");
	m_drcuml->symbol_add(&m_core->m_r[eR14_FIQ], sizeof(uint32_t), "r14_fiq");
	m_drcuml->symbol_add(&m_core->m_r[eSPSR_FIQ], sizeof(uint32_t), "spsr_fiq");
	m_drcuml->symbol_add(&m_core->m_r[eR13_IRQ], sizeof(uint32_t), "r13_irq");
	m_drcuml->symbol_add(&m_core->m_r[eR14_IRQ], sizeof(uint32_t), "r14_irq");
	m_drcuml->symbol_add(&m_core->m_r[eSPSR_IRQ], sizeof(uint32_t), "spsr_irq");
	m_drcuml->symbol_add(&m_core->m_r[eR13_SVC], sizeof(uint32_t), "r13_svc");
	m_drcuml->symbol_add(&m_core->m_r[eR14_SVC], sizeof(uint32_t), "r14_svc");
	m_drcuml->symbol_add(&m_core->m_r[eSPSR_SVC], sizeof(uint32_t), "spsr_svc");
	m_drcuml->symbol_add(&m_core->m_r[eR13_ABT], sizeof(uint32_t), "r13_abt");
	m_drcuml->symbol_add(&m_core->m_r[eR14_ABT], sizeof(uint32_t), "r14_abt");
	m_drcuml->symbol_add(&m_core->m_r[eSPSR_ABT], sizeof(uint32_t), "spsr_abt");
	m_drcuml->symbol_add(&m_core->m_r[eR13_UND], sizeof(uint32_t), "r13_und");
	m_drcuml->symbol_add(&m_core->m_r[eR14_UND], sizeof(uint32_t), "r14_und");
	m_drcuml->symbol_add(&m_core->m_r[eSPSR_UND], sizeof(uint32_t), "spsr_und");
	m_drcuml->symbol_add(&m_core->m_icount, sizeof(int), "icount");

	// initialize the front-end helper
	m_drcfe = std::make_unique<arm7_frontend>(this, COMPILE_BACKWARDS_BYTES, COMPILE_FORWARDS_BYTES, SINGLE_INSTRUCTION_MODE ? 1 : COMPILE_MAX_SEQUENCE);

	// mark the cache dirty so it is updated on next execute
	m_cache_dirty = true;*/

	state_add( ARM7_PC,    "PC", m_pc).callexport().formatstr("%08X");
	state_add(STATE_GENPC, "GENPC", m_pc).callexport().noshow();
	state_add(STATE_GENPCBASE, "CURPC", m_pc).callexport().noshow();
	/* registers shared by all operating modes */
	state_add( ARM7_R0,    "R0",   m_r[ 0]).formatstr("%08X");
	state_add( ARM7_R1,    "R1",   m_r[ 1]).formatstr("%08X");
	state_add( ARM7_R2,    "R2",   m_r[ 2]).formatstr("%08X");
	state_add( ARM7_R3,    "R3",   m_r[ 3]).formatstr("%08X");
	state_add( ARM7_R4,    "R4",   m_r[ 4]).formatstr("%08X");
	state_add( ARM7_R5,    "R5",   m_r[ 5]).formatstr("%08X");
	state_add( ARM7_R6,    "R6",   m_r[ 6]).formatstr("%08X");
	state_add( ARM7_R7,    "R7",   m_r[ 7]).formatstr("%08X");
	state_add( ARM7_R8,    "R8",   m_r[ 8]).formatstr("%08X");
	state_add( ARM7_R9,    "R9",   m_r[ 9]).formatstr("%08X");
	state_add( ARM7_R10,   "R10",  m_r[10]).formatstr("%08X");
	state_add( ARM7_R11,   "R11",  m_r[11]).formatstr("%08X");
	state_add( ARM7_R12,   "R12",  m_r[12]).formatstr("%08X");
	state_add( ARM7_R13,   "R13",  m_r[13]).formatstr("%08X");
	state_add( ARM7_R14,   "R14",  m_r[14]).formatstr("%08X");
	state_add( ARM7_R15,   "R15",  m_r[15]).formatstr("%08X");
	/* FIRQ Mode Shadowed Registers */
	state_add( ARM7_FR8,   "FR8",  m_r[eR8_FIQ]  ).formatstr("%08X");
	state_add( ARM7_FR9,   "FR9",  m_r[eR9_FIQ]  ).formatstr("%08X");
	state_add( ARM7_FR10,  "FR10", m_r[eR10_FIQ] ).formatstr("%08X");
	state_add( ARM7_FR11,  "FR11", m_r[eR11_FIQ] ).formatstr("%08X");
	state_add( ARM7_FR12,  "FR12", m_r[eR12_FIQ] ).formatstr("%08X");
	state_add( ARM7_FR13,  "FR13", m_r[eR13_FIQ] ).formatstr("%08X");
	state_add( ARM7_FR14,  "FR14", m_r[eR14_FIQ] ).formatstr("%08X");
	state_add( ARM7_FSPSR, "FR16", m_r[eSPSR_FIQ]).formatstr("%08X");
	/* IRQ Mode Shadowed Registers */
	state_add( ARM7_IR13,  "IR13", m_r[eR13_IRQ] ).formatstr("%08X");
	state_add( ARM7_IR14,  "IR14", m_r[eR14_IRQ] ).formatstr("%08X");
	state_add( ARM7_ISPSR, "IR16", m_r[eSPSR_IRQ]).formatstr("%08X");
	/* Supervisor Mode Shadowed Registers */
	state_add( ARM7_SR13,  "SR13", m_r[eR13_SVC] ).formatstr("%08X");
	state_add( ARM7_SR14,  "SR14", m_r[eR14_SVC] ).formatstr("%08X");
	state_add( ARM7_SSPSR, "SR16", m_r[eSPSR_SVC]).formatstr("%08X");
	/* Abort Mode Shadowed Registers */
	state_add( ARM7_AR13,  "AR13", m_r[eR13_ABT] ).formatstr("%08X");
	state_add( ARM7_AR14,  "AR14", m_r[eR14_ABT] ).formatstr("%08X");
	state_add( ARM7_ASPSR, "AR16", m_r[eSPSR_ABT]).formatstr("%08X");
	/* Undefined Mode Shadowed Registers */
	state_add( ARM7_UR13,  "UR13", m_r[eR13_UND] ).formatstr("%08X");
	state_add( ARM7_UR14,  "UR14", m_r[eR14_UND] ).formatstr("%08X");
	state_add( ARM7_USPSR, "UR16", m_r[eSPSR_UND]).formatstr("%08X");

	state_add(STATE_GENFLAGS, "GENFLAGS", m_cpsr).formatstr("%13s").noshow();

	update_fault_table();
	calculate_nvc_flags();

	for (uint32_t mode = 0; mode < ARM7_NUM_MODES; mode++)
	{
		for (uint32_t reg = 0; reg < 17; reg++)
		{
			m_register_pointers[mode][reg] = &m_r[s_register_table[mode][reg]];
		}
	}
}

void arm7_cpu_device::device_stop()
{
	if (m_drcfe != nullptr)
	{
		m_drcfe = nullptr;
	}
	if (m_drcuml != nullptr)
	{
		m_drcuml = nullptr;
	}
}

void arm7_cpu_device::calculate_nvc_flags()
{
	for (uint32_t rn = 0; rn < 2; rn++)
	{
		for (uint32_t op2 = 0; op2 < 2; op2++)
		{
			for (uint32_t rd = 0; rd < 2; rd++)
			{
				s_add_nvc_flags[(rn << 2) | (op2 << 1) | rd]
					= (rd ? N_MASK : 0)
					| (((~(rn ^ op2) & (rn ^ rd)) & 1) ? V_MASK : 0)
					| ((((rn & op2) | (rn & ~rd) | (op2 & ~rd)) & 1) ? C_MASK : 0);
				s_sub_nvc_flags[(rn << 2) | (op2 << 1) | rd]
					= (rd ? N_MASK : 0)
					| ((((rn ^ op2) & (rn ^ rd)) & 1) ? V_MASK : 0)
					| ((((rn & ~op2) | (rn & ~rd) | (~op2 & ~rd)) & 1) ? C_MASK : 0);
			}
		}
	}
}

void arm7_cpu_device::update_fault_table()
{
	s_read_fault_word_user = 0;
	s_read_fault_word_no_user = 0;
	s_write_fault_word_user = 0;
	s_write_fault_word_no_user = 0;
	for (uint8_t ap = 0; ap < 4; ap++)
	{
		for (uint8_t access_control = 0; access_control < 4; access_control++)
		{
			uint8_t system = (m_control & COPRO_CTRL_SYSTEM) ? 1 : 0;
			uint8_t rom = (m_control & COPRO_CTRL_ROM) ? 1 : 0;
			const uint32_t index = (ap << 2) | access_control;
			s_read_fault_word_user |= (s_read_fault_table_user[index] = decode_fault(1, ap, access_control, system, rom, 0)) << (index << 1);
			s_read_fault_word_no_user |= (s_read_fault_table_no_user[index] = decode_fault(0, ap, access_control, system, rom, 0)) << (index << 1);
			s_write_fault_word_user |= (s_write_fault_table_user[index] = decode_fault(1, ap, access_control, system, rom, 1)) << (index << 1);
			s_write_fault_word_no_user |= (s_write_fault_table_no_user[index] = decode_fault(0, ap, access_control, system, rom, 1)) << (index << 1);
		}
	}
}

void arm946es_cpu_device::device_start()
{
	arm9_cpu_device::device_start();

	save_item(NAME(cp15_control));
	save_item(NAME(cp15_itcm_base));
	save_item(NAME(cp15_dtcm_base));
	save_item(NAME(cp15_itcm_size));
	save_item(NAME(cp15_dtcm_size));
	save_item(NAME(cp15_itcm_end));
	save_item(NAME(cp15_dtcm_end));
	save_item(NAME(cp15_itcm_reg));
	save_item(NAME(cp15_dtcm_reg));
	save_item(NAME(ITCM));
	save_item(NAME(DTCM));
}


void arm7_cpu_device::state_export(const device_state_entry &entry)
{
	switch (entry.index())
	{
		case STATE_GENPC:
		case STATE_GENPCBASE:
			m_pc = GET_PC;
			break;
	}
}


void arm7_cpu_device::state_string_export(const device_state_entry &entry, std::string &str) const
{
	switch (entry.index())
	{
		case STATE_GENFLAGS:
			str = string_format("%c%c%c%c%c%c%c%c %s",
				(m_nflag)         ? 'N' : '-',
				(m_zflag)         ? 'Z' : '-',
				(m_cflag)         ? 'C' : '-',
				(m_vflag)         ? 'V' : '-',
				(m_cpsr & Q_MASK) ? 'Q' : '-',
				(m_cpsr & I_MASK) ? 'I' : '-',
				(m_cpsr & F_MASK) ? 'F' : '-',
				(m_tflag)         ? 'T' : '-',
				GetModeText(m_cpsr));
		break;
	}
}

void arm7_cpu_device::device_reset()
{
	memset(m_r, 0, sizeof(m_r));
	m_pendingIrq = false;
	m_pendingFiq = false;
	m_pendingAbtD = false;
	m_pendingAbtP = false;
	m_pendingUnd = false;
	m_pendingSwi = false;
	m_pending_interrupt = false;
	m_control = 0;
	m_tlbBase = 0;
	m_tlb_base_mask = 0;
	m_faultStatus[0] = 0;
	m_faultStatus[1] = 0;
	m_faultAddress = 0;
	m_fcsePID = 0;
	m_pid_offset = 0;
	m_domainAccessControl = 0;
	m_stashed_icount = -1;
	memset(m_decoded_access_control, 0, sizeof(uint8_t) * 16);

	/* start up in SVC mode with interrupts disabled. */
	set_cpsr(I_MASK | F_MASK | 0x10 | eARM7_MODE_SVC);
	m_r[eR15] = 0 | m_vectorbase;

	m_cache_dirty = true;
}


#define UNEXECUTED() \
	{							\
		m_r[eR15] += 4;	\
		m_icount--;	/* Any unexecuted instruction only takes 1 cycle (page 193) */	\
	}

template <arm7_cpu_device::pid_mode CHECK_PID>
void arm7_cpu_device::update_insn_prefetch_mmu(uint32_t curr_pc)
{
	if (m_insn_prefetch_address[m_insn_prefetch_index] != curr_pc)
	{
		m_insn_prefetch_count = 0;
		m_insn_prefetch_index = 0;
	}

	if (m_insn_prefetch_count == m_insn_prefetch_depth)
		return;

	const uint32_t to_fetch = m_insn_prefetch_depth - m_insn_prefetch_count;
	if (to_fetch == 0)
		return;

	uint32_t index = m_insn_prefetch_depth + (m_insn_prefetch_index - to_fetch);
	if (index >= m_insn_prefetch_depth) index -= m_insn_prefetch_depth;

	uint32_t pc = curr_pc + m_insn_prefetch_count * 4;
	uint32_t i = 0;
	for (; i < to_fetch; i++)
	{
		uint32_t translated_pc = pc;
		const uint32_t translated_insn = arm7_tlb_translate_check<NO_FETCH, ARM_MODE, CHECK_PID>(translated_pc);
		if (!translated_insn)
		{
			m_insn_prefetch_translated[index] = ~0;
			break;
		}
		m_insn_prefetch_buffer[index] = translated_insn;
		m_insn_prefetch_address[index] = pc;
		m_insn_prefetch_translated[index] = translated_pc;
		pc += 4;

		index++;
		if (index >= m_insn_prefetch_depth) index -= m_insn_prefetch_depth;
	}
	m_insn_prefetch_count += i;
}

void arm7_cpu_device::update_insn_prefetch(uint32_t curr_pc)
{
	if (m_insn_prefetch_address[m_insn_prefetch_index] != curr_pc)
	{
		m_insn_prefetch_count = 0;
		m_insn_prefetch_index = 0;
	}

	if (m_insn_prefetch_count == m_insn_prefetch_depth)
		return;

	const uint32_t to_fetch = m_insn_prefetch_depth - m_insn_prefetch_count;
	const uint32_t start_index = (m_insn_prefetch_depth + (m_insn_prefetch_index - to_fetch)) % m_insn_prefetch_depth;

	uint32_t pc = curr_pc + m_insn_prefetch_count * 4;
	uint32_t i = 0;
	for (; i < to_fetch; i++)
	{
		uint32_t index = (i + start_index) % m_insn_prefetch_depth;
		m_insn_prefetch_buffer[index] = m_direct->read_dword(pc);
		m_insn_prefetch_address[index] = pc;
		m_insn_prefetch_translated[index] = pc;
		pc += 4;
	}
	m_insn_prefetch_count += i;
}

uint32_t arm7_cpu_device::insn_fetch_thumb(uint32_t pc, bool& translated)
{
	translated = !(m_insn_prefetch_translated[m_insn_prefetch_index] & 1);
	if (pc & 2)
	{
		const uint32_t insn = (uint16_t)(m_insn_prefetch_buffer[m_insn_prefetch_index] >> m_prefetch_word1_shift);
		m_insn_prefetch_index = (m_insn_prefetch_index + 1) % m_insn_prefetch_count;
		m_insn_prefetch_count--;
		return insn;
	}
	return (uint16_t)(m_insn_prefetch_buffer[m_insn_prefetch_index] >> m_prefetch_word0_shift);
}

uint32_t arm7_cpu_device::insn_fetch_arm(uint32_t pc, bool& translated)
{
	translated = !(m_insn_prefetch_translated[m_insn_prefetch_index] & 1);
	const uint32_t insn = m_insn_prefetch_buffer[m_insn_prefetch_index];
	m_insn_prefetch_index++;
	if (m_insn_prefetch_index >= m_insn_prefetch_count)
		m_insn_prefetch_index -= m_insn_prefetch_count;
	m_insn_prefetch_count--;
	return insn;
}

int arm7_cpu_device::get_insn_prefetch_index(uint32_t address)
{
	address &= ~3;
	for (uint32_t i = 0; i < m_insn_prefetch_depth; i++)
	{
		if (m_insn_prefetch_address[i] == address)
		{
			return (int)i;
		}
	}
	return -1;
}

#include "arm7ops.hxx"

template <arm7_cpu_device::insn_mode THUMB,
		  arm7_cpu_device::copro_mode MMU_ENABLED,
		  arm7_cpu_device::prefetch_mode PREFETCH,
		  arm7_cpu_device::pid_mode CHECK_PID,
		  arm7_cpu_device::debug_mode DEBUG>
void arm7_cpu_device::execute_core()
{
	do
	{
		uint32_t pc = R15;
		//printf("%x: %x\n", pc & ~3, m_icount);

		if (DEBUG)
			debugger_instruction_hook(this, pc);

		if (THUMB)
		{
			// "In Thumb state, bit [0] is undefined and must be ignored. Bits [31:1] contain the PC."
			offs_t raddr = pc & (~1);

			if (MMU_ENABLED)
			{
				if (PREFETCH)
				{
					update_insn_prefetch_mmu<CHECK_PID>(raddr & ~3);

					bool translated = false;
					const uint32_t insn = insn_fetch_thumb(raddr, translated);
					if (translated)
					{
						execute_thumb_insn<MMU_ENABLED>(insn, pc);
						ARM7_ICOUNT -= 3;
					}
					else
					{
						m_pendingAbtP = true;
						m_pending_interrupt = true;
					}
				}
				else
				{
					const uint32_t word_pc = raddr & ~1;
					const uint32_t opcode = arm7_tlb_translate_check<FETCH, THUMB_MODE, CHECK_PID>(word_pc);
					if (opcode)
					{
						execute_thumb_insn<MMU_ENABLED>(opcode, pc);
						ARM7_ICOUNT -= 3;
					}
					else
					{
						m_pendingAbtP = true;
						m_pending_interrupt = true;
					}
				}
			}
			else
			{
				if (PREFETCH)
				{
					update_insn_prefetch(raddr & ~3);
					bool ignored = false;
					const uint32_t insn = insn_fetch_thumb(raddr, ignored);
					execute_thumb_insn<MMU_ENABLED>(insn, pc);
					ARM7_ICOUNT -= 3;
				}
				else
				{
					execute_thumb_insn<MMU_ENABLED>(m_direct->read_word(raddr), pc);
					ARM7_ICOUNT -= 3;
				}
			}
		}
		else
		{
			/* load 32 bit instruction */

			// "In ARM state, bits [1:0] of r15 are undefined and must be ignored. Bits [31:2] contain the PC."
			uint32_t insn = 0;
			if (PREFETCH)
			{
				offs_t raddr = pc & (~3);
				if (MMU_ENABLED)
					update_insn_prefetch_mmu<CHECK_PID>(raddr);
				else
					update_insn_prefetch(raddr);

				bool translated = false;
				insn = insn_fetch_arm(raddr, translated);
				if (!translated)
				{
					m_pendingAbtP = true;
					m_pending_interrupt = true;
					goto skip_arm_exec;
				}
			}
			else
			{
				if (MMU_ENABLED)
				{
					//insn = arm7_tlb_translate_check<FETCH, ARM_MODE, CHECK_PID>(raddr);
					if (CHECK_PID && pc < 0x2000000)
					{
						pc += m_pid_offset;
					}

					const uint32_t entry_index = pc >> COPRO_TLB_VADDR_FLTI_MASK_SHIFT;
					//const translation_entry &entry = m_translated_table[pc >> COPRO_TLB_VADDR_FLTI_MASK_SHIFT];

					if (m_early_faultless[entry_index])
					{
						insn = m_direct->read_dword(m_section_bits[entry_index] | (pc & ~(COPRO_TLB_SECTION_PAGE_MASK | 3)));
					}
					else if (m_lvl1_type[entry_index] == COPRO_TLB_SECTION_TABLE || m_lvl1_type[entry_index] == COPRO_TLB_UNMAPPED)
					{
						m_pendingAbtP = true;
						m_pending_interrupt = true;
						goto skip_arm_exec;
					}
					else
					{
						// Entry is the physical address of a coarse second-level table
						const uint8_t permission = (m_domainAccessControl >> (m_dac_index[entry_index] << 1)) & 3;
						const uint32_t desc_lvl2 = arm7_tlb_get_second_level_descriptor(m_lvl1_type[entry_index] == COPRO_TLB_COARSE_TABLE ? TLB_COARSE : TLB_FINE, pc & ~3);
						if ((permission != 1) && (permission != 3))
						{
							uint8_t domain = m_dac_index[entry_index];
							fatalerror("ARM7: Not Yet Implemented: Coarse Table, Section Domain fault on virtual address, vaddr = %08x, domain = %08x, PC = %08x\n", pc & ~3, domain, m_r[eR15]);
						}

						switch (desc_lvl2 & 3)
						{
							case COPRO_TLB_UNMAPPED:
								m_pendingAbtP = true;
								m_pending_interrupt = true;
								goto skip_arm_exec;

							case COPRO_TLB_LARGE_PAGE:
								// Large page descriptor
								if (THUMB)
									insn = m_direct->read_word((desc_lvl2 & COPRO_TLB_LARGE_PAGE_MASK) | (pc & ~(COPRO_TLB_LARGE_PAGE_MASK | 3)));
								else
									insn = m_direct->read_dword((desc_lvl2 & COPRO_TLB_LARGE_PAGE_MASK) | (pc & ~(COPRO_TLB_LARGE_PAGE_MASK | 3)));
								break;

							case COPRO_TLB_SMALL_PAGE:
							{
								// Small page descriptor
								uint8_t ap = ((((desc_lvl2 >> 4) & 0xFF) >> (((pc >> 10) & 3) << 1)) & 3) << 2;
								if (detect_read_fault(m_dac_index[entry_index], ap) == FAULT_NONE)
								{
									if (THUMB)
										insn = m_direct->read_word((desc_lvl2 & COPRO_TLB_SMALL_PAGE_MASK) | (pc & ~(COPRO_TLB_SMALL_PAGE_MASK | 3)));
									else
										insn = m_direct->read_dword((desc_lvl2 & COPRO_TLB_SMALL_PAGE_MASK) | (pc & ~(COPRO_TLB_SMALL_PAGE_MASK | 3)));
								}
								else
								{
									m_pendingAbtP = true;
									m_pending_interrupt = true;
									goto skip_arm_exec;
								}
							}
							case COPRO_TLB_TINY_PAGE:
								// Tiny page descriptor
								if (m_lvl1_type[entry_index] == 1)
								{
									LOG(("ARM7: It would appear that we're looking up a tiny page from a coarse TLB lookup.  This is bad. vaddr = %08x\n", pc & ~3));
								}
								if (THUMB)
									insn = m_direct->read_word((desc_lvl2 & COPRO_TLB_TINY_PAGE_MASK) | (pc & ~(COPRO_TLB_TINY_PAGE_MASK | 3)));
								else
									insn = m_direct->read_dword((desc_lvl2 & COPRO_TLB_TINY_PAGE_MASK) | (pc & ~(COPRO_TLB_TINY_PAGE_MASK | 3)));
								break;
						}
					}
				}
				else
				{
					insn = m_direct->read_dword(pc & ~3);
				}
			}

			if ((insn >> 28) != 0xe)
			{
#if 0
				switch (insn >> INSN_COND_SHIFT)
				{
					case COND_EQ:
						if (m_zflag)
							{}
						else
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_NE:
						if (m_zflag)
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_CS:
						if (m_cflag)
							{}
						else
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_CC:
						if (m_cflag)
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_MI:
						if (m_nflag)
							{}
						else
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_PL:
						if (m_nflag)
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_VS:
						if (m_vflag)
							{}
						else
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_VC:
						if (m_vflag)
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_HI:
						if (!m_cflag || m_zflag)
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_LS:
						if (!m_cflag || m_zflag)
							{}
						else
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_GE:
						if (m_nflag != m_vflag)
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_LT:
						if (m_nflag != m_vflag)
							{}
						else
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_GT:
						if (m_zflag || (m_nflag != m_vflag))
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_LE:
						if (m_zflag || (m_nflag != m_vflag))
							{}
						else
							{ UNEXECUTED(); goto skip_arm_exec; }
						break;
					case COND_NV:
						if (m_archRev >= 5)
							execute_arm9_insn(insn);
						UNEXECUTED();
						goto skip_arm_exec;
						break;
					case COND_AL:
						break;
				}
#endif
			}

			execute_arm7_insn<MMU_ENABLED>(insn);
		}
skip_arm_exec:

		arm7_check_irq_state();

	} while (m_icount > 0);
}

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::execute_thumb_insn(const uint32_t op, uint32_t pc)
{
	switch ((uint16_t)op >> 6)
	{
		case 0x000: case 0x001: case 0x002: case 0x003: case 0x004: case 0x005: case 0x006: case 0x007:
		case 0x008: case 0x009: case 0x00a: case 0x00b: case 0x00c: case 0x00d: case 0x00e: case 0x00f:
		case 0x010: case 0x011: case 0x012: case 0x013: case 0x014: case 0x015: case 0x016: case 0x017:
		case 0x018: case 0x019: case 0x01a: case 0x01b: case 0x01c: case 0x01d: case 0x01e: case 0x01f:
			tg00_0(op, pc);
			break;
		case 0x020: case 0x021: case 0x022: case 0x023: case 0x024: case 0x025: case 0x026: case 0x027:
		case 0x028: case 0x029: case 0x02a: case 0x02b: case 0x02c: case 0x02d: case 0x02e: case 0x02f:
		case 0x030: case 0x031: case 0x032: case 0x033: case 0x034: case 0x035: case 0x036: case 0x037:
		case 0x038: case 0x039: case 0x03a: case 0x03b: case 0x03c: case 0x03d: case 0x03e: case 0x03f:
			tg00_1(op, pc);
			break;
		case 0x040: case 0x041: case 0x042: case 0x043: case 0x044: case 0x045: case 0x046: case 0x047:
		case 0x048: case 0x049: case 0x04a: case 0x04b: case 0x04c: case 0x04d: case 0x04e: case 0x04f:
		case 0x050: case 0x051: case 0x052: case 0x053: case 0x054: case 0x055: case 0x056: case 0x057:
		case 0x058: case 0x059: case 0x05a: case 0x05b: case 0x05c: case 0x05d: case 0x05e: case 0x05f:
			tg01_0(op, pc);
			break;
		case 0x060: case 0x061: case 0x062: case 0x063: case 0x064: case 0x065: case 0x066: case 0x067:
			tg01_10(op, pc);
			break;
		case 0x068: case 0x069: case 0x06a: case 0x06b: case 0x06c: case 0x06d: case 0x06e: case 0x06f:
			tg01_11(op, pc);
			break;
		case 0x070: case 0x071: case 0x072: case 0x073: case 0x074: case 0x075: case 0x076: case 0x077:
			tg01_12(op, pc);
			break;
		case 0x078: case 0x079: case 0x07a: case 0x07b: case 0x07c: case 0x07d: case 0x07e: case 0x07f:
			tg01_13(op, pc);
			break;
		case 0x080: case 0x081: case 0x082: case 0x083: case 0x084: case 0x085: case 0x086: case 0x087:
		case 0x088: case 0x089: case 0x08a: case 0x08b: case 0x08c: case 0x08d: case 0x08e: case 0x08f:
		case 0x090: case 0x091: case 0x092: case 0x093: case 0x094: case 0x095: case 0x096: case 0x097:
		case 0x098: case 0x099: case 0x09a: case 0x09b: case 0x09c: case 0x09d: case 0x09e: case 0x09f:
			tg02_0(op, pc);
			break;
		case 0x0a0: case 0x0a1: case 0x0a2: case 0x0a3: case 0x0a4: case 0x0a5: case 0x0a6: case 0x0a7:
		case 0x0a8: case 0x0a9: case 0x0aa: case 0x0ab: case 0x0ac: case 0x0ad: case 0x0ae: case 0x0af:
		case 0x0b0: case 0x0b1: case 0x0b2: case 0x0b3: case 0x0b4: case 0x0b5: case 0x0b6: case 0x0b7:
		case 0x0b8: case 0x0b9: case 0x0ba: case 0x0bb: case 0x0bc: case 0x0bd: case 0x0be: case 0x0bf:
			tg02_1(op, pc);
			break;
		case 0x0c0: case 0x0c1: case 0x0c2: case 0x0c3: case 0x0c4: case 0x0c5: case 0x0c6: case 0x0c7:
		case 0x0c8: case 0x0c9: case 0x0ca: case 0x0cb: case 0x0cc: case 0x0cd: case 0x0ce: case 0x0cf:
		case 0x0d0: case 0x0d1: case 0x0d2: case 0x0d3: case 0x0d4: case 0x0d5: case 0x0d6: case 0x0d7:
		case 0x0d8: case 0x0d9: case 0x0da: case 0x0db: case 0x0dc: case 0x0dd: case 0x0de: case 0x0df:
			tg03_0(op, pc);
			break;
		case 0x0e0: case 0x0e1: case 0x0e2: case 0x0e3: case 0x0e4: case 0x0e5: case 0x0e6: case 0x0e7:
		case 0x0e8: case 0x0e9: case 0x0ea: case 0x0eb: case 0x0ec: case 0x0ed: case 0x0ee: case 0x0ef:
		case 0x0f0: case 0x0f1: case 0x0f2: case 0x0f3: case 0x0f4: case 0x0f5: case 0x0f6: case 0x0f7:
		case 0x0f8: case 0x0f9: case 0x0fa: case 0x0fb: case 0x0fc: case 0x0fd: case 0x0fe: case 0x0ff:
			tg03_1(op, pc);
			break;
		case 0x100: tg04_00_00(op, pc); break;	case 0x101: tg04_00_01(op, pc); break;	case 0x102: tg04_00_02(op, pc); break;	case 0x103: tg04_00_03(op, pc); break;
		case 0x104: tg04_00_04(op, pc); break;	case 0x105: tg04_00_05(op, pc); break;	case 0x106: tg04_00_06(op, pc); break;	case 0x107: tg04_00_07(op, pc); break;
		case 0x108: tg04_00_08(op, pc); break;	case 0x109: tg04_00_09(op, pc); break;	case 0x10a: tg04_00_0a(op, pc); break;	case 0x10b: tg04_00_0b(op, pc); break;
		case 0x10c: tg04_00_0c(op, pc); break;	case 0x10d: tg04_00_0d(op, pc); break;	case 0x10e: tg04_00_0e(op, pc); break;	case 0x10f: tg04_00_0f(op, pc); break;
		case 0x110: tg04_01_00(op, pc); break;	case 0x111: tg04_01_01(op, pc); break;	case 0x112: tg04_01_02(op, pc); break;	case 0x113: tg04_01_03(op, pc); break;
		case 0x114: tg04_01_10(op, pc); break;	case 0x115: tg04_01_11(op, pc); break;	case 0x116: tg04_01_12(op, pc); break;	case 0x117: tg04_01_13(op, pc); break;
		case 0x118: tg04_01_20(op, pc); break;	case 0x119: tg04_01_21(op, pc); break;	case 0x11a: tg04_01_22(op, pc); break;	case 0x11b: tg04_01_23(op, pc); break;
		case 0x11c: tg04_01_30(op, pc); break;	case 0x11d: tg04_01_31(op, pc); break;	case 0x11e: tg04_01_32(op, pc); break;	case 0x11f: tg04_01_33(op, pc); break;
			tg03_0(op, pc);
			break;
		case 0x120: case 0x121: case 0x122: case 0x123: case 0x124: case 0x125: case 0x126: case 0x127:
		case 0x128: case 0x129: case 0x12a: case 0x12b: case 0x12c: case 0x12d: case 0x12e: case 0x12f:
		case 0x130: case 0x131: case 0x132: case 0x133: case 0x134: case 0x135: case 0x136: case 0x137:
		case 0x138: case 0x139: case 0x13a: case 0x13b: case 0x13c: case 0x13d: case 0x13e: case 0x13f:
			tg04_0203<MMU>(op, pc);
			break;
		case 0x140: case 0x141: case 0x142: case 0x143: case 0x144: case 0x145: case 0x146: case 0x147:
			tg05_0<MMU>(op, pc);
			break;
		case 0x148: case 0x149: case 0x14a: case 0x14b: case 0x14c: case 0x14d: case 0x14e: case 0x14f:
			tg05_1<MMU>(op, pc);
			break;
		case 0x150: case 0x151: case 0x152: case 0x153: case 0x154: case 0x155: case 0x156: case 0x157:
			tg05_2<MMU>(op, pc);
			break;
		case 0x158: case 0x159: case 0x15a: case 0x15b: case 0x15c: case 0x15d: case 0x15e: case 0x15f:
			tg05_3<MMU>(op, pc);
			break;
		case 0x160: case 0x161: case 0x162: case 0x163: case 0x164: case 0x165: case 0x166: case 0x167:
			tg05_4<MMU>(op, pc);
			break;
		case 0x168: case 0x169: case 0x16a: case 0x16b: case 0x16c: case 0x16d: case 0x16e: case 0x16f:
			tg05_5<MMU>(op, pc);
			break;
		case 0x170: case 0x171: case 0x172: case 0x173: case 0x174: case 0x175: case 0x176: case 0x177:
			tg05_6<MMU>(op, pc);
			break;
		case 0x178: case 0x179: case 0x17a: case 0x17b: case 0x17c: case 0x17d: case 0x17e: case 0x17f:
			tg05_7<MMU>(op, pc);
			break;
		case 0x180: case 0x181: case 0x182: case 0x183: case 0x184: case 0x185: case 0x186: case 0x187:
		case 0x188: case 0x189: case 0x18a: case 0x18b: case 0x18c: case 0x18d: case 0x18e: case 0x18f:
		case 0x190: case 0x191: case 0x192: case 0x193: case 0x194: case 0x195: case 0x196: case 0x197:
		case 0x198: case 0x199: case 0x19a: case 0x19b: case 0x19c: case 0x19d: case 0x19e: case 0x19f:
			tg06_0<MMU>(op, pc);
			break;
		case 0x1a0: case 0x1a1: case 0x1a2: case 0x1a3: case 0x1a4: case 0x1a5: case 0x1a6: case 0x1a7:
		case 0x1a8: case 0x1a9: case 0x1aa: case 0x1ab: case 0x1ac: case 0x1ad: case 0x1ae: case 0x1af:
		case 0x1b0: case 0x1b1: case 0x1b2: case 0x1b3: case 0x1b4: case 0x1b5: case 0x1b6: case 0x1b7:
		case 0x1b8: case 0x1b9: case 0x1ba: case 0x1bb: case 0x1bc: case 0x1bd: case 0x1be: case 0x1bf:
			tg06_1<MMU>(op, pc);
			break;
		case 0x1c0: case 0x1c1: case 0x1c2: case 0x1c3: case 0x1c4: case 0x1c5: case 0x1c6: case 0x1c7:
		case 0x1c8: case 0x1c9: case 0x1ca: case 0x1cb: case 0x1cc: case 0x1cd: case 0x1ce: case 0x1cf:
		case 0x1d0: case 0x1d1: case 0x1d2: case 0x1d3: case 0x1d4: case 0x1d5: case 0x1d6: case 0x1d7:
		case 0x1d8: case 0x1d9: case 0x1da: case 0x1db: case 0x1dc: case 0x1dd: case 0x1de: case 0x1df:
			tg07_0<MMU>(op, pc);
			break;
		case 0x1e0: case 0x1e1: case 0x1e2: case 0x1e3: case 0x1e4: case 0x1e5: case 0x1e6: case 0x1e7:
		case 0x1e8: case 0x1e9: case 0x1ea: case 0x1eb: case 0x1ec: case 0x1ed: case 0x1ee: case 0x1ef:
		case 0x1f0: case 0x1f1: case 0x1f2: case 0x1f3: case 0x1f4: case 0x1f5: case 0x1f6: case 0x1f7:
		case 0x1f8: case 0x1f9: case 0x1fa: case 0x1fb: case 0x1fc: case 0x1fd: case 0x1fe: case 0x1ff:
			tg07_1<MMU>(op, pc);
			break;
		case 0x200: case 0x201: case 0x202: case 0x203: case 0x204: case 0x205: case 0x206: case 0x207:
		case 0x208: case 0x209: case 0x20a: case 0x20b: case 0x20c: case 0x20d: case 0x20e: case 0x20f:
		case 0x210: case 0x211: case 0x212: case 0x213: case 0x214: case 0x215: case 0x216: case 0x217:
		case 0x218: case 0x219: case 0x21a: case 0x21b: case 0x21c: case 0x21d: case 0x21e: case 0x21f:
			tg08_0<MMU>(op, pc);
			break;
		case 0x220: case 0x221: case 0x222: case 0x223: case 0x224: case 0x225: case 0x226: case 0x227:
		case 0x228: case 0x229: case 0x22a: case 0x22b: case 0x22c: case 0x22d: case 0x22e: case 0x22f:
		case 0x230: case 0x231: case 0x232: case 0x233: case 0x234: case 0x235: case 0x236: case 0x237:
		case 0x238: case 0x239: case 0x23a: case 0x23b: case 0x23c: case 0x23d: case 0x23e: case 0x23f:
			tg08_1<MMU>(op, pc);
			break;
		case 0x240: case 0x241: case 0x242: case 0x243: case 0x244: case 0x245: case 0x246: case 0x247:
		case 0x248: case 0x249: case 0x24a: case 0x24b: case 0x24c: case 0x24d: case 0x24e: case 0x24f:
		case 0x250: case 0x251: case 0x252: case 0x253: case 0x254: case 0x255: case 0x256: case 0x257:
		case 0x258: case 0x259: case 0x25a: case 0x25b: case 0x25c: case 0x25d: case 0x25e: case 0x25f:
			tg09_0<MMU>(op, pc);
			break;
		case 0x260: case 0x261: case 0x262: case 0x263: case 0x264: case 0x265: case 0x266: case 0x267:
		case 0x268: case 0x269: case 0x26a: case 0x26b: case 0x26c: case 0x26d: case 0x26e: case 0x26f:
		case 0x270: case 0x271: case 0x272: case 0x273: case 0x274: case 0x275: case 0x276: case 0x277:
		case 0x278: case 0x279: case 0x27a: case 0x27b: case 0x27c: case 0x27d: case 0x27e: case 0x27f:
			tg09_1<MMU>(op, pc);
			break;
		case 0x280: case 0x281: case 0x282: case 0x283: case 0x284: case 0x285: case 0x286: case 0x287:
		case 0x288: case 0x289: case 0x28a: case 0x28b: case 0x28c: case 0x28d: case 0x28e: case 0x28f:
		case 0x290: case 0x291: case 0x292: case 0x293: case 0x294: case 0x295: case 0x296: case 0x297:
		case 0x298: case 0x299: case 0x29a: case 0x29b: case 0x29c: case 0x29d: case 0x29e: case 0x29f:
			tg0a_0(op, pc);
			break;
		case 0x2a0: case 0x2a1: case 0x2a2: case 0x2a3: case 0x2a4: case 0x2a5: case 0x2a6: case 0x2a7:
		case 0x2a8: case 0x2a9: case 0x2aa: case 0x2ab: case 0x2ac: case 0x2ad: case 0x2ae: case 0x2af:
		case 0x2b0: case 0x2b1: case 0x2b2: case 0x2b3: case 0x2b4: case 0x2b5: case 0x2b6: case 0x2b7:
		case 0x2b8: case 0x2b9: case 0x2ba: case 0x2bb: case 0x2bc: case 0x2bd: case 0x2be: case 0x2bf:
			tg0a_1(op, pc);
			break;
		case 0x2c0: case 0x2c1: case 0x2c2: case 0x2c3: tg0b_0(op, pc); break;
		case 0x2c4: case 0x2c5: case 0x2c6: case 0x2c7: tg0b_1(op, pc); break;
		case 0x2c8: case 0x2c9: case 0x2ca: case 0x2cb: tg0b_2(op, pc); break;
		case 0x2cc: case 0x2cd: case 0x2ce: case 0x2cf: tg0b_3(op, pc); break;
		case 0x2d0: case 0x2d1: case 0x2d2: case 0x2d3: tg0b_4<MMU>(op, pc); break;
		case 0x2d4: case 0x2d5: case 0x2d6: case 0x2d7: tg0b_5<MMU>(op, pc); break;
		case 0x2d8: case 0x2d9: case 0x2da: case 0x2db: tg0b_6(op, pc); break;
		case 0x2dc: case 0x2dd: case 0x2de: case 0x2df: tg0b_7(op, pc); break;
		case 0x2e0: case 0x2e1: case 0x2e2: case 0x2e3: tg0b_8(op, pc); break;
		case 0x2e4: case 0x2e5: case 0x2e6: case 0x2e7: tg0b_9(op, pc); break;
		case 0x2e8: case 0x2e9: case 0x2ea: case 0x2eb: tg0b_a(op, pc); break;
		case 0x2ec: case 0x2ed: case 0x2ee: case 0x2ef: tg0b_b(op, pc); break;
		case 0x2f0: case 0x2f1: case 0x2f2: case 0x2f3: tg0b_c<MMU>(op, pc); break;
		case 0x2f4: case 0x2f5: case 0x2f6: case 0x2f7: tg0b_d<MMU>(op, pc); break;
		case 0x2f8: case 0x2f9: case 0x2fa: case 0x2fb: tg0b_e(op, pc); break;
		case 0x2fc: case 0x2fd: case 0x2fe: case 0x2ff: tg0b_f(op, pc); break;
		case 0x300: case 0x301: case 0x302: case 0x303: case 0x304: case 0x305: case 0x306: case 0x307:
		case 0x308: case 0x309: case 0x30a: case 0x30b: case 0x30c: case 0x30d: case 0x30e: case 0x30f:
		case 0x310: case 0x311: case 0x312: case 0x313: case 0x314: case 0x315: case 0x316: case 0x317:
		case 0x318: case 0x319: case 0x31a: case 0x31b: case 0x31c: case 0x31d: case 0x31e: case 0x31f:
			tg0c_0<MMU>(op, pc);
			break;
		case 0x320: case 0x321: case 0x322: case 0x323: case 0x324: case 0x325: case 0x326: case 0x327:
		case 0x328: case 0x329: case 0x32a: case 0x32b: case 0x32c: case 0x32d: case 0x32e: case 0x32f:
		case 0x330: case 0x331: case 0x332: case 0x333: case 0x334: case 0x335: case 0x336: case 0x337:
		case 0x338: case 0x339: case 0x33a: case 0x33b: case 0x33c: case 0x33d: case 0x33e: case 0x33f:
			tg0c_1<MMU>(op, pc);
			break;
		case 0x340: case 0x341: case 0x342: case 0x343: tg0d_0(op, pc); break;
		case 0x344: case 0x345: case 0x346: case 0x347: tg0d_1(op, pc); break;
		case 0x348: case 0x349: case 0x34a: case 0x34b: tg0d_2(op, pc); break;
		case 0x34c: case 0x34d: case 0x34e: case 0x34f: tg0d_3(op, pc); break;
		case 0x350: case 0x351: case 0x352: case 0x353: tg0d_4(op, pc); break;
		case 0x354: case 0x355: case 0x356: case 0x357: tg0d_5(op, pc); break;
		case 0x358: case 0x359: case 0x35a: case 0x35b: tg0d_6(op, pc); break;
		case 0x35c: case 0x35d: case 0x35e: case 0x35f: tg0d_7(op, pc); break;
		case 0x360: case 0x361: case 0x362: case 0x363: tg0d_8(op, pc); break;
		case 0x364: case 0x365: case 0x366: case 0x367: tg0d_9(op, pc); break;
		case 0x368: case 0x369: case 0x36a: case 0x36b: tg0d_a(op, pc); break;
		case 0x36c: case 0x36d: case 0x36e: case 0x36f: tg0d_b(op, pc); break;
		case 0x370: case 0x371: case 0x372: case 0x373: tg0d_c(op, pc); break;
		case 0x374: case 0x375: case 0x376: case 0x377: tg0d_d(op, pc); break;
		case 0x378: case 0x379: case 0x37a: case 0x37b: tg0d_e(op, pc); break;
		case 0x37c: case 0x37d: case 0x37e: case 0x37f: tg0d_f(op, pc); break;
		case 0x380: case 0x381: case 0x382: case 0x383: case 0x384: case 0x385: case 0x386: case 0x387:
		case 0x388: case 0x389: case 0x38a: case 0x38b: case 0x38c: case 0x38d: case 0x38e: case 0x38f:
		case 0x390: case 0x391: case 0x392: case 0x393: case 0x394: case 0x395: case 0x396: case 0x397:
		case 0x398: case 0x399: case 0x39a: case 0x39b: case 0x39c: case 0x39d: case 0x39e: case 0x39f:
			tg0e_0(op, pc);
			break;
		case 0x3a0: case 0x3a1: case 0x3a2: case 0x3a3: case 0x3a4: case 0x3a5: case 0x3a6: case 0x3a7:
		case 0x3a8: case 0x3a9: case 0x3aa: case 0x3ab: case 0x3ac: case 0x3ad: case 0x3ae: case 0x3af:
		case 0x3b0: case 0x3b1: case 0x3b2: case 0x3b3: case 0x3b4: case 0x3b5: case 0x3b6: case 0x3b7:
		case 0x3b8: case 0x3b9: case 0x3ba: case 0x3bb: case 0x3bc: case 0x3bd: case 0x3be: case 0x3bf:
			tg0e_1(op, pc);
			break;
		case 0x3c0: case 0x3c1: case 0x3c2: case 0x3c3: case 0x3c4: case 0x3c5: case 0x3c6: case 0x3c7:
		case 0x3c8: case 0x3c9: case 0x3ca: case 0x3cb: case 0x3cc: case 0x3cd: case 0x3ce: case 0x3cf:
		case 0x3d0: case 0x3d1: case 0x3d2: case 0x3d3: case 0x3d4: case 0x3d5: case 0x3d6: case 0x3d7:
		case 0x3d8: case 0x3d9: case 0x3da: case 0x3db: case 0x3dc: case 0x3dd: case 0x3de: case 0x3df:
			tg0f_0(op, pc);
			break;
		case 0x3e0: case 0x3e1: case 0x3e2: case 0x3e3: case 0x3e4: case 0x3e5: case 0x3e6: case 0x3e7:
		case 0x3e8: case 0x3e9: case 0x3ea: case 0x3eb: case 0x3ec: case 0x3ed: case 0x3ee: case 0x3ef:
		case 0x3f0: case 0x3f1: case 0x3f2: case 0x3f3: case 0x3f4: case 0x3f5: case 0x3f6: case 0x3f7:
		case 0x3f8: case 0x3f9: case 0x3fa: case 0x3fb: case 0x3fc: case 0x3fd: case 0x3fe: case 0x3ff:
			tg0f_1(op, pc);
			break;
	}
}

#define COND_EQ_CHECK		if (!m_zflag) { UNEXECUTED(); return; }
#define COND_NE_CHECK		if (m_zflag) { UNEXECUTED(); return; }
#define COND_CS_CHECK		if (!m_cflag) { UNEXECUTED(); return; }
#define COND_CC_CHECK		if (m_cflag) { UNEXECUTED(); return; }
#define COND_MI_CHECK		if (!m_nflag) { UNEXECUTED(); return; }
#define COND_PL_CHECK		if (m_nflag) { UNEXECUTED(); return; }
#define COND_VS_CHECK		if (!m_vflag) { UNEXECUTED(); return; }
#define COND_VC_CHECK		if (m_vflag) { UNEXECUTED(); return; }
#define COND_HI_CHECK		if (!m_cflag || m_zflag) { UNEXECUTED(); return; }
#define COND_LS_CHECK		if (!(!m_cflag || m_zflag)) { UNEXECUTED(); return; }
#define COND_GE_CHECK		if (m_nflag != m_vflag) { UNEXECUTED(); return; }
#define COND_LT_CHECK		if (m_nflag == m_vflag) { UNEXECUTED(); return; }
#define COND_GT_CHECK		if (m_zflag || (m_nflag != m_vflag)) { UNEXECUTED(); return; }
#define COND_LE_CHECK		if (!(m_zflag || (m_nflag != m_vflag))) { UNEXECUTED(); return; }
#define COND_NV_CHECK		if (m_archRev >= 5) { execute_arm9_insn(insn); m_r[eR15] += 4; ARM7_ICOUNT -= 3; } else { UNEXECUTED(); } return;

template <arm7_cpu_device::copro_mode MMU>
void arm7_cpu_device::execute_arm7_insn(const uint32_t insn)
{
	switch (insn >> 20)
	{
		case 0x000: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0x100: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0x200: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0x300: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0x400: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0x500: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0x600: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0x700: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0x800: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0x900: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0xa00: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0xb00: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0xc00: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0xd00: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0xe00: arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_AND>(insn); break;
		case 0xf00: COND_NV_CHECK; break;
		case 0x001: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0x101: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0x201: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0x301: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0x401: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0x501: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0x601: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0x701: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0x801: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0x901: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0xa01: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0xb01: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0xc01: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0xd01: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0xe01: arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_AND>(insn); break;
		case 0xf01: COND_NV_CHECK; break;
		case 0x002: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0x102: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0x202: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0x302: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0x402: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0x502: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0x602: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0x702: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0x802: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0x902: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0xa02: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0xb02: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0xc02: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0xd02: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0xe02: arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_STORE, OPCODE_EOR>(insn); break;
		case 0xf02: COND_NV_CHECK; break;
		case 0x003: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0x103: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0x203: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0x303: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0x403: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0x503: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0x603: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0x703: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0x803: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0x903: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0xa03: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0xb03: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0xc03: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0xd03: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0xe03: arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_UNSIGNED, MUL_ACCUM, IS_LOAD, OPCODE_EOR>(insn); break;
		case 0xf03: COND_NV_CHECK; break;
		case 0x004: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0x104: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0x204: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0x304: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0x404: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0x504: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0x604: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0x704: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0x804: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0x904: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0xa04: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0xb04: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0xc04: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0xd04: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0xe04: arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SUB>(insn); break;
		case 0xf04: COND_NV_CHECK; break;
		case 0x005: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0x105: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0x205: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0x305: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0x405: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0x505: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0x605: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0x705: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0x805: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0x905: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0xa05: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0xb05: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0xc05: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0xd05: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0xe05: arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SUB>(insn); break;
		case 0xf05: COND_NV_CHECK; break;
		case 0x006: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0x106: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0x206: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0x306: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0x406: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0x506: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0x606: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0x706: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0x806: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0x906: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0xa06: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0xb06: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0xc06: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0xd06: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0xe06: arm7ops_0<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSB>(insn); break;
		case 0xf06: COND_NV_CHECK; break;
		case 0x007: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0x107: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0x207: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0x307: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0x407: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0x507: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0x607: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0x707: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0x807: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0x907: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0xa07: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0xb07: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0xc07: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0xd07: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0xe07: arm7ops_0<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, MUL_WORD, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSB>(insn); break;
		case 0xf07: COND_NV_CHECK; break;
		case 0x008: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0x108: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0x208: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0x308: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0x408: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0x508: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0x608: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0x708: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0x808: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0x908: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0xa08: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0xb08: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0xc08: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0xd08: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0xe08: arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADD>(insn); break;
		case 0xf08: COND_NV_CHECK; break;
		case 0x009: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0x109: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0x209: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0x309: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0x409: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0x509: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0x609: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0x709: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0x809: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0x909: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0xa09: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0xb09: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0xc09: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0xd09: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0xe09: arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADD>(insn); break;
		case 0xf09: COND_NV_CHECK; break;
		case 0x00a: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0x10a: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0x20a: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0x30a: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0x40a: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0x50a: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0x60a: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0x70a: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0x80a: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0x90a: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0xa0a: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0xb0a: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0xc0a: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0xd0a: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0xe0a: arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_STORE, OPCODE_ADC>(insn); break;
		case 0xf0a: COND_NV_CHECK; break;
		case 0x00b: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0x10b: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0x20b: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0x30b: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0x40b: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0x50b: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0x60b: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0x70b: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0x80b: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0x90b: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0xa0b: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0xb0b: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0xc0b: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0xd0b: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0xe0b: arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_UNSIGNED, MUL_ONLY, IS_LOAD, OPCODE_ADC>(insn); break;
		case 0xf0b: COND_NV_CHECK; break;
		case 0x00c: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0x10c: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0x20c: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0x30c: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0x40c: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0x50c: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0x60c: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0x70c: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0x80c: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0x90c: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0xa0c: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0xb0c: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0xc0c: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0xd0c: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0xe0c: arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_SBC>(insn); break;
		case 0xf0c: COND_NV_CHECK; break;
		case 0x00d: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0x10d: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0x20d: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0x30d: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0x40d: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0x50d: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0x60d: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0x70d: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0x80d: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0x90d: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0xa0d: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0xb0d: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0xc0d: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0xd0d: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0xe0d: arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_SBC>(insn); break;
		case 0xf0d: COND_NV_CHECK; break;
		case 0x00e: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0x10e: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0x20e: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0x30e: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0x40e: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0x50e: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0x60e: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0x70e: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0x80e: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0x90e: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0xa0e: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0xb0e: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0xc0e: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0xd0e: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0xe0e: arm7ops_0<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_STORE, OPCODE_RSC>(insn); break;
		case 0xf0e: COND_NV_CHECK; break;
		case 0x00f: COND_EQ_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0x10f: COND_NE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0x20f: COND_CS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0x30f: COND_CC_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0x40f: COND_MI_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0x50f: COND_PL_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0x60f: COND_VS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0x70f: COND_VC_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0x80f: COND_HI_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0x90f: COND_LS_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0xa0f: COND_GE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0xb0f: COND_LT_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0xc0f: COND_GT_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0xd0f: COND_LE_CHECK; arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0xe0f: arm7ops_0<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, MUL_LONG, MUL_SIGNED, MUL_ONLY, IS_LOAD, OPCODE_RSC>(insn); break;
		case 0xf0f: COND_NV_CHECK; break;
		case 0x010: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x014: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0x110: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x114: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0x210: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x214: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0x310: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x314: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0x410: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x414: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0x510: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x514: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0x610: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x614: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0x710: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x714: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0x810: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x814: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0x910: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0x914: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0xa10: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0xa14: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0xb10: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0xb14: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0xc10: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0xc14: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0xd10: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0xd14: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0xe10: arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_TST>(insn); break;
		case 0xe14: arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMP>(insn); break;
		case 0xf10: case 0xf14: COND_NV_CHECK; break;
		case 0x012: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x016: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0x112: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x116: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0x212: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x216: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0x312: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x316: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0x412: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x416: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0x512: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x516: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0x612: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x616: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0x712: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x716: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0x812: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x816: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0x912: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0x916: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0xa12: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0xa16: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0xb12: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0xb16: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0xc12: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0xc16: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0xd12: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0xd16: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0xe12: arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_TEQ>(insn); break;
		case 0xe16: arm7ops_1<MMU, OFFSET_DOWN, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_CMN>(insn); break;
		case 0xf12: case 0xf16: COND_NV_CHECK; break;
		case 0x011: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x015: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0x111: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x115: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0x211: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x215: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0x311: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x315: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0x411: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x415: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0x511: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x515: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0x611: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x615: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0x711: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x715: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0x811: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x815: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0x911: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0x915: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0xa11: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0xa15: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0xb11: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0xb15: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0xc11: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0xc15: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0xd11: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0xd15: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0xe11: arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TST>(insn); break;
		case 0xe15: arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMP>(insn); break;
		case 0xf11: case 0xf15: COND_NV_CHECK; break;
		case 0x013: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x017: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0x113: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x117: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0x213: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x217: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0x313: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x317: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0x413: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x417: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0x513: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x517: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0x613: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x617: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0x713: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x717: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0x813: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x817: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0x913: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0x917: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0xa13: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0xa17: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0xb13: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0xb17: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0xc13: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0xc17: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0xd13: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0xd17: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0xe13: arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_TEQ>(insn); break;
		case 0xe17: arm7ops_1<MMU, OFFSET_DOWN, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_CMN>(insn); break;
		case 0xf13: case 0xf17: COND_NV_CHECK; break;
		case 0x018: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x01c: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0x118: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x11c: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0x218: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x21c: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0x318: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x31c: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0x418: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x41c: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0x518: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x51c: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0x618: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x61c: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0x718: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x71c: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0x818: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x81c: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0x918: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0x91c: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0xa18: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0xa1c: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0xb18: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0xb1c: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0xc18: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0xc1c: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0xd18: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0xd1c: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0xe18: arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_ORR>(insn); break;
		case 0xe1c: arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, NO_WRITEBACK, IS_STORE, PSR_OP, OPCODE_BIC>(insn); break;
		case 0xf18: case 0xf1c: COND_NV_CHECK; break;
		case 0x01a: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x01e: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0x11a: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x11e: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0x21a: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x21e: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0x31a: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x31e: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0x41a: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x41e: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0x51a: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x51e: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0x61a: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x61e: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0x71a: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x71e: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0x81a: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x81e: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0x91a: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0x91e: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0xa1a: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0xa1e: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0xb1a: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0xb1e: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0xc1a: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0xc1e: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0xd1a: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0xd1e: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0xe1a: arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MOV>(insn); break;
		case 0xe1e: arm7ops_1<MMU, OFFSET_UP, NO_FLAGS, WRITEBACK, IS_STORE, PSR_OP, OPCODE_MVN>(insn); break;
		case 0xf1a: case 0xf1e: COND_NV_CHECK; break;
		case 0x019: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x01d: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0x119: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x11d: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0x219: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x21d: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0x319: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x31d: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0x419: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x41d: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0x519: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x51d: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0x619: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x61d: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0x719: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x71d: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0x819: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x81d: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0x919: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0x91d: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0xa19: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0xa1d: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0xb19: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0xb1d: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0xc19: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0xc1d: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0xd19: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0xd1d: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0xe19: arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_ORR>(insn); break;
		case 0xe1d: arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, NO_WRITEBACK, IS_LOAD, ALU_OP, OPCODE_BIC>(insn); break;
		case 0xf19: case 0xf1d: COND_NV_CHECK; break;
		case 0x01b: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x01f: COND_EQ_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0x11b: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x11f: COND_NE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0x21b: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x21f: COND_CS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0x31b: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x31f: COND_CC_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0x41b: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x41f: COND_MI_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0x51b: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x51f: COND_PL_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0x61b: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x61f: COND_VS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0x71b: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x71f: COND_VC_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0x81b: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x81f: COND_HI_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0x91b: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0x91f: COND_LS_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0xa1b: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0xa1f: COND_GE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0xb1b: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0xb1f: COND_LT_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0xc1b: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0xc1f: COND_GT_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0xd1b: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0xd1f: COND_LE_CHECK; arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0xe1b: arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MOV>(insn); break;
		case 0xe1f: arm7ops_1<MMU, OFFSET_UP, SET_FLAGS, WRITEBACK, IS_LOAD, ALU_OP, OPCODE_MVN>(insn); break;
		case 0xf1b: case 0xf1f: COND_NV_CHECK; break;
		case 0x020: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x022: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x024: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x026: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x028: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x02a: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x02c: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x02e: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0x120: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x122: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x124: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x126: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x128: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x12a: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x12c: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x12e: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0x220: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x222: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x224: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x226: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x228: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x22a: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x22c: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x22e: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0x320: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x322: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x324: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x326: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x328: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x32a: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x32c: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x32e: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0x420: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x422: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x424: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x426: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x428: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x42a: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x42c: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x42e: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0x520: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x522: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x524: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x526: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x528: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x52a: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x52c: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x52e: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0x620: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x622: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x624: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x626: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x628: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x62a: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x62c: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x62e: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0x720: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x722: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x724: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x726: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x728: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x72a: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x72c: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x72e: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0x820: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x822: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x824: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x826: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x828: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x82a: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x82c: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x82e: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0x920: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0x922: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0x924: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0x926: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0x928: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0x92a: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0x92c: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0x92e: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0xa20: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0xa22: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0xa24: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0xa26: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0xa28: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0xa2a: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0xa2c: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0xa2e: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0xb20: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0xb22: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0xb24: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0xb26: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0xb28: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0xb2a: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0xb2c: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0xb2e: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0xc20: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0xc22: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0xc24: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0xc26: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0xc28: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0xc2a: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0xc2c: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0xc2e: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0xd20: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0xd22: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0xd24: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0xd26: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0xd28: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0xd2a: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0xd2c: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0xd2e: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0xe20: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_AND>(insn); break;
		case 0xe22: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_EOR>(insn); break;
		case 0xe24: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SUB>(insn); break;
		case 0xe26: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSB>(insn); break;
		case 0xe28: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADD>(insn); break;
		case 0xe2a: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ADC>(insn); break;
		case 0xe2c: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_SBC>(insn); break;
		case 0xe2e: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_RSC>(insn); break;
		case 0xf20: case 0xf22: case 0xf24: case 0xf26: case 0xf28: case 0xf2a: case 0xf2c: case 0xf2e: COND_NV_CHECK; break;
		case 0x021: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x023: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x025: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x027: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x029: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x02b: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x02d: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x02f: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0x121: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x123: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x125: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x127: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x129: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x12b: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x12d: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x12f: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0x221: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x223: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x225: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x227: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x229: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x22b: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x22d: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x22f: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0x321: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x323: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x325: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x327: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x329: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x32b: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x32d: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x32f: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0x421: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x423: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x425: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x427: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x429: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x42b: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x42d: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x42f: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0x521: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x523: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x525: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x527: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x529: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x52b: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x52d: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x52f: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0x621: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x623: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x625: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x627: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x629: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x62b: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x62d: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x62f: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0x721: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x723: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x725: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x727: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x729: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x72b: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x72d: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x72f: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0x821: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x823: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x825: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x827: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x829: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x82b: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x82d: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x82f: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0x921: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0x923: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0x925: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0x927: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0x929: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0x92b: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0x92d: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0x92f: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0xa21: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0xa23: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0xa25: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0xa27: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0xa29: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0xa2b: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0xa2d: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0xa2f: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0xb21: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0xb23: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0xb25: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0xb27: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0xb29: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0xb2b: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0xb2d: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0xb2f: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0xc21: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0xc23: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0xc25: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0xc27: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0xc29: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0xc2b: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0xc2d: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0xc2f: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0xd21: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0xd23: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0xd25: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0xd27: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0xd29: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0xd2b: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0xd2d: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0xd2f: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;

		case 0xe21: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_AND>(insn); break;
		case 0xe23: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_EOR>(insn); break;
		case 0xe25: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SUB>(insn); break;
		case 0xe27: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSB>(insn); break;
		case 0xe29: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADD>(insn); break;
		case 0xe2b: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ADC>(insn); break;
		case 0xe2d: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_SBC>(insn); break;
		case 0xe2f: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_RSC>(insn); break;
		case 0xf21: case 0xf23: case 0xf25: case 0xf27: case 0xf29: case 0xf2b: case 0xf2d: case 0xf2f: COND_NV_CHECK; break;
		case 0x030: COND_EQ_CHECK; HandlePSRTransfer(insn); break;
		case 0x032: COND_EQ_CHECK; HandlePSRTransfer(insn); break;
		case 0x034: COND_EQ_CHECK; HandlePSRTransfer(insn); break;
		case 0x036: COND_EQ_CHECK; HandlePSRTransfer(insn); break;
		case 0x130: COND_NE_CHECK; HandlePSRTransfer(insn); break;
		case 0x132: COND_NE_CHECK; HandlePSRTransfer(insn); break;
		case 0x134: COND_NE_CHECK; HandlePSRTransfer(insn); break;
		case 0x136: COND_NE_CHECK; HandlePSRTransfer(insn); break;
		case 0x230: COND_CS_CHECK; HandlePSRTransfer(insn); break;
		case 0x232: COND_CS_CHECK; HandlePSRTransfer(insn); break;
		case 0x234: COND_CS_CHECK; HandlePSRTransfer(insn); break;
		case 0x236: COND_CS_CHECK; HandlePSRTransfer(insn); break;
		case 0x330: COND_CC_CHECK; HandlePSRTransfer(insn); break;
		case 0x332: COND_CC_CHECK; HandlePSRTransfer(insn); break;
		case 0x334: COND_CC_CHECK; HandlePSRTransfer(insn); break;
		case 0x336: COND_CC_CHECK; HandlePSRTransfer(insn); break;
		case 0x430: COND_MI_CHECK; HandlePSRTransfer(insn); break;
		case 0x432: COND_MI_CHECK; HandlePSRTransfer(insn); break;
		case 0x434: COND_MI_CHECK; HandlePSRTransfer(insn); break;
		case 0x436: COND_MI_CHECK; HandlePSRTransfer(insn); break;
		case 0x530: COND_PL_CHECK; HandlePSRTransfer(insn); break;
		case 0x532: COND_PL_CHECK; HandlePSRTransfer(insn); break;
		case 0x534: COND_PL_CHECK; HandlePSRTransfer(insn); break;
		case 0x536: COND_PL_CHECK; HandlePSRTransfer(insn); break;
		case 0x630: COND_VS_CHECK; HandlePSRTransfer(insn); break;
		case 0x632: COND_VS_CHECK; HandlePSRTransfer(insn); break;
		case 0x634: COND_VS_CHECK; HandlePSRTransfer(insn); break;
		case 0x636: COND_VS_CHECK; HandlePSRTransfer(insn); break;
		case 0x730: COND_VC_CHECK; HandlePSRTransfer(insn); break;
		case 0x732: COND_VC_CHECK; HandlePSRTransfer(insn); break;
		case 0x734: COND_VC_CHECK; HandlePSRTransfer(insn); break;
		case 0x736: COND_VC_CHECK; HandlePSRTransfer(insn); break;
		case 0x830: COND_HI_CHECK; HandlePSRTransfer(insn); break;
		case 0x832: COND_HI_CHECK; HandlePSRTransfer(insn); break;
		case 0x834: COND_HI_CHECK; HandlePSRTransfer(insn); break;
		case 0x836: COND_HI_CHECK; HandlePSRTransfer(insn); break;
		case 0x930: COND_LS_CHECK; HandlePSRTransfer(insn); break;
		case 0x932: COND_LS_CHECK; HandlePSRTransfer(insn); break;
		case 0x934: COND_LS_CHECK; HandlePSRTransfer(insn); break;
		case 0x936: COND_LS_CHECK; HandlePSRTransfer(insn); break;
		case 0xa30: COND_GE_CHECK; HandlePSRTransfer(insn); break;
		case 0xa32: COND_GE_CHECK; HandlePSRTransfer(insn); break;
		case 0xa34: COND_GE_CHECK; HandlePSRTransfer(insn); break;
		case 0xa36: COND_GE_CHECK; HandlePSRTransfer(insn); break;
		case 0xb30: COND_LT_CHECK; HandlePSRTransfer(insn); break;
		case 0xb32: COND_LT_CHECK; HandlePSRTransfer(insn); break;
		case 0xb34: COND_LT_CHECK; HandlePSRTransfer(insn); break;
		case 0xb36: COND_LT_CHECK; HandlePSRTransfer(insn); break;
		case 0xc30: COND_GT_CHECK; HandlePSRTransfer(insn); break;
		case 0xc32: COND_GT_CHECK; HandlePSRTransfer(insn); break;
		case 0xc34: COND_GT_CHECK; HandlePSRTransfer(insn); break;
		case 0xc36: COND_GT_CHECK; HandlePSRTransfer(insn); break;
		case 0xd30: COND_LE_CHECK; HandlePSRTransfer(insn); break;
		case 0xd32: COND_LE_CHECK; HandlePSRTransfer(insn); break;
		case 0xd34: COND_LE_CHECK; HandlePSRTransfer(insn); break;
		case 0xd36: COND_LE_CHECK; HandlePSRTransfer(insn); break;
		case 0xe30: HandlePSRTransfer(insn); break;
		case 0xe32: HandlePSRTransfer(insn); break;
		case 0xe34: HandlePSRTransfer(insn); break;
		case 0xe36: HandlePSRTransfer(insn); break;
		case 0xf30: case 0xf32: case 0xf34: case 0xf36: COND_NV_CHECK; break;
		case 0x031: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x033: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x035: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x037: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0x131: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x133: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x135: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x137: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0x231: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x233: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x235: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x237: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0x331: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x333: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x335: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x337: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0x431: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x433: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x435: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x437: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0x531: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x533: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x535: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x537: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0x631: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x633: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x635: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x637: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0x731: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x733: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x735: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x737: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0x831: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x833: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x835: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x837: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0x931: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0x933: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0x935: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0x937: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0xa31: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0xa33: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0xa35: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0xa37: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0xb31: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0xb33: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0xb35: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0xb37: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0xc31: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0xc33: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0xc35: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0xc37: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0xd31: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0xd33: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0xd35: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0xd37: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0xe31: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TST>(insn); break;
		case 0xe33: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_TEQ>(insn); break;
		case 0xe35: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMP>(insn); break;
		case 0xe37: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_CMN>(insn); break;
		case 0xf31: case 0xf33: case 0xf35: case 0xf37: COND_NV_CHECK; break;
		case 0x038: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x03a: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x03c: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x03e: COND_EQ_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0x138: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x13a: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x13c: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x13e: COND_NE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0x238: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x23a: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x23c: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x23e: COND_CS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0x338: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x33a: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x33c: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x33e: COND_CC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0x438: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x43a: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x43c: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x43e: COND_MI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0x538: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x53a: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x53c: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x53e: COND_PL_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0x638: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x63a: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x63c: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x63e: COND_VS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0x738: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x73a: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x73c: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x73e: COND_VC_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0x838: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x83a: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x83c: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x83e: COND_HI_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0x938: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0x93a: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0x93c: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0x93e: COND_LS_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0xa38: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0xa3a: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0xa3c: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0xa3e: COND_GE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0xb38: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0xb3a: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0xb3c: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0xb3e: COND_LT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0xc38: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0xc3a: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0xc3c: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0xc3e: COND_GT_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0xd38: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0xd3a: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0xd3c: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0xd3e: COND_LE_CHECK; HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0xe38: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_ORR>(insn); break;
		case 0xe3a: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MOV>(insn); break;
		case 0xe3c: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_BIC>(insn); break;
		case 0xe3e: HandleALU<IMM_OP2, NO_FLAGS, OPCODE_MVN>(insn); break;
		case 0xf38: case 0xf3a: case 0xf3c: case 0xf3e: COND_NV_CHECK; break;
		case 0x039: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x03b: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x03d: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x03f: COND_EQ_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0x139: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x13b: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x13d: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x13f: COND_NE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0x239: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x23b: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x23d: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x23f: COND_CS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0x339: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x33b: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x33d: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x33f: COND_CC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0x439: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x43b: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x43d: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x43f: COND_MI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0x539: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x53b: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x53d: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x53f: COND_PL_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0x639: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x63b: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x63d: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x63f: COND_VS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0x739: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x73b: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x73d: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x73f: COND_VC_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0x839: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x83b: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x83d: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x83f: COND_HI_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0x939: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0x93b: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0x93d: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0x93f: COND_LS_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0xa39: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0xa3b: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0xa3d: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0xa3f: COND_GE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0xb39: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0xb3b: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0xb3d: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0xb3f: COND_LT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0xc39: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0xc3b: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0xc3d: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0xc3f: COND_GT_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0xd39: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0xd3b: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0xd3d: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0xd3f: COND_LE_CHECK; HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0xe39: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_ORR>(insn); break;
		case 0xe3b: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MOV>(insn); break;
		case 0xe3d: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_BIC>(insn); break;
		case 0xe3f: HandleALU<IMM_OP2, SET_FLAGS, OPCODE_MVN>(insn); break;
		case 0xf39: case 0xf3b: case 0xf3d: case 0xf3f: COND_NV_CHECK; break;
		case 0x040: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x140: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x240: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x340: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x440: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x540: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x640: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x740: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x840: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x940: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa40: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb40: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc40: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd40: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe40: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf40: COND_NV_CHECK; break;
		case 0x041: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x141: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x241: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x341: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x441: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x541: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x641: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x741: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x841: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x941: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa41: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb41: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc41: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd41: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe41: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf41: COND_NV_CHECK; break;
		case 0x042: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x142: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x242: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x342: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x442: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x542: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x642: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x742: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x842: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x942: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xa42: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xb42: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xc42: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xd42: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xe42: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xf42: COND_NV_CHECK; break;
		case 0x043: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x143: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x243: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x343: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x443: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x543: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x643: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x743: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x843: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x943: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa43: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb43: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc43: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd43: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe43: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf43: COND_NV_CHECK; break;
		case 0x044: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x144: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x244: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x344: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x444: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x544: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x644: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x744: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x844: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x944: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa44: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb44: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc44: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd44: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe44: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf44: COND_NV_CHECK; break;
		case 0x045: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x145: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x245: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x345: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x445: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x545: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x645: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x745: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x845: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x945: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa45: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb45: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc45: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd45: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe45: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf45: COND_NV_CHECK; break;
		case 0x046: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x146: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x246: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x346: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x446: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x546: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x646: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x746: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x846: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x946: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xa46: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xb46: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xc46: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xd46: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xe46: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xf46: COND_NV_CHECK; break;
		case 0x047: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x147: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x247: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x347: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x447: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x547: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x647: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x747: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x847: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x947: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa47: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb47: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc47: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd47: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe47: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf47: COND_NV_CHECK; break;
		case 0x048: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x148: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x248: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x348: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x448: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x548: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x648: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x748: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x848: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x948: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa48: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb48: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc48: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd48: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe48: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf48: COND_NV_CHECK; break;
		case 0x049: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x149: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x249: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x349: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x449: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x549: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x649: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x749: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x849: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x949: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa49: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb49: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc49: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd49: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe49: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf49: COND_NV_CHECK; break;
		case 0x04a: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x14a: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x24a: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x34a: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x44a: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x54a: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x64a: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x74a: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x84a: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x94a: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xa4a: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xb4a: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xc4a: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xd4a: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xe4a: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xf4a: COND_NV_CHECK; break;
		case 0x04b: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x14b: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x24b: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x34b: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x44b: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x54b: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x64b: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x74b: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x84b: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x94b: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa4b: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb4b: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc4b: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd4b: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe4b: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf4b: COND_NV_CHECK; break;
		case 0x04c: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x14c: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x24c: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x34c: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x44c: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x54c: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x64c: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x74c: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x84c: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x94c: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa4c: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb4c: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc4c: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd4c: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe4c: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf4c: COND_NV_CHECK; break;
		case 0x04d: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x14d: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x24d: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x34d: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x44d: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x54d: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x64d: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x74d: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x84d: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x94d: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa4d: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb4d: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc4d: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd4d: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe4d: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf4d: COND_NV_CHECK; break;
		case 0x04e: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x14e: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x24e: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x34e: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x44e: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x54e: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x64e: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x74e: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x84e: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x94e: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xa4e: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xb4e: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xc4e: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xd4e: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xe4e: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xf4e: COND_NV_CHECK; break;
		case 0x04f: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x14f: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x24f: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x34f: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x44f: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x54f: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x64f: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x74f: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x84f: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x94f: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa4f: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb4f: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc4f: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd4f: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe4f: HandleMemSingle<MMU, REG_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf4f: COND_NV_CHECK; break;
		case 0x050: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x150: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x250: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x350: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x450: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x550: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x650: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x750: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x850: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x950: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa50: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb50: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc50: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd50: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe50: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf50: COND_NV_CHECK; break;
		case 0x051: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x151: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x251: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x351: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x451: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x551: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x651: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x751: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x851: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x951: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa51: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb51: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc51: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd51: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe51: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf51: COND_NV_CHECK; break;
		case 0x052: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x152: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x252: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x352: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x452: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x552: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x652: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x752: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x852: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x952: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xa52: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xb52: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xc52: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xd52: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xe52: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xf52: COND_NV_CHECK; break;
		case 0x053: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x153: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x253: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x353: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x453: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x553: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x653: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x753: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x853: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x953: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa53: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb53: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc53: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd53: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe53: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf53: COND_NV_CHECK; break;
		case 0x054: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x154: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x254: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x354: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x454: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x554: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x654: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x754: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x854: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x954: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa54: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb54: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc54: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd54: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe54: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf54: COND_NV_CHECK; break;
		case 0x055: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x155: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x255: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x355: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x455: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x555: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x655: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x755: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x855: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x955: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa55: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb55: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc55: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd55: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe55: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf55: COND_NV_CHECK; break;
		case 0x056: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x156: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x256: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x356: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x456: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x556: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x656: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x756: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x856: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x956: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xa56: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xb56: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xc56: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xd56: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xe56: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xf56: COND_NV_CHECK; break;
		case 0x057: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x157: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x257: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x357: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x457: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x557: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x657: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x757: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x857: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x957: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa57: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb57: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc57: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd57: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe57: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf57: COND_NV_CHECK; break;
		case 0x058: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x158: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x258: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x358: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x458: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x558: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x658: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x758: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x858: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x958: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa58: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb58: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc58: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd58: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe58: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf58: COND_NV_CHECK; break;
		case 0x059: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x159: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x259: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x359: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x459: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x559: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x659: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x759: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x859: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x959: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa59: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb59: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc59: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd59: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe59: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf59: COND_NV_CHECK; break;
		case 0x05a: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x15a: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x25a: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x35a: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x45a: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x55a: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x65a: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x75a: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x85a: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x95a: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xa5a: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xb5a: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xc5a: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xd5a: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xe5a: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xf5a: COND_NV_CHECK; break;
		case 0x05b: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x15b: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x25b: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x35b: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x45b: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x55b: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x65b: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x75b: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x85b: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x95b: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa5b: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb5b: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc5b: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd5b: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe5b: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf5b: COND_NV_CHECK; break;
		case 0x05c: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x15c: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x25c: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x35c: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x45c: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x55c: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x65c: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x75c: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x85c: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x95c: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa5c: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb5c: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc5c: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd5c: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe5c: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf5c: COND_NV_CHECK; break;
		case 0x05d: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x15d: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x25d: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x35d: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x45d: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x55d: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x65d: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x75d: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x85d: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x95d: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa5d: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb5d: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc5d: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd5d: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe5d: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf5d: COND_NV_CHECK; break;
		case 0x05e: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x15e: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x25e: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x35e: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x45e: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x55e: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x65e: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x75e: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x85e: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x95e: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xa5e: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xb5e: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xc5e: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xd5e: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xe5e: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xf5e: COND_NV_CHECK; break;
		case 0x05f: COND_EQ_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x15f: COND_NE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x25f: COND_CS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x35f: COND_CC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x45f: COND_MI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x55f: COND_PL_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x65f: COND_VS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x75f: COND_VC_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x85f: COND_HI_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x95f: COND_LS_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa5f: COND_GE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb5f: COND_LT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc5f: COND_GT_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd5f: COND_LE_CHECK; HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe5f: HandleMemSingle<MMU, REG_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf5f: COND_NV_CHECK; break;
		case 0x060: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x160: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x260: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x360: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x460: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x560: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x660: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x760: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x860: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x960: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa60: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb60: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc60: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd60: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe60: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf60: COND_NV_CHECK; break;
		case 0x061: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x161: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x261: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x361: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x461: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x561: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x661: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x761: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x861: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x961: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa61: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb61: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc61: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd61: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe61: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf61: COND_NV_CHECK; break;
		case 0x062: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x162: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x262: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x362: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x462: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x562: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x662: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x762: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x862: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x962: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xa62: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xb62: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xc62: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xd62: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xe62: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xf62: COND_NV_CHECK; break;
		case 0x063: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x163: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x263: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x363: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x463: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x563: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x663: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x763: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x863: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x963: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa63: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb63: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc63: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd63: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe63: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf63: COND_NV_CHECK; break;
		case 0x064: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x164: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x264: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x364: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x464: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x564: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x664: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x764: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x864: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x964: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa64: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb64: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc64: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd64: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe64: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf64: COND_NV_CHECK; break;
		case 0x065: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x165: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x265: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x365: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x465: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x565: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x665: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x765: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x865: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x965: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa65: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb65: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc65: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd65: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe65: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf65: COND_NV_CHECK; break;
		case 0x066: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x166: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x266: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x366: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x466: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x566: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x666: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x766: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x866: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x966: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xa66: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xb66: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xc66: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xd66: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xe66: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xf66: COND_NV_CHECK; break;
		case 0x067: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x167: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x267: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x367: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x467: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x567: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x667: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x767: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x867: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x967: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa67: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb67: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc67: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd67: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe67: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf67: COND_NV_CHECK; break;
		case 0x068: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x168: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x268: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x368: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x468: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x568: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x668: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x768: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x868: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x968: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa68: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb68: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc68: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd68: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe68: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf68: COND_NV_CHECK; break;
		case 0x069: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x169: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x269: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x369: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x469: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x569: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x669: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x769: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x869: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x969: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa69: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb69: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc69: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd69: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe69: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf69: COND_NV_CHECK; break;
		case 0x06a: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x16a: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x26a: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x36a: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x46a: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x56a: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x66a: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x76a: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x86a: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x96a: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xa6a: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xb6a: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xc6a: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xd6a: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xe6a: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xf6a: COND_NV_CHECK; break;
		case 0x06b: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x16b: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x26b: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x36b: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x46b: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x56b: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x66b: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x76b: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x86b: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x96b: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa6b: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb6b: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc6b: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd6b: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe6b: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf6b: COND_NV_CHECK; break;
		case 0x06c: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x16c: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x26c: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x36c: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x46c: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x56c: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x66c: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x76c: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x86c: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x96c: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa6c: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb6c: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc6c: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd6c: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe6c: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf6c: COND_NV_CHECK; break;
		case 0x06d: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x16d: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x26d: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x36d: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x46d: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x56d: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x66d: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x76d: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x86d: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x96d: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa6d: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb6d: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc6d: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd6d: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe6d: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf6d: COND_NV_CHECK; break;
		case 0x06e: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x16e: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x26e: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x36e: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x46e: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x56e: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x66e: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x76e: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x86e: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x96e: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xa6e: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xb6e: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xc6e: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xd6e: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xe6e: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xf6e: COND_NV_CHECK; break;
		case 0x06f: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x16f: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x26f: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x36f: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x46f: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x56f: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x66f: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x76f: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x86f: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x96f: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa6f: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb6f: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc6f: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd6f: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe6f: HandleMemSingle<MMU, IMM_OP2, POST_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf6f: COND_NV_CHECK; break;
		case 0x070: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x170: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x270: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x370: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x470: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x570: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x670: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x770: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x870: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x970: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa70: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb70: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc70: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd70: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe70: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf70: COND_NV_CHECK; break;
		case 0x071: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x171: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x271: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x371: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x471: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x571: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x671: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x771: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x871: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x971: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa71: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb71: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc71: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd71: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe71: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf71: COND_NV_CHECK; break;
		case 0x072: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x172: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x272: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x372: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x472: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x572: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x672: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x772: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x872: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x972: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xa72: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xb72: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xc72: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xd72: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xe72: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xf72: COND_NV_CHECK; break;
		case 0x073: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x173: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x273: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x373: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x473: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x573: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x673: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x773: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x873: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x973: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa73: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb73: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc73: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd73: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe73: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf73: COND_NV_CHECK; break;
		case 0x074: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x174: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x274: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x374: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x474: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x574: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x674: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x774: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x874: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x974: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa74: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb74: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc74: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd74: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe74: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf74: COND_NV_CHECK; break;
		case 0x075: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x175: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x275: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x375: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x475: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x575: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x675: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x775: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x875: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x975: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa75: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb75: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc75: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd75: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe75: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf75: COND_NV_CHECK; break;
		case 0x076: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x176: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x276: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x376: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x476: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x576: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x676: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x776: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x876: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x976: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xa76: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xb76: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xc76: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xd76: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xe76: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xf76: COND_NV_CHECK; break;
		case 0x077: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x177: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x277: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x377: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x477: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x577: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x677: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x777: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x877: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x977: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa77: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb77: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc77: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd77: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe77: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_DOWN, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf77: COND_NV_CHECK; break;
		case 0x078: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x178: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x278: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x378: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x478: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x578: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x678: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x778: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x878: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x978: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa78: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb78: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc78: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd78: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe78: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf78: COND_NV_CHECK; break;
		case 0x079: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x179: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x279: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x379: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x479: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x579: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x679: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x779: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x879: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x979: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa79: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb79: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc79: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd79: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe79: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf79: COND_NV_CHECK; break;
		case 0x07a: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x17a: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x27a: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x37a: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x47a: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x57a: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x67a: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x77a: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x87a: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0x97a: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xa7a: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xb7a: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xc7a: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xd7a: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xe7a: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_STORE>(insn); break;
		case 0xf7a: COND_NV_CHECK; break;
		case 0x07b: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x17b: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x27b: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x37b: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x47b: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x57b: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x67b: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x77b: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x87b: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0x97b: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa7b: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb7b: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc7b: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd7b: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe7b: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_DWORD, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf7b: COND_NV_CHECK; break;
		case 0x07c: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x17c: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x27c: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x37c: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x47c: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x57c: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x67c: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x77c: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x87c: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0x97c: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xa7c: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xb7c: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xc7c: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xd7c: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xe7c: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_STORE>(insn); break;
		case 0xf7c: COND_NV_CHECK; break;
		case 0x07d: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x17d: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x27d: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x37d: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x47d: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x57d: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x67d: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x77d: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x87d: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0x97d: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xa7d: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xb7d: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xc7d: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xd7d: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xe7d: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, NO_WRITEBACK, IS_LOAD>(insn); break;
		case 0xf7d: COND_NV_CHECK; break;
		case 0x07e: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x17e: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x27e: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x37e: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x47e: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x57e: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x67e: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x77e: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x87e: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0x97e: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xa7e: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xb7e: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xc7e: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xd7e: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xe7e: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_STORE>(insn); break;
		case 0xf7e: COND_NV_CHECK; break;
		case 0x07f: COND_EQ_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x17f: COND_NE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x27f: COND_CS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x37f: COND_CC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x47f: COND_MI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x57f: COND_PL_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x67f: COND_VS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x77f: COND_VC_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x87f: COND_HI_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0x97f: COND_LS_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xa7f: COND_GE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xb7f: COND_LT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xc7f: COND_GT_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xd7f: COND_LE_CHECK; HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xe7f: HandleMemSingle<MMU, IMM_OP2, PRE_INDEXED, OFFSET_UP, SIZE_BYTE, WRITEBACK, IS_LOAD>(insn); break;
		case 0xf7f: COND_NV_CHECK; break;
		case 0x080: case 0x081: COND_EQ_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x180: case 0x181: COND_NE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x280: case 0x281: COND_CS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x380: case 0x381: COND_CC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x480: case 0x481: COND_MI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x580: case 0x581: COND_PL_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x680: case 0x681: COND_VS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x780: case 0x781: COND_VC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x880: case 0x881: COND_HI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x980: case 0x981: COND_LS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xa80: case 0xa81: COND_GE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xb80: case 0xb81: COND_LT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xc80: case 0xc81: COND_GT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xd80: case 0xd81: COND_LE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xe80: case 0xe81: HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xf80: case 0xf81: COND_NV_CHECK; break;
		case 0x082: case 0x083: COND_EQ_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x182: case 0x183: COND_NE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x282: case 0x283: COND_CS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x382: case 0x383: COND_CC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x482: case 0x483: COND_MI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x582: case 0x583: COND_PL_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x682: case 0x683: COND_VS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x782: case 0x783: COND_VC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x882: case 0x883: COND_HI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x982: case 0x983: COND_LS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xa82: case 0xa83: COND_GE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xb82: case 0xb83: COND_LT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xc82: case 0xc83: COND_GT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xd82: case 0xd83: COND_LE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xe82: case 0xe83: HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xf82: case 0xf83: COND_NV_CHECK; break;
		case 0x084: case 0x085: COND_EQ_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x184: case 0x185: COND_NE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x284: case 0x285: COND_CS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x384: case 0x385: COND_CC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x484: case 0x485: COND_MI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x584: case 0x585: COND_PL_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x684: case 0x685: COND_VS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x784: case 0x785: COND_VC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x884: case 0x885: COND_HI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x984: case 0x985: COND_LS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xa84: case 0xa85: COND_GE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xb84: case 0xb85: COND_LT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xc84: case 0xc85: COND_GT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xd84: case 0xd85: COND_LE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xe84: case 0xe85: HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xf84: case 0xf85: COND_NV_CHECK; break;
		case 0x086: case 0x087: COND_EQ_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x186: case 0x187: COND_NE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x286: case 0x287: COND_CS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x386: case 0x387: COND_CC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x486: case 0x487: COND_MI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x586: case 0x587: COND_PL_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x686: case 0x687: COND_VS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x786: case 0x787: COND_VC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x886: case 0x887: COND_HI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x986: case 0x987: COND_LS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xa86: case 0xa87: COND_GE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xb86: case 0xb87: COND_LT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xc86: case 0xc87: COND_GT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xd86: case 0xd87: COND_LE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xe86: case 0xe87: HandleMemBlock<MMU, POST_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xf86: case 0xf87: COND_NV_CHECK; break;
		case 0x088: case 0x089: COND_EQ_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x188: case 0x189: COND_NE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x288: case 0x289: COND_CS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x388: case 0x389: COND_CC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x488: case 0x489: COND_MI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x588: case 0x589: COND_PL_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x688: case 0x689: COND_VS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x788: case 0x789: COND_VC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x888: case 0x889: COND_HI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x988: case 0x989: COND_LS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xa88: case 0xa89: COND_GE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xb88: case 0xb89: COND_LT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xc88: case 0xc89: COND_GT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xd88: case 0xd89: COND_LE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xe88: case 0xe89: HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xf88: case 0xf89: COND_NV_CHECK; break;
		case 0x08a: case 0x08b: COND_EQ_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x18a: case 0x18b: COND_NE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x28a: case 0x28b: COND_CS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x38a: case 0x38b: COND_CC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x48a: case 0x48b: COND_MI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x58a: case 0x58b: COND_PL_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x68a: case 0x68b: COND_VS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x78a: case 0x78b: COND_VC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x88a: case 0x88b: COND_HI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x98a: case 0x98b: COND_LS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xa8a: case 0xa8b: COND_GE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xb8a: case 0xb8b: COND_LT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xc8a: case 0xc8b: COND_GT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xd8a: case 0xd8b: COND_LE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xe8a: case 0xe8b: HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xf8a: case 0xf8b: COND_NV_CHECK; break;
		case 0x08c: case 0x08d: COND_EQ_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x18c: case 0x18d: COND_NE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x28c: case 0x28d: COND_CS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x38c: case 0x38d: COND_CC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x48c: case 0x48d: COND_MI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x58c: case 0x58d: COND_PL_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x68c: case 0x68d: COND_VS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x78c: case 0x78d: COND_VC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x88c: case 0x88d: COND_HI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x98c: case 0x98d: COND_LS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xa8c: case 0xa8d: COND_GE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xb8c: case 0xb8d: COND_LT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xc8c: case 0xc8d: COND_GT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xd8c: case 0xd8d: COND_LE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xe8c: case 0xe8d: HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xf8c: case 0xf8d: COND_NV_CHECK; break;
		case 0x08e: case 0x08f: COND_EQ_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x18e: case 0x18f: COND_NE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x28e: case 0x28f: COND_CS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x38e: case 0x38f: COND_CC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x48e: case 0x48f: COND_MI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x58e: case 0x58f: COND_PL_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x68e: case 0x68f: COND_VS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x78e: case 0x78f: COND_VC_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x88e: case 0x88f: COND_HI_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x98e: case 0x98f: COND_LS_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xa8e: case 0xa8f: COND_GE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xb8e: case 0xb8f: COND_LT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xc8e: case 0xc8f: COND_GT_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xd8e: case 0xd8f: COND_LE_CHECK; HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xe8e: case 0xe8f: HandleMemBlock<MMU, POST_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xf8e: case 0xf8f: COND_NV_CHECK; break;
		case 0x090: case 0x091: COND_EQ_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x190: case 0x191: COND_NE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x290: case 0x291: COND_CS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x390: case 0x391: COND_CC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x490: case 0x491: COND_MI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x590: case 0x591: COND_PL_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x690: case 0x691: COND_VS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x790: case 0x791: COND_VC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x890: case 0x891: COND_HI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x990: case 0x991: COND_LS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xa90: case 0xa91: COND_GE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xb90: case 0xb91: COND_LT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xc90: case 0xc91: COND_GT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xd90: case 0xd91: COND_LE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xe90: case 0xe91: HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xf90: case 0xf91: COND_NV_CHECK; break;
		case 0x092: case 0x093: COND_EQ_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x192: case 0x193: COND_NE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x292: case 0x293: COND_CS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x392: case 0x393: COND_CC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x492: case 0x493: COND_MI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x592: case 0x593: COND_PL_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x692: case 0x693: COND_VS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x792: case 0x793: COND_VC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x892: case 0x893: COND_HI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x992: case 0x993: COND_LS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xa92: case 0xa93: COND_GE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xb92: case 0xb93: COND_LT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xc92: case 0xc93: COND_GT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xd92: case 0xd93: COND_LE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xe92: case 0xe93: HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xf92: case 0xf93: COND_NV_CHECK; break;
		case 0x094: case 0x095: COND_EQ_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x194: case 0x195: COND_NE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x294: case 0x295: COND_CS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x394: case 0x395: COND_CC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x494: case 0x495: COND_MI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x594: case 0x595: COND_PL_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x694: case 0x695: COND_VS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x794: case 0x795: COND_VC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x894: case 0x895: COND_HI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x994: case 0x995: COND_LS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xa94: case 0xa95: COND_GE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xb94: case 0xb95: COND_LT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xc94: case 0xc95: COND_GT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xd94: case 0xd95: COND_LE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xe94: case 0xe95: HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xf94: case 0xf95: COND_NV_CHECK; break;
		case 0x096: case 0x097: COND_EQ_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x196: case 0x197: COND_NE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x296: case 0x297: COND_CS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x396: case 0x397: COND_CC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x496: case 0x497: COND_MI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x596: case 0x597: COND_PL_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x696: case 0x697: COND_VS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x796: case 0x797: COND_VC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x896: case 0x897: COND_HI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0x996: case 0x997: COND_LS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xa96: case 0xa97: COND_GE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xb96: case 0xb97: COND_LT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xc96: case 0xc97: COND_GT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xd96: case 0xd97: COND_LE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xe96: case 0xe97: HandleMemBlock<MMU, PRE_INDEXED, OFFSET_DOWN, S_BIT, WRITEBACK>(insn); break;
		case 0xf96: case 0xf97: COND_NV_CHECK; break;
		case 0x098: case 0x099: COND_EQ_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x198: case 0x199: COND_NE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x298: case 0x299: COND_CS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x398: case 0x399: COND_CC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x498: case 0x499: COND_MI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x598: case 0x599: COND_PL_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x698: case 0x699: COND_VS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x798: case 0x799: COND_VC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x898: case 0x899: COND_HI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0x998: case 0x999: COND_LS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xa98: case 0xa99: COND_GE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xb98: case 0xb99: COND_LT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xc98: case 0xc99: COND_GT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xd98: case 0xd99: COND_LE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xe98: case 0xe99: HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, NO_WRITEBACK>(insn); break;
		case 0xf98: case 0xf99: COND_NV_CHECK; break;
		case 0x09a: case 0x09b: COND_EQ_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x19a: case 0x19b: COND_NE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x29a: case 0x29b: COND_CS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x39a: case 0x39b: COND_CC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x49a: case 0x49b: COND_MI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x59a: case 0x59b: COND_PL_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x69a: case 0x69b: COND_VS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x79a: case 0x79b: COND_VC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x89a: case 0x89b: COND_HI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0x99a: case 0x99b: COND_LS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xa9a: case 0xa9b: COND_GE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xb9a: case 0xb9b: COND_LT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xc9a: case 0xc9b: COND_GT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xd9a: case 0xd9b: COND_LE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xe9a: case 0xe9b: HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, NO_S_BIT, WRITEBACK>(insn); break;
		case 0xf9a: case 0xf9b: COND_NV_CHECK; break;
		case 0x09c: case 0x09d: COND_EQ_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x19c: case 0x19d: COND_NE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x29c: case 0x29d: COND_CS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x39c: case 0x39d: COND_CC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x49c: case 0x49d: COND_MI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x59c: case 0x59d: COND_PL_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x69c: case 0x69d: COND_VS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x79c: case 0x79d: COND_VC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x89c: case 0x89d: COND_HI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0x99c: case 0x99d: COND_LS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xa9c: case 0xa9d: COND_GE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xb9c: case 0xb9d: COND_LT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xc9c: case 0xc9d: COND_GT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xd9c: case 0xd9d: COND_LE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xe9c: case 0xe9d: HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, NO_WRITEBACK>(insn); break;
		case 0xf9c: case 0xf9d: COND_NV_CHECK; break;
		case 0x09e: case 0x09f: COND_EQ_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x19e: case 0x19f: COND_NE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x29e: case 0x29f: COND_CS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x39e: case 0x39f: COND_CC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x49e: case 0x49f: COND_MI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x59e: case 0x59f: COND_PL_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x69e: case 0x69f: COND_VS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x79e: case 0x79f: COND_VC_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x89e: case 0x89f: COND_HI_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0x99e: case 0x99f: COND_LS_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xa9e: case 0xa9f: COND_GE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xb9e: case 0xb9f: COND_LT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xc9e: case 0xc9f: COND_GT_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xd9e: case 0xd9f: COND_LE_CHECK; HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xe9e: case 0xe9f: HandleMemBlock<MMU, PRE_INDEXED, OFFSET_UP, S_BIT, WRITEBACK>(insn); break;
		case 0xf9e: case 0xf9f: COND_NV_CHECK; break;
		case 0x0a0: case 0x0a1: case 0x0a2: case 0x0a3: case 0x0a4: case 0x0a5: case 0x0a6: case 0x0a7:
		case 0x0a8: case 0x0a9: case 0x0aa: case 0x0ab: case 0x0ac: case 0x0ad: case 0x0ae: case 0x0af: COND_EQ_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0x1a0: case 0x1a1: case 0x1a2: case 0x1a3: case 0x1a4: case 0x1a5: case 0x1a6: case 0x1a7:
		case 0x1a8: case 0x1a9: case 0x1aa: case 0x1ab: case 0x1ac: case 0x1ad: case 0x1ae: case 0x1af: COND_NE_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0x2a0: case 0x2a1: case 0x2a2: case 0x2a3: case 0x2a4: case 0x2a5: case 0x2a6: case 0x2a7:
		case 0x2a8: case 0x2a9: case 0x2aa: case 0x2ab: case 0x2ac: case 0x2ad: case 0x2ae: case 0x2af: COND_CS_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0x3a0: case 0x3a1: case 0x3a2: case 0x3a3: case 0x3a4: case 0x3a5: case 0x3a6: case 0x3a7:
		case 0x3a8: case 0x3a9: case 0x3aa: case 0x3ab: case 0x3ac: case 0x3ad: case 0x3ae: case 0x3af: COND_CC_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0x4a0: case 0x4a1: case 0x4a2: case 0x4a3: case 0x4a4: case 0x4a5: case 0x4a6: case 0x4a7:
		case 0x4a8: case 0x4a9: case 0x4aa: case 0x4ab: case 0x4ac: case 0x4ad: case 0x4ae: case 0x4af: COND_MI_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0x5a0: case 0x5a1: case 0x5a2: case 0x5a3: case 0x5a4: case 0x5a5: case 0x5a6: case 0x5a7:
		case 0x5a8: case 0x5a9: case 0x5aa: case 0x5ab: case 0x5ac: case 0x5ad: case 0x5ae: case 0x5af: COND_PL_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0x6a0: case 0x6a1: case 0x6a2: case 0x6a3: case 0x6a4: case 0x6a5: case 0x6a6: case 0x6a7:
		case 0x6a8: case 0x6a9: case 0x6aa: case 0x6ab: case 0x6ac: case 0x6ad: case 0x6ae: case 0x6af: COND_VS_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0x7a0: case 0x7a1: case 0x7a2: case 0x7a3: case 0x7a4: case 0x7a5: case 0x7a6: case 0x7a7:
		case 0x7a8: case 0x7a9: case 0x7aa: case 0x7ab: case 0x7ac: case 0x7ad: case 0x7ae: case 0x7af: COND_VC_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0x8a0: case 0x8a1: case 0x8a2: case 0x8a3: case 0x8a4: case 0x8a5: case 0x8a6: case 0x8a7:
		case 0x8a8: case 0x8a9: case 0x8aa: case 0x8ab: case 0x8ac: case 0x8ad: case 0x8ae: case 0x8af: COND_HI_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0x9a0: case 0x9a1: case 0x9a2: case 0x9a3: case 0x9a4: case 0x9a5: case 0x9a6: case 0x9a7:
		case 0x9a8: case 0x9a9: case 0x9aa: case 0x9ab: case 0x9ac: case 0x9ad: case 0x9ae: case 0x9af: COND_LS_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0xaa0: case 0xaa1: case 0xaa2: case 0xaa3: case 0xaa4: case 0xaa5: case 0xaa6: case 0xaa7:
		case 0xaa8: case 0xaa9: case 0xaaa: case 0xaab: case 0xaac: case 0xaad: case 0xaae: case 0xaaf: COND_GE_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0xba0: case 0xba1: case 0xba2: case 0xba3: case 0xba4: case 0xba5: case 0xba6: case 0xba7:
		case 0xba8: case 0xba9: case 0xbaa: case 0xbab: case 0xbac: case 0xbad: case 0xbae: case 0xbaf: COND_LT_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0xca0: case 0xca1: case 0xca2: case 0xca3: case 0xca4: case 0xca5: case 0xca6: case 0xca7:
		case 0xca8: case 0xca9: case 0xcaa: case 0xcab: case 0xcac: case 0xcad: case 0xcae: case 0xcaf: COND_GT_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0xda0: case 0xda1: case 0xda2: case 0xda3: case 0xda4: case 0xda5: case 0xda6: case 0xda7:
		case 0xda8: case 0xda9: case 0xdaa: case 0xdab: case 0xdac: case 0xdad: case 0xdae: case 0xdaf: COND_LE_CHECK; HandleBranch<BRANCH>(insn); break;
		case 0xea0: case 0xea1: case 0xea2: case 0xea3: case 0xea4: case 0xea5: case 0xea6: case 0xea7:
		case 0xea8: case 0xea9: case 0xeaa: case 0xeab: case 0xeac: case 0xead: case 0xeae: case 0xeaf: HandleBranch<BRANCH>(insn); break;
		case 0xfa0: case 0xfa1: case 0xfa2: case 0xfa3: case 0xfa4: case 0xfa5: case 0xfa6: case 0xfa7:
		case 0xfa8: case 0xfa9: case 0xfaa: case 0xfab: case 0xfac: case 0xfad: case 0xfae: case 0xfaf: COND_NV_CHECK; break;
		case 0x0b0: case 0x0b1: case 0x0b2: case 0x0b3: case 0x0b4: case 0x0b5: case 0x0b6: case 0x0b7:
		case 0x0b8: case 0x0b9: case 0x0ba: case 0x0bb: case 0x0bc: case 0x0bd: case 0x0be: case 0x0bf: COND_EQ_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0x1b0: case 0x1b1: case 0x1b2: case 0x1b3: case 0x1b4: case 0x1b5: case 0x1b6: case 0x1b7:
		case 0x1b8: case 0x1b9: case 0x1ba: case 0x1bb: case 0x1bc: case 0x1bd: case 0x1be: case 0x1bf: COND_NE_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0x2b0: case 0x2b1: case 0x2b2: case 0x2b3: case 0x2b4: case 0x2b5: case 0x2b6: case 0x2b7:
		case 0x2b8: case 0x2b9: case 0x2ba: case 0x2bb: case 0x2bc: case 0x2bd: case 0x2be: case 0x2bf: COND_CS_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0x3b0: case 0x3b1: case 0x3b2: case 0x3b3: case 0x3b4: case 0x3b5: case 0x3b6: case 0x3b7:
		case 0x3b8: case 0x3b9: case 0x3ba: case 0x3bb: case 0x3bc: case 0x3bd: case 0x3be: case 0x3bf: COND_CC_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0x4b0: case 0x4b1: case 0x4b2: case 0x4b3: case 0x4b4: case 0x4b5: case 0x4b6: case 0x4b7:
		case 0x4b8: case 0x4b9: case 0x4ba: case 0x4bb: case 0x4bc: case 0x4bd: case 0x4be: case 0x4bf: COND_MI_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0x5b0: case 0x5b1: case 0x5b2: case 0x5b3: case 0x5b4: case 0x5b5: case 0x5b6: case 0x5b7:
		case 0x5b8: case 0x5b9: case 0x5ba: case 0x5bb: case 0x5bc: case 0x5bd: case 0x5be: case 0x5bf: COND_PL_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0x6b0: case 0x6b1: case 0x6b2: case 0x6b3: case 0x6b4: case 0x6b5: case 0x6b6: case 0x6b7:
		case 0x6b8: case 0x6b9: case 0x6ba: case 0x6bb: case 0x6bc: case 0x6bd: case 0x6be: case 0x6bf: COND_VS_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0x7b0: case 0x7b1: case 0x7b2: case 0x7b3: case 0x7b4: case 0x7b5: case 0x7b6: case 0x7b7:
		case 0x7b8: case 0x7b9: case 0x7ba: case 0x7bb: case 0x7bc: case 0x7bd: case 0x7be: case 0x7bf: COND_VC_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0x8b0: case 0x8b1: case 0x8b2: case 0x8b3: case 0x8b4: case 0x8b5: case 0x8b6: case 0x8b7:
		case 0x8b8: case 0x8b9: case 0x8ba: case 0x8bb: case 0x8bc: case 0x8bd: case 0x8be: case 0x8bf: COND_HI_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0x9b0: case 0x9b1: case 0x9b2: case 0x9b3: case 0x9b4: case 0x9b5: case 0x9b6: case 0x9b7:
		case 0x9b8: case 0x9b9: case 0x9ba: case 0x9bb: case 0x9bc: case 0x9bd: case 0x9be: case 0x9bf: COND_LS_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0xab0: case 0xab1: case 0xab2: case 0xab3: case 0xab4: case 0xab5: case 0xab6: case 0xab7:
		case 0xab8: case 0xab9: case 0xaba: case 0xabb: case 0xabc: case 0xabd: case 0xabe: case 0xabf: COND_GE_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0xbb0: case 0xbb1: case 0xbb2: case 0xbb3: case 0xbb4: case 0xbb5: case 0xbb6: case 0xbb7:
		case 0xbb8: case 0xbb9: case 0xbba: case 0xbbb: case 0xbbc: case 0xbbd: case 0xbbe: case 0xbbf: COND_LT_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0xcb0: case 0xcb1: case 0xcb2: case 0xcb3: case 0xcb4: case 0xcb5: case 0xcb6: case 0xcb7:
		case 0xcb8: case 0xcb9: case 0xcba: case 0xcbb: case 0xcbc: case 0xcbd: case 0xcbe: case 0xcbf: COND_GT_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0xdb0: case 0xdb1: case 0xdb2: case 0xdb3: case 0xdb4: case 0xdb5: case 0xdb6: case 0xdb7:
		case 0xdb8: case 0xdb9: case 0xdba: case 0xdbb: case 0xdbc: case 0xdbd: case 0xdbe: case 0xdbf: COND_LE_CHECK; HandleBranch<BRANCH_LINK>(insn); break;
		case 0xeb0: case 0xeb1: case 0xeb2: case 0xeb3: case 0xeb4: case 0xeb5: case 0xeb6: case 0xeb7:
		case 0xeb8: case 0xeb9: case 0xeba: case 0xebb: case 0xebc: case 0xebd: case 0xebe: case 0xebf: HandleBranch<BRANCH_LINK>(insn); break;
		case 0xfb0: case 0xfb1: case 0xfb2: case 0xfb3: case 0xfb4: case 0xfb5: case 0xfb6: case 0xfb7:
		case 0xfb8: case 0xfb9: case 0xfba: case 0xfbb: case 0xfbc: case 0xfbd: case 0xfbe: case 0xfbf: COND_NV_CHECK; break;
		case 0x0c0: case 0x0c1: case 0x0c2: case 0x0c3: case 0x0c4: case 0x0c5: case 0x0c6: case 0x0c7: case 0x0c8: case 0x0c9: case 0x0ca: case 0x0cb: case 0x0cc: case 0x0cd: case 0x0ce: case 0x0cf:
			COND_EQ_CHECK; HandleCoProcDT(insn); break;
		case 0x1c0: case 0x1c1: case 0x1c2: case 0x1c3: case 0x1c4: case 0x1c5: case 0x1c6: case 0x1c7: case 0x1c8: case 0x1c9: case 0x1ca: case 0x1cb: case 0x1cc: case 0x1cd: case 0x1ce: case 0x1cf:
			COND_NE_CHECK; HandleCoProcDT(insn); break;
		case 0x2c0: case 0x2c1: case 0x2c2: case 0x2c3: case 0x2c4: case 0x2c5: case 0x2c6: case 0x2c7: case 0x2c8: case 0x2c9: case 0x2ca: case 0x2cb: case 0x2cc: case 0x2cd: case 0x2ce: case 0x2cf:
			COND_CS_CHECK; HandleCoProcDT(insn); break;
		case 0x3c0: case 0x3c1: case 0x3c2: case 0x3c3: case 0x3c4: case 0x3c5: case 0x3c6: case 0x3c7: case 0x3c8: case 0x3c9: case 0x3ca: case 0x3cb: case 0x3cc: case 0x3cd: case 0x3ce: case 0x3cf:
			COND_CC_CHECK; HandleCoProcDT(insn); break;
		case 0x4c0: case 0x4c1: case 0x4c2: case 0x4c3: case 0x4c4: case 0x4c5: case 0x4c6: case 0x4c7: case 0x4c8: case 0x4c9: case 0x4ca: case 0x4cb: case 0x4cc: case 0x4cd: case 0x4ce: case 0x4cf:
			COND_MI_CHECK; HandleCoProcDT(insn); break;
		case 0x5c0: case 0x5c1: case 0x5c2: case 0x5c3: case 0x5c4: case 0x5c5: case 0x5c6: case 0x5c7: case 0x5c8: case 0x5c9: case 0x5ca: case 0x5cb: case 0x5cc: case 0x5cd: case 0x5ce: case 0x5cf:
			COND_PL_CHECK; HandleCoProcDT(insn); break;
		case 0x6c0: case 0x6c1: case 0x6c2: case 0x6c3: case 0x6c4: case 0x6c5: case 0x6c6: case 0x6c7: case 0x6c8: case 0x6c9: case 0x6ca: case 0x6cb: case 0x6cc: case 0x6cd: case 0x6ce: case 0x6cf:
			COND_VS_CHECK; HandleCoProcDT(insn); break;
		case 0x7c0: case 0x7c1: case 0x7c2: case 0x7c3: case 0x7c4: case 0x7c5: case 0x7c6: case 0x7c7: case 0x7c8: case 0x7c9: case 0x7ca: case 0x7cb: case 0x7cc: case 0x7cd: case 0x7ce: case 0x7cf:
			COND_VC_CHECK; HandleCoProcDT(insn); break;
		case 0x8c0: case 0x8c1: case 0x8c2: case 0x8c3: case 0x8c4: case 0x8c5: case 0x8c6: case 0x8c7: case 0x8c8: case 0x8c9: case 0x8ca: case 0x8cb: case 0x8cc: case 0x8cd: case 0x8ce: case 0x8cf:
			COND_HI_CHECK; HandleCoProcDT(insn); break;
		case 0x9c0: case 0x9c1: case 0x9c2: case 0x9c3: case 0x9c4: case 0x9c5: case 0x9c6: case 0x9c7: case 0x9c8: case 0x9c9: case 0x9ca: case 0x9cb: case 0x9cc: case 0x9cd: case 0x9ce: case 0x9cf:
			COND_LS_CHECK; HandleCoProcDT(insn); break;
		case 0xac0: case 0xac1: case 0xac2: case 0xac3: case 0xac4: case 0xac5: case 0xac6: case 0xac7: case 0xac8: case 0xac9: case 0xaca: case 0xacb: case 0xacc: case 0xacd: case 0xace: case 0xacf:
			COND_GE_CHECK; HandleCoProcDT(insn); break;
		case 0xbc0: case 0xbc1: case 0xbc2: case 0xbc3: case 0xbc4: case 0xbc5: case 0xbc6: case 0xbc7: case 0xbc8: case 0xbc9: case 0xbca: case 0xbcb: case 0xbcc: case 0xbcd: case 0xbce: case 0xbcf:
			COND_LT_CHECK; HandleCoProcDT(insn); break;
		case 0xcc0: case 0xcc1: case 0xcc2: case 0xcc3: case 0xcc4: case 0xcc5: case 0xcc6: case 0xcc7: case 0xcc8: case 0xcc9: case 0xcca: case 0xccb: case 0xccc: case 0xccd: case 0xcce: case 0xccf:
			COND_GT_CHECK; HandleCoProcDT(insn); break;
		case 0xdc0: case 0xdc1: case 0xdc2: case 0xdc3: case 0xdc4: case 0xdc5: case 0xdc6: case 0xdc7: case 0xdc8: case 0xdc9: case 0xdca: case 0xdcb: case 0xdcc: case 0xdcd: case 0xdce: case 0xdcf:
			COND_LE_CHECK; HandleCoProcDT(insn); break;
		case 0xec0: case 0xec1: case 0xec2: case 0xec3: case 0xec4: case 0xec5: case 0xec6: case 0xec7: case 0xec8: case 0xec9: case 0xeca: case 0xecb: case 0xecc: case 0xecd: case 0xece: case 0xecf:
			HandleCoProcDT(insn); break;
		case 0xfc0: case 0xfc1: case 0xfc2: case 0xfc3: case 0xfc4: case 0xfc5: case 0xfc6: case 0xfc7: case 0xfc8: case 0xfc9: case 0xfca: case 0xfcb: case 0xfcc: case 0xfcd: case 0xfce: case 0xfcf:
			COND_NV_CHECK; break;
		case 0x0d0: case 0x0d1: case 0x0d2: case 0x0d3: case 0x0d4: case 0x0d5: case 0x0d6: case 0x0d7: case 0x0d8: case 0x0d9: case 0x0da: case 0x0db: case 0x0dc: case 0x0dd: case 0x0de: case 0x0df:
			COND_EQ_CHECK; HandleCoProcDT(insn); break;
		case 0x1d0: case 0x1d1: case 0x1d2: case 0x1d3: case 0x1d4: case 0x1d5: case 0x1d6: case 0x1d7: case 0x1d8: case 0x1d9: case 0x1da: case 0x1db: case 0x1dc: case 0x1dd: case 0x1de: case 0x1df:
			COND_NE_CHECK; HandleCoProcDT(insn); break;
		case 0x2d0: case 0x2d1: case 0x2d2: case 0x2d3: case 0x2d4: case 0x2d5: case 0x2d6: case 0x2d7: case 0x2d8: case 0x2d9: case 0x2da: case 0x2db: case 0x2dc: case 0x2dd: case 0x2de: case 0x2df:
			COND_CS_CHECK; HandleCoProcDT(insn); break;
		case 0x3d0: case 0x3d1: case 0x3d2: case 0x3d3: case 0x3d4: case 0x3d5: case 0x3d6: case 0x3d7: case 0x3d8: case 0x3d9: case 0x3da: case 0x3db: case 0x3dc: case 0x3dd: case 0x3de: case 0x3df:
			COND_CC_CHECK; HandleCoProcDT(insn); break;
		case 0x4d0: case 0x4d1: case 0x4d2: case 0x4d3: case 0x4d4: case 0x4d5: case 0x4d6: case 0x4d7: case 0x4d8: case 0x4d9: case 0x4da: case 0x4db: case 0x4dc: case 0x4dd: case 0x4de: case 0x4df:
			COND_MI_CHECK; HandleCoProcDT(insn); break;
		case 0x5d0: case 0x5d1: case 0x5d2: case 0x5d3: case 0x5d4: case 0x5d5: case 0x5d6: case 0x5d7: case 0x5d8: case 0x5d9: case 0x5da: case 0x5db: case 0x5dc: case 0x5dd: case 0x5de: case 0x5df:
			COND_PL_CHECK; HandleCoProcDT(insn); break;
		case 0x6d0: case 0x6d1: case 0x6d2: case 0x6d3: case 0x6d4: case 0x6d5: case 0x6d6: case 0x6d7: case 0x6d8: case 0x6d9: case 0x6da: case 0x6db: case 0x6dc: case 0x6dd: case 0x6de: case 0x6df:
			COND_VS_CHECK; HandleCoProcDT(insn); break;
		case 0x7d0: case 0x7d1: case 0x7d2: case 0x7d3: case 0x7d4: case 0x7d5: case 0x7d6: case 0x7d7: case 0x7d8: case 0x7d9: case 0x7da: case 0x7db: case 0x7dc: case 0x7dd: case 0x7de: case 0x7df:
			COND_VC_CHECK; HandleCoProcDT(insn); break;
		case 0x8d0: case 0x8d1: case 0x8d2: case 0x8d3: case 0x8d4: case 0x8d5: case 0x8d6: case 0x8d7: case 0x8d8: case 0x8d9: case 0x8da: case 0x8db: case 0x8dc: case 0x8dd: case 0x8de: case 0x8df:
			COND_HI_CHECK; HandleCoProcDT(insn); break;
		case 0x9d0: case 0x9d1: case 0x9d2: case 0x9d3: case 0x9d4: case 0x9d5: case 0x9d6: case 0x9d7: case 0x9d8: case 0x9d9: case 0x9da: case 0x9db: case 0x9dc: case 0x9dd: case 0x9de: case 0x9df:
			COND_LS_CHECK; HandleCoProcDT(insn); break;
		case 0xad0: case 0xad1: case 0xad2: case 0xad3: case 0xad4: case 0xad5: case 0xad6: case 0xad7: case 0xad8: case 0xad9: case 0xada: case 0xadb: case 0xadc: case 0xadd: case 0xade: case 0xadf:
			COND_GE_CHECK; HandleCoProcDT(insn); break;
		case 0xbd0: case 0xbd1: case 0xbd2: case 0xbd3: case 0xbd4: case 0xbd5: case 0xbd6: case 0xbd7: case 0xbd8: case 0xbd9: case 0xbda: case 0xbdb: case 0xbdc: case 0xbdd: case 0xbde: case 0xbdf:
			COND_LT_CHECK; HandleCoProcDT(insn); break;
		case 0xcd0: case 0xcd1: case 0xcd2: case 0xcd3: case 0xcd4: case 0xcd5: case 0xcd6: case 0xcd7: case 0xcd8: case 0xcd9: case 0xcda: case 0xcdb: case 0xcdc: case 0xcdd: case 0xcde: case 0xcdf:
			COND_GT_CHECK; HandleCoProcDT(insn); break;
		case 0xdd0: case 0xdd1: case 0xdd2: case 0xdd3: case 0xdd4: case 0xdd5: case 0xdd6: case 0xdd7: case 0xdd8: case 0xdd9: case 0xdda: case 0xddb: case 0xddc: case 0xddd: case 0xdde: case 0xddf:
			COND_LE_CHECK; HandleCoProcDT(insn); break;
		case 0xed0: case 0xed1: case 0xed2: case 0xed3: case 0xed4: case 0xed5: case 0xed6: case 0xed7: case 0xed8: case 0xed9: case 0xeda: case 0xedb: case 0xedc: case 0xedd: case 0xede: case 0xedf:
			HandleCoProcDT(insn); break;
		case 0xfd0: case 0xfd1: case 0xfd2: case 0xfd3: case 0xfd4: case 0xfd5: case 0xfd6: case 0xfd7: case 0xfd8: case 0xfd9: case 0xfda: case 0xfdb: case 0xfdc: case 0xfdd: case 0xfde: case 0xfdf:
			COND_NV_CHECK; break;
		case 0x0e0: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0e1: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0e2: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0e3: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0e4: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0e5: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0e6: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0e7: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0e8: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0e9: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0ea: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0eb: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0ec: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0ed: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0ee: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x0ef: COND_EQ_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e0: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e1: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e2: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e3: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e4: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e5: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e6: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e7: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e8: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1e9: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1ea: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1eb: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1ec: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1ed: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1ee: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x1ef: COND_NE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e0: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e1: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e2: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e3: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e4: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e5: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e6: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e7: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e8: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2e9: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2ea: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2eb: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2ec: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2ed: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2ee: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x2ef: COND_CS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e0: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e1: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e2: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e3: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e4: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e5: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e6: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e7: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e8: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3e9: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3ea: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3eb: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3ec: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3ed: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3ee: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x3ef: COND_CC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e0: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e1: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e2: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e3: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e4: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e5: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e6: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e7: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e8: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4e9: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4ea: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4eb: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4ec: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4ed: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4ee: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x4ef: COND_MI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e0: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e1: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e2: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e3: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e4: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e5: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e6: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e7: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e8: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5e9: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5ea: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5eb: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5ec: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5ed: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5ee: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x5ef: COND_PL_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e0: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e1: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e2: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e3: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e4: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e5: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e6: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e7: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e8: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6e9: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6ea: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6eb: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6ec: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6ed: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6ee: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x6ef: COND_VS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e0: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e1: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e2: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e3: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e4: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e5: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e6: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e7: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e8: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7e9: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7ea: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7eb: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7ec: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7ed: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7ee: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x7ef: COND_VC_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e0: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e1: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e2: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e3: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e4: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e5: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e6: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e7: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e8: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8e9: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8ea: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8eb: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8ec: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8ed: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8ee: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x8ef: COND_HI_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e0: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e1: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e2: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e3: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e4: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e5: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e6: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e7: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e8: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9e9: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9ea: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9eb: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9ec: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9ed: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9ee: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0x9ef: COND_LS_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae0: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae1: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae2: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae3: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae4: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae5: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae6: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae7: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae8: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xae9: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xaea: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xaeb: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xaec: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xaed: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xaee: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xaef: COND_GE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe0: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe1: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe2: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe3: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe4: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe5: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe6: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe7: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe8: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbe9: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbea: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbeb: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbec: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbed: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbee: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xbef: COND_LT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce0: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce1: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce2: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce3: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce4: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce5: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce6: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce7: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce8: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xce9: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xcea: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xceb: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xcec: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xced: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xcee: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xcef: COND_GT_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde0: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde1: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde2: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde3: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde4: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde5: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde6: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde7: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde8: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xde9: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xdea: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xdeb: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xdec: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xded: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xdee: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xdef: COND_LE_CHECK; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); ARM7_ICOUNT -= 3; R15 += 4; break;
		case 0xee0: case 0xee1: case 0xee2: case 0xee3: case 0xee4: case 0xee5: case 0xee6: case 0xee7:
		case 0xee8: case 0xee9: case 0xeea: case 0xeeb: case 0xeec: case 0xeed: case 0xeee: case 0xeef:
			ARM7_ICOUNT -= 3; if (insn & 0x10) HandleCoProcRT(insn); else arm7_do_callback(*m_program, insn, 0, 0); R15 += 4;
			break;
		case 0xfe0: case 0xfe1: case 0xfe2: case 0xfe3: case 0xfe4: case 0xfe5: case 0xfe6: case 0xfe7:
		case 0xfe8: case 0xfe9: case 0xfea: case 0xfeb: case 0xfec: case 0xfed: case 0xfee: case 0xfef:
			COND_NV_CHECK;
			break;
		case 0x0f0: case 0x0f1: case 0x0f2: case 0x0f3: case 0x0f4: case 0x0f5: case 0x0f6: case 0x0f7: case 0x0f8: case 0x0f9: case 0x0fa: case 0x0fb: case 0x0fc: case 0x0fd: case 0x0fe: case 0x0ff:
			COND_EQ_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0x1f0: case 0x1f1: case 0x1f2: case 0x1f3: case 0x1f4: case 0x1f5: case 0x1f6: case 0x1f7: case 0x1f8: case 0x1f9: case 0x1fa: case 0x1fb: case 0x1fc: case 0x1fd: case 0x1fe: case 0x1ff:
			COND_NE_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0x2f0: case 0x2f1: case 0x2f2: case 0x2f3: case 0x2f4: case 0x2f5: case 0x2f6: case 0x2f7: case 0x2f8: case 0x2f9: case 0x2fa: case 0x2fb: case 0x2fc: case 0x2fd: case 0x2fe: case 0x2ff:
			COND_CS_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0x3f0: case 0x3f1: case 0x3f2: case 0x3f3: case 0x3f4: case 0x3f5: case 0x3f6: case 0x3f7: case 0x3f8: case 0x3f9: case 0x3fa: case 0x3fb: case 0x3fc: case 0x3fd: case 0x3fe: case 0x3ff:
			COND_CC_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0x4f0: case 0x4f1: case 0x4f2: case 0x4f3: case 0x4f4: case 0x4f5: case 0x4f6: case 0x4f7: case 0x4f8: case 0x4f9: case 0x4fa: case 0x4fb: case 0x4fc: case 0x4fd: case 0x4fe: case 0x4ff:
			COND_MI_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0x5f0: case 0x5f1: case 0x5f2: case 0x5f3: case 0x5f4: case 0x5f5: case 0x5f6: case 0x5f7: case 0x5f8: case 0x5f9: case 0x5fa: case 0x5fb: case 0x5fc: case 0x5fd: case 0x5fe: case 0x5ff:
			COND_PL_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0x6f0: case 0x6f1: case 0x6f2: case 0x6f3: case 0x6f4: case 0x6f5: case 0x6f6: case 0x6f7: case 0x6f8: case 0x6f9: case 0x6fa: case 0x6fb: case 0x6fc: case 0x6fd: case 0x6fe: case 0x6ff:
			COND_VS_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0x7f0: case 0x7f1: case 0x7f2: case 0x7f3: case 0x7f4: case 0x7f5: case 0x7f6: case 0x7f7: case 0x7f8: case 0x7f9: case 0x7fa: case 0x7fb: case 0x7fc: case 0x7fd: case 0x7fe: case 0x7ff:
			COND_VC_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0x8f0: case 0x8f1: case 0x8f2: case 0x8f3: case 0x8f4: case 0x8f5: case 0x8f6: case 0x8f7: case 0x8f8: case 0x8f9: case 0x8fa: case 0x8fb: case 0x8fc: case 0x8fd: case 0x8fe: case 0x8ff:
			COND_HI_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0x9f0: case 0x9f1: case 0x9f2: case 0x9f3: case 0x9f4: case 0x9f5: case 0x9f6: case 0x9f7: case 0x9f8: case 0x9f9: case 0x9fa: case 0x9fb: case 0x9fc: case 0x9fd: case 0x9fe: case 0x9ff:
			COND_LS_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0xaf0: case 0xaf1: case 0xaf2: case 0xaf3: case 0xaf4: case 0xaf5: case 0xaf6: case 0xaf7: case 0xaf8: case 0xaf9: case 0xafa: case 0xafb: case 0xafc: case 0xafd: case 0xafe: case 0xaff:
			COND_GE_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0xbf0: case 0xbf1: case 0xbf2: case 0xbf3: case 0xbf4: case 0xbf5: case 0xbf6: case 0xbf7: case 0xbf8: case 0xbf9: case 0xbfa: case 0xbfb: case 0xbfc: case 0xbfd: case 0xbfe: case 0xbff:
			COND_LT_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0xcf0: case 0xcf1: case 0xcf2: case 0xcf3: case 0xcf4: case 0xcf5: case 0xcf6: case 0xcf7: case 0xcf8: case 0xcf9: case 0xcfa: case 0xcfb: case 0xcfc: case 0xcfd: case 0xcfe: case 0xcff:
			COND_GT_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0xdf0: case 0xdf1: case 0xdf2: case 0xdf3: case 0xdf4: case 0xdf5: case 0xdf6: case 0xdf7: case 0xdf8: case 0xdf9: case 0xdfa: case 0xdfb: case 0xdfc: case 0xdfd: case 0xdfe: case 0xdff:
			COND_LE_CHECK; m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0xef0: case 0xef1: case 0xef2: case 0xef3: case 0xef4: case 0xef5: case 0xef6: case 0xef7: case 0xef8: case 0xef9: case 0xefa: case 0xefb: case 0xefc: case 0xefd: case 0xefe: case 0xeff:
			m_pendingSwi = true; m_pending_interrupt = true; ARM7_ICOUNT -= 3; break;
		case 0xff0: case 0xff1: case 0xff2: case 0xff3: case 0xff4: case 0xff5: case 0xff6: case 0xff7: case 0xff8: case 0xff9: case 0xffa: case 0xffb: case 0xffc: case 0xffd: case 0xffe: case 0xfff:
			COND_NV_CHECK; break;
	}
}

void arm7_cpu_device::execute_arm9_insn(const uint32_t insn)
{
	const uint32_t op_offset = (insn & 0xF800000) >> 23;
	if (op_offset < 0x14 || op_offset >= 0x18)
		return;
	HandleBranchHBit(insn);
}

void arm7_cpu_device::execute_run()
{
	do
	{
		if (machine().debug_flags & DEBUG_FLAG_ENABLED)
			if (m_pid_offset == 0)
				if (m_prefetch_enabled)
					if (m_tflag)
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<THUMB_MODE, MMU_ON, PREFETCH_ON, IGNORE_PID, CHECK_HOOK>();
						else
							execute_core<THUMB_MODE, MMU_OFF, PREFETCH_ON, IGNORE_PID, CHECK_HOOK>();
					else
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<ARM_MODE, MMU_ON, PREFETCH_ON, IGNORE_PID, CHECK_HOOK>();
						else
							execute_core<ARM_MODE, MMU_OFF, PREFETCH_ON, IGNORE_PID, CHECK_HOOK>();
				else
					if (m_tflag)
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<THUMB_MODE, MMU_ON, PREFETCH_OFF, IGNORE_PID, CHECK_HOOK>();
						else
							execute_core<THUMB_MODE, MMU_OFF, PREFETCH_OFF, IGNORE_PID, CHECK_HOOK>();
					else
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<ARM_MODE, MMU_ON, PREFETCH_OFF, IGNORE_PID, CHECK_HOOK>();
						else
							execute_core<ARM_MODE, MMU_OFF, PREFETCH_OFF, IGNORE_PID, CHECK_HOOK>();
			else
				if (m_prefetch_enabled)
					if (m_tflag)
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<THUMB_MODE, MMU_ON, PREFETCH_ON, VALID_PID, CHECK_HOOK>();
						else
							execute_core<THUMB_MODE, MMU_OFF, PREFETCH_ON, VALID_PID, CHECK_HOOK>();
					else
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<ARM_MODE, MMU_ON, PREFETCH_ON, VALID_PID, CHECK_HOOK>();
						else
							execute_core<ARM_MODE, MMU_OFF, PREFETCH_ON, VALID_PID, CHECK_HOOK>();
				else
					if (m_tflag)
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<THUMB_MODE, MMU_ON, PREFETCH_OFF, VALID_PID, CHECK_HOOK>();
						else
							execute_core<THUMB_MODE, MMU_OFF, PREFETCH_OFF, VALID_PID, CHECK_HOOK>();
					else
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<ARM_MODE, MMU_ON, PREFETCH_OFF, VALID_PID, CHECK_HOOK>();
						else
							execute_core<ARM_MODE, MMU_OFF, PREFETCH_OFF, VALID_PID, CHECK_HOOK>();
		else
			if (m_pid_offset == 0)
				if (m_prefetch_enabled)
					if (m_tflag)
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<THUMB_MODE, MMU_ON, PREFETCH_ON, IGNORE_PID, NO_HOOK>();
						else
							execute_core<THUMB_MODE, MMU_OFF, PREFETCH_ON, IGNORE_PID, NO_HOOK>();
					else
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<ARM_MODE, MMU_ON, PREFETCH_ON, IGNORE_PID, NO_HOOK>();
						else
							execute_core<ARM_MODE, MMU_OFF, PREFETCH_ON, IGNORE_PID, NO_HOOK>();
				else
					if (m_tflag)
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<THUMB_MODE, MMU_ON, PREFETCH_OFF, IGNORE_PID, NO_HOOK>();
						else
							execute_core<THUMB_MODE, MMU_OFF, PREFETCH_OFF, IGNORE_PID, NO_HOOK>();
					else
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<ARM_MODE, MMU_ON, PREFETCH_OFF, IGNORE_PID, NO_HOOK>();
						else
							execute_core<ARM_MODE, MMU_OFF, PREFETCH_OFF, IGNORE_PID, NO_HOOK>();
			else
				if (m_prefetch_enabled)
					if (m_tflag)
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<THUMB_MODE, MMU_ON, PREFETCH_ON, VALID_PID, NO_HOOK>();
						else
							execute_core<THUMB_MODE, MMU_OFF, PREFETCH_ON, VALID_PID, NO_HOOK>();
					else
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<ARM_MODE, MMU_ON, PREFETCH_ON, VALID_PID, NO_HOOK>();
						else
							execute_core<ARM_MODE, MMU_OFF, PREFETCH_ON, VALID_PID, NO_HOOK>();
				else
					if (m_tflag)
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<THUMB_MODE, MMU_ON, PREFETCH_OFF, VALID_PID, NO_HOOK>();
						else
							execute_core<THUMB_MODE, MMU_OFF, PREFETCH_OFF, VALID_PID, NO_HOOK>();
					else
						if (COPRO_CTRL & COPRO_CTRL_MMU_EN)
							execute_core<ARM_MODE, MMU_ON, PREFETCH_OFF, VALID_PID, NO_HOOK>();
						else
							execute_core<ARM_MODE, MMU_OFF, PREFETCH_OFF, VALID_PID, NO_HOOK>();

		if (m_stashed_icount >= 0)
		{
			m_icount = m_stashed_icount;
			m_stashed_icount = -1;
		}
	} while (m_icount > 0);
}


void arm7_cpu_device::execute_set_input(int irqline, int state)
{
	switch (irqline) {
	case ARM7_IRQ_LINE: /* IRQ */
		m_pendingIrq = state ? true : false;
		break;

	case ARM7_FIRQ_LINE: /* FIRQ */
		m_pendingFiq = state ? true : false;
		break;

	case ARM7_ABORT_EXCEPTION:
		m_pendingAbtD = state ? true : false;
		break;

	case ARM7_ABORT_PREFETCH_EXCEPTION:
		m_pendingAbtP = state ? true : false;
		break;

	case ARM7_UNDEFINE_EXCEPTION:
		m_pendingUnd = state ? true : false;
		break;
	}

	update_irq_state();
	arm7_check_irq_state();
}


util::disasm_interface *arm7_cpu_device::create_disassembler()
{
	return new arm7_disassembler(this);
}

bool arm7_cpu_device::get_t_flag() const
{
	return m_tflag ? true : false;
}


/* ARM system coprocessor support */

WRITE32_MEMBER( arm7_cpu_device::arm7_do_callback )
{
	m_pendingUnd = true;
	m_pending_interrupt = true;
}

READ32_MEMBER( arm7_cpu_device::arm7_rt_r_callback )
{
	uint32_t opcode = offset;
	uint8_t cReg = ( opcode & INSN_COPRO_CREG ) >> INSN_COPRO_CREG_SHIFT;
	uint8_t op2 =  ( opcode & INSN_COPRO_OP2 )  >> INSN_COPRO_OP2_SHIFT;
	uint8_t op3 =    opcode & INSN_COPRO_OP3;
	uint8_t cpnum = (opcode & INSN_COPRO_CPNUM) >> INSN_COPRO_CPNUM_SHIFT;
	uint32_t data = 0;

//    printf("cpnum %d cReg %d op2 %d op3 %d (%x)\n", cpnum, cReg, op2, op3, GET_REGISTER(arm, 15));

	// we only handle system copro here
	if (cpnum != 15)
	{
		if (m_archFlags & ARCHFLAG_XSCALE)
		{
			// handle XScale specific CP14
			if (cpnum == 14)
			{
				switch( cReg )
				{
					case 1: // clock counter
						data = (uint32_t)total_cycles();
						break;

					default:
						break;
				}
			}
			else
			{
				fatalerror("XScale: Unhandled coprocessor %d (archFlags %x)\n", cpnum, m_archFlags);
			}

			return data;
		}
		else
		{
			LOG( ("ARM7: Unhandled coprocessor %d (archFlags %x)\n", cpnum, m_archFlags) );
			m_pendingUnd = true;
			m_pending_interrupt = true;
			return 0;
		}
	}

	switch( cReg )
	{
		case 4:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			// RESERVED
			LOG( ( "arm7_rt_r_callback CR%d, RESERVED\n", cReg ) );
			break;
		case 0:             // ID
			switch(op2)
			{
			case 0:
				data = m_copro_id;
				break;
			case 1: // cache type
				data = 0x0f0d2112;  // HACK: value expected by ARMWrestler (probably Nintendo DS ARM9's value)
				//data = (6 << 25) | (1 << 24) | (0x172 << 12) | (0x172 << 0); // ARM920T (S3C24xx)
				break;
			case 2: // TCM type
				data = 0;
				break;
			case 3: // TLB type
				data = 0;
				break;
			case 4: // MPU type
				data = 0;
				break;
			}
			LOG( ( "arm7_rt_r_callback, ID %02x (%02x) -> %08x (PC=%08x)\n",op2,m_archRev,data,GET_PC ) );
			break;
		case 1:             // Control
			data = COPRO_CTRL | 0x70;   // bits 4-6 always read back as "1" (bit 3 too in XScale)
			break;
		case 2:             // Translation Table Base
			data = COPRO_TLB_BASE;
			break;
		case 3:             // Domain Access Control
			LOG( ( "arm7_rt_r_callback, Domain Access Control\n" ) );
			data = COPRO_DOMAIN_ACCESS_CONTROL;
			break;
		case 5:             // Fault Status
			LOG( ( "arm7_rt_r_callback, Fault Status\n" ) );
			switch (op3)
			{
				case 0: data = COPRO_FAULT_STATUS_D; break;
				case 1: data = COPRO_FAULT_STATUS_P; break;
			}
			break;
		case 6:             // Fault Address
			LOG( ( "arm7_rt_r_callback, Fault Address\n" ) );
			data = COPRO_FAULT_ADDRESS;
			break;
		case 13:            // Read Process ID (PID)
			LOG( ( "arm7_rt_r_callback, Read PID\n" ) );
			data = COPRO_FCSE_PID;
			break;
		case 14:            // Read Breakpoint
			LOG( ( "arm7_rt_r_callback, Read Breakpoint\n" ) );
			break;
		case 15:            // Test, Clock, Idle
			LOG( ( "arm7_rt_r_callback, Test / Clock / Idle \n" ) );
			break;
	}

	return data;
}

WRITE32_MEMBER( arm7_cpu_device::arm7_rt_w_callback )
{
	uint32_t opcode = offset;
	uint8_t cReg = ( opcode & INSN_COPRO_CREG ) >> INSN_COPRO_CREG_SHIFT;
	uint8_t op2 =  ( opcode & INSN_COPRO_OP2 )  >> INSN_COPRO_OP2_SHIFT;
	uint8_t op3 =    opcode & INSN_COPRO_OP3;
	uint8_t cpnum = (opcode & INSN_COPRO_CPNUM) >> INSN_COPRO_CPNUM_SHIFT;

	// handle XScale specific CP14 - just eat writes for now
	if (cpnum != 15)
	{
		if (cpnum == 14)
		{
			LOG( ("arm7_rt_w_callback: write %x to XScale CP14 reg %d\n", data, cReg) );
			return;
		}
		else
		{
			LOG( ("ARM7: Unhandled coprocessor %d\n", cpnum) );
			m_pendingUnd = true;
			m_pending_interrupt = true;
			return;
		}
	}

	switch( cReg )
	{
		case 0:
		case 4:
		case 10:
		case 11:
		case 12:
			// RESERVED
			LOG( ( "arm7_rt_w_callback CR%d, RESERVED = %08x\n", cReg, data) );
			break;
		case 1:             // Control
		{
			LOG( ( "arm7_rt_w_callback Control = %08x (%d) (%d)\n", data, op2, op3 ) );
			LOG( ( "    MMU:%d, Address Fault:%d, Data Cache:%d, Write Buffer:%d\n",
					data & COPRO_CTRL_MMU_EN, ( data & COPRO_CTRL_ADDRFAULT_EN ) >> COPRO_CTRL_ADDRFAULT_EN_SHIFT,
					( data & COPRO_CTRL_DCACHE_EN ) >> COPRO_CTRL_DCACHE_EN_SHIFT,
					( data & COPRO_CTRL_WRITEBUF_EN ) >> COPRO_CTRL_WRITEBUF_EN_SHIFT ) );
			LOG( ( "    Endianness:%d, System:%d, ROM:%d, Instruction Cache:%d\n",
					( data & COPRO_CTRL_ENDIAN ) >> COPRO_CTRL_ENDIAN_SHIFT,
					( data & COPRO_CTRL_SYSTEM ) >> COPRO_CTRL_SYSTEM_SHIFT,
					( data & COPRO_CTRL_ROM ) >> COPRO_CTRL_ROM_SHIFT,
					( data & COPRO_CTRL_ICACHE_EN ) >> COPRO_CTRL_ICACHE_EN_SHIFT ) );
			LOG( ( "    Int Vector Adjust:%d\n", ( data & COPRO_CTRL_INTVEC_ADJUST ) >> COPRO_CTRL_INTVEC_ADJUST_SHIFT ) );

			uint32_t old_enable = COPRO_CTRL & COPRO_CTRL_MMU_EN;
			COPRO_CTRL = data & COPRO_CTRL_MASK;
			if ((COPRO_CTRL & COPRO_CTRL_MMU_EN) != old_enable)
				set_mode_changed();
			if (data & COPRO_CTRL_MMU_EN)
			{
				for (size_t i = 0; i < 0x1000; i++)
				{
					const uint32_t desc_lvl1 = m_tlb_base[i];
					//translation_entry &entry = m_translated_table[i];
					m_lvl1_type[i] = desc_lvl1 & 3;
					m_dac_index[i] = (desc_lvl1 >> 5) & 0xf;
					m_lvl1_ap[i] = (desc_lvl1 >> 8) & 0xc;
					m_section_bits[i] = desc_lvl1 & COPRO_TLB_SECTION_PAGE_MASK;

					const uint32_t index = m_lvl1_ap[i] | m_decoded_access_control[m_dac_index[i]];
					m_section_read_fault[i] = m_read_fault_table[index];
					m_section_write_fault[i] = m_write_fault_table[index];
					m_early_faultless[i] = (m_lvl1_type[i] == COPRO_TLB_SECTION_TABLE && m_section_read_fault[i] == FAULT_NONE) ? 1 : 0;
				}
			}
			break;
		}
		case 2:             // Translation Table Base
			LOG( ( "arm7_rt_w_callback TLB Base = %08x (%d) (%d)\n", data, op2, op3 ) );
			COPRO_TLB_BASE = data;
			m_tlb_base_mask = data & COPRO_TLB_BASE_MASK;
			m_tlb_base = (uint32_t*)m_direct->read_ptr(m_tlb_base_mask);
			break;
		case 3:             // Domain Access Control
			LOG( ( "arm7_rt_w_callback Domain Access Control = %08x (%d) (%d)\n", data, op2, op3 ) );
			COPRO_DOMAIN_ACCESS_CONTROL = data;
			for (int i = 0; i < 32; i += 2)
			{
				m_decoded_access_control[i >> 1] = ((COPRO_DOMAIN_ACCESS_CONTROL >> i) & 3);
			}
			break;
		case 5:             // Fault Status
			LOG( ( "arm7_rt_w_callback Fault Status = %08x (%d) (%d)\n", data, op2, op3 ) );
			switch (op3)
			{
				case 0: COPRO_FAULT_STATUS_D = data; break;
				case 1: COPRO_FAULT_STATUS_P = data; break;
			}
			break;
		case 6:             // Fault Address
			LOG( ( "arm7_rt_w_callback Fault Address = %08x (%d) (%d)\n", data, op2, op3 ) );
			COPRO_FAULT_ADDRESS = data;
			break;
		case 7:             // Cache Operations
//            LOG( ( "arm7_rt_w_callback Cache Ops = %08x (%d) (%d)\n", data, op2, op3 ) );
			break;
		case 8:             // TLB Operations
			LOG( ( "arm7_rt_w_callback TLB Ops = %08x (%d) (%d)\n", data, op2, op3 ) );
			break;
		case 9:             // Read Buffer Operations
			LOG( ( "arm7_rt_w_callback Read Buffer Ops = %08x (%d) (%d)\n", data, op2, op3 ) );
			break;
		case 13:            // Write Process ID (PID)
			LOG( ( "arm7_rt_w_callback Write PID = %08x (%d) (%d)\n", data, op2, op3 ) );
			COPRO_FCSE_PID = data;
			m_pid_offset = (((COPRO_FCSE_PID >> 25) & 0x7F)) * 0x2000000;
			set_mode_changed();
			break;
		case 14:            // Write Breakpoint
			LOG( ( "arm7_rt_w_callback Write Breakpoint = %08x (%d) (%d)\n", data, op2, op3 ) );
			break;
		case 15:            // Test, Clock, Idle
			LOG( ( "arm7_rt_w_callback Test / Clock / Idle = %08x (%d) (%d)\n", data, op2, op3 ) );
			break;
	}
}

READ32_MEMBER( arm946es_cpu_device::arm7_rt_r_callback )
{
	uint32_t opcode = offset;
	uint8_t cReg = ( opcode & INSN_COPRO_CREG ) >> INSN_COPRO_CREG_SHIFT;
	uint8_t op2 =  ( opcode & INSN_COPRO_OP2 )  >> INSN_COPRO_OP2_SHIFT;
	uint8_t op3 =    opcode & INSN_COPRO_OP3;
	uint8_t cpnum = (opcode & INSN_COPRO_CPNUM) >> INSN_COPRO_CPNUM_SHIFT;
	uint32_t data = 0;

	//printf("arm7946: read cpnum %d cReg %d op2 %d op3 %d (%x)\n", cpnum, cReg, op2, op3, opcode);
	if (cpnum == 15)
	{
		switch( cReg )
		{
			case 0:
				switch (op2)
				{
					case 0: // chip ID
						data = 0x41059461;
						break;

					case 1: // cache ID
						data = 0x0f0d2112;
						break;

					case 2: // TCM size
						data = (6 << 6) | (5 << 18);
						break;
				}
				break;

			case 1:
				return cp15_control;
				break;

			case 9:
				if (op3 == 1)
				{
					if (op2 == 0)
					{
						return cp15_dtcm_reg;
					}
					else
					{
						return cp15_itcm_reg;
					}
				}
				break;
		}
	}

	return data;
}

WRITE32_MEMBER( arm946es_cpu_device::arm7_rt_w_callback )
{
	uint32_t opcode = offset;
	uint8_t cReg = ( opcode & INSN_COPRO_CREG ) >> INSN_COPRO_CREG_SHIFT;
	uint8_t op2 =  ( opcode & INSN_COPRO_OP2 )  >> INSN_COPRO_OP2_SHIFT;
	uint8_t op3 =    opcode & INSN_COPRO_OP3;
	uint8_t cpnum = (opcode & INSN_COPRO_CPNUM) >> INSN_COPRO_CPNUM_SHIFT;

//  printf("arm7946: copro %d write %x to cReg %d op2 %d op3 %d (mask %08x)\n", cpnum, data, cReg, op2, op3, mem_mask);

	if (cpnum == 15)
	{
		switch (cReg)
		{
			case 1: // control
				cp15_control = data;
				RefreshDTCM();
				RefreshITCM();
				break;

			case 2: // Protection Unit cacheability bits
				break;

			case 3: // write bufferability bits for PU
				break;

			case 5: // protection unit region controls
				break;

			case 6: // protection unit region controls 2
				break;

			case 7: // cache commands
				break;

			case 9: // cache lockdown & TCM controls
				if (op3 == 1)
				{
					if (op2 == 0)
					{
						cp15_dtcm_reg = data;
						RefreshDTCM();
					}
					else if (op2 == 1)
					{
						cp15_itcm_reg = data;
						RefreshITCM();
					}
				}
				break;
		}
	}
}

void arm946es_cpu_device::RefreshDTCM()
{
	if (cp15_control & (1<<16))
	{
		cp15_dtcm_base = (cp15_dtcm_reg & ~0xfff);
		cp15_dtcm_size = 512 << ((cp15_dtcm_reg & 0x3f) >> 1);
		cp15_dtcm_end = cp15_dtcm_base + cp15_dtcm_size;
		//printf("DTCM enabled: base %08x size %x\n", cp15_dtcm_base, cp15_dtcm_size);
	}
	else
	{
		cp15_dtcm_base = 0xffffffff;
		cp15_dtcm_size = cp15_dtcm_end = 0;
	}
}

void arm946es_cpu_device::RefreshITCM()
{
	if (cp15_control & (1<<18))
	{
		cp15_itcm_base = 0; //(cp15_itcm_reg & ~0xfff);
		cp15_itcm_size = 512 << ((cp15_itcm_reg & 0x3f) >> 1);
		cp15_itcm_end = cp15_itcm_base + cp15_itcm_size;
		//printf("ITCM enabled: base %08x size %x\n", cp15_dtcm_base, cp15_dtcm_size);
	}
	else
	{
		cp15_itcm_base = 0xffffffff;
		cp15_itcm_size = cp15_itcm_end = 0;
	}
}

void arm946es_cpu_device::arm7_cpu_write32_mmu(uint32_t addr, uint32_t data) { arm7_cpu_write32(addr, data); }
void arm946es_cpu_device::arm7_cpu_write32(uint32_t addr, uint32_t data)
{
	addr &= ~3;

	if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
	{
		uint32_t *wp = (uint32_t *)&ITCM[addr&0x7fff];
		*wp = data;
		return;
	}
	else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
	{
		uint32_t *wp = (uint32_t *)&DTCM[addr&0x3fff];
		*wp = data;
		return;
	}

	m_program->write_dword(addr, data);
}

void arm946es_cpu_device::arm7_cpu_write16_mmu(uint32_t addr, uint16_t data) { arm7_cpu_write16(addr, data); }
void arm946es_cpu_device::arm7_cpu_write16(uint32_t addr, uint16_t data)
{
	addr &= ~1;
	if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
	{
		uint16_t *wp = (uint16_t *)&ITCM[addr&0x7fff];
		*wp = data;
		return;
	}
	else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
	{
		uint16_t *wp = (uint16_t *)&DTCM[addr&0x3fff];
		*wp = data;
		return;
	}

	m_program->write_word(addr, data);
}

void arm946es_cpu_device::arm7_cpu_write8_mmu(uint32_t addr, uint8_t data) { arm7_cpu_write8(addr, data); }
void arm946es_cpu_device::arm7_cpu_write8(uint32_t addr, uint8_t data)
{
	if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
	{
		ITCM[addr&0x7fff] = data;
		return;
	}
	else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
	{
		DTCM[addr&0x3fff] = data;
		return;
	}

	m_program->write_byte(addr, data);
}

uint32_t arm946es_cpu_device::arm7_cpu_read32_mmu(uint32_t addr) { return arm7_cpu_read32(addr); }
uint32_t arm946es_cpu_device::arm7_cpu_read32(uint32_t addr)
{
	uint32_t result;

	if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
	{
		if (addr & 3)
		{
			uint32_t *wp = (uint32_t *)&ITCM[(addr & ~3)&0x7fff];
			result = *wp;
			result = (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
		}
		else
		{
			uint32_t *wp = (uint32_t *)&ITCM[addr&0x7fff];
			result = *wp;
		}
	}
	else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
	{
		if (addr & 3)
		{
			uint32_t *wp = (uint32_t *)&DTCM[(addr & ~3)&0x3fff];
			result = *wp;
			result = (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
		}
		else
		{
			uint32_t *wp = (uint32_t *)&DTCM[addr&0x3fff];
			result = *wp;
		}
	}
	else
	{
		if (addr & 3)
		{
			result = m_program->read_dword(addr & ~3);
			result = (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
		}
		else
		{
			result = m_program->read_dword(addr);
		}
	}
	return result;
}

uint32_t arm946es_cpu_device::arm7_cpu_read16_mmu(uint32_t addr) { return arm7_cpu_read16(addr); }
uint32_t arm946es_cpu_device::arm7_cpu_read16(uint32_t addr)
{
	addr &= ~1;

	if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
	{
		uint16_t *wp = (uint16_t *)&ITCM[addr & 0x7fff];
		return *wp;
	}
	else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
	{
		uint16_t *wp = (uint16_t *)&DTCM[addr &0x3fff];
		return *wp;
	}

	return m_program->read_word(addr);
}

uint8_t arm946es_cpu_device::arm7_cpu_read8_mmu(uint32_t addr) { return arm7_cpu_read8(addr); }
uint8_t arm946es_cpu_device::arm7_cpu_read8(uint32_t addr)
{
	if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
	{
		return ITCM[addr & 0x7fff];
	}
	else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
	{
		return DTCM[addr & 0x3fff];
	}

	// Handle through normal 8 bit handler (for 32 bit cpu)
	return m_program->read_byte(addr);
}

void arm7_cpu_device::arm7_dt_r_callback(const uint32_t insn, uint32_t *prn)
{
	uint8_t cpn = (insn >> 8) & 0xF;
	if ((m_archFlags & ARCHFLAG_XSCALE) && (cpn == 0))
	{
		LOG( ( "arm7_dt_r_callback: DSP Coprocessor 0 (CP0) not yet emulated (PC %08x)\n", GET_PC ) );
	}
	else
	{
		m_pendingUnd = true;
		m_pending_interrupt = true;
	}
}


void arm7_cpu_device::arm7_dt_w_callback(const uint32_t insn, uint32_t *prn)
{
	uint8_t cpn = (insn >> 8) & 0xF;
	if ((m_archFlags & ARCHFLAG_XSCALE) && (cpn == 0))
	{
		LOG( ( "arm7_dt_w_callback: DSP Coprocessor 0 (CP0) not yet emulated (PC %08x)\n", GET_PC ) );
	}
	else
	{
		m_pendingUnd = true;
		m_pending_interrupt = true;
	}
}


/***************************************************************************
 * Default Memory Handlers
 ***************************************************************************/
void arm7_cpu_device::arm7_cpu_write32(uint32_t addr, uint32_t data) { m_program->write_dword(addr & ~3, data); }
void arm7_cpu_device::arm7_cpu_write32_mmu(uint32_t addr, uint32_t data)
{
	if (!arm7_tlb_translate<TLB_WRITE>(addr))
		return;

	m_program->write_dword(addr & ~3, data);
}

void arm7_cpu_device::arm7_cpu_write16(uint32_t addr, uint16_t data) { m_program->write_word(addr & ~1, data); }
void arm7_cpu_device::arm7_cpu_write16_mmu(uint32_t addr, uint16_t data)
{
	if (!arm7_tlb_translate<TLB_WRITE>(addr))
		return;

	m_program->write_word(addr & ~1, data);
}

void arm7_cpu_device::arm7_cpu_write8(uint32_t addr, uint8_t data) { m_program->write_byte(addr, data); }
void arm7_cpu_device::arm7_cpu_write8_mmu(uint32_t addr, uint8_t data)
{
	if (!arm7_tlb_translate<TLB_WRITE>(addr))
		return;

	m_program->write_byte(addr, data);
}

uint32_t arm7_cpu_device::arm7_cpu_read32_mmu(uint32_t addr)
{
	if (!arm7_tlb_translate<TLB_READ>(addr))
		return 0;

	if (addr & 3)
	{
		const uint32_t result = m_program->read_dword(addr & ~3);
		return (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
	}
	else
	{
		return m_program->read_dword(addr);
	}
}

uint32_t arm7_cpu_device::arm7_cpu_read32(uint32_t addr)
{
	if (addr & 3)
	{
		const uint32_t result = m_program->read_dword(addr & ~3);
		return (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
	}
	else
	{
		return m_program->read_dword(addr);
	}
}

uint32_t arm7_cpu_device::arm7_cpu_read16_mmu(uint32_t addr)
{
	if (!arm7_tlb_translate<TLB_READ>(addr))
		return 0;

	uint32_t result = m_program->read_word(addr & ~1);

	if (addr & 1)
	{
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 24);
	}

	return result;
}

uint32_t arm7_cpu_device::arm7_cpu_read16(uint32_t addr)
{
	uint32_t result = m_program->read_word(addr & ~1);

	if (addr & 1)
	{
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 24);
	}

	return result;
}

uint8_t arm7_cpu_device::arm7_cpu_read8_mmu(uint32_t addr)
{
	if (!arm7_tlb_translate<TLB_READ>(addr))
		return 0;
	return m_program->read_byte(addr);
}

uint8_t arm7_cpu_device::arm7_cpu_read8(uint32_t addr)
{
	return m_program->read_byte(addr);
}

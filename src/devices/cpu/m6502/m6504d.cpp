// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    m6504d.cpp

    Mostek 6502, NMOS variant with reduced address bus, disassembler

***************************************************************************/

#include "m6504d.h"
#include "cpu/m6502/m6504d.hxx"

m6504_disassembler::m6504_disassembler() : m6502_base_disassembler(disasm_entries)
{
}

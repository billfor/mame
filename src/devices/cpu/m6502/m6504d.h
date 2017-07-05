// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    m6504d.h

    Mostek 6502, NMOS variant with reduced address bus, disassembler

***************************************************************************/

#ifndef MAME_CPU_M6502_M6504D_H
#define MAME_CPU_M6502_M6504D_H

#pragma once

#include "m6502d.h"

class m6504_disassembler : public m6502_base_disassembler
{
public:
	m6504_disassembler();
	virtual ~m6504_disassembler() = default;

private:
	static const disasm_entry disasm_entries[0x100];
};

#endif

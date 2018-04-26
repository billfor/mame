// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/****************************************************************************

    Palm m515 (MC68VZ328) emulation

    Driver by Ryan Holtz

****************************************************************************/

#include "emu.h"
#include "cpu/m68000/m68000.h"
#include "machine/mc68328.h"
#include "screen.h"

#define MC68VZ328_TAG "mc68vz328"
#define RAM_TAG "ram"

class palmcolor_state : public driver_device
{
public:
	palmcolor_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_lsi(*this, MC68VZ328_TAG)
		, m_ram(*this, RAM_TAG)
	{
	}

	void palmcolor(machine_config &config);
	void memory_map(address_map &map);

protected:
	required_device<cpu_device> m_maincpu;
	required_device<mc68vz328_device> m_lsi;
	required_device<ram_device> m_ram;

	void machine_start() override;
	void machine_reset() override;

	DECLARE_PALETTE_INIT(palmcolor);
};


void palmcolor_state::machine_start()
{
}

void palmcolor_state::machine_reset()
{
}

PALETTE_INIT_MEMBER(palmcolor_state, palmcolor)
{
	palette.set_pen_color(0, 0x7b, 0x8c, 0x5a);
	palette.set_pen_color(1, 0x00, 0x00, 0x00);
}


void palmcolor_state::memory_map(address_map &map)
{
	map(0x00000000, 0xffffffff).rw(m_lsi, FUNC(mc68vz328_device::mem_r), FUNC(mc68vz328_device::mem_w));
}

MACHINE_CONFIG_START(palmcolor_state::palmcolor)
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", MC68VZ328_CPU, 32768*1012)        /* 33.161216 MHz(?) */
	MCFG_CPU_PROGRAM_MAP(memory_map)

	MCFG_QUANTUM_TIME(attotime::from_hz(60))

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(1260))
	/* video hardware */
	MCFG_SCREEN_VIDEO_ATTRIBUTES(VIDEO_UPDATE_BEFORE_VBLANK)
	MCFG_SCREEN_SIZE(160, 220)
	MCFG_SCREEN_VISIBLE_AREA(0, 159, 0, 219)
	MCFG_SCREEN_UPDATE_DEVICE(MC68VZ328_TAG, mc68vz328_device, screen_update)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_PALETTE_ADD("palette", 2)
	MCFG_PALETTE_INIT_OWNER(palmcolor_state, palmcolor)

	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("16M")

	MCFG_DEVICE_ADD(MC68VZ328_TAG, MC68VZ328, 0)
	MCFG_MC68328_CPU("maincpu")
	MCFG_MC68VZ328_BOOT_REGION("bios")
	MCFG_MC68VZ328_RAM_TAG(RAM_TAG)
MACHINE_CONFIG_END

static INPUT_PORTS_START(palmcolor)
INPUT_PORTS_END



ROM_START( palmm505 )
	ROM_REGION16_BE( 0x408000, "bios", 0 )
	ROM_SYSTEM_BIOS( 0, "4.0e", "Palm OS 4.0 (English)" )
	ROMX_LOAD( "palmos40-en-m505.rom", 0x008000, 0x400000, CRC(822a4679) SHA1(a4f5e9f7edb1926647ea07969200c5c5e1521bdf), ROM_GROUPWORD | ROM_BIOS(1) )
	ROM_RELOAD(0x000000, 0x004000)
	ROM_SYSTEM_BIOS( 1, "4.1e", "Palm OS 4.1 (English)" )
	ROMX_LOAD( "palmos41-en-m505.rom", 0x008000, 0x400000, CRC(d248202a) SHA1(65e1bd08b244c589b4cd10fe573e0376aba90e5f), ROM_GROUPWORD | ROM_BIOS(2) )
	ROM_RELOAD(0x000000, 0x004000)
	ROM_DEFAULT_BIOS( "4.1e" )
ROM_END

ROM_START( palmm515 )
	ROM_REGION16_BE( 0x400000, "bios", 0 )
	ROM_SYSTEM_BIOS( 0, "4.1e", "Palm OS 4.1 (English)" )
	ROMX_LOAD( "palmos41-en-m515.rom", 0x000000, 0x400000, CRC(6e143436) SHA1(a0767ea26cc493a3f687525d173903fef89f1acb), ROM_GROUPWORD | ROM_BIOS(1) )
	//ROM_RELOAD(0x000000, 0x004000)
	ROM_DEFAULT_BIOS( "4.1e" )
ROM_END

//    YEAR  NAME      PARENT    COMPAT   MACHINE      INPUT      STATE            INIT   COMPANY          FULLNAME               FLAGS
COMP( 2001, palmm505, 0,        0,       palmcolor,   palmcolor, palmcolor_state, 0,     "Palm Inc",      "Palm m505",           MACHINE_NOT_WORKING )
COMP( 2001, palmm515, 0,        0,       palmcolor,   palmcolor, palmcolor_state, 0,     "Palm Inc",      "Palm m515",           MACHINE_NOT_WORKING )

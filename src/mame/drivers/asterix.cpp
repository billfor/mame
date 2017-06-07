// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

Asterix

TODO:
 - the konami logo: in the original the outline is drawn, then there's a slight
   delay of 1 or 2 seconds, then it fills from the top to the bottom with the
   colour, including the word "Konami"
 - Verify clocks, PCB has 2 OSCs. 32MHz & 24MHz

***************************************************************************/

#include "emu.h"
#include "cpu/m68000/m68000.h"
#include "cpu/z80/z80.h"
#include "includes/konamipt.h"
#include "machine/eepromser.h"
#include "screen.h"
#include "sound/k053260.h"
#include "sound/ym2151.h"
#include "speaker.h"
#include "video/k053244_k053245.h"
#include "video/k053251.h"
#include "video/k054156_k054157_k056832.h"
#include "video/kvideodac.h"

class asterix_state : public driver_device
{
public:
	enum
	{
		TIMER_NMI
	};

	asterix_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_audiocpu(*this, "audiocpu"),
		m_tilemap(*this, "tilemap"),
		m_sprites(*this, "sprites"),
		m_mixer(*this, "mixer"),
		m_videodac(*this, "videodac"),
		m_screen(*this, "screen")
	{ }

	/* video-related */
	u16         m_spritebank;
	int         m_spritebanks[4];

	/* misc */
	u8       m_cur_control2;
	u16      m_prot[2];

	/* devices */
	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_audiocpu;
	required_device<k054156_054157_device> m_tilemap;
	required_device<k05324x_device> m_sprites;
	required_device<k053251_device> m_mixer;
	required_device<kvideodac_device> m_videodac;
	required_device<screen_device> m_screen;

	DECLARE_WRITE16_MEMBER(control2_w);
	DECLARE_WRITE8_MEMBER(sound_arm_nmi_w);
	DECLARE_WRITE16_MEMBER(sound_irq_w);
	DECLARE_WRITE16_MEMBER(protection_w);
	DECLARE_WRITE16_MEMBER(asterix_spritebank_w);
	DECLARE_DRIVER_INIT(asterix);
	virtual void machine_start() override;
	virtual void machine_reset() override;
	INTERRUPT_GEN_MEMBER(asterix_interrupt);
	void reset_spritebank();

	void fr_setup(flow_render::manager *manager);

protected:
	virtual void device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr) override;
};

void asterix_state::fr_setup(flow_render::manager *manager)
{
	auto rm = m_mixer   ->flow_render_get_renderer();
	auto rv = m_videodac->flow_render_get_renderer();

	manager->connect(m_tilemap->flow_render_get_renderer("a")->out(), rm->inp("0 color"));
	manager->set_constant(rm->inp("0 attr"), 0x3f);

	manager->set_constant(rm->inp("1 color"), 0);
	manager->set_constant(rm->inp("1 attr"), 0x3f);

	manager->connect(m_tilemap->flow_render_get_renderer("b")->out(), rm->inp("2 color"));
	manager->set_constant(rm->inp("2 attr"), 0x3f);
	manager->connect(m_tilemap->flow_render_get_renderer("c")->out(), rm->inp("3 color"));
	manager->connect(m_tilemap->flow_render_get_renderer("d")->out(), rm->inp("4 color"));

	manager->connect(rm->out("color"), rv->inp("color"));
	manager->connect(rm->out("attr"),  rv->inp("attr"));

	manager->connect(rv->out(), m_screen->flow_render_get_renderer()->inp());
}

#if 0
READ16_MEMBER(asterix_state::control2_r)
{
	return m_cur_control2;
}
#endif

WRITE16_MEMBER(asterix_state::control2_w)
{
	if (ACCESSING_BITS_0_7)
	{
		m_cur_control2 = data;
		/* bit 0 is data */
		/* bit 1 is cs (active low) */
		/* bit 2 is clock (active high) */
		ioport("EEPROMOUT")->write(data, 0xff);

		/* bit 5 is select tile bank */
		//*//		m_tilemap->set_tile_bank((data & 0x20) >> 5);
	}
}

INTERRUPT_GEN_MEMBER(asterix_state::asterix_interrupt)
{
	// global interrupt masking
	//*//	if (!m_tilemap->is_irq_enabled(0))
	//*//		return;

	device.execute().set_input_line(5, HOLD_LINE); /* ??? All irqs have the same vector, and the mask used is 0 or 7 */
}

void asterix_state::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	switch (id)
	{
	case TIMER_NMI:
		m_audiocpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE);
		break;
	default:
		assert_always(false, "Unknown id in asterix_state::device_timer");
	}
}

WRITE8_MEMBER(asterix_state::sound_arm_nmi_w)
{
	m_audiocpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	timer_set(attotime::from_usec(5), TIMER_NMI);
}

WRITE16_MEMBER(asterix_state::sound_irq_w)
{
	m_audiocpu->set_input_line(0, HOLD_LINE);
}

// Check the routine at 7f30 in the ead version.
// You're not supposed to laugh.
// This emulation is grossly overkill but hey, I'm having fun.

WRITE16_MEMBER(asterix_state::protection_w)
{
	COMBINE_DATA(m_prot + offset);

	if (offset == 1)
	{
		uint32_t cmd = (m_prot[0] << 16) | m_prot[1];
		switch (cmd >> 24)
		{
		case 0x64:
		{
			uint32_t param1 = (space.read_word(cmd & 0xffffff) << 16)
				| space.read_word((cmd & 0xffffff) + 2);
			uint32_t param2 = (space.read_word((cmd & 0xffffff) + 4) << 16)
				| space.read_word((cmd & 0xffffff) + 6);

			switch (param1 >> 24)
			{
			case 0x22:
			{
				int size = param2 >> 24;
				param1 &= 0xffffff;
				param2 &= 0xffffff;
				while(size >= 0)
				{
					space.write_word(param2, space.read_word(param1));
					param1 += 2;
					param2 += 2;
					size--;
				}
				break;
			}
			}
			break;
		}
		}
	}
}

static ADDRESS_MAP_START( main_map, AS_PROGRAM, 16, asterix_state )
	AM_RANGE(0x000000, 0x0fffff) AM_ROM
	AM_RANGE(0x100000, 0x107fff) AM_RAM
	AM_RANGE(0x180000, 0x1807ff) AM_DEVREADWRITE("sprites", k05324x_device, k053245_word_r, k053245_word_w)
	AM_RANGE(0x180800, 0x180fff) AM_RAM                             // extra RAM, or mirror for the above?
	AM_RANGE(0x200000, 0x20000f) AM_DEVREADWRITE("sprites", k05324x_device, k053244_word_r, k053244_word_w)
	AM_RANGE(0x280000, 0x280fff) AM_RAM_DEVWRITE("palette", palette_device, write) AM_SHARE("palette")
	AM_RANGE(0x300000, 0x30001f) AM_DEVREADWRITE("sprites", k05324x_device, k053244_lsb_r, k053244_lsb_w)
	AM_RANGE(0x380000, 0x380001) AM_READ_PORT("IN0")
	AM_RANGE(0x380002, 0x380003) AM_READ_PORT("IN1")
	AM_RANGE(0x380100, 0x380101) AM_WRITE(control2_w)
	AM_RANGE(0x380200, 0x380203) AM_DEVREADWRITE8("k053260", k053260_device, main_read, main_write, 0x00ff)
	AM_RANGE(0x380300, 0x380301) AM_WRITE(sound_irq_w)
//	AM_RANGE(0x380400, 0x380401) AM_WRITE(asterix_spritebank_w)
	AM_RANGE(0x380500, 0x38051f) AM_DEVICE8("mixer", k053251_device, map, 0x00ff)
	AM_RANGE(0x380600, 0x380601) AM_NOP                             // Watchdog
	AM_RANGE(0x380700, 0x380707) AM_DEVICE("tilemap", k054156_054157_device, vsccs)
	AM_RANGE(0x380800, 0x380803) AM_WRITE(protection_w)
	AM_RANGE(0x400000, 0x400fff) AM_DEVREADWRITE("tilemap", k054156_054157_device, vram16_r, vram16_w)
	AM_RANGE(0x420000, 0x421fff) AM_DEVREAD("tilemap", k054156_054157_device, rom16_r)
	AM_RANGE(0x440000, 0x44003f) AM_DEVICE("tilemap", k054156_054157_device, vacset)
ADDRESS_MAP_END

static ADDRESS_MAP_START( sound_map, AS_PROGRAM, 8, asterix_state )
	AM_RANGE(0x0000, 0xefff) AM_ROM
	AM_RANGE(0xf000, 0xf7ff) AM_RAM
	AM_RANGE(0xf801, 0xf801) AM_DEVREADWRITE("ymsnd", ym2151_device, status_r, data_w)
	AM_RANGE(0xfa00, 0xfa2f) AM_DEVREADWRITE("k053260", k053260_device, read, write)
	AM_RANGE(0xfc00, 0xfc00) AM_WRITE(sound_arm_nmi_w)
	AM_RANGE(0xfe00, 0xfe00) AM_DEVWRITE("ymsnd", ym2151_device, register_w)
ADDRESS_MAP_END



static INPUT_PORTS_START( asterix )
	PORT_START("IN0")
	KONAMI16_LSB(1, IPT_UNKNOWN, IPT_START1)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0xf800, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN1")
	KONAMI16_LSB(2, IPT_UNKNOWN, IPT_START2)
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_SPECIAL ) PORT_READ_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, do_read)
	PORT_BIT( 0x0200, IP_ACTIVE_HIGH, IPT_SPECIAL ) PORT_READ_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, ready_read)
	PORT_SERVICE_NO_TOGGLE(0x0400, IP_ACTIVE_LOW )
	PORT_BIT( 0xf800, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START( "EEPROMOUT" )
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OUTPUT ) PORT_WRITE_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, di_write)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_OUTPUT ) PORT_WRITE_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, cs_write)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_OUTPUT ) PORT_WRITE_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, clk_write)
INPUT_PORTS_END


void asterix_state::machine_start()
{
	save_item(NAME(m_cur_control2));
	save_item(NAME(m_prot));

	save_item(NAME(m_spritebank));
	save_item(NAME(m_spritebanks));
}

void asterix_state::machine_reset()
{
	m_cur_control2 = 0;
	m_prot[0] = 0;
	m_prot[1] = 0;

	m_spritebank = 0;
	for (int i = 0; i < 4; i++)
	{
		m_spritebanks[i] = 0;
	}
}

static MACHINE_CONFIG_START( asterix )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M68000, XTAL_24MHz/2) // 12MHz
	MCFG_CPU_PROGRAM_MAP(main_map)

	MCFG_CPU_ADD("audiocpu", Z80, XTAL_32MHz/4) // 8MHz Z80E ??
	MCFG_CPU_PROGRAM_MAP(sound_map)

	MCFG_EEPROM_SERIAL_ER5911_8BIT_ADD("eeprom")

	/* video hardware */
	MCFG_FLOW_RENDER_MANAGER_ADD("fr_manager")
	MCFG_FLOW_RENDER_MANAGER_SETUP(":", asterix_state, fr_setup)

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_RAW_PARAMS(XTAL_24MHz/4, 384, 48, 48+288, 264, 15, 15+224)
	MCFG_SCREEN_FLOW_RENDER_RGB()

	MCFG_PALETTE_ADD("palette", 2048)
	MCFG_PALETTE_ENABLE_SHADOWS()
	MCFG_PALETTE_FORMAT(xBBBBBGGGGGRRRRR)

	MCFG_GFXDECODE_ADD("gfxdecode", "palette", empty)

	MCFG_K054156_054157_ADD("tilemap", XTAL_24MHz/4, 2, 2, 16)
	MCFG_K054156_054157_VBLANK_CB(INPUTLINE("maincpu", 5)) // Actual line unknown, all have the same vector and mask is 0 or 7

	MCFG_DEVICE_ADD("sprites", K053244, 0)
	MCFG_GFX_PALETTE("palette")
	MCFG_K05324X_OFFSETS(-3, -1)

	MCFG_K053251_ADD("mixer", 1)

	MCFG_KVIDEODAC_ADD("videodac", "palette", 0x3, 0.6, 0, 1.0)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MCFG_YM2151_ADD("ymsnd", XTAL_32MHz/8) // 4MHz
	MCFG_SOUND_ROUTE(0, "lspeaker", 1.0)
	MCFG_SOUND_ROUTE(1, "rspeaker", 1.0)

	MCFG_K053260_ADD("k053260", XTAL_32MHz/8) // 4MHz
	MCFG_SOUND_ROUTE(0, "lspeaker", 0.75)
	MCFG_SOUND_ROUTE(1, "rspeaker", 0.75)
MACHINE_CONFIG_END


ROM_START( asterix )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "068_ea_d01.8c", 0x000000,  0x20000, CRC(61d6621d) SHA1(908a344e9bbce0c7544bd049494258d1d3ad073b) )
	ROM_LOAD16_BYTE( "068_ea_d02.8d", 0x000001,  0x20000, CRC(53aac057) SHA1(7401ca5b70f384688c3353fc1ac9ef0b27814c66) )
	ROM_LOAD16_BYTE( "068a03.7c", 0x080000,  0x20000, CRC(8223ebdc) SHA1(e4aa39e4bc1d210bdda5b0cb41d6c8006c48dd24) )
	ROM_LOAD16_BYTE( "068a04.7d", 0x080001,  0x20000, CRC(9f351828) SHA1(e03842418f08e6267eeea03362450da249af73be) )

	ROM_REGION( 0x010000, "audiocpu", 0 )
	ROM_LOAD( "068_a05.5f", 0x000000, 0x010000,  CRC(d3d0d77b) SHA1(bfa77a8bf651dc27f481e96a2d63242084cc214c) )

	ROM_REGION( 0x100000, "tilemap", 0 )
	ROM_LOAD32_WORD_SWAP( "068a12.16k", 0x000000, 0x080000, CRC(b9da8e9c) SHA1(a46878916833923e421da0667e37620ae0b77744) )
	ROM_LOAD32_WORD_SWAP( "068a11.12k", 0x000002, 0x080000, CRC(7eb07a81) SHA1(672c0c60834df7816d33d88643e4575b8ca9bcc1) )

	ROM_REGION( 0x400000, "sprites", 0 )
	ROM_LOAD32_WORD( "068a08.7k", 0x000000, 0x200000, CRC(c41278fe) SHA1(58e5f67a67ae97e0b264489828cd7e74662c5ed5) )
	ROM_LOAD32_WORD( "068a07.3k", 0x000002, 0x200000, CRC(32efdbc4) SHA1(b7e8610aa22249176d82b750e2549d1eea6abe4f) )

	ROM_REGION( 0x200000, "k053260", 0 )
	ROM_LOAD( "068a06.1e", 0x000000, 0x200000, CRC(6df9ec0e) SHA1(cee60312e9813bd6579f3ac7c3c2521a8e633eca) )

	ROM_REGION( 0x80, "eeprom", 0 )
	ROM_LOAD( "asterix.nv", 0x0000, 0x0080, CRC(490085c8) SHA1(2a79e7c79db4b4fb0e6a7249cfd6a57e74b170e3) )
ROM_END

ROM_START( asterixeac )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "068_ea_c01.8c", 0x000000,  0x20000, CRC(0ccd1feb) SHA1(016d642e3a745f0564aa93f0f66d5c0f37962990) )
	ROM_LOAD16_BYTE( "068_ea_c02.8d", 0x000001,  0x20000, CRC(b0805f47) SHA1(b58306164e8fec69002656993ae80abbc8f136cd) )
	ROM_LOAD16_BYTE( "068a03.7c", 0x080000,  0x20000, CRC(8223ebdc) SHA1(e4aa39e4bc1d210bdda5b0cb41d6c8006c48dd24) )
	ROM_LOAD16_BYTE( "068a04.7d", 0x080001,  0x20000, CRC(9f351828) SHA1(e03842418f08e6267eeea03362450da249af73be) )

	ROM_REGION( 0x010000, "audiocpu", 0 )
	ROM_LOAD( "068_a05.5f", 0x000000, 0x010000,  CRC(d3d0d77b) SHA1(bfa77a8bf651dc27f481e96a2d63242084cc214c) )

	ROM_REGION( 0x100000, "tilemap", 0 )
	ROM_LOAD32_WORD_SWAP( "068a12.16k", 0x000000, 0x080000, CRC(b9da8e9c) SHA1(a46878916833923e421da0667e37620ae0b77744) )
	ROM_LOAD32_WORD_SWAP( "068a11.12k", 0x000002, 0x080000, CRC(7eb07a81) SHA1(672c0c60834df7816d33d88643e4575b8ca9bcc1) )

	ROM_REGION( 0x400000, "sprites", 0 )
	ROM_LOAD32_WORD( "068a08.7k", 0x000000, 0x200000, CRC(c41278fe) SHA1(58e5f67a67ae97e0b264489828cd7e74662c5ed5) )
	ROM_LOAD32_WORD( "068a07.3k", 0x000002, 0x200000, CRC(32efdbc4) SHA1(b7e8610aa22249176d82b750e2549d1eea6abe4f) )

	ROM_REGION( 0x200000, "k053260", 0 )
	ROM_LOAD( "068a06.1e", 0x000000, 0x200000, CRC(6df9ec0e) SHA1(cee60312e9813bd6579f3ac7c3c2521a8e633eca) )

	ROM_REGION( 0x80, "eeprom", 0 )
	ROM_LOAD( "asterixeac.nv", 0x0000, 0x0080, CRC(490085c8) SHA1(2a79e7c79db4b4fb0e6a7249cfd6a57e74b170e3) )
ROM_END

ROM_START( asterixeaa )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "068_ea_a01.8c", 0x000000,  0x20000, CRC(85b41d8e) SHA1(e1326f6d61b8097f5201d5bd37e4d2a357d17b47) )
	ROM_LOAD16_BYTE( "068_ea_a02.8d", 0x000001,  0x20000, CRC(8e886305) SHA1(41a9de2cdad8c1185b4d13ea5b4a9309716947c5) )
	ROM_LOAD16_BYTE( "068a03.7c", 0x080000,  0x20000, CRC(8223ebdc) SHA1(e4aa39e4bc1d210bdda5b0cb41d6c8006c48dd24) )
	ROM_LOAD16_BYTE( "068a04.7d", 0x080001,  0x20000, CRC(9f351828) SHA1(e03842418f08e6267eeea03362450da249af73be) )

	ROM_REGION( 0x010000, "audiocpu", 0 )
	ROM_LOAD( "068_a05.5f", 0x000000, 0x010000,  CRC(d3d0d77b) SHA1(bfa77a8bf651dc27f481e96a2d63242084cc214c) )

	ROM_REGION( 0x100000, "tilemap", 0 )
	ROM_LOAD32_WORD_SWAP( "068a12.16k", 0x000000, 0x080000, CRC(b9da8e9c) SHA1(a46878916833923e421da0667e37620ae0b77744) )
	ROM_LOAD32_WORD_SWAP( "068a11.12k", 0x000002, 0x080000, CRC(7eb07a81) SHA1(672c0c60834df7816d33d88643e4575b8ca9bcc1) )

	ROM_REGION( 0x400000, "sprites", 0 )
	ROM_LOAD32_WORD( "068a08.7k", 0x000000, 0x200000, CRC(c41278fe) SHA1(58e5f67a67ae97e0b264489828cd7e74662c5ed5) )
	ROM_LOAD32_WORD( "068a07.3k", 0x000002, 0x200000, CRC(32efdbc4) SHA1(b7e8610aa22249176d82b750e2549d1eea6abe4f) )

	ROM_REGION( 0x200000, "k053260", 0 )
	ROM_LOAD( "068a06.1e", 0x000000, 0x200000, CRC(6df9ec0e) SHA1(cee60312e9813bd6579f3ac7c3c2521a8e633eca) )

	ROM_REGION( 0x80, "eeprom", 0 )
	ROM_LOAD( "asterixeaa.nv", 0x0000, 0x0080, CRC(30275de0) SHA1(4bbf90a4e5b20406153329e9e7c4c2bf72676f8d) )
ROM_END

ROM_START( asterixaad )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "068_aa_d01.8c", 0x000000,  0x20000, CRC(3fae5f1f) SHA1(73ef65dac8e1cd4d9a3695963231e3a2a860b486) )
	ROM_LOAD16_BYTE( "068_aa_d02.8d", 0x000001,  0x20000, CRC(171f0ba0) SHA1(1665f23194da5811e4708ad0495378957b6e6251) )
	ROM_LOAD16_BYTE( "068a03.7c", 0x080000,  0x20000, CRC(8223ebdc) SHA1(e4aa39e4bc1d210bdda5b0cb41d6c8006c48dd24) )
	ROM_LOAD16_BYTE( "068a04.7d", 0x080001,  0x20000, CRC(9f351828) SHA1(e03842418f08e6267eeea03362450da249af73be) )

	ROM_REGION( 0x010000, "audiocpu", 0 )
	ROM_LOAD( "068_a05.5f", 0x000000, 0x010000,  CRC(d3d0d77b) SHA1(bfa77a8bf651dc27f481e96a2d63242084cc214c) )

	ROM_REGION( 0x100000, "tilemap", 0 )
	ROM_LOAD32_WORD_SWAP( "068a12.16k", 0x000000, 0x080000, CRC(b9da8e9c) SHA1(a46878916833923e421da0667e37620ae0b77744) )
	ROM_LOAD32_WORD_SWAP( "068a11.12k", 0x000002, 0x080000, CRC(7eb07a81) SHA1(672c0c60834df7816d33d88643e4575b8ca9bcc1) )

	ROM_REGION( 0x400000, "sprites", 0 )
	ROM_LOAD32_WORD( "068a08.7k", 0x000000, 0x200000, CRC(c41278fe) SHA1(58e5f67a67ae97e0b264489828cd7e74662c5ed5) )
	ROM_LOAD32_WORD( "068a07.3k", 0x000002, 0x200000, CRC(32efdbc4) SHA1(b7e8610aa22249176d82b750e2549d1eea6abe4f) )

	ROM_REGION( 0x200000, "k053260", 0 )
	ROM_LOAD( "068a06.1e", 0x000000, 0x200000, CRC(6df9ec0e) SHA1(cee60312e9813bd6579f3ac7c3c2521a8e633eca) )

	ROM_REGION( 0x80, "eeprom", 0 )
	ROM_LOAD( "asterixaad.nv", 0x0000, 0x0080, CRC(bcca86a7) SHA1(1191b0011749e2516df723c9d63da9c2304fa594) )
ROM_END

ROM_START( asterixj )
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "068_ja_d01.8c", 0x000000,  0x20000, CRC(2bc10940) SHA1(e25cc97435f157bed9c28d9e9277c9f47d4fb5fb) )
	ROM_LOAD16_BYTE( "068_ja_d02.8d", 0x000001,  0x20000, CRC(de438300) SHA1(8d72988409e6c28a06fb2325087d27ebd2d02c92) )
	ROM_LOAD16_BYTE( "068a03.7c", 0x080000,  0x20000, CRC(8223ebdc) SHA1(e4aa39e4bc1d210bdda5b0cb41d6c8006c48dd24) )
	ROM_LOAD16_BYTE( "068a04.7d", 0x080001,  0x20000, CRC(9f351828) SHA1(e03842418f08e6267eeea03362450da249af73be) )

	ROM_REGION( 0x010000, "audiocpu", 0 )
	ROM_LOAD( "068_a05.5f", 0x000000, 0x010000,  CRC(d3d0d77b) SHA1(bfa77a8bf651dc27f481e96a2d63242084cc214c) )

	ROM_REGION( 0x100000, "tilemap", 0 )
	ROM_LOAD32_WORD_SWAP( "068a12.16k", 0x000000, 0x080000, CRC(b9da8e9c) SHA1(a46878916833923e421da0667e37620ae0b77744) )
	ROM_LOAD32_WORD_SWAP( "068a11.12k", 0x000002, 0x080000, CRC(7eb07a81) SHA1(672c0c60834df7816d33d88643e4575b8ca9bcc1) )

	ROM_REGION( 0x400000, "sprites", 0 )
	ROM_LOAD32_WORD( "068a08.7k", 0x000000, 0x200000, CRC(c41278fe) SHA1(58e5f67a67ae97e0b264489828cd7e74662c5ed5) )
	ROM_LOAD32_WORD( "068a07.3k", 0x000002, 0x200000, CRC(32efdbc4) SHA1(b7e8610aa22249176d82b750e2549d1eea6abe4f) )

	ROM_REGION( 0x200000, "k053260", 0 )
	ROM_LOAD( "068a06.1e", 0x000000, 0x200000, CRC(6df9ec0e) SHA1(cee60312e9813bd6579f3ac7c3c2521a8e633eca) )

	ROM_REGION( 0x80, "eeprom", 0 )
	ROM_LOAD( "asterixj.nv", 0x0000, 0x0080, CRC(84229f2c) SHA1(34c7491c731fbf741dfd53bfc559d91201ccfb03) )
ROM_END


DRIVER_INIT_MEMBER(asterix_state,asterix)
{
#if 1
	*(u16 *)(memregion("maincpu")->base() + 0x07f34) = 0x602a;
	*(u16 *)(memregion("maincpu")->base() + 0x00008) = 0x0400;
#endif
}


GAME( 1992, asterix,    0,       asterix, asterix, asterix_state, asterix, ROT0, "Konami", "Asterix (ver EAD)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_SUPPORTS_SAVE )
GAME( 1992, asterixeac, asterix, asterix, asterix, asterix_state, asterix, ROT0, "Konami", "Asterix (ver EAC)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_SUPPORTS_SAVE )
GAME( 1992, asterixeaa, asterix, asterix, asterix, asterix_state, asterix, ROT0, "Konami", "Asterix (ver EAA)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_SUPPORTS_SAVE )
GAME( 1992, asterixaad, asterix, asterix, asterix, asterix_state, asterix, ROT0, "Konami", "Asterix (ver AAD)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_SUPPORTS_SAVE )
GAME( 1992, asterixj,   asterix, asterix, asterix, asterix_state, asterix, ROT0, "Konami", "Asterix (ver JAD)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_SUPPORTS_SAVE )

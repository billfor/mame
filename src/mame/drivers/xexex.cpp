// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    Xexex  (c) 1991 Konami - GX067


Xexex
Konami 1991

PCB Layout
----------
GX067 PWB352898B
|--------------------------------------------------------|
|MB3722   067B07.1E                         067B14.1N    |
| 054544  067B06.3E  84256                  067B13.2N    |
|        067JAA05.4E                   |------| |------| |
|CN5               |------|            |053246A |053247A |
| YM2151   8464    |054539|            |      | |      | |
| 054744           |      |            |      | |      | |
|          Z80E    |      |            |      | |      | |
| 051550           |------|            |------| |------| |
|J                                                       |
|A 054573  |------| 2018                 5168(X10)       |
|M 054573  |054338| 2018                                 |
|M 054573  |      | 2018     |------|  |------| |------| |
|A 054574  |      |          |053251|  |054157| |054156| |
|          |------|          |      |  |      | |      | |
|  053252         067B04.13F |------|  |      | |      | |
|           067B03.13D      2018       |      | |      | |
|    054743  84256   84256  2018       |------| |------| |
| 18.432MHz      067JAA02.16F |------|                   |
| 32MHz     067JAA01.16D      |053250|        067B12.17N |
|            |------------|   |      |                   |
|TEST_SW     |   68000    |   |------|        067B11.19N |
|005273(X6)  |            |                              |
|  ER5911.19B|------------|                   067B10.20N |
|                                                        |
|                  067B08.22F                 067B09.22N |
|--------------------------------------------------------|
Notes:
      68000  - Clock 16.000MHz [32/2]
      Z80E   - Clock 8.000MHz [32/4]
      YM2151 - Clock 4.000MHz [32/8]
      2018   - Motorola MCM2018 2kx8 SRAM (DIP24)
      84256  - Fujitsu MB84256 32kx8 SRAM (DIP28)
      5168   - Sharp LH5168 8kx8 SRAM (DIP28)
      ER5911 - EEPROM (128 bytes)
      CN5    - 4 pin connector for stereo sound output
      067*   - EPROM/mask ROM
      MB3722 - Power AMP IC

      Custom Chips
      ------------
      053250  - Road generator
      053251  - Priority encoder
      053252  - Timing/Interrupt controller. Clock input 32MHz
      054157  \
      054156  / Tilemap generators
      053246A \
      053247A / Sprite generators
      054539  - 8-Channel ADPCM sound generator. Clock input 18.432MHz. Clock outputs 18.432/4 & 18.432/8
      054573  - Video DAC (one for each R,G,B video signal)
      054574  - Possibly RGB mixer/DAC/filter? (connected to 054573)
      054338  - Color mixer for special effects/alpha blending etc (connected to 054573 & 054574 and 2018 RAM)
      051550  - EMI filter for credit/coin counter
      005273  - Resistor array for player 3 & player 4 controls (PL3/4 connectors not populated)
      054544  - Audio DAC/filter
      054744  - PAL16L8
      054743  - PAL20L10

      Sync Measurements
      -----------------
      HSync - 15.3670kHz
      VSync - 54.0657Hz


****************************************************************************

The following bugs appear to be fixed:

General:

- game doesn't slow down like the arcade
- sprite lag, dithering, flicking (DMA)
- line effects go out of sync (K053250 also does DMA)
- inconsistent reverb (maths bug)
- lasers don't change color (IRQ masking)
- xexex057gre_1 (delayed sfx, missing speech, Xexexj only: random 1-up note)
- xexex057gre_2 (reversed stereo)
- xexex065gre (coin up problems, IRQ order)

- L1: xexex067gre (tilemap boundary), misaligned bosses (swapXY)
- L2: xexex061gre (K054157 offset)
- L4: half the foreground missing (LVC no-wraparound)
- L5: poly-face boss missing (coordinate masking)
- L6: sticky galaxies (LVC scroll bug)
- L7: misaligned ship patches (swapXY)


Unresolved Issues:

- random 1-up notes still pop up in the world version (filtered temporarily)
- mono/stereo softdip has no effect (xexex057gre_3, external mixing?)
- K053250 shows a one-frame glitch at stage 1 boss (DMA timing?)
- stage 3 intro missing alpha effect (known K054338 deficiency)
- the stage 4 boss(tentacles) sometimes appears darker (palette update timing?)
- the furthest layer in stage 5 shakes when scrolling up or down (needs verification)
- Elaine's end-game graphics has wrong masking effect (known non-zoomed pdrawgfx issue)

***************************************************************************/

#include "emu.h"
#include "includes/konamipt.h"
#include "cpu/m68000/m68000.h"
#include "cpu/z80/z80.h"
#include "machine/eepromser.h"
#include "machine/k053252.h"
#include "machine/k054321.h"
#include "sound/flt_vol.h"
#include "sound/k054539.h"
#include "sound/ym2151.h"
#include "video/k053246_k053247_k055673.h"
#include "video/k053250.h"
#include "video/k053251.h"
#include "video/k054156_k054157_k056832.h"
#include "video/k054338.h"
#include "speaker.h"

class xexex_state : public driver_device, public flow_render::interface
{
public:
	xexex_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		flow_render::interface(mconfig, *this),
		m_workram(*this, "workram"),
		m_spriteram(*this, "spriteram"),
		m_maincpu(*this, "maincpu"),
		m_audiocpu(*this, "audiocpu"),
		m_k054539(*this, "k054539"),
		m_filter1l(*this, "filter1l"),
		m_filter1r(*this, "filter1r"),
		m_filter2l(*this, "filter2l"),
		m_filter2r(*this, "filter2r"),
		m_tilemap(*this, "tilemap"),
		m_sprites(*this, "sprites"),
		m_lvc(*this, "lvc"),
		m_mixer(*this, "mixer"),
		m_video_timings(*this, "video_timings"),
		m_blender(*this, "blender"),
		m_palette(*this, "palette"),
		m_screen(*this, "screen"),
		m_soundctrl(*this, "soundctrl") { }

	/* memory pointers */
	required_shared_ptr<u16> m_workram;
	required_shared_ptr<u16> m_spriteram;

	/* video-related */
	bool m_chenmix;
	flow_render::renderer *m_renderer_prefilter[2], *m_renderer_postfilter;
	flow_render::input_sb_u16 *m_renderer_input_prefilter[2];
	flow_render::input_sb_u16 *m_renderer_input_postfilter[5];
	flow_render::output_sb_u16 *m_renderer_output_prefilter[2];
	flow_render::output_sb_u16 *m_renderer_output_postfilter[4];

	/* misc */
	u16 m_cur_control2;
	u16 m_cur_interrupt;

	/* devices */
	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_audiocpu;
	required_device<k054539_device> m_k054539;
	required_device<filter_volume_device> m_filter1l;
	required_device<filter_volume_device> m_filter1r;
	required_device<filter_volume_device> m_filter2l;
	required_device<filter_volume_device> m_filter2r;
	required_device<k054156_054157_device> m_tilemap;
	required_device<k053246_053247_device> m_sprites;
	required_device<k053250_device> m_lvc;
	required_device<k053251_device> m_mixer;
	required_device<k053252_device> m_video_timings;
	required_device<k054338_device> m_blender;
	required_device<palette_device> m_palette;
	required_device<screen_device> m_screen;
	required_device<k054321_device> m_soundctrl;

	DECLARE_WRITE_LINE_MEMBER(objdma_w);
	DECLARE_WRITE_LINE_MEMBER(whatever_w);

	DECLARE_READ16_MEMBER(control2_r);
	DECLARE_WRITE16_MEMBER(control2_w);
	DECLARE_WRITE16_MEMBER(sound_irq_w);
	DECLARE_WRITE8_MEMBER(sound_bankswitch_w);
	DECLARE_DRIVER_INIT(xexex);
	virtual void machine_start() override;
	virtual void machine_reset() override;

	K054539_CB_MEMBER(ym_set_mixing);

	void flow_render_register_renderers() override;
	void update_irq();
	void sprites_wiring(u32 output, u16 &color, u16 &attr);
	offs_t sprites_remap(offs_t offset);
	void fr_setup(flow_render::manager *manager);
	void render_prefilter(bool render_front, const rectangle &cliprect);
	void render_postfilter(const rectangle &cliprect);
};

void xexex_state::fr_setup(flow_render::manager *manager)
{
	auto rb  = m_blender->flow_render_get_renderer();
	auto rm1 = m_mixer  ->flow_render_get_renderer();
	auto rm2 = m_mixer  ->flow_render_get_renderer("secondary");
	auto rs  = m_sprites->flow_render_get_renderer();
	auto rl  = m_lvc    ->flow_render_get_renderer();

	manager->connect(rs->out("color"), rm1->inp("0 color"));
	manager->connect(rs->out("color"), rm2->inp("0 color"));
	manager->connect(rs->out("attr"),  rm1->inp("0 attr"));
	manager->connect(rs->out("attr"),  rm2->inp("0 attr"));
	manager->connect(rl->out("color"), rm1->inp("1 color"));
	manager->connect(rl->out("color"), rm2->inp("1 color"));
	manager->connect(rl->out("attr"),  rm1->inp("1 attr"));
	manager->connect(rl->out("attr"),  rm2->inp("1 attr"));

	manager->connect(m_tilemap->flow_render_get_renderer("b")->out(), m_renderer_input_prefilter[0]);
	manager->connect(m_tilemap->flow_render_get_renderer("b")->out(), m_renderer_input_prefilter[1]);
	manager->connect(m_renderer_output_prefilter[0], rm1->inp("2 color"));
	manager->connect(m_renderer_output_prefilter[1], rm2->inp("2 color"));

	manager->set_constant(rm1->inp("2 attr"), 0);
	manager->set_constant(rm2->inp("2 attr"), 0);
	manager->connect(m_tilemap->flow_render_get_renderer("c")->out(), rm1->inp("3 color"));
	manager->connect(m_tilemap->flow_render_get_renderer("c")->out(), rm2->inp("3 color"));
	manager->connect(m_tilemap->flow_render_get_renderer("d")->out(), rm1->inp("4 color"));
	manager->connect(m_tilemap->flow_render_get_renderer("d")->out(), rm2->inp("4 color"));

	manager->connect(rm1->out("color"), m_renderer_input_postfilter[0]);
	manager->connect(rm1->out("attr"),  m_renderer_input_postfilter[1]);
	manager->connect(rm2->out("color"), m_renderer_input_postfilter[2]);
	manager->connect(rm2->out("attr"),  m_renderer_input_postfilter[3]);
	manager->connect(m_tilemap->flow_render_get_renderer("a")->out(), m_renderer_input_postfilter[4]);

	manager->connect(m_renderer_output_postfilter[0], rb->inp("0 color"));
	manager->connect(m_renderer_output_postfilter[1], rb->inp("0 attr"));
	manager->connect(m_renderer_output_postfilter[2], rb->inp("1 color"));
	manager->connect(m_renderer_output_postfilter[3], rb->inp("1 attr"));

	manager->connect(rb->out(), m_screen->flow_render_get_renderer()->inp());
}

void xexex_state::flow_render_register_renderers()
{
	for(int i=0; i<2; i++) {
		bool render_front = i == 0;
		m_renderer_prefilter[i] = flow_render_create_renderer([this, render_front](const rectangle &cliprect){ render_prefilter(render_front, cliprect); }, i ? "back" : "front");
		m_renderer_input_prefilter[i] = m_renderer_prefilter[i]->create_input_sb_u16();
		m_renderer_output_prefilter[i] = m_renderer_prefilter[i]->create_output_sb_u16();
	}

	static const char *const io_names[] = { "fc", "fa", "bc", "ba", "tc" };
	m_renderer_postfilter = flow_render_create_renderer([this](const rectangle &cliprect){ render_postfilter(cliprect); }, "post");
	for(int i=0; i<5; i++) {
		m_renderer_input_postfilter[i] = m_renderer_postfilter->create_input_sb_u16(io_names[i]);
		if(i != 4)
			m_renderer_output_postfilter[i] = m_renderer_postfilter->create_output_sb_u16(io_names[i]);
	}
}

void xexex_state::render_prefilter(bool render_front, const rectangle &cliprect)
{
	bitmap_ind16 &inp = m_renderer_input_prefilter [render_front ? 0 : 1]->bitmap();
	bitmap_ind16 &out = m_renderer_output_prefilter[render_front ? 0 : 1]->bitmap();
	if(render_front != m_chenmix) {
		for(int y = cliprect.min_y; y <= cliprect.max_y; y++) {
			const u16 *ci = &inp.pix16(y, cliprect.min_x);
			u16 *co = &out.pix16(y, cliprect.min_x);
			for(int x = cliprect.min_x; x <= cliprect.max_x; x++) {
				u16 col = *ci++;
				*co++ = col & 0x100 ? col & 0x1f0 : col;
			}
		}
	} else {
		for(int y = cliprect.min_y; y <= cliprect.max_y; y++) {
			const u16 *ci = &inp.pix16(y, cliprect.min_x);
			u16 *co = &out.pix16(y, cliprect.min_x);
			memcpy(co, ci, (cliprect.max_x - cliprect.min_x + 1)*2);
		}
	}
}

void xexex_state::render_postfilter(const rectangle &cliprect)
{
	bitmap_ind16 &fci = m_renderer_input_postfilter[0]->bitmap();
	bitmap_ind16 &fai = m_renderer_input_postfilter[1]->bitmap();
	bitmap_ind16 &bci = m_renderer_input_postfilter[2]->bitmap();
	bitmap_ind16 &bai = m_renderer_input_postfilter[3]->bitmap();
	bitmap_ind16 &tci = m_renderer_input_postfilter[4]->bitmap();

	bitmap_ind16 &fco = m_renderer_output_postfilter[0]->bitmap();
	bitmap_ind16 &fao = m_renderer_output_postfilter[1]->bitmap();
	bitmap_ind16 &bco = m_renderer_output_postfilter[2]->bitmap();
	bitmap_ind16 &bao = m_renderer_output_postfilter[3]->bitmap();

	for(int y = cliprect.min_y; y <= cliprect.max_y; y++) {
		const u16 *fcip = &fci.pix16(y, cliprect.min_x);
		const u16 *faip = &fai.pix16(y, cliprect.min_x);
		const u16 *bcip = &bci.pix16(y, cliprect.min_x);
		const u16 *baip = &bai.pix16(y, cliprect.min_x);
		const u16 *tcip = &tci.pix16(y, cliprect.min_x);

		u16 *fcop = &fco.pix16(y, cliprect.min_x);
		u16 *faop = &fao.pix16(y, cliprect.min_x);
		u16 *bcop = &bco.pix16(y, cliprect.min_x);
		u16 *baop = &bao.pix16(y, cliprect.min_x);

		for(int x = cliprect.min_x; x <= cliprect.max_x; x++) {
			u16 tcol = *tcip++;
			if(tcol & 0xf) {
				*faop++ = *baop++ = 0x8000;
				*fcop++ = *bcop++ = tcol | 0x700;
				fcip++;
				faip++;
				bcip++;
				baip++;
			} else {
				u16 fc = *fcop++ = *fcip++;
				u16 bc = *bcop++ = *bcip++;
				*faop++ = *faip++ | ((fc & 0x700) == 0x100 ? 0x10 : 0);
				*baop++ = *baip++ | ((bc & 0x700) == 0x100 ? 0x10 : 0);
			}
		}
	}
}

WRITE_LINE_MEMBER(xexex_state::objdma_w)
{
	// Unclear, possibly correct
	whatever_w(state);

	if(state)
		m_cur_interrupt |=  0x0020;
	else
		m_cur_interrupt &= ~0x0020;
	update_irq();
}

WRITE_LINE_MEMBER(xexex_state::whatever_w)
{
	if(state)
		m_cur_interrupt |=  0x0040;
	else
		m_cur_interrupt &= ~0x0040;
	update_irq();
}

void xexex_state::sprites_wiring(u32 output, u16 &color, u16 &attr)
{
	color = output & 0x1ff;
	attr  = ((output & 0xc000) >> 6) | ((output & 0x3e00) >> 8);
}

offs_t xexex_state::sprites_remap(offs_t offset)
{
	return ((offset & 0x00e) << 1) | ((offset & 0xff0) << 3);
}


void xexex_state::update_irq()
{
	u16 active = m_cur_control2 & m_cur_interrupt;

	m_maincpu->set_input_line(6, active & 0x0020 ? ASSERT_LINE : CLEAR_LINE);
	m_maincpu->set_input_line(5, active & 0x0040 ? ASSERT_LINE : CLEAR_LINE);
}

READ16_MEMBER(xexex_state::control2_r)
{
	return m_cur_control2;
}

WRITE16_MEMBER(xexex_state::control2_w)
{
	COMBINE_DATA(&m_cur_control2);

	/* bit 0  is data */
	/* bit 1  is cs (active low) */
	/* bit 2  is clock (active high) */
	/* bit 5  is enable irq 6 (?) */
	/* bit 6  is enable irq 5 (objdma) */
	/* bit 8 = enable sprite ROM reading */
	/* bit 9 = mix/tilemap b alpha invert */
	/* bit 11 is watchdog */

	ioport("EEPROMOUT")->write(m_cur_control2, 0xff);
	m_sprites->set_objcha(m_cur_control2 & 0x0100);
	m_chenmix = m_cur_control2 & 0x200;

	update_irq();
}

WRITE16_MEMBER(xexex_state::sound_irq_w)
{
	m_audiocpu->set_input_line(0, HOLD_LINE);
}

WRITE8_MEMBER(xexex_state::sound_bankswitch_w)
{
	membank("z80bank")->set_entry(data & 0x07);
}

K054539_CB_MEMBER(xexex_state::ym_set_mixing)
{
	m_filter1l->flt_volume_set_volume((71.0 * left) / 55.0);
	m_filter1r->flt_volume_set_volume((71.0 * right) / 55.0);
	m_filter2l->flt_volume_set_volume((71.0 * left) / 55.0);
	m_filter2r->flt_volume_set_volume((71.0 * right) / 55.0);
}

static ADDRESS_MAP_START( main_map, AS_PROGRAM, 16, xexex_state )
	AM_RANGE(0x000000, 0x07ffff) AM_ROM
	AM_RANGE(0x080000, 0x08ffff) AM_RAM AM_SHARE("workram")         // work RAM
	AM_RANGE(0x090000, 0x097fff) AM_RAM AM_MIRROR(0x8000) AM_SHARE("spriteram")
	AM_RANGE(0x0c0000, 0x0c003f) AM_DEVICE("tilemap", k054156_054157_device, vacset)
	AM_RANGE(0x0c2000, 0x0c2007) AM_DEVICE("sprites", k053246_053247_device, objset1)
	AM_RANGE(0x0c4000, 0x0c4001) AM_DEVREAD("sprites", k053246_053247_device, rom16_r)
	AM_RANGE(0x0c6000, 0x0c7fff) AM_RAM AM_SHARE("lvcram")
	AM_RANGE(0x0c8000, 0x0c800f) AM_DEVICE8("lvc", k053250_device, map, 0x00ff)
	AM_RANGE(0x0ca000, 0x0ca01f) AM_DEVICE("blender", k054338_device, map)
	AM_RANGE(0x0cc000, 0x0cc01f) AM_DEVICE8("mixer", k053251_device, map, 0x00ff)
	AM_RANGE(0x0d0000, 0x0d001f) AM_DEVICE8("video_timings", k053252_device, map, 0x00ff)
	AM_RANGE(0x0d4000, 0x0d4001) AM_WRITE(sound_irq_w)
	AM_RANGE(0x0d6000, 0x0d601f) AM_DEVICE8("soundctrl", k054321_device, main_map, 0x00ff)
	AM_RANGE(0x0d8000, 0x0d8007) AM_DEVICE("tilemap", k054156_054157_device, vsccs)
	AM_RANGE(0x0da000, 0x0da001) AM_READ_PORT("P1")
	AM_RANGE(0x0da002, 0x0da003) AM_READ_PORT("P2")
	AM_RANGE(0x0dc000, 0x0dc001) AM_READ_PORT("SYSTEM")
	AM_RANGE(0x0dc002, 0x0dc003) AM_READ_PORT("EEPROM")
	AM_RANGE(0x0de000, 0x0de001) AM_READWRITE(control2_r, control2_w)
	AM_RANGE(0x100000, 0x17ffff) AM_ROM
	AM_RANGE(0x180000, 0x181fff) AM_MIRROR(0x2000) AM_DEVREADWRITE("tilemap", k054156_054157_device, vram16_r, vram16_w)
	AM_RANGE(0x190000, 0x191fff) AM_DEVREAD("tilemap", k054156_054157_device, rom16_r)
	AM_RANGE(0x1a0000, 0x1a1fff) AM_DEVREAD("lvc", k053250_device, rom_r)
	AM_RANGE(0x1b0000, 0x1b1fff) AM_RAM_DEVWRITE("palette", palette_device, write) AM_SHARE("palette")
ADDRESS_MAP_END


static ADDRESS_MAP_START( sound_map, AS_PROGRAM, 8, xexex_state )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0x8000, 0xbfff) AM_ROMBANK("z80bank")
	AM_RANGE(0xc000, 0xdfff) AM_RAM
	AM_RANGE(0xe000, 0xe22f) AM_DEVREADWRITE("k054539", k054539_device, read, write)
	AM_RANGE(0xec00, 0xec01) AM_DEVREADWRITE("ymsnd", ym2151_device, read, write)
	AM_RANGE(0xf000, 0xf003) AM_DEVICE("soundctrl", k054321_device, sound_map)
	AM_RANGE(0xf800, 0xf800) AM_WRITE(sound_bankswitch_w)
ADDRESS_MAP_END


static INPUT_PORTS_START( xexex )
	PORT_START("SYSTEM")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("P1")
	KONAMI16_LSB(1, IPT_UNKNOWN, IPT_START1 )

	PORT_START("P2")
	KONAMI16_LSB(2, IPT_UNKNOWN, IPT_START2 )

	PORT_START("EEPROM")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL ) PORT_READ_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, do_read)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_SPECIAL ) PORT_READ_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, ready_read)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_SERVICE_NO_TOGGLE( 0x08, IP_ACTIVE_LOW )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START( "EEPROMOUT" )
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OUTPUT ) PORT_WRITE_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, di_write)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_OUTPUT ) PORT_WRITE_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, cs_write)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_OUTPUT ) PORT_WRITE_LINE_DEVICE_MEMBER("eeprom", eeprom_serial_er5911_device, clk_write)
INPUT_PORTS_END

void xexex_state::machine_start()
{
	membank("z80bank")->configure_entries(0, 8, memregion("audiocpu")->base(), 0x4000);
	membank("z80bank")->set_entry(0);

	save_item(NAME(m_chenmix));
	save_item(NAME(m_cur_control2));
	save_item(NAME(m_cur_interrupt));
}

void xexex_state::machine_reset()
{
	m_cur_control2 = 0;
	m_cur_interrupt = 0;
	m_chenmix = false;
	m_k054539->init_flags(k054539_device::REVERSE_STEREO);
	update_irq();
}

static MACHINE_CONFIG_START( xexex )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M68000, XTAL_32MHz/2) // 16MHz
	MCFG_CPU_PROGRAM_MAP(main_map)

	MCFG_CPU_ADD("audiocpu", Z80, XTAL_32MHz/4) // Z80E 8Mhz
	MCFG_CPU_PROGRAM_MAP(sound_map)

	MCFG_QUANTUM_TIME(attotime::from_hz(1920))

	MCFG_EEPROM_SERIAL_ER5911_8BIT_ADD("eeprom")

	/* video hardware */
	MCFG_FLOW_RENDER_MANAGER_ADD("fr_manager")
	MCFG_FLOW_RENDER_MANAGER_SETUP(":", xexex_state, fr_setup)

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_VIDEO_ATTRIBUTES(VIDEO_UPDATE_BEFORE_VBLANK)
	MCFG_SCREEN_RAW_PARAMS(XTAL_32MHz/4, 512, 56, 56+384, 289, 15, 15+256)
	MCFG_SCREEN_FLOW_RENDER_RGB()

	MCFG_PALETTE_ADD("palette", 2048)
	MCFG_PALETTE_FORMAT(XRGB)

	MCFG_K054156_054157_ADD("tilemap", XTAL_32MHz/4, 2, 4, 24)

	MCFG_K053246_053247_ADD("sprites", XTAL_32MHz/4, "spriteram")
	MCFG_K053246_053247_WIRING_CB(xexex_state, sprites_wiring)
	MCFG_K053246_053247_DMA_REMAP_CB(xexex_state, sprites_remap)
	MCFG_K053246_053247_DMAIRQ_CB(WRITELINE(xexex_state, objdma_w))

	MCFG_K053250_ADD("lvc", XTAL_32MHz/4, ":lvcram")

	MCFG_K053251_ADD("mixer", 0)

	MCFG_DEVICE_ADD("video_timings", K053252, XTAL_32MHz/4)
	MCFG_K053252_INT1_CB(INPUTLINE("maincpu", 4))
	MCFG_K053252_VBLANK_CB(DEVWRITELINE(":sprites", k053246_053247_device, vblank_w))
	MCFG_K053252_KSNOTIFIER_CB(DEVKSNOTIFIER(":tilemap", k054156_054157_device, ksnotifier_w))
	MCFG_KSNOTIFIER_CHAIN(DEVKSNOTIFIER(":sprites", k053246_053247_device, ksnotifier_w))

	MCFG_K054338_ADD("blender", "palette")

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MCFG_K054321_ADD("soundctrl", ":lspeaker", ":rspeaker")

	MCFG_YM2151_ADD("ymsnd", XTAL_32MHz/8) // 4MHz
	MCFG_SOUND_ROUTE(0, "filter1l", 0.50)
	MCFG_SOUND_ROUTE(0, "filter1r", 0.50)
	MCFG_SOUND_ROUTE(1, "filter2l", 0.50)
	MCFG_SOUND_ROUTE(1, "filter2r", 0.50)

	MCFG_DEVICE_ADD("k054539", K054539, XTAL_18_432MHz)
	MCFG_K054539_APAN_CB(xexex_state, ym_set_mixing)
	MCFG_SOUND_ROUTE(0, "lspeaker", 1.0)
	MCFG_SOUND_ROUTE(0, "rspeaker", 1.0)
	MCFG_SOUND_ROUTE(1, "lspeaker", 1.0)
	MCFG_SOUND_ROUTE(1, "rspeaker", 1.0)

	MCFG_FILTER_VOLUME_ADD("filter1l", 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 1.0)
	MCFG_FILTER_VOLUME_ADD("filter1r", 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 1.0)
	MCFG_FILTER_VOLUME_ADD("filter2l", 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 1.0)
	MCFG_FILTER_VOLUME_ADD("filter2r", 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 1.0)
MACHINE_CONFIG_END


ROM_START( xexex ) /* Europe, Version AA */
	ROM_REGION( 0x180000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "067eaa01.16d", 0x000000, 0x040000, CRC(3ebcb066) SHA1(83a20433d9fdcc8b8d7133991f9a8164dddb61f3) )
	ROM_LOAD16_BYTE( "067eaa02.16f", 0x000001, 0x040000, CRC(36ea7a48) SHA1(34f8046d7ecf5ea66c59c5bc0d7627942c28fd3b) )
	ROM_LOAD16_BYTE( "067b03.13d",   0x100000, 0x040000, CRC(97833086) SHA1(a564f7b1b52c774d78a59f4418c7ecccaf94ad41) )
	ROM_LOAD16_BYTE( "067b04.13f",   0x100001, 0x040000, CRC(26ec5dc8) SHA1(9da62683bfa8f16607cbea2d59a1446ec8588c5b) )

	ROM_REGION( 0x020000, "audiocpu", 0 )
	ROM_LOAD( "067eaa05.4e", 0x000000, 0x020000, CRC(0e33d6ec) SHA1(4dd68cb78c779e2d035e43fec35a7672ed1c259b) )

	ROM_REGION( 0x200000, "tilemap", 0 )
	ROM_LOAD32_WORD_SWAP( "067b14.1n",   0x000000, 0x100000, CRC(02a44bfa) SHA1(ad95df4dbf8842820ef20f54407870afb6d0e4a3) )
	ROM_LOAD32_WORD_SWAP( "067b13.2n",   0x000002, 0x100000, CRC(633c8eb5) SHA1(a11f78003a1dffe2d8814d368155059719263082) )

	ROM_REGION( 0x400000, "sprites", 0 )
	ROM_LOAD64_WORD_SWAP( "067b12.17n",  0x000000, 0x100000, CRC(08d611b0) SHA1(9cac60131e0411f173acd8ef3f206e5e58a7e5d2) )
	ROM_LOAD64_WORD_SWAP( "067b11.19n",  0x000002, 0x100000, CRC(a26f7507) SHA1(6bf717cb9fcad59a2eafda967f14120b9ebbc8c5) )
	ROM_LOAD64_WORD_SWAP( "067b10.20n",  0x000004, 0x100000, CRC(ee31db8d) SHA1(c41874fb8b401ea9cdd327ee6239b5925418cf7b) )
	ROM_LOAD64_WORD_SWAP( "067b09.22n",  0x000006, 0x100000, CRC(88f072ef) SHA1(7ecc04dbcc29b715117e970cc96e11137a21b83a) )

	ROM_REGION( 0x080000, "lvc", 0 )
	ROM_LOAD( "067b08.22f",  0x000000, 0x080000, CRC(ca816b7b) SHA1(769ce3700e41200c34adec98598c0fe371fe1e6d) )

	ROM_REGION( 0x300000, "k054539", 0 )
	ROM_LOAD( "067b06.3e",   0x000000, 0x200000, CRC(3b12fce4) SHA1(c69172d9965b8da8a539812fac92d5f1a3c80d17) )
	ROM_LOAD( "067b07.1e",   0x200000, 0x100000, CRC(ec87fe1b) SHA1(ec9823aea5a1fc5c47c8262e15e10b28be87231c) )

	ROM_REGION( 0x80, "eeprom", 0 ) // default eeprom to prevent game booting upside down with error
	ROM_LOAD( "er5911.19b",  0x0000, 0x0080, CRC(155624cc) SHA1(457f921e3a5d053c53e4f1a44941eb0a1f22e1b2) )
ROM_END



ROM_START( orius ) /* USA, Version AA */
	ROM_REGION( 0x180000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "067uaa01.16d", 0x000000, 0x040000, CRC(f1263d3e) SHA1(c8a10b90e754ec7a72a23ac85b888f071ca40bb1) )
	ROM_LOAD16_BYTE( "067uaa02.16f", 0x000001, 0x040000, CRC(77709f64) SHA1(c26f09c9723facb89ab8aae2a036be4e6892d4bf) )
	ROM_LOAD16_BYTE( "067b03.13d",   0x100000, 0x040000, CRC(97833086) SHA1(a564f7b1b52c774d78a59f4418c7ecccaf94ad41) )
	ROM_LOAD16_BYTE( "067b04.13f",   0x100001, 0x040000, CRC(26ec5dc8) SHA1(9da62683bfa8f16607cbea2d59a1446ec8588c5b) )

	ROM_REGION( 0x020000, "audiocpu", 0 )
	ROM_LOAD( "067uaa05.4e", 0x000000, 0x020000, CRC(0e33d6ec) SHA1(4dd68cb78c779e2d035e43fec35a7672ed1c259b) )

	ROM_REGION( 0x200000, "tilemap", 0 )
	ROM_LOAD32_WORD_SWAP( "067b14.1n",   0x000000, 0x100000, CRC(02a44bfa) SHA1(ad95df4dbf8842820ef20f54407870afb6d0e4a3) )
	ROM_LOAD32_WORD_SWAP( "067b13.2n",   0x000002, 0x100000, CRC(633c8eb5) SHA1(a11f78003a1dffe2d8814d368155059719263082) )

	ROM_REGION( 0x400000, "sprites", 0 )
	ROM_LOAD64_WORD_SWAP( "067b12.17n",  0x000000, 0x100000, CRC(08d611b0) SHA1(9cac60131e0411f173acd8ef3f206e5e58a7e5d2) )
	ROM_LOAD64_WORD_SWAP( "067b11.19n",  0x000002, 0x100000, CRC(a26f7507) SHA1(6bf717cb9fcad59a2eafda967f14120b9ebbc8c5) )
	ROM_LOAD64_WORD_SWAP( "067b10.20n",  0x000004, 0x100000, CRC(ee31db8d) SHA1(c41874fb8b401ea9cdd327ee6239b5925418cf7b) )
	ROM_LOAD64_WORD_SWAP( "067b09.22n",  0x000006, 0x100000, CRC(88f072ef) SHA1(7ecc04dbcc29b715117e970cc96e11137a21b83a) )

	ROM_REGION( 0x080000, "lvc", 0 )
	ROM_LOAD( "067b08.22f",  0x000000, 0x080000, CRC(ca816b7b) SHA1(769ce3700e41200c34adec98598c0fe371fe1e6d) )

	ROM_REGION( 0x300000, "k054539", 0 )
	ROM_LOAD( "067b06.3e",   0x000000, 0x200000, CRC(3b12fce4) SHA1(c69172d9965b8da8a539812fac92d5f1a3c80d17) )
	ROM_LOAD( "067b07.1e",   0x200000, 0x100000, CRC(ec87fe1b) SHA1(ec9823aea5a1fc5c47c8262e15e10b28be87231c) )

	ROM_REGION( 0x80, "eeprom", 0 ) // default eeprom to prevent game booting upside down with error
	ROM_LOAD( "er5911.19b",  0x0000, 0x0080, CRC(547ee4e4) SHA1(089601fcfa513f129d6e2587594b932d4a8fde18) ) //sldh
ROM_END

ROM_START( xexexa ) /* Asia, Version AA */
	ROM_REGION( 0x180000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "067aaa01.16d", 0x000000, 0x040000, CRC(cf557144) SHA1(4ce587580d953b88864652dd66485d49ca719ec5) )
	ROM_LOAD16_BYTE( "067aaa02.16f", 0x000001, 0x040000, CRC(b7b98d52) SHA1(ca2343bf37f779699b6782772e559ea5662c1742) )
	ROM_LOAD16_BYTE( "067b03.13d",   0x100000, 0x040000, CRC(97833086) SHA1(a564f7b1b52c774d78a59f4418c7ecccaf94ad41) )
	ROM_LOAD16_BYTE( "067b04.13f",   0x100001, 0x040000, CRC(26ec5dc8) SHA1(9da62683bfa8f16607cbea2d59a1446ec8588c5b) )

	ROM_REGION( 0x020000, "audiocpu", 0 )
	ROM_LOAD( "067eaa05.4e", 0x000000, 0x020000, CRC(0e33d6ec) SHA1(4dd68cb78c779e2d035e43fec35a7672ed1c259b) )

	ROM_REGION( 0x200000, "tilemap", 0 )
	ROM_LOAD32_WORD_SWAP( "067b14.1n",   0x000000, 0x100000, CRC(02a44bfa) SHA1(ad95df4dbf8842820ef20f54407870afb6d0e4a3) )
	ROM_LOAD32_WORD_SWAP( "067b13.2n",   0x000002, 0x100000, CRC(633c8eb5) SHA1(a11f78003a1dffe2d8814d368155059719263082) )

	ROM_REGION( 0x400000, "sprites", 0 )
	ROM_LOAD64_WORD_SWAP( "067b12.17n",  0x000000, 0x100000, CRC(08d611b0) SHA1(9cac60131e0411f173acd8ef3f206e5e58a7e5d2) )
	ROM_LOAD64_WORD_SWAP( "067b11.19n",  0x000002, 0x100000, CRC(a26f7507) SHA1(6bf717cb9fcad59a2eafda967f14120b9ebbc8c5) )
	ROM_LOAD64_WORD_SWAP( "067b10.20n",  0x000004, 0x100000, CRC(ee31db8d) SHA1(c41874fb8b401ea9cdd327ee6239b5925418cf7b) )
	ROM_LOAD64_WORD_SWAP( "067b09.22n",  0x000006, 0x100000, CRC(88f072ef) SHA1(7ecc04dbcc29b715117e970cc96e11137a21b83a) )

	ROM_REGION( 0x080000, "lvc", 0 )
	ROM_LOAD( "067b08.22f",  0x000000, 0x080000, CRC(ca816b7b) SHA1(769ce3700e41200c34adec98598c0fe371fe1e6d) )

	ROM_REGION( 0x300000, "k054539", 0 )
	ROM_LOAD( "067b06.3e",   0x000000, 0x200000, CRC(3b12fce4) SHA1(c69172d9965b8da8a539812fac92d5f1a3c80d17) )
	ROM_LOAD( "067b07.1e",   0x200000, 0x100000, CRC(ec87fe1b) SHA1(ec9823aea5a1fc5c47c8262e15e10b28be87231c) )

	ROM_REGION( 0x80, "eeprom", 0 ) // default eeprom to prevent game booting upside down with error
	ROM_LOAD( "er5911.19b",  0x0000, 0x0080, CRC(051c14c6) SHA1(23addbaa2ce323c06551b343ca45dea4fd2b9eee) ) // sldh
ROM_END

ROM_START( xexexj ) /* Japan, Version AA */
	ROM_REGION( 0x180000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "067jaa01.16d", 0x000000, 0x040000, CRC(06e99784) SHA1(d53fe3724608992a6938c36aa2719dc545d6b89e) )
	ROM_LOAD16_BYTE( "067jaa02.16f", 0x000001, 0x040000, CRC(30ae5bc4) SHA1(60491e31eef64a9206d1372afa32d83c6c0968b3) )
	ROM_LOAD16_BYTE( "067b03.13d",   0x100000, 0x040000, CRC(97833086) SHA1(a564f7b1b52c774d78a59f4418c7ecccaf94ad41) )
	ROM_LOAD16_BYTE( "067b04.13f",   0x100001, 0x040000, CRC(26ec5dc8) SHA1(9da62683bfa8f16607cbea2d59a1446ec8588c5b) )

	ROM_REGION( 0x020000, "audiocpu", 0 )
	ROM_LOAD( "067jaa05.4e", 0x000000, 0x020000, CRC(2f4dd0a8) SHA1(bfa76c9c968f1beba648a2911510e3d666a8fe3a) )

	ROM_REGION( 0x200000, "tilemap", 0 )
	ROM_LOAD32_WORD_SWAP( "067b14.1n",   0x000000, 0x100000, CRC(02a44bfa) SHA1(ad95df4dbf8842820ef20f54407870afb6d0e4a3) )
	ROM_LOAD32_WORD_SWAP( "067b13.2n",   0x000002, 0x100000, CRC(633c8eb5) SHA1(a11f78003a1dffe2d8814d368155059719263082) )

	ROM_REGION( 0x400000, "sprites", 0 )
	ROM_LOAD64_WORD_SWAP( "067b12.17n",  0x000000, 0x100000, CRC(08d611b0) SHA1(9cac60131e0411f173acd8ef3f206e5e58a7e5d2) )
	ROM_LOAD64_WORD_SWAP( "067b11.19n",  0x000002, 0x100000, CRC(a26f7507) SHA1(6bf717cb9fcad59a2eafda967f14120b9ebbc8c5) )
	ROM_LOAD64_WORD_SWAP( "067b10.20n",  0x000004, 0x100000, CRC(ee31db8d) SHA1(c41874fb8b401ea9cdd327ee6239b5925418cf7b) )
	ROM_LOAD64_WORD_SWAP( "067b09.22n",  0x000006, 0x100000, CRC(88f072ef) SHA1(7ecc04dbcc29b715117e970cc96e11137a21b83a) )

	ROM_REGION( 0x080000, "lvc", 0 )
	ROM_LOAD( "067b08.22f",  0x000000, 0x080000, CRC(ca816b7b) SHA1(769ce3700e41200c34adec98598c0fe371fe1e6d) )

	ROM_REGION( 0x300000, "k054539", 0 )
	ROM_LOAD( "067b06.3e",   0x000000, 0x200000, CRC(3b12fce4) SHA1(c69172d9965b8da8a539812fac92d5f1a3c80d17) )
	ROM_LOAD( "067b07.1e",   0x200000, 0x100000, CRC(ec87fe1b) SHA1(ec9823aea5a1fc5c47c8262e15e10b28be87231c) )

	ROM_REGION( 0x80, "eeprom", 0 ) // default eeprom to prevent game booting upside down with error
	ROM_LOAD( "er5911.19b",  0x0000, 0x0080, CRC(79a79c7b) SHA1(02eb235226949af0147d6d0fd2bd3d7a68083ae6) ) // sldh
ROM_END


DRIVER_INIT_MEMBER(xexex_state,xexex)
{
}

GAME( 1991, xexex,  0,     xexex, xexex, xexex_state, xexex, ROT0, "Konami", "Xexex (ver EAA)", MACHINE_SUPPORTS_SAVE )
GAME( 1991, orius,  xexex, xexex, xexex, xexex_state, xexex, ROT0, "Konami", "Orius (ver UAA)", MACHINE_SUPPORTS_SAVE )
GAME( 1991, xexexa, xexex, xexex, xexex, xexex_state, xexex, ROT0, "Konami", "Xexex (ver AAA)", MACHINE_SUPPORTS_SAVE )
GAME( 1991, xexexj, xexex, xexex, xexex, xexex_state, xexex, ROT0, "Konami", "Xexex (ver JAA)", MACHINE_SUPPORTS_SAVE )

// license:BSD-3-Clause
/**************************************************************************
    Pirate Ship

 PWB(A)354460B

 MC68HC00FN16

 054539  - 8-Channel ADPCM sound generator. Clock input 18.432MHz. Clock outputs 18.432/4 & 18.432/8
 053250  - LVC road generator
 053246A - Sprite generator
 055673  - Sprite generator
 055555  - Mixer/Priority encoder
 056832  - Tilemap generator
 054156  - Tilemap generator
 053252  - CRTC


 053250 config:

 SELC (69)  GND
 SEL1 (83)  GND
 SEL0 (82)  GND
 MODE (68)  GND

 TODO: Music stops if a coin is inserted. MAME or BTNAB?

**************************************************************************/

#include "emu.h"
#include "speaker.h"
#include "cpu/m68000/m68000.h"
#include "machine/gen_latch.h"
#include "machine/k053252.h"
#include "machine/nvram.h"
#include "machine/ticket.h"
#include "sound/k054539.h"
#include "video/k053246_k053247_k055673.h"
#include "video/k053250.h"
#include "video/k054000.h"
#include "video/k054156_k054157_k056832.h"
#include "video/k055555.h"
#include "video/kvideodac.h"

class piratesh_state : public driver_device
{
public:
	piratesh_state(const machine_config &mconfig, device_type type, const char *tag)
	: driver_device(mconfig, type, tag),
	m_maincpu(*this,"maincpu"),
	m_lvc(*this, "lvc"),
	m_video_timings(*this, "video_timings"),
	m_tilemap(*this, "tilemap"),
	m_sprites(*this, "sprites"),
	m_mixer(*this, "mixer"),
	m_k054539(*this, "k054539"),
	m_videodac(*this, "videodac"),
	m_screen(*this, "screen")
	{ }

	required_device<cpu_device> m_maincpu;

	required_device<k053250_device> m_lvc;
	required_device<k053252_device> m_video_timings;
	required_device<k054156_056832_device> m_tilemap;
	required_device<k053246_055673_device> m_sprites;
	required_device<k055555_device> m_mixer;
	required_device<k054539_device> m_k054539;
	required_device<kvideodac_device> m_videodac;
	required_device<screen_device> m_screen;

	uint8_t m_int_enable;
	uint8_t m_int_status;
	uint16_t m_control;

	void update_interrupts();

	DECLARE_WRITE_LINE_MEMBER(vblankirq_w);

	DECLARE_WRITE16_MEMBER(control1_w);
	DECLARE_WRITE16_MEMBER(control2_w);
	DECLARE_WRITE16_MEMBER(control3_w);
	DECLARE_WRITE16_MEMBER(irq_ack_w);
	DECLARE_CUSTOM_INPUT_MEMBER(helm_r);
	DECLARE_CUSTOM_INPUT_MEMBER(battery_r);

	DECLARE_WRITE16_MEMBER(k055555_disp_hack_w);

	DECLARE_MACHINE_START(piratesh);
	DECLARE_MACHINE_RESET(piratesh);
	DECLARE_VIDEO_START(piratesh);
	DECLARE_WRITE_LINE_MEMBER(k054539_nmi_gen);
	TIMER_DEVICE_CALLBACK_MEMBER(piratesh_interrupt);

	void fr_setup(flow_render::manager *manager);
};

void piratesh_state::fr_setup(flow_render::manager *manager)
{
	auto rv = m_videodac->flow_render_get_renderer();
	auto rm = m_mixer   ->flow_render_get_renderer();
	auto rs = m_sprites ->flow_render_get_renderer();
	auto rl = m_lvc     ->flow_render_get_renderer();

	manager->connect(m_tilemap->flow_render_get_renderer("a")->out(), rm->inp("a color"));
	manager->connect(m_tilemap->flow_render_get_renderer("b")->out(), rm->inp("b color"));
	manager->connect(m_tilemap->flow_render_get_renderer("c")->out(), rm->inp("c color"));
	manager->connect(m_tilemap->flow_render_get_renderer("d")->out(), rm->inp("d color"));

	manager->connect(rs->out("color"), rm->inp("s2 color"));
	manager->connect(rs->out("attr"),  rm->inp("s2 attr"));

	manager->connect(rl->out("color"), rm->inp("s3 color"));
	manager->connect(rl->out("attr"),  rm->inp("s3 attr"));

	manager->set_constant(rm->inp("s1 color"), 0);
	manager->set_constant(rm->inp("s1 attr"), 0);
	manager->set_constant(rm->inp("o color"), 0);
	manager->set_constant(rm->inp("o attr"), 0);

	manager->connect(rm->out("0 color"), rv->inp("color"));
	manager->connect(rm->out("0 attr"),  rv->inp("attr"));

	manager->connect(rv->out(), m_screen->flow_render_get_renderer()->inp());
}

void piratesh_state::update_interrupts()
{
	m_maincpu->set_input_line(M68K_IRQ_2, m_int_status & 2 ? ASSERT_LINE : CLEAR_LINE); // INT 1
	m_maincpu->set_input_line(M68K_IRQ_4, m_int_status & 1 ? ASSERT_LINE : CLEAR_LINE);
	m_maincpu->set_input_line(M68K_IRQ_5, m_int_status & 4 ? ASSERT_LINE : CLEAR_LINE);
}

WRITE_LINE_MEMBER(piratesh_state::vblankirq_w)
{
	if((m_int_enable & 2) && state)
		m_int_status |= 2;
	else
		m_int_status &= ~2;
	update_interrupts();
}

/*
 Priority issues:

 1. On title screen, stars should be behind the helm
 2. The Konami logo is a square transition
 3.

*/
#if 0
K056832_CB_MEMBER(piratesh_state::piratesh_tile_callback)
{
	// Layer
	// Code
	// Color
	// Flags
//  if (*color != 0)
//      printf("%x %x %x\n", layer, *code, *color >> 2);

	*color = (m_layer_colorbase[layer] << 4) + ((*color >> 2));// & 0x0f);
}

K055673_CB_MEMBER(piratesh_state::piratesh_sprite_callback)
{
	int c = *color;

	*color = (c & 0x001f);
	//int pri = (c >> 5) & 7;
	// .... .... ...x xxxx - Color
	// .... .... xxx. .... - Priority?
	// .... ..x. .... .... - ?
	// ..x. .... .... .... - ?

#if 0
	int layerpri[4];
	static const int pris[4] = { K55_PRIINP_0, K55_PRIINP_3, K55_PRIINP_6, K55_PRIINP_7 };

	for (uint32_t i = 0; i < 4; i++)
	{
		layerpri[i] = m_k055555->K055555_read_register(pris[i]);
	}

	// TODO: THIS IS ALL WRONG
	if (pri <= layerpri[0])
		*priority_mask = 0;
	else if (pri <= layerpri[1])
		*priority_mask = 0xf0;
	else if (pri <= layerpri[2])
		*priority_mask = 0xf0|0xcc;
	else
		*priority_mask = 0xf0|0xcc|0xaa;
#endif

	*priority_mask = 0;

	// 0 - Sprites over everything
	// f0 -
	// f0 cc -
	// f0 cc aa -

	// 1111 0000
	// 1100 1100
	// 1010 1010
}
#endif

#if 1
TIMER_DEVICE_CALLBACK_MEMBER(piratesh_state::piratesh_interrupt)
{
	int scanline = param;

	// IRQ2 - CCUINT1 (VBL START)
	// IRQ4 - Sound
	// IRQ5 - CCUINT2 (VBL END)

	if (scanline == 240)
	{
	  //		m_k053250->vblank_w(1);

		if (m_int_enable & 2)
		{
			m_int_status |= 2;
			update_interrupts();
		}
	}

	if (scanline == 0)
	{
	  //		m_k053250->vblank_w(0);

		if (m_int_enable & 4)
		{
			m_int_status |= 4;
			update_interrupts();
		}
	}
}
#endif

WRITE16_MEMBER(piratesh_state::control1_w)
{
	// .... ..xx .... ....      - Unknown
	// .... .x.. .... ....      - Unknown - Active during attract, clear during game
	// .... x... .... ....      - Lamp? (active when waiting to start game)

	if (data & ~0x0f00)
		printf("CTRL3: %x %x %x\n", offset, data, mem_mask);
}

WRITE16_MEMBER(piratesh_state::control2_w)
{
	// .... .... ...x ....      - Unknown (always 1?)
	// .... .... ..x. ....      - Unknown
	// .... .... .x.. ....      - Counter out
	// .... .... x... ....      - Counter in
	// .... ...x .... ....      - 053246A OBJCRBK (Pin 9)
	// .... ..x. .... ....      - LV related
	// .... x... .... ....      - INT4/SND control (0=clear 1=enable)
	// ...x .... .... ....      - INT2/CCUINT1 control (0=clear 1=enable)
	// ..x. .... .... ....      - INT5/CCUINT2 control (0=clear 1=enable)
	// .x.. .... .... ....      - Unknown
	// x... .... .... ....      - Unknown

	m_int_enable = (data >> 11) & 7;
	m_int_status &= m_int_enable;
	update_interrupts();

	if (data & ~0xfbf0)
		printf("CTRL2: %x %x %x\n", offset, data, mem_mask);
}

WRITE16_MEMBER(piratesh_state::control3_w)
{
	// .... .... .... ...x      - Watchdog? (051550?)
	// .... .... .... ..x.      - 056832 ROM bank control
	// .... .... ...x ....      - Ticket dispenser enable (active high)
	// .... .... ..x. ....      - Hopper enable (active high)
	// .... ...x .... ....      - Unknown (always 1?)

	COMBINE_DATA(&m_control);

	if ((data & ~0x0133) || (~data & 0x100))
		printf("CTRL1 W: %x %x %x\n", offset, data, mem_mask);

//  printf("CTRL 1: %x\n", data & 0x0010);
	machine().device<ticket_dispenser_device>("ticket")->motor_w(m_control & 0x0010 ? 1 : 0);
	machine().device<ticket_dispenser_device>("hopper")->motor_w(m_control & 0x0020 ? 1 : 0);

	m_tilemap->set_banking(m_control & 2 ? 0x80000 : 0);
}

WRITE16_MEMBER(piratesh_state::k055555_disp_hack_w)
{
	m_mixer->disp_w(space, 0, data);
}

static ADDRESS_MAP_START( piratesh_map, AS_PROGRAM, 16, piratesh_state )
	AM_RANGE(0x000000, 0x07ffff) AM_ROM
	AM_RANGE(0x080000, 0x083fff) AM_RAM AM_SHARE("nvram")
	AM_RANGE(0x084000, 0x087fff) AM_RAM
	AM_RANGE(0x100000, 0x10001f) AM_DEVICE8("video_timings", k053252_device, map, 0x00ff)
	AM_RANGE(0x180000, 0x18003f) AM_DEVICE("tilemap", k054156_056832_device, vacset)
	AM_RANGE(0x280000, 0x280007) AM_DEVICE("sprites", k053246_055673_device, objset1)
	AM_RANGE(0x290000, 0x29000f) AM_DEVREAD("sprites", k053246_055673_device, rom16_r)
	AM_RANGE(0x290010, 0x29001f) AM_DEVICE("sprites", k053246_055673_device, objset2)
	AM_RANGE(0x2a0000, 0x2a3fff) AM_RAM AM_SHARE("spriteram") // SPRITES
	AM_RANGE(0x2b0000, 0x2b000f) AM_DEVICE8("lvc", k053250_device, map, 0x00ff)
	AM_RANGE(0x30005a, 0x30005b) AM_WRITE(k055555_disp_hack_w)
	AM_RANGE(0x300000, 0x3000ff) AM_DEVICE8("mixer", k055555_device, map, 0x00ff)
	AM_RANGE(0x380000, 0x381fff) AM_RAM_DEVWRITE("palette", palette_device, write) AM_SHARE("palette")
	AM_RANGE(0x400000, 0x400001) AM_READ_PORT("IN0")
	AM_RANGE(0x400002, 0x400003) AM_READ_PORT("IN1")
	AM_RANGE(0x400004, 0x400005) AM_READ_PORT("DSW1")
	AM_RANGE(0x400006, 0x400007) AM_READ_PORT("DSW2")
	AM_RANGE(0x400008, 0x400009) AM_READ_PORT("SPECIAL")
	AM_RANGE(0x40000c, 0x40000d) AM_WRITE(control1_w)
	AM_RANGE(0x400010, 0x400011) AM_WRITE(control2_w)
	AM_RANGE(0x400014, 0x400015) AM_WRITE(control3_w)
	AM_RANGE(0x500000, 0x501fff) AM_DEVREAD("tilemap", k054156_056832_device, rom16_r)
	AM_RANGE(0x580000, 0x581fff) AM_DEVREAD("lvc", k053250_device, rom_r)
	AM_RANGE(0x600000, 0x6004ff) AM_DEVREADWRITE8("k054539", k054539_device, read, write, 0xff00) // SOUND
	AM_RANGE(0x680000, 0x681fff) AM_DEVREADWRITE("tilemap", k054156_056832_device, vram16_r, vram16_w)
	AM_RANGE(0x700000, 0x703fff) AM_RAM AM_SHARE("lvcram")
ADDRESS_MAP_END


WRITE_LINE_MEMBER(piratesh_state::k054539_nmi_gen)
{
	static int m_sound_intck = 0; // TODO: KILL ME

	// Trigger an interrupt on the rising edge
	if (!m_sound_intck && state)
	{
		if (m_int_enable & 1)
		{
			m_int_status |= 1;
			update_interrupts();
		}
	}

	m_sound_intck = state;
}

CUSTOM_INPUT_MEMBER(piratesh_state::helm_r)
{
	// Appears to be a quadrature encoder
	uint8_t xa, xb;
	uint16_t dx = ioport("HELM")->read();

	xa = ((dx + 1) & 7) <= 3;
	xb = (dx & 7) <= 3;

	return (xb << 1) | xa;
}

CUSTOM_INPUT_MEMBER(piratesh_state::battery_r)
{
	// .x MB3790 /ALARM1
	// x. MB3790 /ALARM2

	return 0x3;
}

/**********************************************************************************/

static INPUT_PORTS_START( piratesh )
	PORT_START("IN0")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON3 ) // 7f60  btst $7,$40000
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) // HELM?
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )   // HELM?
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_SERVICE2 ) PORT_NAME("Reset")
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_START )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME(DEF_STR( Test )) PORT_CODE(KEYCODE_F2)

	PORT_START("SPECIAL")
//	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_SPECIAL ) PORT_READ_LINE_DEVICE_MEMBER("k053250", k053250ps_device, dmairq_r)
	PORT_BIT( 0x0300, IP_ACTIVE_HIGH, IPT_UNKNOWN ) // FIXME: NCPU from 053246 (DMA)
	PORT_BIT( 0x0c00, IP_ACTIVE_HIGH, IPT_SPECIAL )PORT_CUSTOM_MEMBER(DEVICE_SELF, piratesh_state, battery_r, nullptr)

	PORT_START("HELM")
	PORT_BIT( 0xff, 0x00, IPT_DIAL ) PORT_SENSITIVITY(25) PORT_KEYDELTA(1)

	PORT_START("IN1")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_SERVICE1 ) PORT_NAME("Service")
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_SPECIAL ) PORT_READ_LINE_DEVICE_MEMBER("ticket", ticket_dispenser_device, line_r)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_SPECIAL ) PORT_READ_LINE_DEVICE_MEMBER("hopper", ticket_dispenser_device, line_r)
	PORT_BIT( 0x1800, IP_ACTIVE_HIGH, IPT_SPECIAL )PORT_CUSTOM_MEMBER(DEVICE_SELF, piratesh_state, helm_r, nullptr)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("DSW1") // TODO: DIP switches are used for settings when battery failure has occurred
	PORT_DIPNAME( 0x0100, 0x0100, "DSW1:0" ) PORT_DIPLOCATION("DSW1:1")
	PORT_DIPSETTING(    0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "DSW1:1" ) PORT_DIPLOCATION("DSW1:2")
	PORT_DIPSETTING(    0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "DSW1:2" ) PORT_DIPLOCATION("DSW1:3")
	PORT_DIPSETTING(    0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "DSW1:3" ) PORT_DIPLOCATION("DSW1:4")
	PORT_DIPSETTING(    0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x3000, 0x1000, DEF_STR( Difficulty ) ) PORT_DIPLOCATION("DSW1:5,6")
	PORT_DIPSETTING(    0x0000, "A" )
	PORT_DIPSETTING(    0x1000, "B" )
	PORT_DIPSETTING(    0x2000, "C" )
	PORT_DIPSETTING(    0x3000, "D" )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Demo_Sounds ) ) PORT_DIPLOCATION("DSW1:7")
	PORT_DIPSETTING(    0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x4000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Free_Play ) ) PORT_DIPLOCATION("DSW1:8")
	PORT_DIPSETTING(    0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )

	PORT_START("DSW2") // TODO: Finish me
	PORT_DIPNAME( 0x0100, 0x0100, "DSW2:0" ) PORT_DIPLOCATION("DSW2:1")
	PORT_DIPSETTING(    0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0200, 0x0200, "DSW2:1" ) PORT_DIPLOCATION("DSW2:2")
	PORT_DIPSETTING(    0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0400, 0x0400, "DSW2:2" ) PORT_DIPLOCATION("DSW2:3")
	PORT_DIPSETTING(    0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, "DSW2:3" ) PORT_DIPLOCATION("DSW2:4")
	PORT_DIPSETTING(    0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "DSW2:4" ) PORT_DIPLOCATION("DSW2:5")
	PORT_DIPSETTING(    0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "DSW2:5" ) PORT_DIPLOCATION("DSW2:6")
	PORT_DIPSETTING(    0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, "DSW2:6" ) PORT_DIPLOCATION("DSW2:7")
	PORT_DIPSETTING(    0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, "Redemption Type" ) PORT_DIPLOCATION("DSW2:8")
	PORT_DIPSETTING(    0x8000, "Ticket" )
	PORT_DIPSETTING(    0x0000, "Capsule" )
INPUT_PORTS_END

/**********************************************************************************/

MACHINE_START_MEMBER(piratesh_state, piratesh)
{
	save_item(NAME(m_int_status));
	save_item(NAME(m_int_enable));
	save_item(NAME(m_control));
}

MACHINE_RESET_MEMBER(piratesh_state,piratesh)
{
	m_int_status = 0;
	m_int_enable = 0;
	m_control = 0;

	// soften chorus(chip 0 channel 0-3), boost voice(chip 0 channel 4-7)
	for (int i=0; i<=7; i++)
	{
	//  m_k054539->set_gain(i, 0.5);
	}

//  // soften percussions(chip 1 channel 0-7)
//  for (i=0; i<=7; i++) m_k054539_2->set_gain(i, 0.5);

}

static MACHINE_CONFIG_START( piratesh )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M68000, XTAL_32MHz/2)
	MCFG_CPU_PROGRAM_MAP(piratesh_map)

	MCFG_NVRAM_ADD_0FILL("nvram")

	MCFG_DEVICE_ADD("video_timings", K053252, XTAL_32MHz/4)
	MCFG_K053252_INT1_CB(WRITELINE(piratesh_state, vblankirq_w))
//	MCFG_K053252_INT2_CB(WRITELINE(piratesh_state, ccuint2_w))
	MCFG_K053252_KSNOTIFIER_CB(DEVKSNOTIFIER(":tilemap", k054156_056832_device, ksnotifier_w))
	MCFG_KSNOTIFIER_CHAIN(DEVKSNOTIFIER(":sprites", k053246_055673_device, ksnotifier_w))
	MCFG_VIDEO_SET_SCREEN("screen")

	MCFG_TIMER_DRIVER_ADD_SCANLINE("scantimer", piratesh_state, piratesh_interrupt, "screen", 0, 1)

	MCFG_MACHINE_START_OVERRIDE(piratesh_state, piratesh)
	MCFG_MACHINE_RESET_OVERRIDE(piratesh_state, piratesh)

	MCFG_TICKET_DISPENSER_ADD("ticket", attotime::from_msec(200), TICKET_MOTOR_ACTIVE_HIGH, TICKET_STATUS_ACTIVE_HIGH)
	MCFG_TICKET_DISPENSER_ADD("hopper", attotime::from_msec(200), TICKET_MOTOR_ACTIVE_HIGH, TICKET_STATUS_ACTIVE_HIGH)

	/* video hardware */
	MCFG_FLOW_RENDER_MANAGER_ADD("fr_manager")
	MCFG_FLOW_RENDER_MANAGER_SETUP(":", piratesh_state, fr_setup)

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_VIDEO_ATTRIBUTES(VIDEO_UPDATE_AFTER_VBLANK)
	MCFG_SCREEN_FLOW_RENDER_RGB()
	MCFG_SCREEN_RAW_PARAMS(8000000, 512, 58, 58+385, 264, 16, 16+224)

	MCFG_PALETTE_ADD("palette", 2048)
	MCFG_PALETTE_FORMAT(BGRX)

	MCFG_KVIDEODAC_ADD("videodac", "palette", 0x300, 0.6, 0, 1.0)
	MCFG_KVIDEODAC_SKIPPED_BITS(2)

	MCFG_K054156_056832_ADD("tilemap", XTAL_32MHz/4, 4, 4, 24)
	MCFG_K054156_056832_DISABLE_VRC2()
	MCFG_K054156_056832_SET_COLOR_BITS_ROTATION(true)

	MCFG_K053246_055673_ADD("sprites", XTAL_32MHz/4, "spriteram")

	MCFG_K055555_ADD("mixer")

	MCFG_K053250_ADD("lvc", XTAL_32MHz/4, ":lvcram")

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MCFG_DEVICE_ADD("k054539", K054539, XTAL_18_432MHz)
	MCFG_K054539_TIMER_HANDLER(WRITELINE(piratesh_state, k054539_nmi_gen))
	MCFG_SOUND_ROUTE(0, "lspeaker", 0.2)
	MCFG_SOUND_ROUTE(1, "rspeaker", 0.2)
MACHINE_CONFIG_END



ROM_START( piratesh )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "360ua-c04.4p", 0x000000, 0x80000, CRC(6d69dd90) SHA1(ccbdbfea406d9cbc3f242211290ba82ccbbe3795) )

	/* tiles */
	ROM_REGION( 0x100000, "tilemap", ROMREGION_ERASE00 ) // 27C4096
	ROM_LOAD32_WORD_SWAP( "360ua-a01.17g", 0x000000, 0x80000, CRC(e39153f5) SHA1(5da9132a2c24a15b55c3f65c26e2ad0467411a88) )

	/* sprites */
	ROM_REGION( 0x200000, "sprites", ROMREGION_ERASE00 ) // 27C4096
	ROM_LOAD64_WORD_SWAP( "360ua-a02.21l", 0x000000, 0x80000, CRC(82207997) SHA1(fe143285a12fab5227e883113d798acad7bf4c97) )
	ROM_LOAD64_WORD_SWAP( "360ua-a03.23l", 0x000002, 0x80000, CRC(a9e36d51) SHA1(1a8de8d8d2abfee5ac0f0822e203846f7f5f1767) )

	/* road generator */
	ROM_REGION( 0x080000, "lvc", ROMREGION_ERASE00 ) // 27C040
	ROM_LOAD( "360ua-a05.26p", 0x000000, 0x80000, CRC(dab7f439) SHA1(2372612c0b04c77a85ccbadc100cb741b85f0481) )

	/* sound data */
	ROM_REGION( 0x100000, "k054539", 0 ) // 27C040
	ROM_LOAD( "360ua-a06.15t", 0x000000, 0x80000, CRC(6816a493) SHA1(4fc4cfbc164d84bbf8d75ccd78c9f40f3273d852) )
	ROM_LOAD( "360ua-a07.17t", 0x080000, 0x80000, CRC(af7127c5) SHA1(b525f3c6b831e3354eba46016d414bedcb3ae8dc) )

//  ROM_REGION( 0x80, "eeprom", 0 ) // default eeprom to prevent game booting upside down with error
//  ROM_LOAD( "piratesh.nv", 0x0000, 0x080, CRC(28df2269) SHA1(3f071c97662745a199f96964e2e79f795bd5a391) )
ROM_END

//    year  name        parent    machine   input     state           init
GAME( 1995, piratesh,   0,        piratesh, piratesh, piratesh_state, 0, ROT90,  "Konami", "Pirate Ship (ver UAA)", MACHINE_IMPERFECT_GRAPHICS )

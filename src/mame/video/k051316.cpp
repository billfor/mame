// license:BSD-3-Clause
// copyright-holders:Fabio Priuli,Acho A. Tang, R. Belmont
/*
Konami 051316 PSAC
------
Manages a 32x32 tilemap (16x16 tiles, 512x512 pixels) which can be zoomed,
distorted and rotated.
It uses two internal 24 bit counters which are incremented while scanning the
picture. The coordinates of the pixel in the tilemap that has to be drawn to
the current beam position are the counters / (2^11).
The chip doesn't directly generate the color information for the pixel, it
just generates a 24 bit address (whose top 16 bits are the contents of the
tilemap RAM), and a "visible" signal. It's up to external circuitry to convert
the address into a pixel color. Most games seem to use 4bpp graphics, but Ajax
uses 7bpp.
If the value in the internal counters is out of the visible range (0..511), it
is truncated and the corresponding address is still generated, but the "visible"
signal is not asserted. The external circuitry might ignore that signal and
still generate the pixel, therefore making the tilemap a continuous playfield
that wraps around instead of a large sprite.

control registers
000-001 X counter starting value / 256
002-003 amount to add to the X counter after each horizontal pixel
004-005 amount to add to the X counter after each line (0 = no rotation)
006-007 Y counter starting value / 256
008-009 amount to add to the Y counter after each horizontal pixel (0 = no rotation)
00a-00b amount to add to the Y counter after each line
00c-00d ROM bank to read, used during ROM testing
00e     bit 0 = enable ROM reading (active low). This only makes the chip output the
                requested address: the ROM is actually read externally, not through
                the chip's data bus.
        bit 1 = unknown
        bit 2 = unknown
00f     unused

*/

#include "emu.h"
#include "k051316.h"

DEFINE_DEVICE_TYPE(K051316, k051316_device, "k051316", "K051316 PSAC")

DEVICE_ADDRESS_MAP_START(map, 8, k051316_device)
	AM_RANGE(0x0, 0x0) AM_WRITE(start_x_h_w)
	AM_RANGE(0x1, 0x1) AM_WRITE(start_x_l_w)
	AM_RANGE(0x2, 0x2) AM_WRITE(incxx_h_w)
	AM_RANGE(0x3, 0x3) AM_WRITE(incxx_l_w)
	AM_RANGE(0x4, 0x4) AM_WRITE(incyx_h_w)
	AM_RANGE(0x5, 0x5) AM_WRITE(incyx_l_w)
	AM_RANGE(0x6, 0x6) AM_WRITE(start_y_h_w)
	AM_RANGE(0x7, 0x7) AM_WRITE(start_y_l_w)
	AM_RANGE(0x8, 0x8) AM_WRITE(incxy_h_w)
	AM_RANGE(0x9, 0x9) AM_WRITE(incxy_l_w)
	AM_RANGE(0xa, 0xa) AM_WRITE(incyy_h_w)
	AM_RANGE(0xb, 0xb) AM_WRITE(incyy_l_w)
	AM_RANGE(0xc, 0xc) AM_WRITE(rom_base_h_w)
	AM_RANGE(0xd, 0xd) AM_WRITE(rom_base_l_w)
	AM_RANGE(0xe, 0xe) AM_WRITE(mode_w)
ADDRESS_MAP_END


k051316_device::k051316_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, K051316, tag, owner, clock),
	  flow_render::interface(mconfig, *this),
	  video_latency::interface(mconfig, *this, 0),
	  m_rom(*this, DEVICE_SELF),
	  m_ram(*this, DEVICE_SELF)
{
	m_wrap = true;
	m_x_offset = 0;
	m_y_offset = 0;
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void k051316_device::device_start()
{
	if(m_ram_based) {
		if(!m_ram)
			fatalerror("k051316 %s: shared ram not found\n", tag());
	} else {
		if(!m_rom)
			fatalerror("k051316 %s: rom region not found\n", tag());
	}

	save_item(NAME(m_tile_ram));
	save_item(NAME(m_start_x));
	save_item(NAME(m_start_y));
	save_item(NAME(m_incxx));
	save_item(NAME(m_incxy));
	save_item(NAME(m_incyx));
	save_item(NAME(m_incyy));
	save_item(NAME(m_rom_base));
	save_item(NAME(m_mode));
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void k051316_device::device_reset()
{
	std::fill(m_tile_ram.begin(), m_tile_ram.end(), 0);
	m_start_x = 0;
	m_start_y = 0;
	m_incxx = 0;
	m_incxy = 0;
	m_incyx = 0;
	m_incyy = 0;
	m_rom_base = 0;
	m_mode = 0;
}

void k051316_device::set_info(int tile_bpp, bool ram_based, std::function<void (u32 address, u32 &code, u16 &color)> mapper)
{
	m_tile_bpp = tile_bpp;
	m_ram_based = ram_based;
	m_mapper = mapper;
}

WRITE8_MEMBER(k051316_device::start_x_h_w)
{
	m_start_x = (m_start_x & 0xff) | (data << 8);
	logerror("start_x %04x (%06x)\n", m_start_x, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::start_x_l_w)
{
	m_start_x = (m_start_x & 0xff00) | data;
	logerror("start_x %04x (%06x)\n", m_start_x, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::start_y_h_w)
{
	m_start_y = (m_start_y & 0xff) | (data << 8);
	logerror("start_y %04x (%06x)\n", m_start_y, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::start_y_l_w)
{
	m_start_y = (m_start_y & 0xff00) | data;
	logerror("start_y %04x (%06x)\n", m_start_y, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::incxx_h_w)
{
	m_incxx = (m_incxx & 0xff) | (data << 8);
	logerror("incxx %04x (%06x)\n", m_incxx, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::incxx_l_w)
{
	m_incxx = (m_incxx & 0xff00) | data;
	logerror("incxx %04x (%06x)\n", m_incxx, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::incxy_h_w)
{
	m_incxy = (m_incxy & 0xff) | (data << 8);
	logerror("incxy %04x (%06x)\n", m_incxy, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::incxy_l_w)
{
	m_incxy = (m_incxy & 0xff00) | data;
	logerror("incxy %04x (%06x)\n", m_incxy, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::incyx_h_w)
{
	m_incyx = (m_incyx & 0xff) | (data << 8);
	logerror("incyx %04x (%06x)\n", m_incyx, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::incyx_l_w)
{
	m_incyx = (m_incyx & 0xff00) | data;
	logerror("incyx %04x (%06x)\n", m_incyx, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::incyy_h_w)
{
	m_incyy = (m_incyy & 0xff) | (data << 8);
	logerror("incyy %04x (%06x)\n", m_incyy, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::incyy_l_w)
{
	m_incyy = (m_incyy & 0xff00) | data;
	logerror("incyy %04x (%06x)\n", m_incyy, space.device().safe_pc());
}

WRITE8_MEMBER(k051316_device::rom_base_h_w)
{
	m_rom_base = (m_rom_base & 0xff) | (data << 8);
}

WRITE8_MEMBER(k051316_device::rom_base_l_w)
{
	m_rom_base = (m_rom_base & 0xff00) | data;
}

WRITE8_MEMBER(k051316_device::mode_w)
{
	if(m_mode != data)
		logerror("mode %02x\n", data);
	m_mode = data;
}

READ8_MEMBER (k051316_device::vram_r)
{
	if(offset & 0x400)
		return m_tile_ram[offset & 0x3ff] >> 8;
	else
		return m_tile_ram[offset];
}
 
WRITE8_MEMBER(k051316_device::vram_w)
{
	if(offset & 0x400)
		m_tile_ram[offset & 0x3ff] = (m_tile_ram[offset & 0x3ff] & 0xff) | data << 8;
	else
		m_tile_ram[offset] = (m_tile_ram[offset] & 0xff00) | data;
} 

void k051316_device::flow_render_register_renderers()
{
	m_renderer = flow_render_create_renderer([this](const rectangle &cliprect) { render(cliprect); });
	m_renderer_output = m_renderer->create_output_sb_u16();
}


READ8_MEMBER( k051316_device::rom_r )
{
	assert(!m_ram_based);

	if(!(m_mode & 1)) {
		u32 adr = (m_rom_base << 11) | offset;
		if(m_tile_bpp == 4)
			return m_rom[adr >> 1];
		else
			return m_rom[adr];
	} else
		return 0;
}

// some games (ajax, rollerg, ultraman, etc.) have external logic that can enable or disable wraparound dynamically
void k051316_device::set_wrap(bool wrap)
{
	m_wrap = wrap;
}

void k051316_device::ksnotifier_w(int clk, int hv, int hfp, int hs, int hbp, int vv, int vfp, int vs, int vbp)
{
	m_y_offset = 0;
	m_x_offset = -16 + hbp - video_latency_get();
}

void k051316_device::render(const rectangle &cliprect)
{
	u32 cx = cliprect.min_x + m_x_offset;
	u32 cy = cliprect.min_y + m_y_offset;

	s16 start_x, start_y;
	s16 incxx, incyx, incxy, incyy;

	start_x = m_start_x;
	start_y = m_start_y;
	incxx = m_incxx;
	incyx = m_incyx;
	incxy = m_incxy;
	incyy = m_incyy;

	u32 base_x = (start_x << 8) + incxx * cx + incyx * cy;
	u32 base_y = (start_y << 8) + incxy * cx + incyy * cy;

	bitmap_ind16 &bcolor = m_renderer_output->bitmap();

	std::function<u32 (u32 adr)> rom_lookup = [](u32 adr) { return 0; };

	if(m_ram_based) {
		const u16 *chars = m_ram;
		u32 cmask = m_ram.mask();
		if(m_tile_bpp == 4)
			rom_lookup = [chars, cmask](u32 adr) { u16 pix = chars[(adr >> 2) & cmask]; return (pix >> ((~adr & 3) << 2)) & 0xf; };

	} else {
		const u8 *chars = m_rom;
		u32 cmask = m_rom.mask();
		switch(m_tile_bpp) {
		case 4: rom_lookup = [chars, cmask](u32 adr) { u8 pix = chars[(adr >> 1) & cmask]; return adr & 1 ? pix & 0xf : pix >> 4; }; break;
		case 7: rom_lookup = [chars, cmask](u32 adr) { return chars[adr & cmask] & 0x7f; }; break;
		case 8: rom_lookup = [chars, cmask](u32 adr) { return chars[adr & cmask]; }; break;
		}
	}

	for(int y = cliprect.min_y; y <= cliprect.max_y; y++) {
		u32 pos_x = base_x;
		u32 pos_y = base_y;
		u16 *pc = &bcolor.pix16(y, cliprect.min_x);
		for(int x = cliprect.min_x; x <= cliprect.max_x; x++) {
			bool outside = !m_wrap && ((pos_x & 0xf00000) || (pos_y & 0xf00000));

			if(!outside) {
				u32 tile_index = ((pos_y & 0x0f8000) >> 10) | ((pos_x & 0x0f8000) >> 15);
				u32 pix_coord = ((pos_y & 0x007800) >> 7) | ((pos_x & 0x007800) >> 11);
				u32 adr = (m_tile_ram[tile_index] << 8) | pix_coord;

				// Probably conditional of the bits in m_mode, see chqflag that is know to use flipy on roz 2
				if(adr & 0x400000)
					adr ^= 0x00000f;
				if(adr & 0x800000)
					adr ^= 0x0000f0;

				u32 code;
				u16 color;
				m_mapper(adr, code, color);
				*pc++ = color | rom_lookup(code);

			} else
				*pc++ = 0;

			pos_x += incxx;
			pos_y += incxy;
		}
		base_x += incyx;
		base_y += incyy;
	}
	
}

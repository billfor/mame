// license:BSD-3-Clause
// copyright-holders:Fabio Priuli,Acho A. Tang, R. Belmont
#ifndef MAME_VIDEO_K051316_H
#define MAME_VIDEO_K051316_H

#pragma once

#include "difr.h"
#include "vlatency.h"

#define MCFG_K051316_ADD(_tag, _tile_bpp, _ram_based, _mapper)	\
	MCFG_DEVICE_ADD(_tag, K051316, 0) \
	downcast<k051316_device *>(device)->set_info(_tile_bpp, _ram_based, _mapper);

#define MCFG_K051316_WRAP(_wrap) \
	downcast<k051316_device *>(device)->set_wrap(_wrap);


class k051316_device : public device_t, public flow_render::interface, public video_latency::interface
{
public:
	k051316_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	/*
	The callback is passed:
	- code (range 00-FF, contents of the first tilemap RAM byte)
	- color (range 00-FF, contents of the first tilemap RAM byte). Note that bit 6
	  seems to be hardcoded as flip X.
	The callback must put:
	- in code the resulting tile number
	- in color the resulting color index
	- if necessary, put flags for the TileMap code in the tile_info
	  structure (e.g. TILE_FLIPX)
	*/

	DECLARE_ADDRESS_MAP(map, 8);

	DECLARE_READ8_MEMBER (vram_r);
	DECLARE_WRITE8_MEMBER(vram_w);
	DECLARE_READ8_MEMBER (rom_r);

	void set_wrap(bool status);
	void set_info(int tile_bpp, bool ram_based, std::function<void (u32 address, u32 &code, u16 &color)> mapper);

	void ksnotifier_w(int clk, int hv, int hfp, int hs, int hbp, int vv, int vfp, int vs, int vbp);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	virtual void flow_render_register_renderers() override;

private:
	optional_region_ptr<u8> m_rom;
	optional_shared_ptr<u16> m_ram;

	std::function<void (u32 address, u32 &code, u16 &color)> m_mapper;

	int m_tile_bpp;
	bool m_wrap, m_ram_based;

	std::array<u16, 32*32> m_tile_ram;

	flow_render::renderer *m_renderer;
	flow_render::output_sb_u16 *m_renderer_output;

	int m_x_offset, m_y_offset;

	s16 m_start_x, m_start_y, m_incxx, m_incxy, m_incyx, m_incyy, m_rom_base;
	u8 m_mode;

	DECLARE_WRITE8_MEMBER(start_x_h_w);
	DECLARE_WRITE8_MEMBER(start_x_l_w);
	DECLARE_WRITE8_MEMBER(start_y_h_w);
	DECLARE_WRITE8_MEMBER(start_y_l_w);
	DECLARE_WRITE8_MEMBER(incxx_h_w);
	DECLARE_WRITE8_MEMBER(incxx_l_w);
	DECLARE_WRITE8_MEMBER(incxy_h_w);
	DECLARE_WRITE8_MEMBER(incxy_l_w);
	DECLARE_WRITE8_MEMBER(incyx_h_w);
	DECLARE_WRITE8_MEMBER(incyx_l_w);
	DECLARE_WRITE8_MEMBER(incyy_h_w);
	DECLARE_WRITE8_MEMBER(incyy_l_w);
	DECLARE_WRITE8_MEMBER(rom_base_h_w);
	DECLARE_WRITE8_MEMBER(rom_base_l_w);
	DECLARE_WRITE8_MEMBER(mode_w);

	void render(const rectangle &cliprect);
};

DECLARE_DEVICE_TYPE(K051316, k051316_device)

#endif // MAME_VIDEO_K051316_H

// license:BSD-3-Clause
// copyright-holders:David Haywood, Olivier Galibert
#ifndef MAME_VIDEO_K054156_K054157_K056832_H
#define MAME_VIDEO_K054156_K054157_K056832_H

#pragma once

#include "ksnotifier.h"
#include "difr.h"
#include "vlatency.h"

#define MCFG_K054156_054157_ADD(_tag, _dotclock, _sizex, _sizey, _vramwidth) \
	MCFG_DEVICE_ADD(_tag, K054156_054157, _dotclock)								\
	downcast<k054156_054157_device *>(device)->set_info(_sizex, _sizey, _vramwidth);

#define MCFG_K054156_054157_INT1_CB(_devcb) \
	devcb = &k054156_054157_device::set_int1_cb(*device, DEVCB_##_devcb);

#define MCFG_K054156_054157_INT2_CB(_devcb) \
	devcb = &k054156_054157_device::set_int2_cb(*device, DEVCB_##_devcb);

#define MCFG_K054156_054157_INT3_CB(_devcb) \
	devcb = &k054156_054157_device::set_int3_cb(*device, DEVCB_##_devcb);

#define MCFG_K054156_054157_VBLANK_CB(_devcb) \
	devcb = &k054156_054157_device::set_vblank_cb(*device, DEVCB_##_devcb);

#define MCFG_K054156_054157_VSYNC_CB(_devcb) \
	devcb = &k054156_054157_device::set_vsync_cb(*device, DEVCB_##_devcb);

#define MCFG_K054156_054157_KSNOTIFIER_CB(_cb) \
	ksnotifier = &k054156_054157_device::set_ksnotifier_cb(*device, _cb);

#define MCFG_K054156_054157_5BPP_ADD(_tag, _dotclock,_sizex, _sizey, _vramwidth) \
	MCFG_DEVICE_ADD(_tag, K054156_054157, _dotclock)								\
	downcast<k054156_054157_device *>(device)->set_5bpp();                  \
	downcast<k054156_054157_device *>(device)->set_info(_sizex, _sizey, _vramwidth);

#define MCFG_K054156_DUAL_054157_ADD(_tag, _dotclock,_sizex, _sizey, _vramwidth) \
	MCFG_DEVICE_ADD(_tag, K054156_054157, _dotclock)								     \
	downcast<k054156_054157_device *>(device)->set_dual();                       \
	downcast<k054156_054157_device *>(device)->set_info(_sizex, _sizey, _vramwidth);

#define MCFG_K054156_056832_ADD(_tag, _dotclock,_sizex, _sizey, _vramwidth) \
	MCFG_DEVICE_ADD(_tag, K054156_056832, _dotclock)								  \
	downcast<k054156_056832_device *>(device)->set_info(_sizex, _sizey, _vramwidth);

#define MCFG_K058143_056832_ADD(_tag, _dotclock,_sizex, _sizey, _vramwidth) \
	MCFG_DEVICE_ADD(_tag, K058143_056832, _dotclock)								\
	downcast<k058143_056832_device *>(device)->set_info(_sizex, _sizey, _vramwidth);

#define MCFG_K054156_056832_DISABLE_VRC2() \
	downcast<k054156_056832_device *>(device)->set_disable_vrc2(true);

#define MCFG_K058143_056832_DISABLE_VRC2() \
	downcast<k054156_056832_device *>(device)->set_disable_vrc2(true);

#define MCFG_K054156_056832_SET_COLOR_BITS_ROTATION(_on) \
	downcast<k054156_056832_device *>(device)->set_color_bits_rotation(_on);



class k054156_056832_device : public device_t, public device_gfx_interface, public device_video_interface, public flow_render::interface, public video_latency::interface
{
public:
	k054156_056832_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);
	k054156_056832_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock);

	template<class Object> static devcb_base &set_int1_cb(device_t &device, Object &&object) { return downcast<k054156_056832_device &>(device).m_int1_cb.set_callback(std::forward<Object>(object)); }
	template<class Object> static devcb_base &set_int2_cb(device_t &device, Object &&object) { return downcast<k054156_056832_device &>(device).m_int2_cb.set_callback(std::forward<Object>(object)); }
	template<class Object> static devcb_base &set_int3_cb(device_t &device, Object &&object) { return downcast<k054156_056832_device &>(device).m_int3_cb.set_callback(std::forward<Object>(object)); }
	template<class Object> static devcb_base &set_vblank_cb(device_t &device, Object &&object) { return downcast<k054156_056832_device &>(device).m_vblank_cb.set_callback(std::forward<Object>(object)); }
	template<class Object> static devcb_base &set_vsync_cb(device_t &device, Object &&object) { return downcast<k054156_056832_device &>(device).m_vsync_cb.set_callback(std::forward<Object>(object)); }
	template<class Object> static ksnotifier_t &set_ksnotifier_cb(device_t &device, Object &&object) { return downcast<k054156_056832_device &>(device).m_ksnotifier_cb.set_callback(std::forward<Object>(object)); }

	void set_info(int _sizex, int _sizey, int _vramwidth);
	void set_disable_vrc2(bool disable);
	void set_color_bits_rotation(bool rotate);

	// For pirate ship.  They used external banking somehow.  Need to
	// be sure how (if) it actually impact tile codes or only rom
	// readback as is currently implemented.
	void set_banking(u32 readback_bank);

	DECLARE_ADDRESS_MAP(vacset, 16);
	DECLARE_ADDRESS_MAP(vacset8, 8);
	virtual DECLARE_ADDRESS_MAP(vsccs, 16);
	virtual DECLARE_ADDRESS_MAP(vsccs8, 8);

	DECLARE_WRITE_LINE_MEMBER(vsync_w);

	DECLARE_WRITE16_MEMBER(reg1_w);
	DECLARE_WRITE16_MEMBER(reg2_w);
	DECLARE_WRITE16_MEMBER(reg3_w);
	DECLARE_WRITE16_MEMBER(reg4_w);
	DECLARE_WRITE16_MEMBER(reg5_w);
	DECLARE_WRITE16_MEMBER(rzs_w);
	DECLARE_WRITE16_MEMBER(ars_w);
	DECLARE_WRITE16_MEMBER(bv_w);
	DECLARE_WRITE16_MEMBER(bh_w);
	DECLARE_WRITE16_MEMBER(mv_w);
	DECLARE_WRITE16_MEMBER(mh_w);
	DECLARE_WRITE16_MEMBER(mpz_w);
	DECLARE_WRITE16_MEMBER(mpa_w);
	DECLARE_WRITE16_MEMBER(cadlm_w);
	DECLARE_WRITE16_MEMBER(cadh_w);
	DECLARE_WRITE16_MEMBER(vrc_w);
	DECLARE_WRITE16_MEMBER(offh_w);
	DECLARE_WRITE16_MEMBER(offv_w);

	DECLARE_WRITE8_MEMBER (reg1_8w);
	DECLARE_WRITE8_MEMBER (reg2_8w);
	DECLARE_WRITE8_MEMBER (reg3_8w);
	DECLARE_WRITE8_MEMBER (reg4_8w);
	DECLARE_WRITE8_MEMBER (reg5_8w);
	DECLARE_WRITE8_MEMBER (rzs_8w);
	DECLARE_WRITE8_MEMBER (ars_8w);
	DECLARE_WRITE8_MEMBER (bv_8w);
	DECLARE_WRITE8_MEMBER (bh_8w);
	DECLARE_WRITE8_MEMBER (mv_8w);
	DECLARE_WRITE8_MEMBER (mh_8w);
	DECLARE_WRITE8_MEMBER (mpz_8w);
	DECLARE_WRITE8_MEMBER (mpa_8w);
	DECLARE_WRITE8_MEMBER (cadlm_8w);
	DECLARE_WRITE8_MEMBER (cadh_8w);
	DECLARE_WRITE8_MEMBER (vrc_8w);
	DECLARE_WRITE8_MEMBER (offh_8w);
	DECLARE_WRITE8_MEMBER (offv_8w);

	DECLARE_WRITE16_MEMBER(vrc2_w);
	DECLARE_WRITE8_MEMBER (vrc2_8w);

	DECLARE_WRITE8_MEMBER (reg1b_w);
	DECLARE_WRITE8_MEMBER (reg2b_w);
	DECLARE_WRITE8_MEMBER (reg3b_w);
	DECLARE_WRITE8_MEMBER (reg4b_w);

	DECLARE_READ8_MEMBER  (vram8_r);
	DECLARE_WRITE8_MEMBER (vram8_w);
	DECLARE_READ16_MEMBER (vram16_r);
	DECLARE_WRITE16_MEMBER(vram16_w);
	DECLARE_READ32_MEMBER (vram32_r);
	DECLARE_WRITE32_MEMBER(vram32_w);

	DECLARE_READ8_MEMBER  (rom8_r);
	DECLARE_READ16_MEMBER (rom16_r);
	DECLARE_READ32_MEMBER (rom32_r);

	void ksnotifier_w(int clk, int hv, int hfp, int hs, int hbp, int vv, int vfp, int vs, int vbp);

protected:
	static const u16 k056832_perlayer[4];
	static const u16 k054157_perlayer[4];

	const u16 *m_global_perlayer;

	bool m_is_054157;
	bool m_is_5bpp;
	bool m_is_dual;
	bool m_disable_vrc2;
	bool m_color_bits_rotation;

	// device-level overrides
	void device_start() override;
	void device_post_load() override;

	virtual void flow_render_register_renderers() override;

private:
	enum class vram_access {
		l8w16  = 0*2+0,
		l8w24  = 0*2+1,
		l16w16 = 1*2+0,
		l16w24 = 1*2+1,
		l32w16 = 2*2+0,
		l32w24 = 2*2+1
	};

	devcb_write_line m_int1_cb, m_int2_cb, m_int3_cb, m_vblank_cb, m_vsync_cb;
	ksnotifier_t m_ksnotifier_cb;

	required_memory_region m_region;

	std::array<flow_render::renderer *, 4> m_renderer;
	std::array<flow_render::output_sb_u16 *, 4> m_renderer_output;

	int m_sizex, m_sizey, m_vramwidth;

	u16 m_global_offx, m_global_offy;
	u32 m_readback_bank;

	u32 m_x[4], m_y[4], m_sx[4], m_sy[4];
	u16 m_mv[4], m_mh[4];
	u16 m_cadlm, m_vrc, m_offh, m_offv;
	u32 m_cpu_cur_x, m_cpu_cur_y;
	u8 m_vrc2[8];
	u8 m_bv[4], m_bh[4];
	u8 m_reg1h, m_reg1l, m_reg2, m_reg3h, m_reg3l, m_reg4, m_reg5;
	u8 m_rzs, m_ars, m_mpz, m_mpa, m_cadh;
	u8 m_reg1b, m_reg2b, m_reg3b, m_reg4b;

	u8 m_irq_state;

	std::vector<u32> m_videoram;
	u32 *m_page_pointers[8][8];
	u32 *m_tilemap_page[4][8][8];
	u32 *m_cur_cpu_page;
	u32 *m_cur_linescroll_page;
	void (*m_info_to_color[4])(u32 info, u32 &color, int &flipx, int &flipy);

	vram_access m_cur_vram_access;

	int m_cur_a0;

	void screen_vblank(screen_device &src, bool state);
	void select_cpu_page();
	void select_linescroll_page();
	void select_vram_access();
	void setup_tilemap(int layer);
	void decode_character_roms();
	void convert_chunky_planar();
	void convert_planar_chunky();
	template<bool gflipx> u32 screen_to_tile_x(s32 x,  u32 delta);
	template<bool gflipx> s32 tile_to_screen_x(u32 tx, u32 delta);
	template<bool gflipy> u32 screen_to_tile_y(s32 y,  u32 delta);
	template<bool gflipy> s32 tile_to_screen_y(u32 ty, u32 delta);

	template<bool gflipx, bool gflipy> void draw_page_512x1(bitmap_ind16 *bitmap, int layer, const rectangle &cliprect, const u32 *page, gfx_element *g, u32 min_x, u32 max_x, u32 min_y, u32 max_y, s32 basex, s32 basey);
	template<bool gflipx, bool gflipy> void draw_page_8x8(bitmap_ind16 *bitmap, int layer, const rectangle &cliprect, const u32 *page, gfx_element *g, u32 min_x, u32 max_x, u32 min_y, u32 max_y, s32 basex, s32 basey);
	template<bool gflipx, bool gflipy> void draw_line_block(bitmap_ind16 *bitmap, int layer, const rectangle &cliprect, u32 deltay, u32 deltax);

	void configure_screen();

	void render(int layer, const rectangle &cliprect);
};

class k054156_054157_device : public k054156_056832_device
{
public:
	k054156_054157_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	DECLARE_ADDRESS_MAP(vsccs, 16) override;
	DECLARE_ADDRESS_MAP(vsccs8, 8) override;

	void set_5bpp();
	void set_dual();
};

class k058143_056832_device : public k054156_056832_device
{
public:
	k058143_056832_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	DECLARE_READ32_MEMBER (lvram16_r);
	DECLARE_WRITE32_MEMBER(lvram16_w);
	DECLARE_READ32_MEMBER (lvram32_r);
	DECLARE_WRITE32_MEMBER(lvram32_w);
};

DECLARE_DEVICE_TYPE(K054156_054157, k054156_054157_device)
DECLARE_DEVICE_TYPE(K054156_056832, k054156_056832_device)
DECLARE_DEVICE_TYPE(K058143_056832, k058143_056832_device)

#endif // MAME_VIDEO_K054156_K054157_K056832_H


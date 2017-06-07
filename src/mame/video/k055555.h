// license:BSD-3-Clause
// copyright-holders:David Haywood, Olivier Galibert
/* */
#ifndef MAME_MACHINE_K055555_H
#define MAME_MACHINE_K055555_H

#pragma once

#include "difr.h"
#include "vlatency.h"

#define MCFG_K055555_ADD(_tag) \
	MCFG_DEVICE_ADD(_tag, K055555, 0)

class k055555_device : public device_t, public flow_render::interface, public video_latency::interface
{
public:
	k055555_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
	~k055555_device();

	DECLARE_WRITE8_MEMBER(bgc_cblk_w);
	DECLARE_WRITE8_MEMBER(bgc_set_w);
	DECLARE_WRITE8_MEMBER(colset0_w);
	DECLARE_WRITE8_MEMBER(colset1_w);
	DECLARE_WRITE8_MEMBER(colset2_w);
	DECLARE_WRITE8_MEMBER(colset3_w);
	DECLARE_WRITE8_MEMBER(colchg_on_w);
	DECLARE_WRITE8_MEMBER(a_pri0_w);
	DECLARE_WRITE8_MEMBER(a_pri1_w);
	DECLARE_WRITE8_MEMBER(a_colpri_w);
	DECLARE_WRITE8_MEMBER(b_pri0_w);
	DECLARE_WRITE8_MEMBER(b_pri1_w);
	DECLARE_WRITE8_MEMBER(b_colpri_w);
	DECLARE_WRITE8_MEMBER(c_pri_w);
	DECLARE_WRITE8_MEMBER(d_pri_w);
	DECLARE_WRITE8_MEMBER(o_pri_w);
	DECLARE_WRITE8_MEMBER(s1_pri_w);
	DECLARE_WRITE8_MEMBER(s2_pri_w);
	DECLARE_WRITE8_MEMBER(s3_pri_w);
	DECLARE_WRITE8_MEMBER(o_inpri_on_w);
	DECLARE_WRITE8_MEMBER(s1_inpri_on_w);
	DECLARE_WRITE8_MEMBER(s2_inpri_on_w);
	DECLARE_WRITE8_MEMBER(s3_inpri_on_w);
	DECLARE_WRITE8_MEMBER(a_cblk_w);
	DECLARE_WRITE8_MEMBER(b_cblk_w);
	DECLARE_WRITE8_MEMBER(c_cblk_w);
	DECLARE_WRITE8_MEMBER(d_cblk_w);
	DECLARE_WRITE8_MEMBER(o_cblk_w);
	DECLARE_WRITE8_MEMBER(s1_cblk_w);
	DECLARE_WRITE8_MEMBER(s2_cblk_w);
	DECLARE_WRITE8_MEMBER(s3_cblk_w);
	DECLARE_WRITE8_MEMBER(s2_cblk_on_w);
	DECLARE_WRITE8_MEMBER(s3_cblk_on_w);
	DECLARE_WRITE8_MEMBER(v_inmix_w);
	DECLARE_WRITE8_MEMBER(v_inmix_on_w);
	DECLARE_WRITE8_MEMBER(os_inmix_w);
	DECLARE_WRITE8_MEMBER(os_inmix_on_w);
	DECLARE_WRITE8_MEMBER(shd_pri1_w);
	DECLARE_WRITE8_MEMBER(shd_pri2_w);
	DECLARE_WRITE8_MEMBER(shd_pri3_w);
	DECLARE_WRITE8_MEMBER(shd_on_w);
	DECLARE_WRITE8_MEMBER(shd_pri_sel_w);
	DECLARE_WRITE8_MEMBER(v_inbri_w);
	DECLARE_WRITE8_MEMBER(os_inbri_w);
	DECLARE_WRITE8_MEMBER(os_inbri_on_w);
	DECLARE_WRITE8_MEMBER(disp_w);

	DECLARE_ADDRESS_MAP(map, 8);

protected:
	// device-level overrides
	void device_start() override;
	void device_reset() override;
	void device_post_load() override;

	virtual void flow_render_register_renderers() override;

private:
	flow_render::renderer *m_renderer;
	std::array<flow_render::input_sb_u16 *, 4> m_renderer_input_simple_color, m_renderer_input_complex_color, m_renderer_input_complex_attr;
	std::array<flow_render::output_sb_u16 *, 2> m_renderer_output_color, m_renderer_output_attr;

	uint8_t m_shadow_value[4][256];

	uint16_t m_color_mask[8];
	uint8_t m_colset[4];
	uint8_t m_cblk[8];
	uint8_t m_cblk_on[2];
	uint8_t m_pri[8+2];
	uint8_t m_inpri_on[4];
	uint8_t m_colpri[2];
	uint8_t m_shd_pri[3];
	uint8_t m_shd_on, m_shd_pri_sel;
	uint8_t m_bgc_cblk, m_bgc_set, m_colchg_on;
	uint8_t m_v_inmix, m_v_inmix_on, m_os_inmix, m_os_inmix_on;
	uint8_t m_v_inbri, m_os_inbri, m_os_inbri_on;
	uint8_t m_disp;

	void update_shadow_value_array(int entry);
	void compute_color_mask(int i);

	void render(const rectangle &cliprect);
};

DECLARE_DEVICE_TYPE(K055555, k055555_device)


#endif // MAME_MACHINE_K055555_H

// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
#pragma once
#ifndef MAME_VIDEO_KVIDEODAC_H
#define MAME_VIDEO_KVIDEODAC_H

#include "difr.h"

#define MCFG_KVIDEODAC_ADD(_tag, _palette_tag, _shadow_mask, _shadow_level, _highlight_mask, _highlight_level) \
	MCFG_DEVICE_ADD(_tag, KVIDEODAC, 0)		 \
	downcast<kvideodac_device *>(device)->set_info(_palette_tag, _shadow_mask, _shadow_level, _highlight_mask, _highlight_level);

#define MCFG_KVIDEODAC_SKIPPED_BITS(_count) \
	downcast<kvideodac_device *>(device)->set_skipped_bits(_count);

class kvideodac_device : public device_t, public flow_render::interface
{
public:
	kvideodac_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);
	~kvideodac_device();

	void set_info(const char *tag, u16 shadow_mask, double shadow_level, u16 highlight_mask, double highlight_level);

	void set_force_shadow(bool force);
	void set_force_highlight(bool force);
	void set_shadow_level(double level);
	void set_highlight_level(double level);
	void set_skipped_bits(int count) { m_skipped_bits = count; }

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void flow_render_register_renderers() override;

private:
	const char *m_palette_tag;
	palette_device *m_palette;

	u8 m_shadow_table[256], m_highlight_table[256], m_shadow_highlight_table[256];
	u16 m_shadow_mask, m_highlight_mask;
	double m_shadow_level, m_highlight_level;
	bool m_force_shadow, m_force_highlight;
	int m_skipped_bits;

	flow_render::renderer *m_renderer;
	flow_render::input_sb_u16 *m_renderer_input_color, *m_renderer_input_attr;
	flow_render::output_sb_rgb *m_renderer_output;

	static void generate_table(u8 *table, double level);

	void render(const rectangle &cliprect);
};

DECLARE_DEVICE_TYPE(KVIDEODAC, kvideodac_device)

#endif

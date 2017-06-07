// license:BSD-3-Clause
// copyright-holders:Olivier Galibert

#include "emu.h"
#include "kvideodac.h"

DEFINE_DEVICE_TYPE(KVIDEODAC, kvideodac_device, "kvideodac", "Konami Video DAC")

kvideodac_device::kvideodac_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, KVIDEODAC, tag, owner, clock),
	  flow_render::interface(mconfig, *this)
{
	m_palette_tag = nullptr;
	m_palette = nullptr;
	m_skipped_bits = 0;
}

kvideodac_device::~kvideodac_device()
{
}

void kvideodac_device::set_info(const char *tag, u16 shadow_mask, double shadow_level, u16 highlight_mask, double highlight_level)
{
	m_palette_tag = tag;
	m_shadow_mask = shadow_mask;
	m_shadow_level = shadow_level;
	m_highlight_mask = highlight_mask;
	m_highlight_level = highlight_level;
}

void kvideodac_device::device_start()
{
	m_palette = siblingdevice<palette_device>(m_palette_tag);

	save_item(NAME(m_shadow_level));
	save_item(NAME(m_highlight_level));
	save_item(NAME(m_force_shadow));
	save_item(NAME(m_force_highlight));

	generate_table(m_shadow_table, m_shadow_level);
	generate_table(m_highlight_table, m_highlight_level);
	generate_table(m_shadow_highlight_table, m_shadow_level*m_highlight_level);

	m_force_shadow = false;
	m_force_highlight = false;
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void kvideodac_device::device_reset()
{
}

void kvideodac_device::set_force_shadow(bool force)
{
	m_force_shadow = force;
}

void kvideodac_device::set_force_highlight(bool force)
{
	m_force_highlight = force;
}

void kvideodac_device::set_shadow_level(double level)
{
	m_shadow_level = level;
	generate_table(m_shadow_table, m_shadow_level);
	generate_table(m_shadow_highlight_table, m_shadow_level*m_highlight_level);
}

void kvideodac_device::set_highlight_level(double level)
{
	m_highlight_level = level;
	generate_table(m_highlight_table, m_highlight_level);
	generate_table(m_shadow_highlight_table, m_shadow_level*m_highlight_level);
}

void kvideodac_device::generate_table(u8 *dest, double level)
{
	for(int i=0; i<256; i++) {
		int v = int(i*level+0.5);
		if(v > 255)
			v = 255;
		dest[i] = v;
	}
}

void kvideodac_device::flow_render_register_renderers()
{
	m_renderer = flow_render_create_renderer([this](const rectangle &cliprect){ render(cliprect); });
	m_renderer_input_color = m_renderer->create_input_sb_u16("color");
	m_renderer_input_attr  = m_renderer->create_input_sb_u16("attr");
	m_renderer_output = m_renderer->create_output_sb_rgb();
}


void kvideodac_device::render(const rectangle &cliprect)
{
	const u32 *pens = m_palette->pens();
	u16 mask = m_palette->entries() - 1;

	std::function<u16 (u16)> line_mapper = [mask](u16 col) -> u16 { return col & mask; };
	if(m_skipped_bits == 2)
		line_mapper = [mask](u16 col) -> u16 { return (((col & 0xfc00) >> 2) | (col & 0x00ff)) & mask; };

	for(int y = cliprect.min_y; y <= cliprect.max_y; y++) {
		const u16 *bcolor = &m_renderer_input_color->bitmap().pix16(y, cliprect.min_x);
		const u16 *battr  = &m_renderer_input_attr->bitmap().pix16(y, cliprect.min_x);
		u32 *dest = &m_renderer_output->bitmap().pix32(y, cliprect.min_x);
		for(int x = cliprect.min_x; x <= cliprect.max_x; x++) {
			u16 bc = line_mapper(*bcolor++);
			u16 ba = *battr++;

			u32 col = pens[bc];

			if(m_force_shadow || m_force_highlight || (ba & (m_shadow_mask|m_highlight_mask))) {
				const u8 *table;
				if(m_force_shadow || (ba & m_shadow_mask))
					if(m_force_highlight || (ba & m_highlight_mask))
						table = m_shadow_highlight_table;
					else
						table = m_shadow_table;
				else
					table = m_highlight_table;
				col = (table[(col >> 16) & 0xff] << 16) |  (table[(col >> 8) & 0xff] << 8) |  (table[col & 0xff]);
			}
			*dest++ = col;
		}
	}
}

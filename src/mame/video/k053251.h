// license:BSD-3-Clause
// copyright-holders: Olivier Galibert
#ifndef MAME_VIDEO_K053251_H
#define MAME_VIDEO_K053251_H

#include "difr.h"
#include "vlatency.h"

#define MCFG_K053251_ADD(_tag, _shadow_layer)		\
	MCFG_DEVICE_ADD(_tag, K053251, 0) \
	downcast<k053251_device *>(device)->set_shadow_layer(_shadow_layer);

class k053251_device : public device_t, public flow_render::interface, public video_latency::interface
{
public:
	k053251_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void set_shadow_layer(int layer);

	DECLARE_WRITE8_MEMBER(pri_w);
	DECLARE_WRITE8_MEMBER(sha_w);
	DECLARE_WRITE8_MEMBER(cblk_w);
	DECLARE_WRITE8_MEMBER(inpri_w);
	DECLARE_WRITE8_MEMBER(extsha_w);

	DECLARE_ADDRESS_MAP(map, 8);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void flow_render_register_renderers() override;

private:
	int m_shadow_layer;

	uint8_t m_pri[5], m_sha[2];
	uint8_t m_inpri, m_extsha;
	uint16_t m_cblk[5];

	flow_render::renderer *m_renderer[2];
	flow_render::input_sb_u16 *m_renderer_input_color[2][5], *m_renderer_input_attr[2][3];
	flow_render::output_sb_u16 *m_renderer_output_color[2], *m_renderer_output_attr[2];

	void render(int pass, const rectangle &cliprect);
};

DECLARE_DEVICE_TYPE(K053251, k053251_device)


#endif // MAME_VIDEO_K053251_H

// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    Pixclock latency measures.

***************************************************************************/

#include "emu.h"
#include "vlatency.h"

video_latency::interface::interface(const machine_config &mconfig, device_t &device, int _clocks)
	: device_interface(device, "video_latency"),
	  m_pre(0),
	  m_cur(_clocks),
	  m_post(0),
	  m_tag_next(nullptr),
	  m_next(nullptr)
{
}

video_latency::interface::~interface()
{
}

void video_latency::interface::video_latency_set_next(const char *_tag)
{
	m_tag_next = _tag;	
}

void video_latency::interface::video_latency_set_post(int _clocks)
{
	m_post = _clocks;
}

void video_latency::interface::video_latency_set_pre(int _clocks)
{
	m_pre = _clocks;
}

void video_latency::interface::video_latency_set_cur(int _clocks)
{
	m_cur = _clocks;
}

void video_latency::interface::interface_pre_start()
{
	if(m_tag_next)
		m_next = dynamic_cast<interface *>(device().siblingdevice(m_tag_next));
	else
		m_next = nullptr;
}

int video_latency::interface::video_latency_get() const
{
	return m_pre + m_post + m_cur + (m_next ? m_next->video_latency_get() : 0);
}

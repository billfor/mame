// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    Pixclock latency measures.

***************************************************************************/

#pragma once

#ifndef MAME_VIDEO_VLATENCY_H
#define MAME_VIDEO_VLATENCY_H

#define MCFG_VLATENCY_NEXT(_tag)	\
	dynamic_cast<video_latency::interface *>(device)->video_latency_set_next(_tag);

#define MCFG_VLATENCY_POST(_clocks)	\
	dynamic_cast<video_latency::interface *>(device)->video_latency_set_post(_clocks);

#define MCFG_VLATENCY_PRE(_clocks)	\
	dynamic_cast<video_latency::interface *>(device)->video_latency_set_pre(_clocks);


namespace video_latency {
	class interface : public device_interface
	{
	public:
		interface(const machine_config &mconfig, device_t &device, int _clocks);
		virtual ~interface();

		void video_latency_set_next(const char *_tag);
		void video_latency_set_post(int _clocks);
		void video_latency_set_pre(int _clocks);
		void video_latency_set_cur(int _clocks);

		int video_latency_get() const;

		virtual void interface_pre_start() override;

	private:
		int m_pre, m_cur, m_post;
		const char *m_tag_next;
		interface *m_next;
	};
}

#endif  /* MAME_VIDEO_VLATENCY_H */

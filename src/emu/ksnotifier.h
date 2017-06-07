// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    Screen timings notifier (aka Konami Screen change Notifier)

***************************************************************************/

#pragma once

#ifndef MAME_KSNOTIFY_H
#define MAME_KSNOTIFY_H

#define KSNOTIFIER(_class, _func) ksnotifier_delegate(&_class::_func, #_class "::" #_func, DEVICE_SELF, (_class *)nullptr)
#define DEVKSNOTIFIER(tag, _class, _func) ksnotifier_delegate(&_class::_func, #_class "::" #_func, tag, (_class *)nullptr)

// machine config helpers for chaining callbacks
#define MCFG_KSNOTIFIER_CHAIN(_desc) ksnotifier = &(*ksnotifier).chain_alloc().set_callback(_desc);

// pixel clock
// horizontal back porch, visible, front porch, sync (in pixel clocks)
// vertical back porch, visible, front porch, sync (in lines)

using ksnotifier_delegate = device_delegate<void (int, int, int, int, int, int, int, int, int)>;

class ksnotifier_t
{
protected:
	enum callback_type
	{
		CALLBACK_NONE,
		CALLBACK_NOTIFIER
	};

public:
	ksnotifier_t(device_t &device);

	bool isnull() const { return (m_type == CALLBACK_NONE); }

	class null_desc
	{
	public:
		null_desc() { }
	};

	ksnotifier_t &set_callback(null_desc null) { reset(CALLBACK_NONE); return *this; }
	ksnotifier_t &set_callback(ksnotifier_delegate func) { reset(CALLBACK_NOTIFIER); m_notifier = func; return *this; }
	void reset() { reset(CALLBACK_NONE); }

	ksnotifier_t &chain_alloc();

	void resolve();
	void resolve_safe();

	void operator()(int clk, int hbp, int hv, int hfp, int hs, int vbp, int vv, int vfp, int vs);

private:
	using adapter_func = void (ksnotifier_t::*)(int, int, int, int, int, int, int, int, int);

	void noop_adapter(int, int, int, int, int, int, int, int, int);
	void notifier_adapter(int, int, int, int, int, int, int, int, int);
	void unresolved_adapter(int, int, int, int, int, int, int, int, int);

	void reset(callback_type type);

	// configuration
	device_t &                    m_device;      // reference to our owning device
	callback_type                 m_type;        // type of callback registered
	const char *                  m_target_tag;  // tag of target object
	ksnotifier_delegate           m_notifier;    // copy of registered notifier
	adapter_func                  m_adapter;     // actual callback to invoke
	std::unique_ptr<ksnotifier_t> m_chain;       // next callback for chained output

};

#endif  /* MAME_KSNOTIFIER_H */

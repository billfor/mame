// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    Screen timings notifier (aka Konami Screen change Notifier)

***************************************************************************/

#include "emu.h"
#include "ksnotifier.h"

ksnotifier_t::ksnotifier_t(device_t &device)
	: m_device(device),
	  m_type(CALLBACK_NONE),
	  m_target_tag(nullptr),
	  m_adapter(&ksnotifier_t::unresolved_adapter)
{
}

void ksnotifier_t::reset(callback_type type)
{
	m_type = type;
	m_target_tag = nullptr;
	m_notifier = ksnotifier_delegate();
	m_adapter = &ksnotifier_t::unresolved_adapter;
	m_chain = nullptr;
}

ksnotifier_t &ksnotifier_t::chain_alloc()
{
	// set up the chained callback pointer
	m_chain.reset(new ksnotifier_t(m_device));
	return *m_chain;
}

void ksnotifier_t::resolve()
{
	const char *name = "unknown";
	try
	{
		switch (m_type)
		{
			case CALLBACK_NONE:
				m_adapter = &ksnotifier_t::noop_adapter;
				break;

			case CALLBACK_NOTIFIER:
				name = m_notifier.name();
				m_notifier.bind_relative_to(*m_device.owner());
				m_adapter = m_notifier.isnull() ? &ksnotifier_t::noop_adapter : &ksnotifier_t::notifier_adapter;
				break;
		}
	}
	catch (binding_type_exception &binderr)
	{
		throw emu_fatalerror("ksnotifier: Error performing a late bind of type %s to %s (name=%s)\n", binderr.m_actual_type.name(), binderr.m_target_type.name(), name);
	}

	// resolve callback chain recursively
	if (m_chain)
		m_chain->resolve();
}

void ksnotifier_t::unresolved_adapter(int, int, int, int, int, int, int, int, int)
{
	throw emu_fatalerror("Attempted to notify through an unresolved ksnotifier");
}

void ksnotifier_t::noop_adapter(int, int, int, int, int, int, int, int, int)
{
}

void ksnotifier_t::notifier_adapter(int clk, int hbp, int hv, int hfp, int hs, int vbp, int vv, int vfp, int vs)
{
	m_notifier(clk, hbp, hv, hfp, hs, vbp, vv, vfp, vs);
}

void ksnotifier_t::operator()(int clk, int hbp, int hv, int hfp, int hs, int vbp, int vv, int vfp, int vs)
{
	(this->*m_adapter)(clk, hbp, hv, hfp, hs, vbp, vv, vfp, vs);
	if (m_chain)
		(*m_chain)(clk, hbp, hv, hfp, hs, vbp, vv, vfp, vs);
}

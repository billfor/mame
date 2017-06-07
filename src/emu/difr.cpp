// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    Device flow rendering interfaces.

***************************************************************************/

#include "emu.h"
#include "difr.h"

flow_render::input::input(std::string name, renderer *rend) : m_name(name), m_renderer(rend)
{
}

flow_render::input::~input()
{
}

flow_render::input_sb_u16::input_sb_u16(std::string name, renderer *rend) : input(name, rend)
{
}

std::string flow_render::input_sb_u16::description()
{
	return "screen-sized u16 bitmap";
}

flow_render::input_sb_rgb::input_sb_rgb(std::string name, renderer *rend) : input(name, rend)
{
}

std::string flow_render::input_sb_rgb::description()
{
	return "screen-sized RGB bitmap";
}

flow_render::output::output(std::string name, renderer *rend) : m_name(name), m_renderer(rend)
{
}

flow_render::output::~output()
{
}

flow_render::output_sb_u16::output_sb_u16(std::string name, renderer *rend) : output(name, rend)
{
}

std::string flow_render::output_sb_u16::description()
{
	return "screen-sized u16 bitmap";
}

bool flow_render::output_sb_u16::is_compatible(const input *inp) const
{
	return dynamic_cast<const input_sb_u16 *>(inp);
}

flow_render::output_sb_rgb::output_sb_rgb(std::string name, renderer *rend) : output(name, rend)
{
}

bool flow_render::output_sb_rgb::is_compatible(const input *inp) const
{
	return dynamic_cast<const input_sb_rgb *>(inp);
}

std::string flow_render::output_sb_rgb::description()
{
	return "screen-sized RGB bitmap";
}

flow_render::renderer::renderer(std::function<void (const rectangle &)> render_cb, std::string name, interface *intf) : m_render_cb(render_cb), m_name(name), m_intf(intf), m_target(false)
{
}

void flow_render::renderer::set_target()
{
	m_target = true;
}

template<typename IO, typename IOL> IOL *flow_render::renderer::check_and_add(std::unordered_map<std::string, std::unique_ptr<IO>> &map,
																			  std::unique_ptr<IOL> &&io, const char *type)
{
	IOL *res = io.get();
	std::string name = io->get_name();
	if(map.find(name) != map.end())
		fatalerror("Duplicate %s %s in renderer %s of device %s\n", type, name.c_str(), get_name().c_str(), m_intf->device().tag());

	map.emplace(name, std::move(io));
	return res;
}

flow_render::input_sb_u16 *flow_render::renderer::create_input_sb_u16(std::string name)
{
	return check_and_add(m_inputs, std::make_unique<input_sb_u16>(name, this), "input");
}

flow_render::input_sb_rgb *flow_render::renderer::create_input_sb_rgb(std::string name)
{
	return check_and_add(m_inputs, std::make_unique<input_sb_rgb>(name, this), "input");
}

flow_render::output_sb_u16 *flow_render::renderer::create_output_sb_u16(std::string name)
{
	return check_and_add(m_outputs, std::make_unique<output_sb_u16>(name, this), "output");
}

flow_render::output_sb_rgb *flow_render::renderer::create_output_sb_rgb(std::string name)
{
	return check_and_add(m_outputs, std::make_unique<output_sb_rgb>(name, this), "output");
}

flow_render::input *flow_render::renderer::inp(std::string name) const
{
	auto i = m_inputs.find(name);
	if(i == m_inputs.end())
		fatalerror("Requesting non-existing input %s in renderer %s of device %s\n", name.c_str(), get_name().c_str(), m_intf->device().tag());
	return i->second.get();
}

flow_render::output *flow_render::renderer::out(std::string name) const
{
	auto i = m_outputs.find(name);
	if(i == m_outputs.end())
		fatalerror("Requesting non-existing output %s in renderer %s of device %s\n", name.c_str(), get_name().c_str(), m_intf->device().tag());
	return i->second.get();
}

std::vector<flow_render::input *> flow_render::renderer::all_inputs() const
{
	std::vector<input *> res;
	for(const auto &i : m_inputs)
		res.push_back(i.second.get());
	return res;
}

std::vector<flow_render::output *> flow_render::renderer::all_outputs() const
{
	std::vector<output *> res;
	for(const auto &i : m_outputs)
		res.push_back(i.second.get());
	return res;
}

flow_render::interface::interface(const machine_config &mconfig, device_t &device)
	: device_interface(device, "flow_render")
{
}

flow_render::interface::~interface()
{
}

flow_render::renderer *flow_render::interface::flow_render_create_renderer(std::function<void (const rectangle &)> render_cb, std::string name)
{
	if(m_renderers.find(name) != m_renderers.end())
		fatalerror("Duplicate renderer %s in device %s\n", name.c_str(), device().tag());

	return m_renderers.emplace(name, std::make_unique<renderer>(render_cb, name, this)).first->second.get();
}

const flow_render::renderer *flow_render::interface::flow_render_get_renderer(std::string name) const
{
	auto i = m_renderers.find(name);
	if(i == m_renderers.end())
		fatalerror("Requesting non-existing renderer %s of device %s\n", name.c_str(), device().tag());
	return i->second.get();
}

void flow_render::interface::flow_render_append_renderers(std::vector<renderer *> &renderers) const
{
	for(const auto &r : m_renderers)
		renderers.push_back(r.second.get());
}

void flow_render::interface::flow_render_do_render(int width, int height, const rectangle &cliprect)
{
	m_manager->do_render(width, height, cliprect);
}

flow_render::manager::manager(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, FLOW_RENDER_MANAGER, tag, owner, clock)
{
}

void flow_render::manager::device_config_complete()
{
	for(auto &fri : device_interface_iterator<flow_render::interface>(mconfig().root_device()))
		m_fri.push_back(&fri);

	for(auto fri : m_fri) {
		fri->flow_render_set_manager(this);
		fri->flow_render_register_renderers();
		fri->flow_render_append_renderers(m_rend);
	}
}

void flow_render::manager::device_start()
{
	m_setup_cb.bind_relative_to(*owner());
	m_setup_cb(this);

	std::unordered_set<renderer *> needed_renderers;
	std::unordered_set<input *> needed_inputs;
	std::unordered_set<output *> known_outputs;

	for(auto frd : m_rend)
		if(frd->is_target()) {
			needed_renderers.insert(frd);
			for(auto inp : frd->all_inputs())
				needed_inputs.insert(inp);
		}

	while(!needed_inputs.empty()) {
		std::unordered_set<output *> needed_outputs;
		for(auto inp : needed_inputs) {
			if(m_input_constants_u32.find(inp) != m_input_constants_u32.end())
				continue;
			auto i = m_input_to_output.find(inp);
			if(i == m_input_to_output.end())
				fatalerror("Nothing connected to required input %s of renderer %s in device %s\n", inp->get_name().c_str(), inp->get_renderer()->get_name().c_str(), inp->get_renderer()->get_interface()->device().tag());
			output *out = i->second;
			if(known_outputs.find(out) == known_outputs.end())
				needed_outputs.insert(out);
		}
		needed_inputs.clear();
		for(auto out : needed_outputs) {
			renderer *rend = out->get_renderer();
			needed_renderers.insert(rend);
			for(auto inp : rend->all_inputs())
				needed_inputs.insert(inp);
			for(auto out2 : rend->all_outputs())
				known_outputs.insert(out2);
		}
	}

	std::unordered_set<output *> generated_outputs;
	while(!needed_renderers.empty()) {
		bool changed = false;
		for(auto i = needed_renderers.begin(); i != needed_renderers.end();) {
			bool complete = true;
			renderer *rend = *i;
			for(auto inp : rend->all_inputs()) {
				if(m_input_constants_u32.find(inp) != m_input_constants_u32.end())
					continue;
				output *out = m_input_to_output[inp];
				if(generated_outputs.find(out) == generated_outputs.end()) {
					complete = false;
					break;
				}
			}
			if(complete) {
				m_ordered_renderers.push_back(rend);
				i = needed_renderers.erase(i);
				changed = true;
				for(auto out : rend->all_outputs())
					generated_outputs.insert(out);
			} else
				i++;
		}
		if(!changed)
			fatalerror("Couldn't topologically sort the renderers\n");
	}
	fprintf(stderr, "Rendering order:\n");
	for(auto rend : m_ordered_renderers)
		fprintf(stderr, "- %s in device %s\n", rend->get_name().c_str(), rend->get_interface()->device().tag());

	std::unordered_map<output *, unsigned int> ibitmap;
	for(auto rend : m_ordered_renderers) {
		for(auto out : rend->all_outputs()) {
			std::unique_ptr<bitmap_t> bitmap_storage;
			if(dynamic_cast<output_sb_u16 *>(out)) {
				auto b = std::make_unique<bitmap_ind16>();
				dynamic_cast<output_sb_u16 *>(out)->set_bitmap(b.get());
				bitmap_storage = std::move(b);
			} else {
				auto b = std::make_unique<bitmap_rgb32>();
				dynamic_cast<output_sb_rgb *>(out)->set_bitmap(b.get());
				bitmap_storage = std::move(b);
			}
			unsigned int id = m_intermediate_bitmaps.size();
			m_intermediate_bitmaps.emplace_back(std::make_unique<intermediate_bitmap>(std::move(bitmap_storage)));
			ibitmap[out] = id;
			m_intermediate_bitmaps[id]->out = out;
		}
	}

	for(auto rend : m_ordered_renderers) {
		for(auto inp : rend->all_inputs()) {
			auto i = m_input_to_output.find(inp);
			if(i != m_input_to_output.end()) {
				auto bitmap = m_intermediate_bitmaps[ibitmap[i->second]]->bitmap.get();
				if(dynamic_cast<input_sb_u16 *>(inp))
					dynamic_cast<input_sb_u16 *>(inp)->set_bitmap(dynamic_cast<bitmap_ind16 *>(bitmap));
				else
					dynamic_cast<input_sb_rgb *>(inp)->set_bitmap(dynamic_cast<bitmap_rgb32 *>(bitmap));
				m_intermediate_bitmaps[ibitmap[i->second]]->inp.push_back(inp);

			} else {
				std::unique_ptr<bitmap_t> bitmap_storage;
				if(dynamic_cast<input_sb_u16 *>(inp)) {
					auto b = std::make_unique<bitmap_ind16>();
					dynamic_cast<input_sb_u16 *>(inp)->set_bitmap(b.get());
					bitmap_storage = std::move(b);
				} else {
					auto b = std::make_unique<bitmap_rgb32>();
					dynamic_cast<input_sb_rgb *>(inp)->set_bitmap(b.get());
					bitmap_storage = std::move(b);
				}
				m_intermediate_bitmaps.emplace_back(std::make_unique<intermediate_bitmap>(std::move(bitmap_storage)));
				m_intermediate_bitmaps.back()->inp.push_back(inp);
				m_intermediate_bitmaps.back()->fixed_value = true;
				m_intermediate_bitmaps.back()->value = m_input_constants_u32[inp];
			}
		}
	}
}

void flow_render::manager::do_render(int width, int height, const rectangle &cliprect)
{
	if(m_intermediate_bitmaps.empty())
		return;

	if(m_intermediate_bitmaps[0]->bitmap->width() != width || m_intermediate_bitmaps[0]->bitmap->height() != height) {
		for(const auto &p : m_intermediate_bitmaps) {
			p->bitmap->resize(width, height);
			p->bitmap->fill(p->value);
		}
	}

	for(auto rend : m_ordered_renderers)
		rend->run_render(cliprect);
}

void flow_render::manager::connect(flow_render::output *out, flow_render::input *inp)
{
	auto i = m_input_to_output.find(inp);
	if(i != m_input_to_output.end())
		fatalerror("Duplicate connection to input %s of renderer %s in device %s\n", inp->get_name().c_str(), inp->get_renderer()->get_name().c_str(), inp->get_renderer()->get_interface()->device().tag());

	if(!out->is_compatible(inp))
		fatalerror("Can't connect output %s of renderer %s in device %s (%s) to input %s of renderer %s in device %s (%s)\n",
				   out->get_name().c_str(), out->get_renderer()->get_name().c_str(), out->get_renderer()->get_interface()->device().tag(), out->description().c_str(),
				   inp->get_name().c_str(), inp->get_renderer()->get_name().c_str(), inp->get_renderer()->get_interface()->device().tag(), inp->description().c_str());

	m_input_to_output[inp] = out;
}

void flow_render::manager::set_constant(flow_render::input *inp, u32 value)
{
	m_input_constants_u32[inp] = value;
}

DEFINE_DEVICE_TYPE_NS(FLOW_RENDER_MANAGER, flow_render, manager, "fr_manager", "Video Flow Rendering Manager")

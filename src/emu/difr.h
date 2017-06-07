// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    Device flow rendering interfaces.

***************************************************************************/

#pragma once

#ifndef MAME_EMU_DIFR_H
#define MAME_EMU_DIFR_H

#define MCFG_FLOW_RENDER_MANAGER_ADD(_tag)	\
	MCFG_DEVICE_ADD(_tag, FLOW_RENDER_MANAGER, 0)

#define MCFG_FLOW_RENDER_MANAGER_SETUP(_tag, _class, _method)	\
	downcast<flow_render::manager *>(device)->set_setup(flow_render::setup_delegate(&_class::_method, #_class "::" #_method, _tag, (_class *)nullptr));

namespace flow_render {
	class renderer;
	class interface;
	class manager;

	using setup_delegate = device_delegate<void (manager *)>;

	class input {
	public:
		input(std::string name, renderer *rend);
		virtual ~input();

		std::string get_name() const { return m_name; }
		renderer *get_renderer() const { return m_renderer; }

		virtual std::string description() = 0;

	private:
		std::string m_name;
		renderer *m_renderer;
	};

	class input_sb_u16 : public input {
	public:
		input_sb_u16(std::string name, renderer *rend);
		virtual std::string description() override;

		bitmap_ind16 &bitmap() const { return *m_bitmap; }
		void set_bitmap(bitmap_ind16 *bitmap) { m_bitmap = bitmap; }

	private:
		bitmap_ind16 *m_bitmap;
	};

	class input_sb_rgb : public input {
	public:
		input_sb_rgb(std::string name, renderer *rend);
		virtual std::string description() override;

		bitmap_rgb32 &bitmap() const { return *m_bitmap; }
		void set_bitmap(bitmap_rgb32 *bitmap) { m_bitmap = bitmap; }

	private:
		bitmap_rgb32 *m_bitmap;
	};

	class output {
	public:
		output(std::string name, renderer *rend);
		virtual ~output();

		std::string get_name() const { return m_name; }
		renderer *get_renderer() const { return m_renderer; }

		virtual std::string description() = 0;
		virtual bool is_compatible(const input *inp) const = 0;

	private:
		std::string m_name;
		renderer *m_renderer;
	};

	class output_sb_u16 : public output {
	public:
		output_sb_u16(std::string name, renderer *rend);
		virtual std::string description() override;
		bool is_compatible(const input *inp) const override;

		bitmap_ind16 &bitmap() const { return *m_bitmap; }
		void set_bitmap(bitmap_ind16 *bitmap)  { m_bitmap = bitmap; }

	private:
		bitmap_ind16 *m_bitmap;
	};

	class output_sb_rgb : public output {
	public:
		output_sb_rgb(std::string name, renderer *rend);
		virtual std::string description() override;
		bool is_compatible(const input *inp) const override;

		bitmap_rgb32 &bitmap() const { return *m_bitmap; }
		void set_bitmap(bitmap_rgb32 *bitmap)  { m_bitmap = bitmap; }

	private:
		bitmap_rgb32 *m_bitmap;
	};

	class uniform {
	public:
		uniform();
	};

	class renderer {
	public:
		renderer(std::function<void (const rectangle &)> render_cb, std::string name, interface *intf);

		input_sb_u16 *create_input_sb_u16(std::string name = "default");
		output_sb_u16 *create_output_sb_u16(std::string name = "default");
		input_sb_rgb *create_input_sb_rgb(std::string name = "default");
		output_sb_rgb *create_output_sb_rgb(std::string name = "default");

		input *inp(std::string name = "default") const;
		output *out(std::string name = "default") const;

		std::vector<input *> all_inputs() const;
		std::vector<output *> all_outputs() const;

		std::string get_name() const { return m_name; }
		interface *get_interface() const { return m_intf; }

		void set_target();
		bool is_target() { return m_target; }

		void run_render(const rectangle &cliprect) { m_render_cb(cliprect); }

	private:
		std::function<void (const rectangle &)> m_render_cb;
		std::string m_name;
		interface *m_intf;
		bool m_target;

		std::unordered_map<std::string, std::unique_ptr<input>> m_inputs;
		std::unordered_map<std::string, std::unique_ptr<output>> m_outputs;

		template<typename IO, typename IOL> IOL *check_and_add(std::unordered_map<std::string, std::unique_ptr<IO>> &map, std::unique_ptr<IOL> &&io, const char *type);
	};

	class interface : public device_interface
	{
	public:
		friend manager;
		interface(const machine_config &mconfig, device_t &device);
		virtual ~interface();

		const renderer *flow_render_get_renderer(std::string name = "default") const;

		void flow_render_do_render(int width, int height, const rectangle &cliprect);

	protected:
		virtual void flow_render_register_renderers() = 0;
		
		renderer *flow_render_create_renderer(std::function<void (const rectangle &)> render_cb, std::string name = "default");

	private:
		manager *m_manager;
		std::unordered_map<std::string, std::unique_ptr<renderer>> m_renderers;

		void flow_render_set_manager(manager *m) { m_manager = m; }
		void flow_render_append_renderers(std::vector<renderer *> &renderers) const;
	};

	class manager : public device_t
	{
	public:
		manager(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

		void set_setup(setup_delegate _cb) { m_setup_cb = _cb; }
		void connect(output *out, input *inp);
		void set_constant(input *inp, u32 value);
		void do_render(int width, int height, const rectangle &cliprect);

	protected:
		void device_config_complete() override;
		void device_start() override;

	private:
		struct intermediate_bitmap {
			std::unique_ptr<bitmap_t> bitmap;
			output *out;
			std::vector<input *> inp;
			bool fixed_value;
			u32 value;

			intermediate_bitmap(std::unique_ptr<bitmap_t> b) : bitmap(std::move(b)), out(nullptr), fixed_value(false), value(0) {}
		};

		setup_delegate m_setup_cb;

		std::vector<flow_render::interface *> m_fri;
		std::vector<flow_render::renderer *> m_rend;

		std::unordered_map<flow_render::input *, flow_render::output *> m_input_to_output;
		std::unordered_map<flow_render::input *, u32> m_input_constants_u32;		

		std::vector<renderer *> m_ordered_renderers;
		std::vector<std::unique_ptr<intermediate_bitmap>> m_intermediate_bitmaps;
	};
}

DECLARE_DEVICE_TYPE_NS(FLOW_RENDER_MANAGER, flow_render, manager)

#endif  /* MAME_EMU_DIFR_H */

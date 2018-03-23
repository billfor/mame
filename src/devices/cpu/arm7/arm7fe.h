// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
#ifndef MAME_CPU_ARM7_ARM7FE_H
#define MAME_CPU_ARM7_ARM7FE_H

#pragma once

#include "arm7.h"
#include "cpu/drcfe.h"

class arm7_frontend : public drc_frontend
{
public:
	arm7_frontend(arm7_cpu_device *arm7, uint32_t window_start, uint32_t window_end, uint32_t max_sequence);

protected:
	virtual bool describe(opcode_desc &desc, const opcode_desc *prev) override;
	virtual bool parse(opcode_desc &desc, const opcode_desc *prev, uint32_t op);

	uint32_t get_cpsr();
	bool get_mode32();

private:
	inline void describe_halfword_transfer(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline void describe_swap(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline void describe_mul_long(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline void describe_mul(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline void describe_alu(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline void describe_psr_transfer(opcode_desc &desc, const opcode_desc *prev, uint32_t op);

	inline bool describe_ops_0123(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_ops_4567(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_ops_89(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_ops_ab(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_ops_cd(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_ops_e(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_ops_f(opcode_desc &desc, const opcode_desc *prev, uint32_t op);

	inline bool describe_thumb(opcode_desc &desc, const opcode_desc *prev);

	arm7_cpu_device *m_cpu;
};

class arm9_frontend : public arm7_frontend
{
public:
	arm9_frontend(arm9_cpu_device *arm9, uint32_t window_start, uint32_t window_end, uint32_t max_sequence);

protected:
	virtual bool parse(opcode_desc &desc, const opcode_desc *prev, uint32_t op) override;

private:
	inline bool describe_arm9_ops_1(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_arm9_ops_57(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_arm9_ops_89(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_arm9_ops_ab(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_arm9_ops_c(opcode_desc &desc, const opcode_desc *prev, uint32_t op);
	inline bool describe_arm9_ops_e(opcode_desc &desc, const opcode_desc *prev, uint32_t op);

	arm9_cpu_device *m_cpu;
};

#endif /* MAME_CPU_ARM7_ARM7FE_H */

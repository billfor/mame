// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * cdelay.c
 *
 */

#include "netlist/devices/net_lib.h"

NETLIST_START(perf)

    SOLVER(Solver, 48000)
    PARAM(Solver.ACCURACY, 1e-20)
    MAINCLOCK(clk, 50000000)

    TTL_7400_NAND(n1,clk,clk)

NETLIST_END()


NETLIST_START(cap_delay)

    /*
     * delay circuit
     *
     */

    /* Standard stuff */

    SOLVER(Solver, 4800)
    PARAM(Solver.ACCURACY, 1e-20)
	PARAM(Solver.DYNAMIC_TS, 1)
	PARAM(Solver.MIN_TIMESTEP, 1e-6)
    CLOCK(clk, 5000)

    TTL_7400_NAND(n1,clk,clk)
    CAP(C, 1e-6)
    NET_C(n1.Q, C.2)
    NET_C(GND, C.1)
    TTL_7400_NAND(n2,n1.Q, n1.Q)

    LOG(logclk, clk)
    LOG(logn1Q, C.2)
    LOG(logn2Q, n1.Q)

NETLIST_END()

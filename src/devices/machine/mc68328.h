// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/**********************************************************************

 Motorola 68(VZ)328 ("DragonBall") System-on-a-Chip implementation

                                                             P P P P P P P   P P P P P P P
                                                             E E E E E E E   J J J J J J J
                                                             1 2 3 4 5 6 7   0 1 2 3 4 5 6
                   D   D D D D                               / / / / / / /   / / / / / / /
                   3   4 5 6 7                             ! ! ! ! ! ! ! !   ! ! ! ! ! ! !
                   /   / / / /                       ! !   C C C C C C C C   C C C C C C C
                   P V P P P P     D D G D D D D T T L U V S S S S S S S S G S S S S S S S
                   B C B B B B D D 1 1 N 1 1 1 1 M C W W C A A A A B B B B N C C C C D D D
                   3 C 4 5 6 7 8 9 0 1 D 2 3 4 5 S K E E C 0 1 2 3 0 1 2 3 D 0 1 2 3 0 1 2
                   | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
              +-------------------------------------------------------------------------------+
              |                                                                               |
              |                                                                               |
              |                                                                               |
              |                                                                               |
              |                                                                               |
      D2/PB2--|                                                                               |--PJ7/!CSD3
      D1/PB1--|                                                                               |--VCC
      D0/PB0--|                                                                               |--PD0/!KBD0/!INT0
         TDO--|                                                                               |--PD1/!KBD1/!INT1
         TDI--|                                                                               |--PD2/!KBD2/!INT2
         GND--|                                                                               |--PD3/!KBD3/!INT3
         !OE--|                                                                               |--PD4/!KBD4/!INT4
    !UDS/PC1--|                                                                               |--PD5/!KBD5/!INT5
         !AS--|                                                                               |--PD6/!KBD6/!INT6
          A0--|                                                                               |--PD7/!KBD7/!INT7
        !LDS--|                                                                               |--GND
        R/!W--|                                                                               |--LD0
  !DTACK/PC5--|                                                                               |--LD1
      !RESET--|                                                                               |--LD2
         VCC--|                                                                               |--LD3
     !WE/PC6--|                                                                               |--LFRM
    !JTAGRST--|                                                                               |--LLP
       BBUSW--|                                  MC68328PV                                    |--LCLK
          A1--|                                   TOP VIEW                                    |--LACD
          A2--|                                                                               |--VCC
          A3--|                                                                               |--PK0/SPMTXD0
          A4--|                                                                               |--PK1/SPMRXD0
          A5--|                                                                               |--PK2/SPMCLK0
          A6--|                                                                               |--PK3/SPSEN
         GND--|                                                                               |--PK4/SPSRXD1
          A7--|                                                                               |--PK5/SPSCLK1
          A8--|                                                                               |--PK6/!CE2
          A9--|                                                                               |--PK7/!CE1
         A10--|                                                                               |--GND
         A11--|                                                                               |--PM0/!CTS
         A12--|                                                                               |--PM1/!RTS
         A13--|                                                                               |--PM2/!IRQ6
         A14--|                                                                               |--PM3/!IRQ3
         VCC--|                                                                               |--PM4/!IRQ2
         A15--|                                                                               |--PM5/!IRQ1
     A16/PA0--|                                                                               |--PM6/!PENIRQ
              |                                                                               |
              |   _                                                                           |
              |  (_)                                                                          |
              |\                                                                              |
              | \                                                                             |
              +-------------------------------------------------------------------------------+
                   | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
                   P P P P P G P P P P P P P P V P P P P P P P P G P P P V C G P P P E X P
                   A A A A A N A A F F F F F F C F F G G G G G G N G G C C L N C M L X T L
                   1 2 3 4 5 D 6 7 0 1 2 3 4 5 C 6 7 7 6 5 4 3 2 D 1 0 0 C K D 4 7 L T A L
                   / / / / /   / / / / / / / /   / / / / / / / /   / / /   O   / / G A L V
                   A A A A A   A A A A A A A A   A A R T ! T ! P   R T M       ! U N L   C
                   1 1 1 2 2   2 2 2 2 2 2 2 2   3 3 T I T I T W   X X O       I A D     C
                   7 8 9 0 1   2 3 4 5 6 7 8 9   0 1 C N O N O M   D D C       R R
                                                     O 1 U 2 U O       L       Q T
                                                         T   T         K       7 G
                                                         1   2                   P
                                                                                 I
                                                                                 O

                   Figure 12-1. MC68328 144-Lead Plastic Thin-Quad Flat Pack Pin Assignment

                      Source: MC68328 (DragonBall)(tm) Integrated Processor User's Manual




                                                                       PG0  P
                                                                        /   M P       P V P
                    M M M   M M M                           ! !        BUSW 5 M P P P M S J
                    A A A   A A A M                         L U         /   / 4 M M M 0 S 6
                    1 1 1   1 1 1 A M M M M M     M M M M P W W       / !   ! / 3 2 1 / / /
                    5 4 3   2 1 0 9 A A A A A     A A A A G E E       R D   D S / / / S ! !
                    / / /   / / / / 8 7 6 5 4     3 2 1 0 1 / /   L   E T   M D D D S D C C
                  V A A A V A A A A / / / / / V V / / / / / ! ! ! V V S A   O A Q Q D C S S
                  S 1 1 1 D 1 1 1 1 A A A A A S S A A A A A L U O D D E C N D 1 M M C L D D
                  S 6 5 4 D 3 2 1 0 9 8 7 6 5 S S 4 3 2 1 0 B B E D D T K C E 0 L H E K 1 2
                  | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
              +-------------------------------------------------------------------------------+
              |     _                                                                         |
              |    (_)                                                                        |
              |                                                                               |
              |                                                                               |
              |                                                                               |
         VSS--|                                                                               |--VDD
         A17--|                                                                               |--D0/PA0
         A18--|                                                                               |--D1/PA1
         A19--|                                                                               |--D2/PA2
     PF3/A20--|                                                                               |--D3/PA3
     PF4/A21--|                                                                               |--D4/PA4
     PF5/A22--|                                                                               |--D5/PA5
     PF6/A23--|                                                                               |--D6/PA6
        LVDD--|                                                                               |--D7/PA7
         VDD--|                                                                               |--VSS
    PJ4/RXD2--|                                                                               |--VSS
    PJ5/TXD2--|                                                                               |--D8
   PJ6/!RTS2--|                                                                               |--D9
   PJ7/!CTS2--|                                                                               |--D10
         VSS--|                                                                               |--D11
         VSS--|                                                                               |--D12
  PE0/SPITXD--|                                                                               |--D13
  PE1/SPIRXD--|                                   MC68VZ328                                   |--D14
 PE2/SPICLK2--|                                   TOP  VIEW                                   |--D15
PE3/!DWE/UCLK-|                                                                               |--LVDD
    PE4/RXD1--|                                                                               |--VDD
    PE5/TXD1--|                                                                               |--PK7/LD7
   PE6/!RTS1--|                                                                               |--PK6/LD6
   PE7/!CTS1--|                                                                               |--PK5/LD5
          NC--|                                                                               |--PK4/LD4
         VDD--|                                                                               |--PK3/!UDS
 PG2/!EMUIRQ--|                                                                               |--PK2/!LDS
PG3/!HIZ/P/!D-|                                                                               |--!CSA0
  PG4/!EMUCS--|                                                                               |--PF7/!CSA1
 PG5/!EMUBRK--|                                                                               |--VSS
         VSS--|                                                                               |--PB0/!CSB0
         VSS--|                                                                               |--PB1/!CSB1/!SDWE
       EXTAL--|                                                                               |--PB2/!CSC0/!RAS0
        XTAL--|                                                                               |--PB3/!CSC1/!RAS1
        LVDD--|                                                                               |--PB4/!CSD0/!CAS0
         VSS--|                                                                               |--VSS
              |                                                                               |
              |                                                                               |
              |                                                                               |
              |                                                                               |
              |                                                                               |
              +-------------------------------------------------------------------------------+
                   | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
                   V N P P P P P P P P P V V P P P P P P P P L V P P P P P P P P V P P P V
                   D C J J J J F F D D D S S D D D D D F C C V D C C C C C C B B S K K B D
                   D   3 2 1 0 2 1 7 6 5 S S 4 3 2 1 0 0 7 6 D D 5 4 3 2 1 0 7 6 S 0 1 5 D
                       / / / / / / / / /     / / / / / / / / D   / / / / / / / /   / / /
                       ! S M M C ! ! ! !     ! ! ! ! ! C L L     L L L L L L P T   ! R !
                       S P I O L I I I I     I I I I I O A C     L F D D D D W O   D ! C
                       S I S S K R R R R     R N N N N N C L     P R 3 2 1 0 M U   R W S
                         C O I O Q Q Q Q     Q T T T T T D K       M         O T   D   D
                         L       5 6 3 2     1 3 2 1 0 R                     1 /   Y   1
                         K                             A                       T   /   /
                         1                             S                       I PWMO2 !CAS1
                                                       T                       N
A
                            Figure 20-1. MC68VZ328 TQFP Pin Assignments - Top View

                             Source: MC68VZ328 Integrated Processor User's Manual

******************************************************************************************************************/

#ifndef MAME_MACHINE_MC68328_H
#define MAME_MACHINE_MC68328_H

#include "machine/ram.h"

#define MCFG_MC68328_CPU(_tag) \
	downcast<mc68328_base_device &>(*device).set_cpu_tag("^" _tag);

#define MCFG_MC68328_OUT_PORT_A_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_a_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PORT_B_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_b_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PORT_C_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_c_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PORT_D_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_d_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PORT_E_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_e_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PORT_F_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_f_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PORT_G_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_g_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PORT_J_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_j_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PORT_K_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_k_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PORT_M_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_port_m_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_A_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_a_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_B_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_b_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_C_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_c_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_D_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_d_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_E_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_e_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_F_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_f_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_G_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_g_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_J_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_j_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_K_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_k_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_PORT_M_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_port_m_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_PWM_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_pwm_callback(DEVCB_##_devcb);

#define MCFG_MC68328_OUT_SPIM_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_out_spim_callback(DEVCB_##_devcb);

#define MCFG_MC68328_IN_SPIM_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_in_spim_callback(DEVCB_##_devcb);

#define MCFG_MC68328_SPIM_XCH_TRIGGER_CB(_devcb) \
	devcb = &downcast<mc68328_base_device &>(*device).set_spim_xch_trigger_callback(DEVCB_##_devcb);

class mc68328_base_device : public device_t
{
public:
	void set_cpu_tag(const char *tag) { m_cpu.set_tag(tag); }
	template <class Object> devcb_base &set_out_port_a_callback(Object &&cb) { return m_out_port_a_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_port_b_callback(Object &&cb) { return m_out_port_b_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_port_c_callback(Object &&cb) { return m_out_port_c_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_port_d_callback(Object &&cb) { return m_out_port_d_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_port_e_callback(Object &&cb) { return m_out_port_e_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_port_f_callback(Object &&cb) { return m_out_port_f_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_port_g_callback(Object &&cb) { return m_out_port_g_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_port_j_callback(Object &&cb) { return m_out_port_j_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_port_k_callback(Object &&cb) { return m_out_port_k_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_port_m_callback(Object &&cb) { return m_out_port_m_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_a_callback(Object &&cb) { return m_in_port_a_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_b_callback(Object &&cb) { return m_in_port_b_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_c_callback(Object &&cb) { return m_in_port_c_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_d_callback(Object &&cb) { return m_in_port_d_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_e_callback(Object &&cb) { return m_in_port_e_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_f_callback(Object &&cb) { return m_in_port_f_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_g_callback(Object &&cb) { return m_in_port_g_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_j_callback(Object &&cb) { return m_in_port_j_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_k_callback(Object &&cb) { return m_in_port_k_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_port_m_callback(Object &&cb) { return m_in_port_m_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_pwm_callback(Object &&cb) { return m_out_pwm_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_out_spim_callback(Object &&cb) { return m_out_spim_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_in_spim_callback(Object &&cb) { return m_in_spim_cb.set_callback(std::forward<Object>(cb)); }
	template <class Object> devcb_base &set_spim_xch_trigger_callback(Object &&cb) { return m_spim_xch_trigger_cb.set_callback(std::forward<Object>(cb)); }

	DECLARE_WRITE16_MEMBER(write);
	DECLARE_READ16_MEMBER(read);
	DECLARE_WRITE_LINE_MEMBER(set_penirq_line);
	void set_port_d_lines(uint8_t state, int bit);

protected:
	mc68328_base_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	void verboselog(int n_level, const char *s_fmt, ...) ATTR_PRINTF(3,4);

	virtual void regs_w(uint32_t address, uint16_t data, uint16_t mem_mask);
	virtual uint16_t regs_r(uint32_t address, uint16_t mem_mask);

	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	uint8_t   m_scr;        // System Control Register

	uint16_t  m_grpbasea;   // Chip Select Group A Base Register
	uint16_t  m_grpbaseb;   // Chip Select Group B Base Register
	uint16_t  m_grpbasec;   // Chip Select Group C Base Register
	uint16_t  m_grpbased;   // Chip Select Group D Base Register
	uint16_t  m_grpmaska;   // Chip Select Group A Mask Register
	uint16_t  m_grpmaskb;   // Chip Select Group B Mask Register
	uint16_t  m_grpmaskc;   // Chip Select Group C Mask Register
	uint16_t  m_grpmaskd;   // Chip Select Group D Mask Register
	uint32_t  m_csa0;       // Group A Chip Select 0 Register
	uint32_t  m_csa1;       // Group A Chip Select 1 Register
	uint32_t  m_csa2;       // Group A Chip Select 2 Register
	uint32_t  m_csa3;       // Group A Chip Select 3 Register
	uint32_t  m_csb0;       // Group B Chip Select 0 Register
	uint32_t  m_csb1;       // Group B Chip Select 1 Register
	uint32_t  m_csb2;       // Group B Chip Select 2 Register
	uint32_t  m_csb3;       // Group B Chip Select 3 Register
	uint32_t  m_csc0;       // Group C Chip Select 0 Register
	uint32_t  m_csc1;       // Group C Chip Select 1 Register
	uint32_t  m_csc2;       // Group C Chip Select 2 Register
	uint32_t  m_csc3;       // Group C Chip Select 3 Register
	uint32_t  m_csd0;       // Group D Chip Select 0 Register
	uint32_t  m_csd1;       // Group D Chip Select 1 Register
	uint32_t  m_csd2;       // Group D Chip Select 2 Register
	uint32_t  m_csd3;       // Group D Chip Select 3 Register

	uint16_t  m_pllcr;      // PLL Control Register
	uint16_t  m_pllfsr;     // PLL Frequency Select Register
	uint8_t   m_pctlr;      // Power Control Register

	uint8_t   m_ivr;        // Interrupt Vector Register
	uint16_t  m_icr;        // Interrupt Control Register
	uint32_t  m_imr;        // Interrupt Mask Register
	uint32_t  m_iwr;        // Interrupt Wakeup Enable Register
	uint32_t  m_isr;        // Interrupt Status Register
	uint32_t  m_ipr;        // Interrupt Pending Register

	uint8_t   m_padir;      // Port A Direction Register
	uint8_t   m_padata;     // Port A Data Register
	uint8_t   m_pasel;      // Port A Select Register

	uint8_t   m_pbdir;      // Port B Direction Register
	uint8_t   m_pbdata;     // Port B Data Register
	uint8_t   m_pbsel;      // Port B Select Register

	uint8_t   m_pcdir;      // Port C Direction Register
	uint8_t   m_pcdata;     // Port C Data Register
	uint8_t   m_pcsel;      // Port C Select Register

	uint8_t   m_pddir;      // Port D Direction Register
	uint8_t   m_pddata;     // Port D Data Register
	uint8_t   m_pdpuen;     // Port D Pullup Enable Register
	uint8_t   m_pdpol;      // Port D Polarity Register
	uint8_t   m_pdirqen;    // Port D IRQ Enable Register
	uint8_t   m_pddataedge; // Port D Data Edge Level
	uint8_t   m_pdirqedge;  // Port D IRQ Edge Register

	uint8_t   m_pedir;      // Port E Direction Register
	uint8_t   m_pedata;     // Port E Data Register
	uint8_t   m_pepuen;     // Port E Pullup Enable Register
	uint8_t   m_pesel;      // Port E Select Register

	uint8_t   m_pfdir;      // Port F Direction Register
	uint8_t   m_pfdata;     // Port F Data Register
	uint8_t   m_pfpuen;     // Port F Pullup Enable Register
	uint8_t   m_pfsel;      // Port F Select Register

	uint8_t   m_pgdir;      // Port G Direction Register
	uint8_t   m_pgdata;     // Port G Data Register
	uint8_t   m_pgpuen;     // Port G Pullup Enable Register
	uint8_t   m_pgsel;      // Port G Select Register

	uint8_t   m_pjdir;      // Port J Direction Register
	uint8_t   m_pjdata;     // Port J Data Register
	uint8_t   m_pjsel;      // Port J Select Register
	uint8_t   m_pkdir;      // Port K Direction Register
	uint8_t   m_pkdata;     // Port K Data Register
	uint8_t   m_pkpuen;     // Port K Pullup Enable Register
	uint8_t   m_pksel;      // Port K Select Register

	uint8_t   m_pmdir;      // Port M Direction Register
	uint8_t   m_pmdata;     // Port M Data Register
	uint8_t   m_pmpuen;     // Port M Pullup Enable Register
	uint8_t   m_pmsel;      // Port M Select Register

	uint16_t  m_pwmc;       // PWM Control Register
	uint16_t  m_pwmp;       // PWM Period Register
	uint16_t  m_pwmw;       // PWM Width Register
	uint16_t  m_pwmcnt;     // PWN Counter

	uint16_t  m_tctl[2];    // Timer Control Register
	uint16_t  m_tprer[2];   // Timer Prescaler Register
	uint16_t  m_tcmp[2];    // Timer Compare Register
	uint16_t  m_tcr[2];     // Timer Capture Register
	uint16_t  m_tcn[2];     // Timer Counter
	uint16_t  m_tstat[2];   // Timer Status
	uint16_t  m_wctlr;      // Watchdog Control Register
	uint16_t  m_wcmpr;      // Watchdog Compare Register
	uint16_t  m_wcn;        // Watchdog Counter
	uint8_t   m_tclear[2];  // Timer Clearable Status

	uint16_t  m_spisr;      // SPIS Register

	uint16_t  m_spimdata;   // SPIM Data Register
	uint16_t  m_spimcont;   // SPIM Control/Status Register

	uint16_t  m_ustcnt;     // UART Status/Control Register
	uint16_t  m_ubaud;      // UART Baud Control Register
	uint16_t  m_urx;        // UART RX Register
	uint16_t  m_utx;        // UART TX Register
	uint16_t  m_umisc;      // UART Misc Register

	uint32_t  m_lssa;       // Screen Starting Address Register
	uint8_t   m_lvpw;       // Virtual Page Width Register
	uint16_t  m_lxmax;      // Screen Width Register
	uint16_t  m_lymax;      // Screen Height Register
	uint16_t  m_lcxp;       // Cursor X Position
	uint16_t  m_lcyp;       // Cursor Y Position
	uint16_t  m_lcwch;      // Cursor Width & Height Register
	uint8_t   m_lblkc;      // Blink Control Register
	uint8_t   m_lpicf;      // Panel Interface Config Register
	uint8_t   m_lpolcf;     // Polarity Config Register
	uint8_t   m_lacdrc;     // ACD (M) Rate Control Register
	uint8_t   m_lpxcd;      // Pixel Clock Divider Register
	uint8_t   m_lckcon;     // Clocking Control Register
	uint8_t   m_llbar;      // Last Buffer Address Register
	uint8_t   m_lotcr;      // Octet Terminal Count Register
	uint8_t   m_lposr;      // Panning Offset Register
	uint8_t   m_lfrcm;      // Frame Rate Control Modulation Register
	uint16_t  m_lgpmr;      // Gray Palette Mapping Register

	uint32_t  m_hmsr;       // RTC Hours Minutes Seconds Register
	uint32_t  m_alarm;      // RTC Alarm Register
	uint16_t  m_rtcctl;     // RTC Control Register
	uint16_t  m_rtcisr;     // RTC Interrupt Status Register
	uint16_t  m_rtcienr;    // RTC Interrupt Enable Register
	uint16_t  m_stpwtch;    // Stopwatch Minutes

	void set_interrupt_line(uint32_t line, uint32_t active);
	void poll_port_d_interrupts();
	uint32_t get_timer_frequency(uint32_t index);
	void maybe_start_timer(uint32_t index, uint32_t new_enable);
	void timer_compare_event(uint32_t index);

	virtual void register_state_save();

	TIMER_CALLBACK_MEMBER(timer1_hit);
	TIMER_CALLBACK_MEMBER(timer2_hit);
	TIMER_CALLBACK_MEMBER(pwm_transition);
	TIMER_CALLBACK_MEMBER(rtc_tick);

	emu_timer *m_gptimer[2];
	emu_timer *m_rtc;
	emu_timer *m_pwm;

	devcb_write8  m_out_port_a_cb;    /* 8-bit output */
	devcb_write8  m_out_port_b_cb;    /* 8-bit output */
	devcb_write8  m_out_port_c_cb;    /* 8-bit output */
	devcb_write8  m_out_port_d_cb;    /* 8-bit output */
	devcb_write8  m_out_port_e_cb;    /* 8-bit output */
	devcb_write8  m_out_port_f_cb;    /* 8-bit output */
	devcb_write8  m_out_port_g_cb;    /* 8-bit output */
	devcb_write8  m_out_port_j_cb;    /* 8-bit output */
	devcb_write8  m_out_port_k_cb;    /* 8-bit output */
	devcb_write8  m_out_port_m_cb;    /* 8-bit output */

	devcb_read8   m_in_port_a_cb;     /* 8-bit input */
	devcb_read8   m_in_port_b_cb;     /* 8-bit input */
	devcb_read8   m_in_port_c_cb;     /* 8-bit input */
	devcb_read8   m_in_port_d_cb;     /* 8-bit input */
	devcb_read8   m_in_port_e_cb;     /* 8-bit input */
	devcb_read8   m_in_port_f_cb;     /* 8-bit input */
	devcb_read8   m_in_port_g_cb;     /* 8-bit input */
	devcb_read8   m_in_port_j_cb;     /* 8-bit input */
	devcb_read8   m_in_port_k_cb;     /* 8-bit input */
	devcb_read8   m_in_port_m_cb;     /* 8-bit input */

	devcb_write8  m_out_pwm_cb;       /* 1-bit output */

	devcb_write16 m_out_spim_cb;      /* 16-bit output */
	devcb_read16  m_in_spim_cb;       /* 16-bit input */

	devcb_write_line m_spim_xch_trigger_cb;    /* SPIM exchange trigger */ /*todo: not really a write line, fix*/

	required_device<cpu_device> m_cpu;
};

class mc68328_device : public mc68328_base_device
{
public:
	mc68328_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);

protected:
	// device-level overrides
	virtual void device_reset() override;

	void register_state_save() override;

	void regs_w(uint32_t address, uint16_t data, uint16_t mem_mask) override;
	uint16_t regs_r(uint32_t address, uint16_t mem_mask) override;
};

#define MCFG_MC68VZ328_BOOT_REGION(_tag) \
	downcast<mc68vz328_device &>(*device).set_boot_region_tag("^" _tag);

#define MCFG_MC68VZ328_RAM_TAG(_tag) \
	downcast<mc68vz328_device &>(*device).set_ram_tag("^" _tag);

class mc68vz328_device : public mc68328_base_device
{
public:
	mc68vz328_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void set_boot_region_tag(const char *tag) { m_boot_region.set_tag(tag); }
	void set_ram_tag(const char *tag) { m_ram.set_tag(tag); }

	DECLARE_WRITE16_MEMBER(mem_w);
	DECLARE_READ16_MEMBER(mem_r);

	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);

protected:
	// device-level overrides
	virtual void device_reset() override;

	void register_state_save() override;

	void regs_w(uint32_t address, uint16_t data, uint16_t mem_mask) override;
	uint16_t regs_r(uint32_t address, uint16_t mem_mask) override;

	bool m_in_boot;

	required_memory_region m_boot_region;
	required_device<ram_device> m_ram;
	uint16_t m_fuck[0x1000000/2];
};

DECLARE_DEVICE_TYPE(MC68328, mc68328_device)
DECLARE_DEVICE_TYPE(MC68VZ328, mc68vz328_device)

#endif // MAME_MACHINE_MC68328_H

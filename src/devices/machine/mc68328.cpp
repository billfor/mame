// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/**********************************************************************

    Motorola 68328 ("DragonBall") System-on-a-Chip implementation

    By Ryan Holtz

**********************************************************************/

#include "emu.h"
#include "machine/mc68328.h"
#include "cpu/m68000/m68000.h"
#include "machine/ram.h"


#define SCR_BETO                0x80
#define SCR_WPV                 0x40
#define SCR_PRV                 0x20
#define SCR_BETEN               0x10
#define SCR_SO                  0x08
#define SCR_DMAP                0x04
#define SCR_WDTH8               0x01

#define ICR_POL6                0x0100
#define ICR_POL3                0x0200
#define ICR_POL2                0x0400
#define ICR_POL1                0x0800
#define ICR_ET6                 0x1000
#define ICR_ET3                 0x2000
#define ICR_ET2                 0x4000
#define ICR_ET1                 0x8000

#define INT_SPIM                0x000001
#define INT_TIMER2              0x000002
#define INT_UART                0x000004
#define INT_WDT                 0x000008
#define INT_RTC                 0x000010
#define INT_RESERVED            0x000020
#define INT_KB                  0x000040
#define INT_PWM                 0x000080
#define INT_INT0                0x000100
#define INT_INT1                0x000200
#define INT_INT2                0x000400
#define INT_INT3                0x000800
#define INT_INT4                0x001000
#define INT_INT5                0x002000
#define INT_INT6                0x004000
#define INT_INT7                0x008000
#define INT_KBDINTS             0x00ff00
#define INT_IRQ1                0x010000
#define INT_IRQ2                0x020000
#define INT_IRQ3                0x040000
#define INT_IRQ6                0x080000
#define INT_PEN                 0x100000
#define INT_SPIS                0x200000
#define INT_TIMER1              0x400000
#define INT_IRQ7                0x800000

#define INT_M68K_LINE1          (INT_IRQ1)
#define INT_M68K_LINE2          (INT_IRQ2)
#define INT_M68K_LINE3          (INT_IRQ3)
#define INT_M68K_LINE4          (INT_INT0 | INT_INT1 | INT_INT2 | INT_INT3 | INT_INT4 | INT_INT5 | INT_INT6 | INT_INT7 | \
									INT_PWM | INT_KB | INT_RTC | INT_WDT | INT_UART | INT_TIMER2 | INT_SPIM)
#define INT_M68K_LINE5          (INT_PEN)
#define INT_M68K_LINE6          (INT_IRQ6 | INT_TIMER1 | INT_SPIS)
#define INT_M68K_LINE7          (INT_IRQ7)
#define INT_M68K_LINE67         (INT_M68K_LINE6 | INT_M68K_LINE7)
#define INT_M68K_LINE567        (INT_M68K_LINE5 | INT_M68K_LINE6 | INT_M68K_LINE7)
#define INT_M68K_LINE4567       (INT_M68K_LINE4 | INT_M68K_LINE5 | INT_M68K_LINE6 | INT_M68K_LINE7)
#define INT_M68K_LINE34567      (INT_M68K_LINE3 | INT_M68K_LINE4 | INT_M68K_LINE5 | INT_M68K_LINE6 | INT_M68K_LINE7)
#define INT_M68K_LINE234567     (INT_M68K_LINE2 | INT_M68K_LINE3 | INT_M68K_LINE4 | INT_M68K_LINE5 | INT_M68K_LINE6 | INT_M68K_LINE7)

#define INT_IRQ1_SHIFT          0x000001
#define INT_IRQ2_SHIFT          0x000002
#define INT_IRQ3_SHIFT          0x000004
#define INT_IRQ6_SHIFT          0x000008
#define INT_PEN_SHIFT           0x000010
#define INT_SPIS_SHIFT          0x000020
#define INT_TIMER1_SHIFT        0x000040
#define INT_IRQ7_SHIFT          0x000080

#define INT_ACTIVE              1
#define INT_INACTIVE            0

#define GRPBASE_BASE_ADDR       0xfff0
#define GRPBASE_VALID           0x0001

#define GRPMASK_BASE_MASK       0xfff0

#define CSAB_COMPARE            0xff000000
#define CSAB_BSW                0x00010000
#define CSAB_MASK               0x0000ff00
#define CSAB_RO                 0x00000008
#define CSAB_WAIT               0x00000007

#define CSCD_COMPARE            0xfff00000
#define CSCD_BSW                0x00010000
#define CSCD_MASK               0x0000fff0
#define CSCD_RO                 0x00000008
#define CSCD_WAIT               0x00000007

#define PLLCR_PIXCLK_SEL        0x3800
#define PLLCR_PIXCLK_SEL_DIV2       0x0000
#define PLLCR_PIXCLK_SEL_DIV4       0x0800
#define PLLCR_PIXCLK_SEL_DIV8       0x1000
#define PLLCR_PIXCLK_SEL_DIV16      0x1800
#define PLLCR_PIXCLK_SEL_DIV1_0     0x2000
#define PLLCR_PIXCLK_SEL_DIV1_1     0x2800
#define PLLCR_PIXCLK_SEL_DIV1_2     0x3000
#define PLLCR_PIXCLK_SEL_DIV1_3     0x3800
#define PLLCR_SYSCLK_SEL        0x0700
#define PLLCR_SYSCLK_SEL_DIV2       0x0000
#define PLLCR_SYSCLK_SEL_DIV4       0x0100
#define PLLCR_SYSCLK_SEL_DIV8       0x0200
#define PLLCR_SYSCLK_SEL_DIV16      0x0300
#define PLLCR_SYSCLK_SEL_DIV1_0     0x0400
#define PLLCR_SYSCLK_SEL_DIV1_1     0x0500
#define PLLCR_SYSCLK_SEL_DIV1_2     0x0600
#define PLLCR_SYSCLK_SEL_DIV1_3     0x0700
#define PLLCR_CLKEN             0x0010
#define PLLCR_DISPLL            0x0008

#define PLLFSR_CLK32            0x8000
#define PLLFSR_PROT             0x4000
#define PLLFSR_QCNT             0x0f00
#define PLLFSR_PCNT             0x00ff

#define PCTLR_PC_EN             0x80
#define PCTLR_STOP              0x40
#define PCTLR_WIDTH             0x1f

#define CXP_CC                  0xc000
#define CXP_CC_XLU                  0x0000
#define CXP_CC_BLACK                0x4000
#define CXP_CC_INVERSE              0x8000
#define CXP_CC_INVALID              0xc000
#define CXP_MASK                0x03ff

#define CYP_MASK                0x01ff

#define CWCH_CW                 0x1f00
#define CWCH_CH                 0x001f

#define BLKC_BKEN               0x80
#define BLKC_BD                 0x7f

#define LPICF_PBSIZ             0x06
#define LPICF_PBSIZ_1               0x00
#define LPICF_PBSIZ_2               0x02
#define LPICF_PBSIZ_4               0x04
#define LPICF_PBSIZ_INVALID         0x06

#define LPOLCF_LCKPOL           0x08
#define LPOLCF_FLMPOL           0x04
#define LPOLCF_LPPOL            0x02
#define LPOLCF_PIXPOL           0x01

#define LACDRC_MASK             0x0f

#define LPXCD_MASK              0x3f

#define LCKCON_LCDC_EN          0x80
#define LCKCON_LCDON            0x80
#define LCKCON_DMA16            0x40
#define LCKCON_WS               0x30
#define LCKCON_WS_1                 0x00
#define LCKCON_WS_2                 0x10
#define LCKCON_WS_3                 0x20
#define LCKCON_WS_4                 0x30
#define LCKCON_DWIDTH           0x02
#define LCKCON_PCDS             0x01

#define LBAR_MASK               0x7f

#define LPOSR_BOS               0x08
#define LPOSR_POS               0x07

#define LFRCM_XMOD              0xf0
#define LFRCM_YMOD              0x0f

#define LGPMR_PAL1              0x7000
#define LGPMR_PAL0              0x0700
#define LGPMR_PAL3              0x0070
#define LGPMR_PAL2              0x0007

#define RTCHMSR_HOURS           0x1f000000
#define RTCHMSR_MINUTES         0x003f0000
#define RTCHMSR_SECONDS         0x0000003f

#define RTCCTL_38_4             0x0020
#define RTCCTL_ENABLE           0x0080

#define RTCINT_STOPWATCH        0x0001
#define RTCINT_MINUTE           0x0002
#define RTCINT_ALARM            0x0004
#define RTCINT_DAY              0x0008
#define RTCINT_SECOND           0x0010

#define RTCSTPWTCH_MASK         0x003f

#define TCTL_TEN                0x0001
#define TCTL_TEN_ENABLE             0x0001
#define TCTL_CLKSOURCE          0x000e
#define TCTL_CLKSOURCE_STOP         0x0000
#define TCTL_CLKSOURCE_SYSCLK       0x0002
#define TCTL_CLKSOURCE_SYSCLK16     0x0004
#define TCTL_CLKSOURCE_TIN          0x0006
#define TCTL_CLKSOURCE_32KHZ4       0x0008
#define TCTL_CLKSOURCE_32KHZ5       0x000a
#define TCTL_CLKSOURCE_32KHZ6       0x000c
#define TCTL_CLKSOURCE_32KHZ7       0x000e
#define TCTL_IRQEN              0x0010
#define TCTL_IRQEN_ENABLE           0x0010
#define TCTL_OM                 0x0020
#define TCTL_OM_ACTIVELOW           0x0000
#define TCTL_OM_TOGGLE              0x0020
#define TCTL_CAPTURE            0x00c0
#define TCTL_CAPTURE_NOINT          0x0000
#define TCTL_CAPTURE_RISING         0x0040
#define TCTL_CAPTURE_FALLING        0x0080
#define TCTL_CAPTURE_BOTH           0x00c0
#define TCTL_FRR                0x0100
#define TCTL_FRR_RESTART            0x0000
#define TCTL_FRR_FREERUN            0x0100

#define TSTAT_COMP              0x0001
#define TSTAT_CAPT              0x0002

#define WCTLR_WDRST             0x0008
#define WCTLR_LOCK              0x0004
#define WCTLR_FI                0x0002
#define WCTLR_WDEN              0x0001

#define USTCNT_UART_EN          0x8000
#define USTCNT_RX_EN            0x4000
#define USTCNT_TX_EN            0x2000
#define USTCNT_RX_CLK_CONT      0x1000
#define USTCNT_PARITY_EN        0x0800
#define USTCNT_ODD_EVEN         0x0400
#define USTCNT_STOP_BITS        0x0200
#define USTCNT_8_7              0x0100
#define USTCNT_GPIO_DELTA_EN    0x0080
#define USTCNT_CTS_DELTA_EN     0x0040
#define USTCNT_RX_FULL_EN       0x0020
#define USTCNT_RX_HALF_EN       0x0010
#define USTCNT_RX_RDY_EN        0x0008
#define USTCNT_TX_EMPTY_EN      0x0004
#define USTCNT_TX_HALF_EN       0x0002
#define USTCNT_TX_AVAIL_EN      0x0001

#define UBAUD_GPIO_DELTA        0x8000
#define UBAUD_GPIO              0x4000
#define UBAUD_GPIO_DIR          0x2000
#define UBAUD_GPIO_SRC          0x1000
#define UBAUD_BAUD_SRC          0x0800
#define UBAUD_DIVIDE            0x0700
#define UBAUD_DIVIDE_1              0x0000
#define UBAUD_DIVIDE_2              0x0100
#define UBAUD_DIVIDE_4              0x0200
#define UBAUD_DIVIDE_8              0x0300
#define UBAUD_DIVIDE_16             0x0400
#define UBAUD_DIVIDE_32             0x0500
#define UBAUD_DIVIDE_64             0x0600
#define UBAUD_DIVIDE_128            0x0700
#define UBAUD_PRESCALER         0x00ff

#define URX_FIFO_FULL           0x8000
#define URX_FIFO_HALF           0x4000
#define URX_DATA_READY          0x2000
#define URX_OVRUN               0x0800
#define URX_FRAME_ERROR         0x0400
#define URX_BREAK               0x0200
#define URX_PARITY_ERROR        0x0100

#define UTX_FIFO_EMPTY          0x8000
#define UTX_FIFO_HALF           0x4000
#define UTX_TX_AVAIL            0x2000
#define UTX_SEND_BREAK          0x1000
#define UTX_IGNORE_CTS          0x0800
#define UTX_CTS_STATUS          0x0200
#define UTX_CTS_DELTA           0x0100

#define UMISC_CLK_SRC           0x4000
#define UMISC_FORCE_PERR        0x2000
#define UMISC_LOOP              0x1000
#define UMISC_RTS_CONT          0x0080
#define UMISC_RTS               0x0040
#define UMISC_IRDA_ENABLE       0x0020
#define UMISC_IRDA_LOOP         0x0010

#define SPIS_SPIS_IRQ           0x8000
#define SPIS_IRQEN              0x4000
#define SPIS_ENPOL              0x2000
#define SPIS_DATA_RDY           0x1000
#define SPIS_OVRWR              0x0800
#define SPIS_PHA                0x0400
#define SPIS_POL                0x0200
#define SPIS_SPISEN             0x0100

#define SPIM_CLOCK_COUNT        0x000f
#define SPIM_POL                0x0010
#define SPIM_POL_HIGH               0x0000
#define SPIM_POL_LOW                0x0010
#define SPIM_PHA                0x0020
#define SPIM_PHA_NORMAL             0x0000
#define SPIM_PHA_OPPOSITE           0x0020
#define SPIM_IRQEN              0x0040
#define SPIM_SPIMIRQ            0x0080
#define SPIM_XCH                0x0100
#define SPIM_XCH_IDLE               0x0000
#define SPIM_XCH_INIT               0x0100
#define SPIM_SPMEN              0x0200
#define SPIM_SPMEN_DISABLE          0x0000
#define SPIM_SPMEN_ENABLE           0x0200
#define SPIM_RATE               0xe000
#define SPIM_RATE_4                 0x0000
#define SPIM_RATE_8                 0x2000
#define SPIM_RATE_16                0x4000
#define SPIM_RATE_32                0x6000
#define SPIM_RATE_64                0x8000
#define SPIM_RATE_128               0xa000
#define SPIM_RATE_256               0xc000
#define SPIM_RATE_512               0xe000

#define PWMC_PWMIRQ             0x8000
#define PWMC_IRQEN              0x4000
#define PWMC_LOAD               0x0100
#define PWMC_PIN                0x0080
#define PWMC_POL                0x0040
#define PWMC_PWMEN              0x0010
#define PWMC_CLKSEL             0x0007


#define VERBOSE_LEVEL   (5)

void ATTR_PRINTF(3,4) mc68328_base_device::verboselog(int n_level, const char *s_fmt, ...)
{
	if (VERBOSE_LEVEL >= n_level)
	{
		va_list v;
		char buf[32768];
		va_start(v, s_fmt);
		vsprintf(buf, s_fmt, v);
		va_end(v);
		logerror("%s: %s", machine().describe_context(), buf);
	}
}

DEFINE_DEVICE_TYPE(MC68328, mc68328_device, "mc68328", "MC68328 DragonBall Integrated Processor")
DEFINE_DEVICE_TYPE(MC68VZ328, mc68vz328_device, "mc68vz328", "MC68VZ328 DragonBall Integrated Processor")

mc68328_base_device::mc68328_base_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, m_rtc(nullptr), m_pwm(nullptr)
	, m_out_port_a_cb(*this)
	, m_out_port_b_cb(*this)
	, m_out_port_c_cb(*this)
	, m_out_port_d_cb(*this)
	, m_out_port_e_cb(*this)
	, m_out_port_f_cb(*this)
	, m_out_port_g_cb(*this)
	, m_out_port_j_cb(*this)
	, m_out_port_k_cb(*this)
	, m_out_port_m_cb(*this)
	, m_in_port_a_cb(*this)
	, m_in_port_b_cb(*this)
	, m_in_port_c_cb(*this)
	, m_in_port_d_cb(*this)
	, m_in_port_e_cb(*this)
	, m_in_port_f_cb(*this)
	, m_in_port_g_cb(*this)
	, m_in_port_j_cb(*this)
	, m_in_port_k_cb(*this)
	, m_in_port_m_cb(*this)
	, m_out_pwm_cb(*this)
	, m_out_spim_cb(*this)
	, m_in_spim_cb(*this)
	, m_spim_xch_trigger_cb(*this)
	, m_cpu(*this, finder_base::DUMMY_TAG)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void mc68328_base_device::device_start()
{
	m_out_port_a_cb.resolve_safe();
	m_out_port_b_cb.resolve_safe();
	m_out_port_c_cb.resolve_safe();
	m_out_port_d_cb.resolve_safe();
	m_out_port_e_cb.resolve_safe();
	m_out_port_f_cb.resolve_safe();
	m_out_port_g_cb.resolve_safe();
	m_out_port_j_cb.resolve_safe();
	m_out_port_k_cb.resolve_safe();
	m_out_port_m_cb.resolve_safe();

	m_in_port_a_cb.resolve();
	m_in_port_b_cb.resolve();
	m_in_port_c_cb.resolve();
	m_in_port_d_cb.resolve();
	m_in_port_e_cb.resolve();
	m_in_port_f_cb.resolve();
	m_in_port_g_cb.resolve();
	m_in_port_j_cb.resolve();
	m_in_port_k_cb.resolve();
	m_in_port_m_cb.resolve();

	m_out_pwm_cb.resolve_safe();

	m_out_spim_cb.resolve_safe();
	m_in_spim_cb.resolve();

	m_spim_xch_trigger_cb.resolve_safe();

	m_gptimer[0] = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(mc68328_base_device::timer1_hit),this));
	m_gptimer[1] = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(mc68328_base_device::timer2_hit),this));
	m_rtc = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(mc68328_base_device::rtc_tick),this));
	m_pwm = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(mc68328_base_device::pwm_transition),this));

	register_state_save();
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void mc68328_base_device::device_reset()
{
	m_scr = 0x0c;
	m_grpbasea = 0x0000;
	m_grpbaseb = 0x0000;
	m_grpbasec = 0x0000;
	m_grpbased = 0x0000;
	m_grpmaska = 0x0000;
	m_grpmaskb = 0x0000;
	m_grpmaskc = 0x0000;
	m_grpmaskd = 0x0000;

	m_pllcr = 0x2400;
	m_pllfsr = 0x0123;
	m_pctlr = 0x1f;

	m_ivr = 0x00;
	m_icr = 0x0000;
	m_imr = 0x00ffffff;
	m_iwr = 0x00ffffff;
	m_isr = 0x00000000;
	m_ipr = 0x00000000;

	m_padir = 0x00;
	m_padata = 0x00;
	m_pasel = 0x00;
	m_pbdir = 0x00;
	m_pbdata = 0x00;
	m_pbsel = 0x00;
	m_pcdir = 0x00;
	m_pcdata = 0x00;
	m_pcsel = 0x00;
	m_pddir = 0x00;
	m_pddata = 0x00;
	m_pdpuen = 0xff;
	m_pdpol = 0x00;
	m_pdirqen = 0x00;
	m_pddataedge = 0x00;
	m_pdirqedge = 0x00;
	m_pedir = 0x00;
	m_pedata = 0x00;
	m_pepuen = 0x80;
	m_pesel = 0x80;
	m_pfdir = 0x00;
	m_pfdata = 0x00;
	m_pfpuen = 0xff;
	m_pfsel = 0xff;
	m_pgdir = 0x00;
	m_pgdata = 0x00;
	m_pgpuen = 0xff;
	m_pgsel = 0xff;
	m_pjdir = 0x00;
	m_pjdata = 0x00;
	m_pjsel = 0x00;
	m_pkdir = 0x00;
	m_pkdata = 0x00;
	m_pkpuen = 0xff;
	m_pksel = 0xff;
	m_pmdir = 0x00;
	m_pmdata = 0x00;
	m_pmpuen = 0xff;
	m_pmsel = 0xff;

	m_pwmc = 0x0000;
	m_pwmp = 0x0000;
	m_pwmw = 0x0000;
	m_pwmcnt = 0x0000;

	m_tctl[0] = m_tctl[1] = 0x0000;
	m_tprer[0] = m_tprer[1] = 0x0000;
	m_tcmp[0] = m_tcmp[1] = 0xffff;
	m_tcr[0] = m_tcr[1] = 0x0000;
	m_tcn[0] = m_tcn[1] = 0x0000;
	m_tstat[0] = m_tstat[1] = 0x0000;
	m_wctlr = 0x0000;
	m_wcmpr = 0xffff;
	m_wcn = 0x0000;

	m_spisr = 0x0000;

	m_spimdata = 0x0000;
	m_spimcont = 0x0000;

	m_ustcnt = 0x0000;
	m_ubaud = 0x003f;
	m_urx = 0x0000;
	m_utx = 0x0000;
	m_umisc = 0x0000;

	m_lssa = 0x00000000;
	m_lvpw = 0xff;
	m_lxmax = 0x03ff;
	m_lymax = 0x01ff;
	m_lcxp = 0x0000;
	m_lcyp = 0x0000;
	m_lcwch = 0x0101;
	m_lblkc = 0x7f;
	m_lpicf = 0x00;
	m_lpolcf = 0x00;
	m_lacdrc = 0x00;
	m_lpxcd = 0x00;
	m_lckcon = 0x40;
	m_llbar = 0x3e;
	m_lotcr = 0x3f;
	m_lposr = 0x00;
	m_lfrcm = 0xb9;
	m_lgpmr = 0x1073;

	m_hmsr = 0x00000000;
	m_alarm = 0x00000000;
	m_rtcctl = 0x00;
	m_rtcisr = 0x00;
	m_rtcienr = 0x00;
	m_stpwtch = 0x00;

	m_rtc->adjust(attotime::from_hz(1), 0, attotime::from_hz(1));
}


void mc68328_base_device::set_interrupt_line(uint32_t line, uint32_t active)
{
	if (active)
	{
		m_ipr |= line;

		if (!(m_imr & line) && !(m_isr & line))
		{
			m_isr |= line;

			if (m_isr & INT_M68K_LINE7)
			{
				m_cpu->set_input_line_and_vector(M68K_IRQ_7, ASSERT_LINE, m_ivr | 0x07);
			}
			else if (m_isr & INT_M68K_LINE6)
			{
				m_cpu->set_input_line_and_vector(M68K_IRQ_6, ASSERT_LINE, m_ivr | 0x06);
			}
			else if (m_isr & INT_M68K_LINE5)
			{
				m_cpu->set_input_line_and_vector(M68K_IRQ_5, ASSERT_LINE, m_ivr | 0x05);
			}
			else if (m_isr & INT_M68K_LINE4)
			{
				m_cpu->set_input_line_and_vector(M68K_IRQ_4, ASSERT_LINE, m_ivr | 0x04);
			}
			else if (m_isr & INT_M68K_LINE3)
			{
				m_cpu->set_input_line_and_vector(M68K_IRQ_3, ASSERT_LINE, m_ivr | 0x03);
			}
			else if (m_isr & INT_M68K_LINE2)
			{
				m_cpu->set_input_line_and_vector(M68K_IRQ_2, ASSERT_LINE, m_ivr | 0x02);
			}
			else if (m_isr & INT_M68K_LINE1)
			{
				m_cpu->set_input_line_and_vector(M68K_IRQ_1, ASSERT_LINE, m_ivr | 0x01);
			}
		}
	}
	else
	{
		m_isr &= ~line;

		if ((line & INT_M68K_LINE7) && !(m_isr & INT_M68K_LINE7))
		{
			m_cpu->set_input_line(M68K_IRQ_7, CLEAR_LINE);
		}
		if ((line & INT_M68K_LINE6) && !(m_isr & INT_M68K_LINE6))
		{
			m_cpu->set_input_line(M68K_IRQ_6, CLEAR_LINE);
		}
		if ((line & INT_M68K_LINE5) && !(m_isr & INT_M68K_LINE5))
		{
			m_cpu->set_input_line(M68K_IRQ_5, CLEAR_LINE);
		}
		if ((line & INT_M68K_LINE4) && !(m_isr & INT_M68K_LINE4))
		{
			m_cpu->set_input_line(M68K_IRQ_4, CLEAR_LINE);
		}
		if ((line & INT_M68K_LINE3) && !(m_isr & INT_M68K_LINE3))
		{
			m_cpu->set_input_line(M68K_IRQ_3, CLEAR_LINE);
		}
		if ((line & INT_M68K_LINE2) && !(m_isr & INT_M68K_LINE2))
		{
			m_cpu->set_input_line(M68K_IRQ_2, CLEAR_LINE);
		}
		if ((line & INT_M68K_LINE1) && !(m_isr & INT_M68K_LINE1))
		{
			m_cpu->set_input_line(M68K_IRQ_1, CLEAR_LINE);
		}
	}
}

void mc68328_base_device::poll_port_d_interrupts()
{
	uint8_t line_transitions = m_pddataedge & m_pdirqedge;
	uint8_t line_holds = m_pddata &~ m_pdirqedge;
	uint8_t line_interrupts = (line_transitions | line_holds) & m_pdirqen;

	if (line_interrupts)
	{
		set_interrupt_line(line_interrupts << 8, 1);
	}
	else
	{
		set_interrupt_line(INT_KBDINTS, 0);
	}
}

WRITE_LINE_MEMBER( mc68328_base_device::set_penirq_line )
{
	if (state)
	{
		set_interrupt_line(INT_PEN, 1);
	}
	else
	{
		m_ipr &= ~INT_PEN;
		set_interrupt_line(INT_PEN, 0);
	}
}

void mc68328_base_device::set_port_d_lines(uint8_t state, int bit)
{
	const uint8_t old_button_state = m_pddata;

	if (BIT(state, bit))
	{
		m_pddata |= (1 << bit);
	}
	else
	{
		m_pddata &= ~(1 << bit);
	}

	m_pddataedge |= ~old_button_state & m_pddata;

	poll_port_d_interrupts();
}

uint32_t mc68328_base_device::get_timer_frequency(uint32_t index)
{
	uint32_t frequency = 0;

	switch (m_tctl[index] & TCTL_CLKSOURCE)
	{
		case TCTL_CLKSOURCE_SYSCLK:
			frequency = 32768 * 506;
			break;

		case TCTL_CLKSOURCE_SYSCLK16:
			frequency = (32768 * 506) / 16;
			break;

		case TCTL_CLKSOURCE_32KHZ4:
		case TCTL_CLKSOURCE_32KHZ5:
		case TCTL_CLKSOURCE_32KHZ6:
		case TCTL_CLKSOURCE_32KHZ7:
			frequency = 32768;
			break;
	}
	frequency /= (m_tprer[index] + 1);

	return frequency;
}

void mc68328_base_device::maybe_start_timer(uint32_t index, uint32_t new_enable)
{
	if ((m_tctl[index] & TCTL_TEN) == TCTL_TEN_ENABLE && (m_tctl[index] & TCTL_CLKSOURCE) > TCTL_CLKSOURCE_STOP)
	{
		if ((m_tctl[index] & TCTL_CLKSOURCE) == TCTL_CLKSOURCE_TIN)
		{
			m_gptimer[index]->adjust(attotime::never);
		}
		else if (m_tcmp[index] == 0)
		{
			m_gptimer[index]->adjust(attotime::never);
		}
		else
		{
			uint32_t frequency = get_timer_frequency(index);
			attotime period = (attotime::from_hz(frequency) *  m_tcmp[index]);

			if (new_enable)
			{
				m_tcn[index] = 0x0000;
			}

			m_gptimer[index]->adjust(period);
		}
	}
	else
	{
		m_gptimer[index]->adjust(attotime::never);
	}
}

void mc68328_base_device::timer_compare_event(uint32_t index)
{
	m_tcn[index] = m_tcmp[index];
	m_tstat[index] |= TSTAT_COMP;

	if ((m_tctl[index] & TCTL_FRR) == TCTL_FRR_RESTART)
	{
		uint32_t frequency = get_timer_frequency(index);

		if (frequency > 0)
		{
			attotime period = attotime::from_hz(frequency) * m_tcmp[index];

			m_tcn[index] = 0x0000;

			m_gptimer[index]->adjust(period);
		}
		else
		{
			m_gptimer[index]->adjust(attotime::never);
		}
	}
	else
	{
		uint32_t frequency = get_timer_frequency(index);

		if (frequency > 0)
		{
			attotime period = attotime::from_hz(frequency) * 0x10000;

			m_gptimer[index]->adjust(period);
		}
		else
		{
			m_gptimer[index]->adjust(attotime::never);
		}
	}
	if ((m_tctl[index] & TCTL_IRQEN) == TCTL_IRQEN_ENABLE)
	{
		set_interrupt_line((index == 0) ? INT_TIMER1 : INT_TIMER2, 1);
	}
}

TIMER_CALLBACK_MEMBER( mc68328_base_device::timer1_hit )
{
	timer_compare_event(0);
}

TIMER_CALLBACK_MEMBER( mc68328_base_device::timer2_hit )
{
	timer_compare_event(1);
}

TIMER_CALLBACK_MEMBER( mc68328_base_device::pwm_transition )
{
	if (m_pwmw >= m_pwmp || m_pwmw == 0 || m_pwmp == 0)
	{
		m_pwm->adjust(attotime::never);
		return;
	}

	if (((m_pwmc & PWMC_POL) == 0 && (m_pwmc & PWMC_PIN) != 0) ||
		((m_pwmc & PWMC_POL) != 0 && (m_pwmc & PWMC_PIN) == 0))
	{
		uint32_t frequency = 32768 * 506;
		uint32_t divisor = 4 << (m_pwmc & PWMC_CLKSEL); // ?? Datasheet says 2 <<, but then we're an octave higher than CoPilot.
		attotime period;

		frequency /= divisor;
		period = attotime::from_hz(frequency) * (m_pwmp - m_pwmw);

		m_pwm->adjust(period);

		if (m_pwmc & PWMC_IRQEN)
		{
			set_interrupt_line(INT_PWM, 1);
		}
	}
	else
	{
		uint32_t frequency = 32768 * 506;
		uint32_t divisor = 4 << (m_pwmc & PWMC_CLKSEL); // ?? Datasheet says 2 <<, but then we're an octave higher than CoPilot.
		attotime period;

		frequency /= divisor;
		period = attotime::from_hz(frequency) * m_pwmw;

		m_pwm->adjust(period);
	}

	m_pwmc ^= PWMC_PIN;

	m_out_pwm_cb((offs_t)0, (m_pwmc & PWMC_PIN) ? 1 : 0);
}

TIMER_CALLBACK_MEMBER( mc68328_base_device::rtc_tick )
{
	if (m_rtcctl & RTCCTL_ENABLE)
	{
		uint32_t set_int = 0;

		m_hmsr++;

		if (m_rtcienr & RTCINT_SECOND)
		{
			set_int = 1;
			m_rtcisr |= RTCINT_SECOND;
		}

		if ((m_hmsr & 0x0000003f) == 0x0000003c)
		{
			m_hmsr &= 0xffffffc0;
			m_hmsr += 0x00010000;

			if (m_rtcienr & RTCINT_MINUTE)
			{
				set_int = 1;
				m_rtcisr |= RTCINT_MINUTE;
			}

			if ((m_hmsr & 0x003f0000) == 0x003c0000)
			{
				m_hmsr &= 0xffc0ffff;
				m_hmsr += 0x0100000;

				if ((m_hmsr & 0x1f000000) == 0x18000000)
				{
					m_hmsr &= 0xe0ffffff;

					if (m_rtcienr & RTCINT_DAY)
					{
						set_int = 1;
						m_rtcisr |= RTCINT_DAY;
					}
				}
			}

			if (m_stpwtch != 0x003f)
			{
				m_stpwtch--;
				m_stpwtch &= 0x003f;

				if (m_stpwtch == 0x003f)
				{
					if (m_rtcienr & RTCINT_STOPWATCH)
					{
						set_int = 1;
						m_rtcisr |= RTCINT_STOPWATCH;
					}
				}
			}
		}

		if (m_hmsr == m_alarm)
		{
			if (m_rtcienr & RTCINT_ALARM)
			{
				set_int = 1;
				m_rtcisr |= RTCINT_STOPWATCH;
			}
		}

		if (set_int)
		{
			set_interrupt_line(INT_RTC, 1);
		}
		else
		{
			set_interrupt_line(INT_RTC, 0);
		}
	}
}

#define COMBINE_REGISTER_MSW(reg)	\
	reg &= ~(mem_mask << 16);		\
	reg |= (data & mem_mask) << 16;

#define COMBINE_REGISTER_LSW(reg)		\
	reg &= 0xffff0000 | (~mem_mask);	\
	reg |= data & mem_mask;

WRITE16_MEMBER(mc68328_base_device::write)
{
	offset <<= 1;

	if (offset >= 0xfffff000)
	{
		regs_w(offset, data, mem_mask);
	}

	verboselog(0, "mc68328_w: Unknown address %08x=%04x (%04x)\n", offset, data, mem_mask);
}

void mc68328_base_device::regs_w(uint32_t address, uint16_t data, uint16_t mem_mask)
{
	switch (address)
	{
		case 0x000:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_w: Unknown address (0xfff001) = %02x\n", data & 0x00ff);
			}
			else
			{
				verboselog(2, "mc68328_w: SCR = %02x\n", data >> 8);
			}
			break;

		case 0x100:
			verboselog(2, "mc68328_w: GRPBASEA = %04x\n", data);
			m_grpbasea = data;
			break;

		case 0x102:
			verboselog(2, "mc68328_w: GRPBASEB = %04x\n", data);
			m_grpbaseb = data;
			break;

		case 0x104:
			verboselog(2, "mc68328_w: GRPBASEC = %04x\n", data);
			m_grpbasec = data;
			break;

		case 0x106:
			verboselog(2, "mc68328_w: GRPBASED = %04x\n", data);
			m_grpbased = data;
			break;

		case 0x108:
			verboselog(2, "mc68328_w: GRPMASKA = %04x\n", data);
			m_grpmaska = data;
			break;

		case 0x10a:
			verboselog(2, "mc68328_w: GRPMASKB = %04x\n", data);
			m_grpmaskb = data;
			break;

		case 0x10c:
			verboselog(2, "mc68328_w: GRPMASKC = %04x\n", data);
			m_grpmaskc = data;
			break;

		case 0x10e:
			verboselog(2, "mc68328_w: GRPMASKD = %04x\n", data);
			m_grpmaskd = data;
			break;

		case 0x200:
			verboselog(2, "mc68328_w: PLLCR = %04x\n", data);
			m_pllcr = data;
			break;

		case 0x202:
			verboselog(2, "mc68328_w: PLLFSR = %04x\n", data);
			m_pllfsr = data;
			break;

		case 0x206:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_w: PCTLR = %02x\n", (uint8_t)data);
				m_pctlr = (uint8_t)data;
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfff206) = %02x\n", data >> 8);
			}
			break;

		case 0x300:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_w: Unknown address (0xfff301) = %02x\n", (uint8_t)data);
			}
			else
			{
				verboselog(2, "mc68328_w: IVR = %02x\n", data >> 8);
				m_ivr = data >> 8;
			}
			break;

		case 0x302:
			verboselog(2, "mc68328_w: ICR = %04x\n", data);
			m_icr = data;
			break;

		case 0x304:
		{
			const uint32_t imr_old = m_imr;

			verboselog(2, "mc68328_w: IMR(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_imr);
			m_isr &= ~((data & mem_mask) << 16);

			const uint32_t imr_diff = imr_old ^ m_imr;
			set_interrupt_line(imr_diff, 0);
			break;
		}

		case 0x306:
		{
			const uint32_t imr_old = m_imr;

			verboselog(2, "mc68328_w: IMR(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_imr);
			m_isr &= ~(data & mem_mask);

			const uint32_t imr_diff = imr_old ^ m_imr;
			set_interrupt_line(imr_diff, 0);
			break;
		}

		case 0x308:
			verboselog(2, "mc68328_w: IWR(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_iwr);
			break;

		case 0x30a:
			verboselog(2, "mc68328_w: IWR(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_iwr);
			break;

		case 0x30c:
			verboselog(2, "mc68328_w: ISR(16) = %04x\n", data);
			// Clear edge-triggered IRQ1
			if ((m_icr & ICR_ET1) == ICR_ET1 && (data & INT_IRQ1_SHIFT) == INT_IRQ1_SHIFT)
			{
				m_isr &= ~INT_IRQ1;
			}

			// Clear edge-triggered IRQ2
			if ((m_icr & ICR_ET2) == ICR_ET2 && (data & INT_IRQ2_SHIFT) == INT_IRQ2_SHIFT)
			{
				m_isr &= ~INT_IRQ2;
			}

			// Clear edge-triggered IRQ3
			if ((m_icr & ICR_ET3) == ICR_ET3 && (data & INT_IRQ3_SHIFT) == INT_IRQ3_SHIFT)
			{
				m_isr &= ~INT_IRQ3;
			}

			// Clear edge-triggered IRQ6
			if ((m_icr & ICR_ET6) == ICR_ET6 && (data & INT_IRQ6_SHIFT) == INT_IRQ6_SHIFT)
			{
				m_isr &= ~INT_IRQ6;
			}

			// Clear edge-triggered IRQ7
			if ((data & INT_IRQ7_SHIFT) == INT_IRQ7_SHIFT)
			{
				m_isr &= ~INT_IRQ7;
			}
			break;

		case 0x30e:
			verboselog(2, "mc68328_w: ISR(0) = %04x (Ignored)\n", data);
			break;

		case 0x310:
			verboselog(2, "mc68328_w: IPR(16) = %04x (Ignored)\n", data);
			break;

		case 0x312:
			verboselog(2, "mc68328_w: IPR(0) = %04x (Ignored)\n", data);
			break;

		case 0x400:
			if (mem_mask & 0x00ff)
			{
				// TODO: WRONG! These should be broken out as separate bits.
				verboselog(2, "mc68328_w: PADATA = %02x\n", (uint8_t)data);
				m_padata = (uint8_t)data;
				m_out_port_a_cb((offs_t)0, m_padata);
			}
			else
			{
				m_padir = data >> 8;
				verboselog(2, "mc68328_w: PADIR, out:%02x, in:%02x\n", m_padir, m_padir ^ 0xff);
			}
			break;

		case 0x402:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_w: PASEL = %02x\n", data);
				m_pasel = (uint8_t)data;
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfff402) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0x408:
			if (mem_mask & 0x00ff)
			{
				// TODO: WRONG! These should be broken out as separate bits.
				verboselog(2, "mc68328_w: PBDATA = %02x\n", (uint8_t)data);
				m_pbdata = (uint8_t)data;
				m_out_port_b_cb((offs_t)0, m_pbdata);
			}
			else
			{
				m_pbdir = data >> 8;
				verboselog(2, "mc68328_w: PBDIR, out:%02x, in:%02x\n", m_pbdir, m_pbdir ^ 0xff);
			}
			break;

		case 0x40a:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_w: PBSEL = %02x\n", (uint8_t)data);
				m_pbsel = (uint8_t)data;
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfff40a) = %02x\n", data >> 8);
			}
			break;

		case 0x410:
			if (mem_mask & 0x00ff)
			{
				// TODO: WRONG! These should be broken out as separate bits.
				verboselog(2, "mc68328_w: PCDATA = %02x\n", (uint8_t)data);
				m_pcdata = (uint8_t)data;
				m_out_port_c_cb((offs_t)0, m_pcdata);
			}
			else
			{
				m_pcdir = data >> 8;
				verboselog(2, "mc68328_w: PCDIR, out:%02x, in:%02x\n", m_pcdir, m_pcdir ^ 0xff);
			}
			break;

		case 0x412:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_w: PCSEL = %02x\n", (uint8_t)data);
				m_pcsel = (uint8_t)data;
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfff412) = %02x\n", data >> 8);
			}
			break;

		case 0x418:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_w: PDDATA = %02x\n", (uint8_t)data);

				m_pddataedge &= ~(data & 0x00ff);
				poll_port_d_interrupts();
			}
			else
			{
				m_pddir = data >> 8;
				verboselog(2, "mc68328_w: PDDIR, out:%02x, in:%02x\n", m_pddir, m_pddir ^ 0xff);
			}
			break;

		case 0x41a:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_w: Unknown address (0xfff41b) = %02x\n", (uint8_t)data);
			}
			else
			{
				m_pdpuen = data >> 8;
				verboselog(2, "mc68328_w: PDPUEN = %02x\n", m_pdpuen);
			}
			break;

		case 0x41c:
			if (mem_mask & 0x00ff)
			{
				m_pdirqen = (uint8_t)data;
				verboselog(2, "mc68328_w: PDIRQEN = %02x\n", m_pdirqen);

				poll_port_d_interrupts();
			}
			else
			{
				m_pdpol = data >> 8;
				verboselog(2, "mc68328_w: PDPOL = %02x\n", m_pdpol);
			}
			break;

		case 0x41e:
			if (mem_mask & 0x00ff)
			{
				m_pdirqedge = (uint8_t)data;
				verboselog(2, "mc68328_w: PDIRQEDGE = %02x\n", m_pdirqedge);
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfff41e) = %02x\n", data >> 8);
			}
			break;

		case 0x420:
			if (mem_mask & 0x00ff)
			{
				// TODO: WRONG! These should be broken out as separate bits.
				m_pedata = (uint8_t)data;
				verboselog(2, "mc68328_w: PEDATA = %02x\n", m_pedata);
				m_out_port_e_cb((offs_t)0, m_pedata);
			}
			else
			{
				m_pedir = data >> 8;
				verboselog(2, "mc68328_w: PEDIR, out:%02x, in:%02x\n", m_pedir, m_pedir ^ 0xff);
			}
			break;

		case 0x422:
			if (mem_mask & 0x00ff)
			{
				m_pesel = (uint8_t)data;
				verboselog(2, "mc68328_w: PESEL = %02x\n", m_pesel);
			}
			else
			{
				m_pepuen = data >> 8;
				verboselog(2, "mc68328_w: PEPUEN = %02x\n", m_pepuen);
				m_pedata |= m_pepuen;
			}
			break;

		case 0x428:
			if (mem_mask & 0x00ff)
			{
				// TODO: WRONG! These should be broken out as separate bits.
				m_pfdata = (uint8_t)data;
				verboselog(2, "mc68328_w: PFDATA = %02x\n", m_pfdata);
				m_out_port_f_cb((offs_t)0, m_pfdata);
			}
			else
			{
				m_pfdir = data >> 8;
				verboselog(2, "mc68328_w: PFDIR, out:%02x, in:%02x\n", m_pfdir, m_pfdir ^ 0xff);
			}
			break;

		case 0x42a:
			if (mem_mask & 0x00ff)
			{
				m_pfsel = (uint8_t)data;
				verboselog(2, "mc68328_w: PFSEL = %02x\n", m_pfsel);
			}
			else
			{
				m_pfpuen = data >> 8;
				verboselog(2, "mc68328_w: PFPUEN = %02x\n", m_pfpuen);
			}
			break;

		case 0x430:
			if (mem_mask & 0x00ff)
			{
				// TODO: WRONG! These should be broken out as separate bits.
				m_pgdata = (uint8_t)data;
				verboselog(2, "mc68328_w: PGDATA = %02x\n", m_pgdata);
				m_out_port_g_cb((offs_t)0, m_pgdata);
			}
			else
			{
				m_pgdir = data >> 8;
				verboselog(2, "mc68328_w: PGDIR, out:%02x, in:%02x\n", m_pgdir, m_pgdir ^ 0xff);
			}
			break;

		case 0x432:
			if (mem_mask & 0x00ff)
			{
				m_pgsel = (uint8_t)data;
				verboselog(2, "mc68328_w: PGSEL = %02x\n", m_pgsel);
			}
			else
			{
				m_pgpuen = data >> 8;
				verboselog(2, "mc68328_w: PGPUEN = %02x\n", m_pgpuen);
			}
			break;

		case 0x438:
			if (mem_mask & 0x00ff)
			{
				// TODO: WRONG! These should be broken out as separate bits.
				m_pjdata = (uint8_t)data;
				verboselog(2, "mc68328_w: PJDATA = %02x\n", m_pjdata);
				m_out_port_j_cb((offs_t)0, m_pjdata);
			}
			else
			{
				m_pjdir = data >> 8;
				verboselog(2, "mc68328_w: PJDIR, out:%02x, in:%02x\n", m_pjdir, m_pjdir ^ 0xff);
			}
			break;

		case 0x43a:
			if (mem_mask & 0x00ff)
			{
				m_pjsel = (uint8_t)data;
				verboselog(2, "mc68328_w: PJSEL = %02x\n", m_pjsel);
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfff43a) = %02x\n", data >> 8);
			}
			break;

		case 0x440:
			if (mem_mask & 0x00ff)
			{
				// TODO: WRONG! These should be broken out as separate bits.
				m_pkdata = (uint8_t)data;
				verboselog(2, "mc68328_w: PKDATA = %02x\n", m_pkdata);
				m_out_port_k_cb((offs_t)0, m_pkdata);
			}
			else
			{
				m_pkdir = data >> 8;
				verboselog(2, "mc68328_w: PKDIR, out:%02x, in:%02x\n", m_pkdir, m_pkdir ^ 0xff);
			}
			break;

		case 0x442:
			if (mem_mask & 0x00ff)
			{
				m_pksel = (uint8_t)data;
				verboselog(2, "mc68328_w: PKSEL = %02x\n", m_pksel);
			}
			else
			{
				m_pgpuen = data >> 8;
				verboselog(2, "mc68328_w: PKPUEN = %02x\n", m_pgpuen);
			}
			break;

		case 0x448:
			if (mem_mask & 0x00ff)
			{
				// TODO: WRONG! These should be broken out as separate bits.
				m_pmdata = (uint8_t)data;
				verboselog(2, "mc68328_w: PMDATA = %02x\n", m_pmdata);
				m_out_port_m_cb((offs_t)0, m_pmdata);
			}
			else
			{
				m_pmdir = data >> 8;
				verboselog(2, "mc68328_w: PMDIR, out:%02x, in:%02x\n", m_pmdir, m_pmdir ^ 0xff);
			}
			break;

		case 0x44a:
			if (mem_mask & 0x00ff)
			{
				m_pmsel = (uint8_t)data;
				verboselog(2, "mc68328_w: PMSEL = %02x\n", m_pmsel);
			}
			else
			{
				m_pmpuen = data >> 8;
				verboselog(2, "mc68328_w: PMPUEN = %02x\n", m_pmpuen);
			}
			break;

		case 0x500:
			m_pwmc = data;
			verboselog(2, "mc68328_w: PWMC = %04x\n", m_pwmc);

			if (m_pwmc & PWMC_PWMIRQ)
			{
				set_interrupt_line(INT_PWM, 1);
			}

			m_pwmc &= ~PWMC_LOAD;

			if ((m_pwmc & PWMC_PWMEN) != 0 && m_pwmw != 0 && m_pwmp != 0)
			{
				uint32_t frequency = 32768 * 506;
				uint32_t divisor = 4 << (m_pwmc & PWMC_CLKSEL); // ?? Datasheet says 2 <<, but then we're an octave higher than CoPilot.
				attotime period;
				frequency /= divisor;
				period = attotime::from_hz(frequency) * m_pwmw;
				m_pwm->adjust(period);
				if (m_pwmc & PWMC_IRQEN)
				{
					set_interrupt_line(INT_PWM, 1);
				}
				m_pwmc ^= PWMC_PIN;
			}
			else
			{
				m_pwm->adjust(attotime::never);
			}
			break;

		case 0x502:
			m_pwmp = data;
			verboselog(2, "mc68328_w: PWMP = %04x\n", m_pwmp);
			break;

		case 0x504:
			m_pwmw = data;
			verboselog(2, "mc68328_w: PWMW = %04x\n", m_pwmw);
			break;

		case 0x506:
			verboselog(2, "mc68328_w: PWMCNT = %04x\n", data);
			m_pwmcnt = 0;
			break;

		case 0x600:
		{
			const uint16_t old_tctl = m_tctl[0];
			m_tctl[0] = data;
			verboselog(2, "mc68328_w: TCTL1 = %04x\n", m_tctl[0]);
			if ((old_tctl & TCTL_TEN) == (m_tctl[0] & TCTL_TEN))
			{
				maybe_start_timer(0, 0);
			}
			else if ((old_tctl & TCTL_TEN) != TCTL_TEN_ENABLE && (m_tctl[0] & TCTL_TEN) == TCTL_TEN_ENABLE)
			{
				maybe_start_timer(0, 1);
			}
			break;
		}

		case 0x602:
			m_tprer[0] = data;
			verboselog(2, "mc68328_w: TPRER1 = %04x\n", m_tprer[0]);
			maybe_start_timer(0, 0);
			break;

		case 0x604:
			m_tcmp[0] = data;
			verboselog(2, "mc68328_w: TCMP1 = %04x\n", m_tcmp[0]);
			maybe_start_timer(0, 0);
			break;

		case 0x606:
			verboselog(2, "mc68328_w: TCR1 = %04x (Ignored)\n", data);
			break;

		case 0x608:
			verboselog(2, "mc68328_w: TCN1 = %04x (Ignored)\n", data);
			break;

		case 0x60a:
			verboselog(5, "mc68328_w: TSTAT1 = %04x (Ignored)\n", data);
			m_tstat[0] &= ~m_tclear[0];
			if (!(m_tstat[0] & TSTAT_COMP))
			{
				set_interrupt_line(INT_TIMER1, 0);
			}
			break;

		case 0x60c:
		{
			const uint16_t old_tctl = m_tctl[1];
			m_tctl[1] = data;
			verboselog(2, "mc68328_w: TCTL2 = %04x\n", m_tctl[1]);
			if ((old_tctl & TCTL_TEN) == (m_tctl[1] & TCTL_TEN))
			{
				maybe_start_timer(1, 0);
			}
			else if ((old_tctl & TCTL_TEN) != TCTL_TEN_ENABLE && (m_tctl[1] & TCTL_TEN) == TCTL_TEN_ENABLE)
			{
				maybe_start_timer(1, 1);
			}
			break;
		}

		case 0x60e:
			m_tprer[1] = data;
			verboselog(2, "mc68328_w: TPRER2 = %04x\n", m_tprer[1]);
			maybe_start_timer(1, 0);
			break;

		case 0x610:
			m_tcmp[1] = data;
			verboselog(2, "mc68328_w: TCMP2 = %04x\n", m_tcmp[1]);
			maybe_start_timer(1, 0);
			break;

		case 0x612:
			verboselog(2, "mc68328_w: TCR2 = %04x (Ignored)\n", data);
			break;

		case 0x614:
			verboselog(2, "mc68328_w: TCN2 = %04x (Ignored)\n", data);
			break;

		case 0x616:
			verboselog(2, "mc68328_w: TSTAT2 = %04x (Ignored)\n", data);
			m_tstat[1] &= ~m_tclear[1];
			if (!(m_tstat[1] & TSTAT_COMP))
			{
				set_interrupt_line(INT_TIMER2, 0);
			}
			break;

		case 0x618:
			m_wctlr = data;
			verboselog(2, "mc68328_w: WCTLR = %04x\n", m_wctlr);
			break;

		case 0x61a:
			m_wcmpr = data;
			verboselog(2, "mc68328_w: WCMPR = %04x\n", m_wcmpr);
			break;

		case 0x61c:
			verboselog(2, "mc68328_w: WCN = %04x (Ignored)\n", data);
			break;

		case 0x700:
			m_spisr = data;
			verboselog(2, "mc68328_w: SPISR = %04x\n", m_spisr);
			break;

		case 0x800:
			m_spimdata = data;
			verboselog(2, "mc68328_w: SPIMDATA = %04x\n", m_spimdata);
			m_out_spim_cb(0, m_spimdata, 0xffff);
			break;

		case 0x802:
			verboselog(2, "mc68328_w: SPIMCONT = %04x\n", data);
			verboselog(3, "           Count = %d\n", data & SPIM_CLOCK_COUNT);
			verboselog(3, "           Polarity = %s\n", (data & SPIM_POL) ? "Inverted" : "Active-high");
			verboselog(3, "           Phase = %s\n", (data & SPIM_PHA) ? "Opposite" : "Normal");
			verboselog(3, "           IRQ Enable = %s\n", (data & SPIM_IRQEN) ? "Enable" : "Disable");
			verboselog(3, "           IRQ Pending = %s\n", (data & SPIM_SPIMIRQ) ? "Yes" : "No");
			verboselog(3, "           Exchange = %s\n", (data & SPIM_XCH) ? "Initiate" : "Idle");
			verboselog(3, "           SPIM Enable = %s\n", (data & SPIM_SPMEN) ? "Enable" : "Disable");
			verboselog(3, "           Data Rate = Divide By %d\n", 1 << ((((data & SPIM_RATE) >> 13) & 0x0007) + 2) );
			m_spimcont = data;
			// $$HACK$$ We should probably emulate the ADS7843 A/D device properly.
			if (data & SPIM_XCH)
			{
				m_spimcont &= ~SPIM_XCH;
				m_spim_xch_trigger_cb(0);
				if (data & SPIM_IRQEN)
				{
					m_spimcont |= SPIM_SPIMIRQ;
					verboselog(3, "Triggering SPIM Interrupt\n" );
					set_interrupt_line(INT_SPIM, 1);
				}
			}
			if (!(data & SPIM_IRQEN))
			{
				set_interrupt_line(INT_SPIM, 0);
			}
			break;

		case 0x900:
			m_ustcnt = data;
			verboselog(2, "mc68328_w: USTCNT = %04x\n", m_ustcnt);
			break;

		case 0x902:
			m_ubaud = data;
			verboselog(2, "mc68328_w: UBAUD = %04x\n", m_ubaud);
			break;

		case 0x904:
			verboselog(2, "mc68328_w: URX = %04x\n", data);
			break;

		case 0x906:
			verboselog(2, "mc68328_w: UTX = %04x\n", data);
			break;

		case 0x908:
			m_umisc = data;
			verboselog(2, "mc68328_w: UMISC = %04x\n", m_umisc);
			break;

		case 0xa00:
			verboselog(2, "mc68328_w: LSSA(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_lssa);
			verboselog(3, "              Address: %08x\n", m_lssa);
			break;

		case 0xa02:
			verboselog(2, "mc68328_w: LSSA(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_lssa);
			verboselog(3, "              Address: %08x\n", m_lssa);
			break;

		case 0xa04:
			if (mem_mask & 0x00ff)
			{
				m_lvpw = (uint8_t)data;
				verboselog(2, "mc68328_w: LVPW = %02x\n", m_lvpw);
				verboselog(3, "              Page Width: %d\n", (m_lvpw + 1) * ((m_lpicf & 0x01) ? 8 : 16));
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfffa04) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0xa08:
			m_lxmax = data;
			verboselog(2, "mc68328_w: LXMAX = %04x\n", m_lxmax);
			verboselog(3, "              Width: %d\n", (data & 0x03ff) + 1);
			break;

		case 0xa0a:
			m_lymax = data;
			verboselog(2, "mc68328_w: LYMAX = %04x\n", m_lymax);
			verboselog(3, "              Height: %d\n", (data & 0x03ff) + 1);
			break;

		case 0xa18:
			m_lcxp = data;
			verboselog(2, "mc68328_w: LCXP = %04x\n", m_lcxp);
			verboselog(3, "              X Position: %d\n", data & 0x03ff);
			switch (m_lcxp >> 14)
			{
				case 0:
					verboselog(3, "              Cursor Control: Transparent\n");
					break;

				case 1:
					verboselog(3, "              Cursor Control: Black\n");
					break;

				case 2:
					verboselog(3, "              Cursor Control: Reverse\n");
					break;

				case 3:
					verboselog(3, "              Cursor Control: Invalid\n");
					break;
			}
			break;

		case 0xa1a:
			m_lcyp = data;
			verboselog(2, "mc68328_w: LCYP = %04x\n", m_lcyp);
			verboselog(3, "              Y Position: %d\n", data & 0x01ff);
			break;

		case 0xa1c:
			m_lcwch = data;
			verboselog(2, "mc68328_w: LCWCH = %04x\n", m_lcwch);
			verboselog(3, "              Width:  %d\n", (data >> 8) & 0x1f);
			verboselog(3, "              Height: %d\n", data & 0x1f);
			break;

		case 0xa1e:
			if (mem_mask & 0x00ff)
			{
				m_lblkc = (uint8_t)data;
				verboselog(2, "mc68328_w: LBLKC = %02x\n", m_lblkc);
				verboselog(3, "              Blink Enable:  %d\n", m_lblkc >> 7);
				verboselog(3, "              Blink Divisor: %d\n", m_lblkc & 0x7f);
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfffa1e) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0xa20:
			if (mem_mask & 0x00ff)
			{
				m_lpolcf = (uint8_t)data;
				verboselog(2, "mc68328_w: LPOLCF = %02x\n", m_lpolcf);
				verboselog(3, "              LCD Shift Clock Polarity: %s\n", (m_lpicf & 0x08) ? "Active positive edge of LCLK" : "Active negative edge of LCLK");
				verboselog(3, "              First-line marker polarity: %s\n", (m_lpicf & 0x04) ? "Active Low" : "Active High");
				verboselog(3, "              Line-pulse polarity: %s\n", (m_lpicf & 0x02) ? "Active Low" : "Active High");
				verboselog(3, "              Pixel polarity: %s\n", (m_lpicf & 0x01) ? "Active Low" : "Active High");
			}
			else
			{
				m_lpicf = data >> 8;
				verboselog(2, "mc68328_w: LPICF = %02x\n", m_lpicf);
				switch((m_lpicf >> 1) & 0x03)
				{
					case 0: verboselog(3, "              Bus Size: 1-bit\n"); break;
					case 1: verboselog(3, "              Bus Size: 2-bit\n"); break;
					case 2: verboselog(3, "              Bus Size: 4-bit\n"); break;
					case 3: verboselog(3, "              Bus Size: unused\n"); break;
				}
				verboselog(3, "              Gray scale enable: %d\n", m_lpicf & 0x01);
			}
			break;

		case 0xa22:
			if (mem_mask & 0x00ff)
			{
				m_lacdrc = (uint8_t)data;
				verboselog(2, "mc68328_w: LACDRC = %02x\n", m_lacdrc);
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfffa22) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0xa24:
			if (mem_mask & 0x00ff)
			{
				m_lpxcd = (uint8_t)data;
				verboselog(2, "mc68328_w: LPXCD = %02x\n", m_lpxcd);
				verboselog(3, "              Clock Divisor: %d\n", m_lpxcd + 1);
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfffa24) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0xa26:
			if (mem_mask & 0x00ff)
			{
				m_lckcon = (uint8_t)data;
				verboselog(2, "mc68328_w: LCKCON = %02x\n", m_lckcon);
				verboselog(3, "              LCDC Enable: %d\n", (m_lckcon >> 7) & 0x01);
				verboselog(3, "              DMA Burst Length: %d\n", ((m_lckcon >> 6) & 0x01) ? 16 : 8);
				verboselog(3, "              DMA Bursting Clock Control: %d\n", ((m_lckcon >> 4) & 0x03) + 1);
				verboselog(3, "              Bus Width: %d\n", ((m_lckcon >> 1) & 0x01) ? 8 : 16);
				verboselog(3, "              Pixel Clock Divider Source: %s\n", (m_lckcon & 0x01) ? "PIX" : "SYS");
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfffa26) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0xa28:
			if (mem_mask & 0x00ff)
			{
				m_llbar = (uint8_t)data;
				verboselog(2, "mc68328_w: LLBAR = %02x\n", m_llbar);
				verboselog(3, "              Address: %d\n", (m_llbar & 0x7f) * ((m_lpicf & 0x01) ? 8 : 16));
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfffa28) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0xa2a:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_w: LOTCR = %02x\n", data & 0x00ff);
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfffa2a) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0xa2c:
			if (mem_mask & 0x00ff)
			{
				m_lposr = (uint8_t)data;
				verboselog(2, "mc68328_w: LPOSR = %02x\n", m_lposr);
				verboselog(3, "              Byte Offset: %d\n", (m_lposr >> 3) & 0x01);
				verboselog(3, "              Pixel Offset: %d\n", m_lposr & 0x07);
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfffa2c) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0xa30:
			if (mem_mask & 0x00ff)
			{
				m_lfrcm = (uint8_t)data;
				verboselog(2, "mc68328_w: LFRCM = %02x\n", m_lfrcm);
				verboselog(3, "              X Modulation: %d\n", (m_lfrcm >> 4) & 0x0f);
				verboselog(3, "              Y Modulation: %d\n", m_lfrcm & 0x0f);
			}
			else
			{
				verboselog(2, "mc68328_w: Unknown address (0xfffa30) = %02x\n", (data >> 8) & 0x00ff);
			}
			break;

		case 0xa32:
			m_lgpmr = data;
			verboselog(2, "mc68328_w: LGPMR = %04x\n", m_lgpmr);
			verboselog(3, "              Palette 0: %d\n", (m_lgpmr >>  8) & 0x07);
			verboselog(3, "              Palette 1: %d\n", (m_lgpmr >> 12) & 0x07);
			verboselog(3, "              Palette 2: %d\n", (m_lgpmr >>  0) & 0x07);
			verboselog(3, "              Palette 3: %d\n", (m_lgpmr >>  4) & 0x07);
			break;

		case 0xb00:
			verboselog(2, "mc68328_w: HMSR(0) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_hmsr);
			m_hmsr &= 0x1f3f003f;
			break;

		case 0xb02:
			verboselog(2, "mc68328_w: HMSR(16) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_hmsr);
			m_hmsr &= 0x1f3f003f;
			break;

		case 0xb04:
			verboselog(2, "mc68328_w: ALARM(0) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_alarm);
			m_alarm &= 0x1f3f003f;
			break;

		case 0xb06:
			verboselog(2, "mc68328_w: ALARM(16) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_alarm);
			m_alarm &= 0x1f3f003f;
			break;

		case 0xb0c:
			verboselog(2, "mc68328_w: RTCCTL = %04x\n", data);
			m_rtcctl = data & 0x00a0;
			break;

		case 0xb0e:
			verboselog(2, "mc68328_w: RTCISR = %04x\n", data);
			m_rtcisr &= ~data;
			if (m_rtcisr == 0)
			{
				set_interrupt_line(INT_RTC, 0);
			}
			break;

		case 0xb10:
			verboselog(2, "mc68328_w: RTCIENR = %04x\n", data);
			m_rtcienr = data & 0x001f;
			break;

		case 0xb12:
			verboselog(2, "mc68328_w: STPWTCH = %04x\n", data);
			m_stpwtch = data & 0x003f;
			break;

		default:
			verboselog(0, "mc68328_w: Unknown address (0x%08x) = %04x (%04x)\n", 0xfffff000 + address, data, mem_mask);
			break;
	}
}

READ16_MEMBER(mc68328_base_device::read)
{
	offset <<= 1;

	if (offset >= 0xfffff000)
	{
		return regs_r(offset, mem_mask);
	}

	verboselog(0, "mc68328_r: Unknown address %08x (%04x)\n", offset, mem_mask);
	return 0;
}

uint16_t mc68328_base_device::regs_r(uint32_t address, uint16_t mem_mask)
{
	switch (address)
	{
		case 0x000:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfff001)\n", mem_mask);
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): SCR = %02x\n", mem_mask, m_scr);
				return m_scr << 8;
			}
			break;

		case 0x100:
			verboselog(2, "mc68328_r (%04x): GRPBASEA = %04x\n", mem_mask, m_grpbasea);
			return m_grpbasea;

		case 0x102:
			verboselog(2, "mc68328_r (%04x): GRPBASEB = %04x\n", mem_mask, m_grpbaseb);
			return m_grpbaseb;

		case 0x104:
			verboselog(2, "mc68328_r (%04x): GRPBASEC = %04x\n", mem_mask, m_grpbasec);
			return m_grpbasec;

		case 0x106:
			verboselog(2, "mc68328_r (%04x): GRPBASED = %04x\n", mem_mask, m_grpbased);
			return m_grpbased;

		case 0x108:
			verboselog(2, "mc68328_r (%04x): GRPMASKA = %04x\n", mem_mask, m_grpmaska);
			return m_grpmaska;

		case 0x10a:
			verboselog(2, "mc68328_r (%04x): GRPMASKB = %04x\n", mem_mask, m_grpmaskb);
			return m_grpmaskb;

		case 0x10c:
			verboselog(2, "mc68328_r (%04x): GRPMASKC = %04x\n", mem_mask, m_grpmaskc);
			return m_grpmaskc;

		case 0x10e:
			verboselog(2, "mc68328_r (%04x): GRPMASKD = %04x\n", mem_mask, m_grpmaskd);
			return m_grpmaskd;

		case 0x200:
			verboselog(2, "mc68328_r (%04x): PLLCR = %04x\n", mem_mask, m_pllcr);
			return m_pllcr;

		case 0x202:
			verboselog(2, "mc68328_r (%04x): PLLFSR = %04x\n", mem_mask, m_pllfsr);
			m_pllfsr ^= 0x8000;
			return m_pllfsr;

		case 0x206:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfff206)\n", mem_mask);
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PCTLR = %02x\n", mem_mask, m_pctlr);
				return m_pctlr << 8;
			}
			break;

		case 0x300:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfff301)\n", mem_mask);
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): IVR = %02x\n", mem_mask, m_ivr);
				return m_ivr << 8;
			}
			break;

		case 0x302:
			verboselog(2, "mc68328_r (%04x): ICR = %04x\n", mem_mask, m_icr);
			return m_icr;

		case 0x304:
			verboselog(2, "mc68328_r (%04x): IMR(16) = %04x\n", mem_mask, m_imr >> 16);
			return m_imr >> 16;

		case 0x306:
			verboselog(2, "mc68328_r (%04x): IMR(0) = %04x\n", mem_mask, m_imr & 0x0000ffff);
			return m_imr & 0x0000ffff;

		case 0x308:
			verboselog(2, "mc68328_r (%04x): IWR(16) = %04x\n", mem_mask, m_iwr >> 16);
			return m_iwr >> 16;

		case 0x30a:
			verboselog(2, "mc68328_r (%04x): IWR(0) = %04x\n", mem_mask, m_iwr & 0x0000ffff);
			return m_iwr & 0x0000ffff;

		case 0x30c:
			verboselog(2, "mc68328_r (%04x): ISR(16) = %04x\n", mem_mask, m_isr >> 16);
			return m_isr >> 16;

		case 0x30e:
			verboselog(2, "mc68328_r (%04x): ISR(0) = %04x\n", mem_mask, m_isr & 0x0000ffff);
			return m_isr & 0x0000ffff;

		case 0x310:
			verboselog(2, "mc68328_r (%04x): IPR(16) = %04x\n", mem_mask, m_ipr >> 16);
			return m_ipr >> 16;

		case 0x312:
			verboselog(2, "mc68328_r (%04x): IPR(0) = %04x\n", mem_mask, m_ipr & 0x0000ffff);
			return m_ipr & 0x0000ffff;

		case 0x400:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PADATA = %02x\n", mem_mask, m_padata);
				if (!m_in_port_a_cb.isnull())
				{
					return m_in_port_a_cb(0);
				}
				else
				{
					return m_padata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PADIR = %02x\n", mem_mask, m_padir);
				return m_padir << 8;
			}

		case 0x402:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PASEL = %02x\n", mem_mask, m_pasel);
				return m_pasel;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfff402)\n", mem_mask);
			}
			break;

		case 0x408:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PBDATA = %02x\n", mem_mask, m_pbdata);
				if (!m_in_port_b_cb.isnull())
				{
					return m_in_port_b_cb(0);
				}
				else
				{
					return m_pbdata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PBDIR = %02x\n", mem_mask, m_pbdir);
				return m_pbdir << 8;
			}

		case 0x40a:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PBSEL = %02x\n", mem_mask, m_pbsel);
				return m_pbsel;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfff40a)\n", mem_mask);
			}
			break;

		case 0x410:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PCDATA = %02x\n", mem_mask, m_pcdata);
				if (!m_in_port_c_cb.isnull())
				{
					return m_in_port_c_cb(0);
				}
				else
				{
					return m_pcdata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PCDIR = %02x\n", mem_mask, m_pcdir);
				return m_pcdir << 8;
			}

		case 0x412:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PCSEL = %02x\n", mem_mask, m_pcsel);
				return m_pcsel;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfff412)\n", mem_mask);
			}
			break;

		case 0x418:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PDDATA = %02x\n", mem_mask, m_pddata);
				if (!m_in_port_d_cb.isnull())
				{
					return m_in_port_d_cb(0);
				}
				else
				{
					return m_pddata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PDDIR = %02x\n", mem_mask, m_pddir);
				return m_pddir << 8;
			}

		case 0x41a:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfff41b)\n", mem_mask);
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PDPUEN = %02x\n", mem_mask, m_pdpuen);
				return m_pdpuen << 8;
			}
			break;

		case 0x41c:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PDIRQEN = %02x\n", mem_mask, m_pdirqen);
				return m_pdirqen;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PDPOL = %02x\n", mem_mask, m_pdpol);
				return m_pdpol << 8;
			}

		case 0x41e:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PDIRQEDGE = %02x\n", mem_mask, m_pdirqedge);
				return m_pdirqedge;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfff41e)\n", mem_mask);
			}
			break;

		case 0x420:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PEDATA = %02x\n", mem_mask, m_pedata);
				if (!m_in_port_e_cb.isnull())
				{
					return m_in_port_e_cb(0);
				}
				else
				{
					return m_pedata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PEDIR = %02x\n", mem_mask, m_pedir);
				return m_pedir << 8;
			}

		case 0x422:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PESEL = %02x\n", mem_mask, m_pesel);
				return m_pesel;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PEPUEN = %02x\n", mem_mask, m_pepuen);
				return m_pepuen << 8;
			}

		case 0x428:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PFDATA = %02x\n", mem_mask, m_pfdata);
				if (!m_in_port_f_cb.isnull())
				{
					return m_in_port_f_cb(0);
				}
				else
				{
					return m_pfdata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PFDIR = %02x\n", mem_mask, m_pfdir);
				return m_pfdir << 8;
			}

		case 0x42a:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PFSEL = %02x\n", mem_mask, m_pfsel);
				return m_pfsel;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PFPUEN = %02x\n", mem_mask, m_pfpuen);
				return m_pfpuen << 8;
			}

		case 0x430:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PGDATA = %02x\n", mem_mask, m_pgdata);
				if (!m_in_port_g_cb.isnull())
				{
					return m_in_port_g_cb(0);
				}
				else
				{
					return m_pgdata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PGDIR = %02x\n", mem_mask, m_pgdir);
				return m_pgdir << 8;
			}

		case 0x432:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PGSEL = %02x\n", mem_mask, m_pgsel);
				return m_pgsel;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PGPUEN = %02x\n", mem_mask, m_pgpuen);
				return m_pgpuen << 8;
			}

		case 0x438:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PJDATA = %02x\n", mem_mask, m_pjdata);
				if (!m_in_port_j_cb.isnull())
				{
					return m_in_port_j_cb(0);
				}
				else
				{
					return m_pjdata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PJDIR = %02x\n", mem_mask, m_pjdir);
				return m_pjdir << 8;
			}

		case 0x43a:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PJSEL = %02x\n", mem_mask, m_pjsel);
				return m_pjsel;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfff43a)\n", mem_mask);
			}
			break;

		case 0x440:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PKDATA = %02x\n", mem_mask, m_pkdata);
				if (!m_in_port_k_cb.isnull())
				{
					return m_in_port_k_cb(0);
				}
				else
				{
					return m_pkdata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PKDIR = %02x\n", mem_mask, m_pkdir);
				return m_pkdir << 8;
			}

		case 0x442:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PKSEL = %02x\n", mem_mask, m_pksel);
				return m_pksel;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PKPUEN = %02x\n", mem_mask, m_pkpuen);
				return m_pkpuen << 8;
			}

		case 0x448:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PMDATA = %02x\n", mem_mask, m_pmdata);
				if (!m_in_port_m_cb.isnull())
				{
					return m_in_port_m_cb(0);
				}
				else
				{
					return m_pmdata;
				}
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PMDIR = %02x\n", mem_mask, m_pmdir);
				return m_pmdir << 8;
			}

		case 0x44a:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): PMSEL = %02x\n", mem_mask, m_pmsel);
				return m_pmsel;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): PMPUEN = %02x\n", mem_mask, m_pmpuen);
				return m_pmpuen << 8;
			}

		case 0x500:
		{
			verboselog(2, "mc68328_r (%04x): PWMC = %04x\n", mem_mask, m_pwmc);
			const uint16_t old_pwmc = m_pwmc;
			if (m_pwmc & PWMC_PWMIRQ)
			{
				m_pwmc &= ~PWMC_PWMIRQ;
				set_interrupt_line(INT_PWM, 0);
			}
			return old_pwmc;
		}

		case 0x502:
			verboselog(2, "mc68328_r (%04x): PWMP = %04x\n", mem_mask, m_pwmp);
			return m_pwmp;

		case 0x504:
			verboselog(2, "mc68328_r (%04x): PWMW = %04x\n", mem_mask, m_pwmw);
			return m_pwmw;

		case 0x506:
			verboselog(2, "mc68328_r (%04x): PWMCNT = %04x\n", mem_mask, m_pwmcnt);
			return m_pwmcnt;

		case 0x600:
			verboselog(2, "mc68328_r (%04x): TCTL1 = %04x\n", mem_mask, m_tctl[0]);
			return m_tctl[0];

		case 0x602:
			verboselog(2, "mc68328_r (%04x): TPRER1 = %04x\n", mem_mask, m_tprer[0]);
			return m_tprer[0];

		case 0x604:
			verboselog(2, "mc68328_r (%04x): TCMP1 = %04x\n", mem_mask, m_tcmp[0]);
			return m_tcmp[0];

		case 0x606:
			verboselog(2, "mc68328_r (%04x): TCR1 = %04x\n", mem_mask, m_tcr[0]);
			return m_tcr[0];

		case 0x608:
			verboselog(2, "mc68328_r (%04x): TCN1 = %04x\n", mem_mask, m_tcn[0]);
			return m_tcn[0];

		case 0x60a:
			verboselog(5, "mc68328_r (%04x): TSTAT1 = %04x\n", mem_mask, m_tstat[0]);
			m_tclear[0] |= m_tstat[0];
			return m_tstat[0];

		case 0x60c:
			verboselog(2, "mc68328_r (%04x): TCTL2 = %04x\n", mem_mask, m_tctl[1]);
			return m_tctl[1];

		case 0x60e:
			verboselog(2, "mc68328_r (%04x): TPREP2 = %04x\n", mem_mask, m_tprer[1]);
			return m_tprer[1];

		case 0x610:
			verboselog(2, "mc68328_r (%04x): TCMP2 = %04x\n", mem_mask, m_tcmp[1]);
			return m_tcmp[1];

		case 0x612:
			verboselog(2, "mc68328_r (%04x): TCR2 = %04x\n", mem_mask, m_tcr[1]);
			return m_tcr[1];

		case 0x614:
			verboselog(2, "mc68328_r (%04x): TCN2 = %04x\n", mem_mask, m_tcn[1]);
			return m_tcn[1];

		case 0x616:
			verboselog(2, "mc68328_r (%04x): TSTAT2 = %04x\n", mem_mask, m_tstat[1]);
			m_tclear[1] |= m_tstat[1];
			return m_tstat[1];

		case 0x618:
			verboselog(2, "mc68328_r (%04x): WCTLR = %04x\n", mem_mask, m_wctlr);
			return m_wctlr;

		case 0x61a:
			verboselog(2, "mc68328_r (%04x): WCMPR = %04x\n", mem_mask, m_wcmpr);
			return m_wcmpr;

		case 0x61c:
			verboselog(2, "mc68328_r (%04x): WCN = %04x\n", mem_mask, m_wcn);
			return m_wcn;

		case 0x700:
			verboselog(2, "mc68328_r (%04x): SPISR = %04x\n", mem_mask, m_spisr);
			return m_spisr;

		case 0x800:
			verboselog(2, "mc68328_r (%04x): SPIMDATA = %04x\n", mem_mask, m_spimdata);
			if (!m_in_spim_cb.isnull())
			{
				return m_in_spim_cb(0, 0xffff);
			}
			return m_spimdata;

		case 0x802:
			verboselog(2, "mc68328_r (%04x): SPIMCONT = %04x\n", mem_mask, m_spimcont);
			if (m_spimcont & SPIM_XCH)
			{
				m_spimcont &= ~SPIM_XCH;
				m_spimcont |= SPIM_SPIMIRQ;
				return ((m_spimcont | SPIM_XCH) &~ SPIM_SPIMIRQ);
			}
			return m_spimcont;

		case 0x900:
			verboselog(2, "mc68328_r (%04x): USTCNT = %04x\n", mem_mask, m_ustcnt);
			return m_ustcnt;

		case 0x902:
			verboselog(2, "mc68328_r (%04x): UBAUD = %04x\n", mem_mask, m_ubaud);
			return m_ubaud;

		case 0x904:
			verboselog(5, "mc68328_r (%04x): URX = %04x\n", mem_mask, m_urx);
			return m_urx;

		case 0x906:
			verboselog(5, "mc68328_r (%04x): UTX = %04x\n", mem_mask, m_utx);
			return m_utx | UTX_FIFO_EMPTY | UTX_FIFO_HALF | UTX_TX_AVAIL;

		case 0x908:
			verboselog(2, "mc68328_r (%04x): UMISC = %04x\n", mem_mask, m_umisc);
			return m_umisc;

		case 0xa00:
			verboselog(2, "mc68328_r (%04x): LSSA(16) = %04x\n", mem_mask, m_lssa >> 16);
			return m_lssa >> 16;

		case 0xa02:
			verboselog(2, "mc68328_r (%04x): LSSA(0) = %04x\n", mem_mask, m_lssa & 0x0000ffff);
			return m_lssa & 0x0000ffff;

		case 0xa04:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LVPW = %02x\n", mem_mask, m_lvpw);
				return m_lvpw;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfffa04)\n", mem_mask);
			}
			break;

		case 0xa08:
			verboselog(2, "mc68328_r (%04x): LXMAX = %04x\n", mem_mask, m_lxmax);
			return m_lxmax;

		case 0xa0a:
			verboselog(2, "mc68328_r (%04x): LYMAX = %04x\n", mem_mask, m_lymax);
			return m_lymax;

		case 0xa18:
			verboselog(2, "mc68328_r (%04x): LCXP = %04x\n", mem_mask, m_lcxp);
			return m_lcxp;

		case 0xa1a:
			verboselog(2, "mc68328_r (%04x): LCYP = %04x\n", mem_mask, m_lcyp);
			return m_lcyp;

		case 0xa1c:
			verboselog(2, "mc68328_r (%04x): LCWCH = %04x\n", mem_mask, m_lcwch);
			return m_lcwch;

		case 0xa1e:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LBLKC = %02x\n", mem_mask, m_lblkc);
				return m_lblkc;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfffa1e)\n", mem_mask);
			}
			break;

		case 0xa20:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LPOLCF = %02x\n", mem_mask, m_lpolcf);
				return m_lpolcf;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): LPICF = %02x\n", mem_mask, m_lpicf);
				return m_lpicf << 8;
			}

		case 0xa22:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LACDRC = %02x\n", mem_mask, m_lacdrc);
				return m_lacdrc;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfffa22)\n", mem_mask);
			}
			break;

		case 0xa24:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LPXCD = %02x\n", mem_mask, m_lpxcd);
				return m_lpxcd;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfffa24)\n", mem_mask);
			}
			break;

		case 0xa26:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LCKCON = %02x\n", mem_mask, m_lckcon);
				return m_lckcon;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfffa26)\n", mem_mask);
			}
			break;

		case 0xa28:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LLBAR = %02x\n", mem_mask, m_llbar);
				return m_llbar;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfffa28)\n", mem_mask);
			}
			break;

		case 0xa2a:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LOTCR = %02x\n", mem_mask, m_lotcr);
				return m_lotcr;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfffa2a)\n", mem_mask);
			}
			break;

		case 0xa2c:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LPOSR = %02x\n", mem_mask, m_lposr);
				return m_lposr;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfffa2c)\n", mem_mask);
			}
			break;

		case 0xa30:
			if (mem_mask & 0x00ff)
			{
				verboselog(2, "mc68328_r (%04x): LFRCM = %02x\n", mem_mask, m_lfrcm);
				return m_lfrcm;
			}
			else
			{
				verboselog(2, "mc68328_r (%04x): Unknown address (0xfffa30)\n", mem_mask);
			}
			break;

		case 0xa32:
			verboselog(2, "mc68328_r (%04x): LGPMR = %04x\n", mem_mask, m_lgpmr);
			return m_lgpmr;

		case 0xb00:
			verboselog(2, "mc68328_r (%04x): HMSR(0) = %04x\n", mem_mask, m_hmsr & 0x0000ffff);
			return m_hmsr & 0x0000ffff;

		case 0xb02:
			verboselog(2, "mc68328_r (%04x): HMSR(16) = %04x\n", mem_mask, m_hmsr >> 16);
			return m_hmsr >> 16;

		case 0xb04:
			verboselog(2, "mc68328_r (%04x): ALARM(0) = %04x\n", mem_mask, m_alarm & 0x0000ffff);
			return m_alarm & 0x0000ffff;

		case 0xb06:
			verboselog(2, "mc68328_r (%04x): ALARM(16) = %04x\n", mem_mask, m_alarm >> 16);
			return m_alarm >> 16;

		case 0xb0c:
			verboselog(2, "mc68328_r (%04x): RTCCTL = %04x\n", mem_mask, m_rtcctl);
			return m_rtcctl;

		case 0xb0e:
			verboselog(2, "mc68328_r (%04x): RTCISR = %04x\n", mem_mask, m_rtcisr);
			return m_rtcisr;

		case 0xb10:
			verboselog(2, "mc68328_r (%04x): RTCIENR = %04x\n", mem_mask, m_rtcienr);
			return m_rtcienr;

		case 0xb12:
			verboselog(2, "mc68328_r (%04x): STPWTCH = %04x\n", mem_mask, m_stpwtch);
			return m_stpwtch;

		default:
			verboselog(0, "mc68328_r (%04x): Unknown address (0x%08x)\n", mem_mask, 0xfffff000 + address);
			break;
	}
	return 0;
}

void mc68328_base_device::register_state_save()
{
	save_item(NAME(m_scr));
	save_item(NAME(m_grpbasea));
	save_item(NAME(m_grpbaseb));
	save_item(NAME(m_grpbasec));
	save_item(NAME(m_grpbased));
	save_item(NAME(m_grpmaska));
	save_item(NAME(m_grpmaskb));
	save_item(NAME(m_grpmaskc));
	save_item(NAME(m_grpmaskd));

	save_item(NAME(m_pllcr));
	save_item(NAME(m_pllfsr));
	save_item(NAME(m_pctlr));

	save_item(NAME(m_ivr));
	save_item(NAME(m_icr));
	save_item(NAME(m_imr));
	save_item(NAME(m_iwr));
	save_item(NAME(m_isr));
	save_item(NAME(m_ipr));

	save_item(NAME(m_padir));
	save_item(NAME(m_padata));
	save_item(NAME(m_pasel));
	save_item(NAME(m_pbdir));
	save_item(NAME(m_pbdata));
	save_item(NAME(m_pbsel));
	save_item(NAME(m_pcdir));
	save_item(NAME(m_pcdata));
	save_item(NAME(m_pcsel));
	save_item(NAME(m_pddir));
	save_item(NAME(m_pddata));
	save_item(NAME(m_pdpuen));
	save_item(NAME(m_pdpol));
	save_item(NAME(m_pdirqen));
	save_item(NAME(m_pddataedge));
	save_item(NAME(m_pdirqedge));
	save_item(NAME(m_pedir));
	save_item(NAME(m_pedata));
	save_item(NAME(m_pepuen));
	save_item(NAME(m_pesel));
	save_item(NAME(m_pfdir));
	save_item(NAME(m_pfdata));
	save_item(NAME(m_pfpuen));
	save_item(NAME(m_pfsel));
	save_item(NAME(m_pgdir));
	save_item(NAME(m_pgdata));
	save_item(NAME(m_pgpuen));
	save_item(NAME(m_pgsel));
	save_item(NAME(m_pjdir));
	save_item(NAME(m_pjdata));
	save_item(NAME(m_pjsel));
	save_item(NAME(m_pkdir));
	save_item(NAME(m_pkdata));
	save_item(NAME(m_pkpuen));
	save_item(NAME(m_pksel));
	save_item(NAME(m_pmdir));
	save_item(NAME(m_pmdata));
	save_item(NAME(m_pmpuen));
	save_item(NAME(m_pmsel));

	save_item(NAME(m_pwmc));
	save_item(NAME(m_pwmp));
	save_item(NAME(m_pwmw));
	save_item(NAME(m_pwmcnt));

	save_item(NAME(m_tctl[0]));
	save_item(NAME(m_tctl[1]));
	save_item(NAME(m_tprer[0]));
	save_item(NAME(m_tprer[1]));
	save_item(NAME(m_tcmp[0]));
	save_item(NAME(m_tcmp[1]));
	save_item(NAME(m_tcr[0]));
	save_item(NAME(m_tcr[1]));
	save_item(NAME(m_tcn[0]));
	save_item(NAME(m_tcn[1]));
	save_item(NAME(m_tstat[0]));
	save_item(NAME(m_tstat[1]));
	save_item(NAME(m_wctlr));
	save_item(NAME(m_wcmpr));
	save_item(NAME(m_wcn));

	save_item(NAME(m_spisr));

	save_item(NAME(m_spimdata));
	save_item(NAME(m_spimcont));

	save_item(NAME(m_ustcnt));
	save_item(NAME(m_ubaud));
	save_item(NAME(m_urx));
	save_item(NAME(m_utx));
	save_item(NAME(m_umisc));

	save_item(NAME(m_lssa));
	save_item(NAME(m_lvpw));
	save_item(NAME(m_lxmax));
	save_item(NAME(m_lymax));
	save_item(NAME(m_lcxp));
	save_item(NAME(m_lcyp));
	save_item(NAME(m_lcwch));
	save_item(NAME(m_lblkc));
	save_item(NAME(m_lpicf));
	save_item(NAME(m_lpolcf));
	save_item(NAME(m_lacdrc));
	save_item(NAME(m_lpxcd));
	save_item(NAME(m_lckcon));
	save_item(NAME(m_llbar));
	save_item(NAME(m_lotcr));
	save_item(NAME(m_lposr));
	save_item(NAME(m_lfrcm));
	save_item(NAME(m_lgpmr));

	save_item(NAME(m_hmsr));
	save_item(NAME(m_alarm));
	save_item(NAME(m_rtcctl));
	save_item(NAME(m_rtcisr));
	save_item(NAME(m_rtcienr));
	save_item(NAME(m_stpwtch));
}



mc68328_device::mc68328_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mc68328_base_device(mconfig, MC68328, tag, owner, clock)
{
}

void mc68328_device::register_state_save()
{
	mc68328_base_device::register_state_save();

	save_item(NAME(m_csa0));
	save_item(NAME(m_csa1));
	save_item(NAME(m_csa2));
	save_item(NAME(m_csa3));
	save_item(NAME(m_csb0));
	save_item(NAME(m_csb1));
	save_item(NAME(m_csb2));
	save_item(NAME(m_csb3));
	save_item(NAME(m_csc0));
	save_item(NAME(m_csc1));
	save_item(NAME(m_csc2));
	save_item(NAME(m_csc3));
	save_item(NAME(m_csd0));
	save_item(NAME(m_csd1));
	save_item(NAME(m_csd2));
	save_item(NAME(m_csd3));
}

void mc68328_device::device_reset()
{
	m_csa0 = 0x00010006;
	m_csa1 = 0x00010006;
	m_csa2 = 0x00010006;
	m_csa3 = 0x00010006;
	m_csb0 = 0x00010006;
	m_csb1 = 0x00010006;
	m_csb2 = 0x00010006;
	m_csb3 = 0x00010006;
	m_csc0 = 0x00010006;
	m_csc1 = 0x00010006;
	m_csc2 = 0x00010006;
	m_csc3 = 0x00010006;
	m_csd0 = 0x00010006;
	m_csd1 = 0x00010006;
	m_csd2 = 0x00010006;
	m_csd3 = 0x00010006;
}

void mc68328_device::regs_w(uint32_t address, uint16_t data, uint16_t mem_mask)
{
	switch (address)
	{
		case 0x110:
			verboselog(5, "mc68328_w: CSA0(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csa0);
			break;

		case 0x112:
			verboselog(5, "mc68328_w: CSA0(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csa0);
			break;

		case 0x114:
			verboselog(5, "mc68328_w: CSA1(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csa1);
			break;

		case 0x116:
			verboselog(5, "mc68328_w: CSA1(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csa1);
			break;

		case 0x118:
			verboselog(5, "mc68328_w: CSA2(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csa2);
			break;

		case 0x11a:
			verboselog(5, "mc68328_w: CSA2(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csa2);
			break;

		case 0x11c:
			verboselog(5, "mc68328_w: CSA3(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csa3);
			break;

		case 0x11e:
			verboselog(5, "mc68328_w: CSA3(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csa3);
			break;

		case 0x120:
			verboselog(5, "mc68328_w: CSB0(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csb0);
			break;

		case 0x122:
			verboselog(5, "mc68328_w: CSB0(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csb0);
			break;

		case 0x124:
			verboselog(5, "mc68328_w: CSB1(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csb1);
			break;

		case 0x126:
			verboselog(5, "mc68328_w: CSB1(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csb1);
			break;

		case 0x128:
			verboselog(5, "mc68328_w: CSB2(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csb2);
			break;

		case 0x12a:
			verboselog(5, "mc68328_w: CSB2(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csb2);
			break;

		case 0x12c:
			verboselog(5, "mc68328_w: CSB3(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csb3);
			break;

		case 0x12e:
			verboselog(5, "mc68328_w: CSB3(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csb3);
			break;

		case 0x130:
			verboselog(5, "mc68328_w: CSC0(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csc0);
			break;

		case 0x132:
			verboselog(5, "mc68328_w: CSC0(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csc0);
			break;

		case 0x134:
			verboselog(5, "mc68328_w: CSC1(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csc1);
			break;

		case 0x136:
			verboselog(5, "mc68328_w: CSC1(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csc1);
			break;

		case 0x138:
			verboselog(5, "mc68328_w: CSC2(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csc2);
			break;

		case 0x13a:
			verboselog(5, "mc68328_w: CSC2(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csc2);
			break;

		case 0x13c:
			verboselog(5, "mc68328_w: CSC3(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csc3);
			break;

		case 0x13e:
			verboselog(5, "mc68328_w: CSC3(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csc3);
			break;

		case 0x140:
			verboselog(5, "mc68328_w: CSD0(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csd0);
			break;

		case 0x142:
			verboselog(5, "mc68328_w: CSD0(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csd0);
			break;

		case 0x144:
			verboselog(5, "mc68328_w: CSD1(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csd1);
			break;

		case 0x146:
			verboselog(5, "mc68328_w: CSD1(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csd1);
			break;

		case 0x148:
			verboselog(5, "mc68328_w: CSD2(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csd2);
			break;

		case 0x14a:
			verboselog(5, "mc68328_w: CSD2(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csd2);
			break;

		case 0x14c:
			verboselog(5, "mc68328_w: CSD3(0) = %04x\n", data);
			COMBINE_REGISTER_LSW(m_csd3);
			break;

		case 0x14e:
			verboselog(5, "mc68328_w: CSD3(16) = %04x\n", data);
			COMBINE_REGISTER_MSW(m_csd3);
			break;

		default:
			mc68328_base_device::regs_w(address, data, mem_mask);
			return;
	}
}

uint16_t mc68328_device::regs_r(uint32_t address, uint16_t mem_mask)
{
	switch (address)
	{
		case 0x110:
			verboselog(5, "mc68328_r (%04x): CSA0(0) = %04x\n", mem_mask, m_csa0 & 0x0000ffff);
			return m_csa0 & 0x0000ffff;

		case 0x112:
			verboselog(5, "mc68328_r (%04x): CSA0(16) = %04x\n", mem_mask, m_csa0 >> 16);
			return m_csa0 >> 16;

		case 0x114:
			verboselog(5, "mc68328_r (%04x): CSA1(0) = %04x\n", mem_mask, m_csa1 & 0x0000ffff);
			return m_csa1 & 0x0000ffff;

		case 0x116:
			verboselog(5, "mc68328_r (%04x): CSA1(16) = %04x\n", mem_mask, m_csa1 >> 16);
			return m_csa1 >> 16;

		case 0x118:
			verboselog(5, "mc68328_r (%04x): CSA2(0) = %04x\n", mem_mask, m_csa2 & 0x0000ffff);
			return m_csa2 & 0x0000ffff;

		case 0x11a:
			verboselog(5, "mc68328_r (%04x): CSA2(16) = %04x\n", mem_mask, m_csa2 >> 16);
			return m_csa2 >> 16;

		case 0x11c:
			verboselog(5, "mc68328_r (%04x): CSA3(0) = %04x\n", mem_mask, m_csa3 & 0x0000ffff);
			return m_csa3 & 0x0000ffff;

		case 0x11e:
			verboselog(5, "mc68328_r (%04x): CSA3(16) = %04x\n", mem_mask, m_csa3 >> 16);
			return m_csa3 >> 16;

		case 0x120:
			verboselog(5, "mc68328_r (%04x): CSB0(0) = %04x\n", mem_mask, m_csb0 & 0x0000ffff);
			return m_csb0 & 0x0000ffff;

		case 0x122:
			verboselog(5, "mc68328_r (%04x): CSB0(16) = %04x\n", mem_mask, m_csb0 >> 16);
			return m_csb0 >> 16;

		case 0x124:
			verboselog(5, "mc68328_r (%04x): CSB1(0) = %04x\n", mem_mask, m_csb1 & 0x0000ffff);
			return m_csb1 & 0x0000ffff;

		case 0x126:
			verboselog(5, "mc68328_r (%04x): CSB1(16) = %04x\n", mem_mask, m_csb1 >> 16);
			return m_csb1 >> 16;

		case 0x128:
			verboselog(5, "mc68328_r (%04x): CSB2(0) = %04x\n", mem_mask, m_csb2 & 0x0000ffff);
			return m_csb2 & 0x0000ffff;

		case 0x12a:
			verboselog(5, "mc68328_r (%04x): CSB2(16) = %04x\n", mem_mask, m_csb2 >> 16);
			return m_csb2 >> 16;

		case 0x12c:
			verboselog(5, "mc68328_r (%04x): CSB3(0) = %04x\n", mem_mask, m_csb3 & 0x0000ffff);
			return m_csb3 & 0x0000ffff;

		case 0x12e:
			verboselog(5, "mc68328_r (%04x): CSB3(16) = %04x\n", mem_mask, m_csb3 >> 16);
			return m_csb3 >> 16;

		case 0x130:
			verboselog(5, "mc68328_r (%04x): CSC0(0) = %04x\n", mem_mask, m_csc0 & 0x0000ffff);
			return m_csc0 & 0x0000ffff;

		case 0x132:
			verboselog(5, "mc68328_r (%04x): CSC0(16) = %04x\n", mem_mask, m_csc0 >> 16);
			return m_csc0 >> 16;

		case 0x134:
			verboselog(5, "mc68328_r (%04x): CSC1(0) = %04x\n", mem_mask, m_csc1 & 0x0000ffff);
			return m_csc1 & 0x0000ffff;

		case 0x136:
			verboselog(5, "mc68328_r (%04x): CSC1(16) = %04x\n", mem_mask, m_csc1 >> 16);
			return m_csc1 >> 16;

		case 0x138:
			verboselog(5, "mc68328_r (%04x): CSC2(0) = %04x\n", mem_mask, m_csc2 & 0x0000ffff);
			return m_csc2 & 0x0000ffff;

		case 0x13a:
			verboselog(5, "mc68328_r (%04x): CSC2(16) = %04x\n", mem_mask, m_csc2 >> 16);
			return m_csc2 >> 16;

		case 0x13c:
			verboselog(5, "mc68328_r (%04x): CSC3(0) = %04x\n", mem_mask, m_csc3 & 0x0000ffff);
			return m_csc3 & 0x0000ffff;

		case 0x13e:
			verboselog(5, "mc68328_r (%04x): CSC3(16) = %04x\n", mem_mask, m_csc3 >> 16);
			return m_csc3 >> 16;

		case 0x140:
			verboselog(5, "mc68328_r (%04x): CSD0(0) = %04x\n", mem_mask, m_csd0 & 0x0000ffff);
			return m_csd0 & 0x0000ffff;

		case 0x142:
			verboselog(5, "mc68328_r (%04x): CSD0(16) = %04x\n", mem_mask, m_csd0 >> 16);
			return m_csd0 >> 16;

		case 0x144:
			verboselog(5, "mc68328_r (%04x): CSD1(0) = %04x\n", mem_mask, m_csd1 & 0x0000ffff);
			return m_csd1 & 0x0000ffff;

		case 0x146:
			verboselog(5, "mc68328_r (%04x): CSD1(16) = %04x\n", mem_mask, m_csd1 >> 16);
			return m_csd1 >> 16;

		case 0x148:
			verboselog(5, "mc68328_r (%04x): CSD2(0) = %04x\n", mem_mask, m_csd2 & 0x0000ffff);
			return m_csd2 & 0x0000ffff;

		case 0x14a:
			verboselog(5, "mc68328_r (%04x): CSD2(16) = %04x\n", mem_mask, m_csd2 >> 16);
			return m_csd2 >> 16;

		case 0x14c:
			verboselog(5, "mc68328_r (%04x): CSD3(0) = %04x\n", mem_mask, m_csd3 & 0x0000ffff);
			return m_csd3 & 0x0000ffff;

		case 0x14e:
			verboselog(5, "mc68328_r (%04x): CSD3(16) = %04x\n", mem_mask, m_csd3 >> 16);
			return m_csd3 >> 16;

		default:
			return mc68328_base_device::regs_r(address, mem_mask);
	}
}

uint32_t mc68328_device::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	uint16_t *video_ram = (uint16_t *)(machine().device<ram_device>(RAM_TAG)->pointer() + (m_lssa & 0x00ffffff));

	if (m_lckcon & LCKCON_LCDC_EN)
	{
		for (int y = 0; y < 160; y++)
		{
			uint16_t *line = &bitmap.pix16(y);

			for (int x = 0; x < 160; x += 16)
			{
				const uint16_t word = *(video_ram++);
				for (int b = 0; b < 16; b++)
				{
					line[x + b] = BIT(word, 15 - b);
				}
			}
		}
	}
	else
	{
		for (int y = 0; y < 160; y++)
		{
			uint16_t *line = &bitmap.pix16(y);

			for (int x = 0; x < 160; x++)
			{
				line[x] = 0;
			}
		}
	}
	return 0;
}



mc68vz328_device::mc68vz328_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mc68328_base_device(mconfig, MC68VZ328, tag, owner, clock)
	, m_in_boot(true)
	, m_boot_region(*this, finder_base::DUMMY_TAG)
	, m_ram(*this, finder_base::DUMMY_TAG)
{
}

void mc68vz328_device::device_reset()
{
	m_in_boot = true;
}

void mc68vz328_device::register_state_save()
{
	mc68328_base_device::register_state_save();
}

WRITE16_MEMBER(mc68vz328_device::mem_w)
{
	const uint32_t address = offset << 1;

	if (address >= 0xfffff000)
	{
		regs_w(address & 0xfff, data, mem_mask);
	}
	else if (address >= 0x10000000 || m_in_boot)
	{
		//verboselog(0, "mem_w: %08x = %04x (%04x)\n", address, data, mem_mask);
	}
}

READ16_MEMBER(mc68vz328_device::mem_r)
{
	const uint32_t address = offset << 1;

	if (address >= 0xfffff000)
	{
		return regs_r(address & 0xfff, mem_mask);
	}
	else if (address >= 0x10000000 || m_in_boot)
	{
		const uint32_t region_address = address % m_boot_region->bytes();
		const uint16_t foo = m_boot_region->as_u16(region_address >> 1);
		//verboselog(0, "mem_r: %08x = %04x (%04x)\n", address, foo, mem_mask);
		return foo;
	}

	return 0;
}

void mc68vz328_device::regs_w(uint32_t address, uint16_t data, uint16_t mem_mask)
{
	switch (address)
	{
		case 0x10a:
		{
			m_in_boot = false;

			// Technically we should exit boot-mode when the CPU has enabled the requisite chip-select, but this register is written to at around the right time, so whatever
			address_space &space = m_cpu->space(AS_PROGRAM);
			space.install_read_bank(0x00000000, m_ram->size() - 1, "bank1");
			space.install_write_bank(0x00000000, m_ram->size() - 1, "bank1");
			membank("^bank1")->set_base(m_ram->pointer());

			mc68328_base_device::regs_w(address, data, mem_mask);
			break;
		}

		default:
			mc68328_base_device::regs_w(address, data, mem_mask);
			return;
	}
}

uint16_t mc68vz328_device::regs_r(uint32_t address, uint16_t mem_mask)
{
	switch (address)
	{
		default:
			return mc68328_base_device::regs_r(address, mem_mask);
	}
}

uint32_t mc68vz328_device::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	for (int y = 0; y < 160; y++)
	{
		uint16_t *line = &bitmap.pix16(y);

		for (int x = 0; x < 160; x++)
		{
			line[x] = 0;
		}
	}
	return 0;
}

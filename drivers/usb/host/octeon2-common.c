/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2010, 2011 Cavium Networks
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <asm/octeon/octeon.h>
#include <asm/octeon/cvmx-uctlx-defs.h>

static DEFINE_MUTEX(octeon2_usb_clocks_mutex);

static int octeon2_usb_clock_start_cnt;

void octeon2_usb_clocks_start(void)
{
	u64 div;
	union cvmx_uctlx_if_ena if_ena;
	union cvmx_uctlx_clk_rst_ctl clk_rst_ctl;
	union cvmx_uctlx_uphy_ctl_status uphy_ctl_status;
	union cvmx_uctlx_uphy_portx_ctl_status port_ctl_status;
	int i;
	unsigned long io_clk_64_to_ns;


	mutex_lock(&octeon2_usb_clocks_mutex);

	octeon2_usb_clock_start_cnt++;
	if (octeon2_usb_clock_start_cnt != 1)
		goto exit;

	io_clk_64_to_ns = 64000000000ull / octeon_get_io_clock_rate();

	if_ena.u64 = 0;
	if_ena.s.en = 1;
	cvmx_write_csr(CVMX_UCTLX_IF_ENA(0), if_ena.u64);

	
	clk_rst_ctl.u64 = cvmx_read_csr(CVMX_UCTLX_CLK_RST_CTL(0));

	if (clk_rst_ctl.s.hrst)
		goto end_clock;
	
	clk_rst_ctl.s.p_por = 1;
	clk_rst_ctl.s.hrst = 0;
	clk_rst_ctl.s.p_prst = 0;
	clk_rst_ctl.s.h_clkdiv_rst = 0;
	clk_rst_ctl.s.o_clkdiv_rst = 0;
	clk_rst_ctl.s.h_clkdiv_en = 0;
	clk_rst_ctl.s.o_clkdiv_en = 0;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);

	
	
	clk_rst_ctl.s.p_refclk_sel = 0;
	clk_rst_ctl.s.p_refclk_div = 0;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);

	
	div = octeon_get_io_clock_rate() / 130000000ull;

	switch (div) {
	case 0:
		div = 1;
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		break;
	case 5:
		div = 4;
		break;
	case 6:
	case 7:
		div = 6;
		break;
	case 8:
	case 9:
	case 10:
	case 11:
		div = 8;
		break;
	default:
		div = 12;
		break;
	}
	clk_rst_ctl.s.h_div = div;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);
	
	clk_rst_ctl.u64 = cvmx_read_csr(CVMX_UCTLX_CLK_RST_CTL(0));
	clk_rst_ctl.s.h_clkdiv_en = 1;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);
	
	clk_rst_ctl.s.h_clkdiv_rst = 1;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);

	
	ndelay(io_clk_64_to_ns);

	clk_rst_ctl.s.p_por = 0;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);

	
	mdelay(1);

	uphy_ctl_status.u64 = cvmx_read_csr(CVMX_UCTLX_UPHY_CTL_STATUS(0));
	uphy_ctl_status.s.ate_reset = 1;
	cvmx_write_csr(CVMX_UCTLX_UPHY_CTL_STATUS(0), uphy_ctl_status.u64);

	
	ndelay(10);

	
	uphy_ctl_status.s.ate_reset = 0;
	cvmx_write_csr(CVMX_UCTLX_UPHY_CTL_STATUS(0), uphy_ctl_status.u64);

	ndelay(20);

	
	
	clk_rst_ctl.s.o_clkdiv_rst = 1;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);

	
	clk_rst_ctl.s.o_clkdiv_en = 1;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);

	
	ndelay(io_clk_64_to_ns);

	clk_rst_ctl.s.p_prst = 1;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);

	
	udelay(1);

	
	clk_rst_ctl.s.hrst = 1;
	cvmx_write_csr(CVMX_UCTLX_CLK_RST_CTL(0), clk_rst_ctl.u64);

end_clock:
	

	for (i = 0; i <= 1; i++) {
		port_ctl_status.u64 =
			cvmx_read_csr(CVMX_UCTLX_UPHY_PORTX_CTL_STATUS(i, 0));
		
		port_ctl_status.s.txvreftune = 15;
		port_ctl_status.s.txrisetune = 1;
		port_ctl_status.s.txpreemphasistune = 1;
		cvmx_write_csr(CVMX_UCTLX_UPHY_PORTX_CTL_STATUS(i, 0),
			       port_ctl_status.u64);
	}

	
	cvmx_write_csr(CVMX_UCTLX_EHCI_FLA(0), 0x20ull);
exit:
	mutex_unlock(&octeon2_usb_clocks_mutex);
}
EXPORT_SYMBOL(octeon2_usb_clocks_start);

void octeon2_usb_clocks_stop(void)
{
	mutex_lock(&octeon2_usb_clocks_mutex);
	octeon2_usb_clock_start_cnt--;
	mutex_unlock(&octeon2_usb_clocks_mutex);
}
EXPORT_SYMBOL(octeon2_usb_clocks_stop);

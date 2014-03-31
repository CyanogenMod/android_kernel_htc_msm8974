#ifndef __GRLIB_APBUART_H__
#define __GRLIB_APBUART_H__

#include <asm/io.h>

#define UART_NR		8
static int grlib_apbuart_port_nr;

struct grlib_apbuart_regs_map {
	u32 data;
	u32 status;
	u32 ctrl;
	u32 scaler;
};

struct amba_prom_registers {
	unsigned int phys_addr;
	unsigned int reg_size;
};

#define UART_STATUS_DR   0x00000001	
#define UART_STATUS_TSE  0x00000002	
#define UART_STATUS_THE  0x00000004	
#define UART_STATUS_BR   0x00000008	
#define UART_STATUS_OE   0x00000010	
#define UART_STATUS_PE   0x00000020	
#define UART_STATUS_FE   0x00000040	
#define UART_STATUS_ERR  0x00000078	

#define UART_CTRL_RE     0x00000001	
#define UART_CTRL_TE     0x00000002	
#define UART_CTRL_RI     0x00000004	
#define UART_CTRL_TI     0x00000008	
#define UART_CTRL_PS     0x00000010	
#define UART_CTRL_PE     0x00000020	
#define UART_CTRL_FL     0x00000040	
#define UART_CTRL_LB     0x00000080	

#define APBBASE(port) ((struct grlib_apbuart_regs_map *)((port)->membase))

#define APBBASE_DATA_P(port)	(&(APBBASE(port)->data))
#define APBBASE_STATUS_P(port)	(&(APBBASE(port)->status))
#define APBBASE_CTRL_P(port)	(&(APBBASE(port)->ctrl))
#define APBBASE_SCALAR_P(port)	(&(APBBASE(port)->scaler))

#define UART_GET_CHAR(port)	(__raw_readl(APBBASE_DATA_P(port)))
#define UART_PUT_CHAR(port, v)	(__raw_writel(v, APBBASE_DATA_P(port)))
#define UART_GET_STATUS(port)	(__raw_readl(APBBASE_STATUS_P(port)))
#define UART_PUT_STATUS(port, v)(__raw_writel(v, APBBASE_STATUS_P(port)))
#define UART_GET_CTRL(port)	(__raw_readl(APBBASE_CTRL_P(port)))
#define UART_PUT_CTRL(port, v)	(__raw_writel(v, APBBASE_CTRL_P(port)))
#define UART_GET_SCAL(port)	(__raw_readl(APBBASE_SCALAR_P(port)))
#define UART_PUT_SCAL(port, v)	(__raw_writel(v, APBBASE_SCALAR_P(port)))

#define UART_RX_DATA(s)		(((s) & UART_STATUS_DR) != 0)
#define UART_TX_READY(s)	(((s) & UART_STATUS_THE) != 0)

#endif 

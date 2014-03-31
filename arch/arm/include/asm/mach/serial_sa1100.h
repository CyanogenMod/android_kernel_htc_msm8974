
struct uart_port;
struct uart_info;

struct sa1100_port_fns {
	void	(*set_mctrl)(struct uart_port *, u_int);
	u_int	(*get_mctrl)(struct uart_port *);
	void	(*pm)(struct uart_port *, u_int, u_int);
	int	(*set_wake)(struct uart_port *, u_int);
};

#ifdef CONFIG_SERIAL_SA1100
void sa1100_register_uart_fns(struct sa1100_port_fns *fns);
void sa1100_register_uart(int idx, int port);
#else
#define sa1100_register_uart_fns(fns) do { } while (0)
#define sa1100_register_uart(idx,port) do { } while (0)
#endif

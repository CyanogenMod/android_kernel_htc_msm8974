
#ifndef __LINUX_SPI_AT73C213_H
#define __LINUX_SPI_AT73C213_H

struct at73c213_board_info {
	int		ssc_id;
	struct clk	*dac_clk;
	char		shortname[32];
};

#endif 

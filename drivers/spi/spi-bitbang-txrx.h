

static inline u32
bitbang_txrx_be_cpha0(struct spi_device *spi,
		unsigned nsecs, unsigned cpol, unsigned flags,
		u32 word, u8 bits)
{
	

	
	for (word <<= (32 - bits); likely(bits); bits--) {

		
		if ((flags & SPI_MASTER_NO_TX) == 0)
			setmosi(spi, word & (1 << 31));
		spidelay(nsecs);	

		setsck(spi, !cpol);
		spidelay(nsecs);

		
		word <<= 1;
		if ((flags & SPI_MASTER_NO_RX) == 0)
			word |= getmiso(spi);
		setsck(spi, cpol);
	}
	return word;
}

static inline u32
bitbang_txrx_be_cpha1(struct spi_device *spi,
		unsigned nsecs, unsigned cpol, unsigned flags,
		u32 word, u8 bits)
{
	

	
	for (word <<= (32 - bits); likely(bits); bits--) {

		
		setsck(spi, !cpol);
		if ((flags & SPI_MASTER_NO_TX) == 0)
			setmosi(spi, word & (1 << 31));
		spidelay(nsecs); 

		setsck(spi, cpol);
		spidelay(nsecs);

		
		word <<= 1;
		if ((flags & SPI_MASTER_NO_RX) == 0)
			word |= getmiso(spi);
	}
	return word;
}

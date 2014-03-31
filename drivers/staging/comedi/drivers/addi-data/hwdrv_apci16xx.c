/**
@verbatim

Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.

	ADDI-DATA GmbH
	Dieselstrasse 3
	D-77833 Ottersweier
	Tel: +19(0)7223/9493-0
	Fax: +49(0)7223/9493-92
	http://www.addi-data.com
	info@addi-data.com

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

You should also find the complete GPL in the COPYING file accompanying this source code.

@endverbatim
*/


#include "hwdrv_apci16xx.h"


int i_APCI16XX_InsnConfigInitTTLIO(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_Command = 0;
	unsigned char b_Cpt = 0;
	unsigned char b_NumberOfPort =
		(unsigned char) (this_board->i_NbrTTLChannel / 8);

	
	
	

	if (insn->n >= 1) {
	   
		
		

		b_Command = (unsigned char) data[0];

	   
		
	   

		if ((b_Command == APCI16XX_TTL_INIT) ||
			(b_Command == APCI16XX_TTL_INITDIRECTION) ||
			(b_Command == APCI16XX_TTL_OUTPUTMEMORY)) {
	      
			
	      

			if ((b_Command == APCI16XX_TTL_INITDIRECTION)
				&& ((unsigned char) (insn->n - 1) != b_NumberOfPort)) {
		 
				
		 

				printk("\nBuffer size error");
				i_ReturnValue = -101;
			}

			if ((b_Command == APCI16XX_TTL_OUTPUTMEMORY)
				&& ((unsigned char) (insn->n) != 2)) {
		 
				
		 

				printk("\nBuffer size error");
				i_ReturnValue = -101;
			}
		} else {
	      
			
	      

			printk("\nCommand selection error");
			i_ReturnValue = -100;
		}
	} else {
	   
		
	   

		printk("\nBuffer size error");
		i_ReturnValue = -101;
	}

	
	
	

	if ((i_ReturnValue >= 0) && (b_Command == APCI16XX_TTL_INITDIRECTION)) {
		memset(devpriv->ul_TTLPortConfiguration, 0,
			sizeof(devpriv->ul_TTLPortConfiguration));

	   
		
	   

		for (b_Cpt = 1;
			(b_Cpt <= b_NumberOfPort) && (i_ReturnValue >= 0);
			b_Cpt++) {
	      
			
	      

			if ((data[b_Cpt] != 0) && (data[b_Cpt] != 0xFF)) {
		 
				
		 

				printk("\nPort %d direction selection error",
					(int) b_Cpt);
				i_ReturnValue = -(int) b_Cpt;
			}

	      
			
	      

			devpriv->ul_TTLPortConfiguration[(b_Cpt - 1) / 4] =
				devpriv->ul_TTLPortConfiguration[(b_Cpt -
					1) / 4] | (data[b_Cpt] << (8 * ((b_Cpt -
							1) % 4)));
		}
	}

	
	
	

	if (i_ReturnValue >= 0) {
	   
		
	   

		if ((b_Command == APCI16XX_TTL_INIT)
			|| (b_Command == APCI16XX_TTL_INITDIRECTION)) {
	      
			
	      

			for (b_Cpt = 0; b_Cpt <= b_NumberOfPort; b_Cpt++) {
				if ((b_Cpt % 4) == 0) {
		    
					
		    

					outl(devpriv->
						ul_TTLPortConfiguration[b_Cpt /
							4],
						devpriv->iobase + 32 + b_Cpt);
				}
			}
		}
	}

	
	
	

	if (b_Command == APCI16XX_TTL_OUTPUTMEMORY) {
		if (data[1]) {
			devpriv->b_OutputMemoryStatus = ADDIDATA_ENABLE;
		} else {
			devpriv->b_OutputMemoryStatus = ADDIDATA_DISABLE;
		}
	}

	return i_ReturnValue;
}



int i_APCI16XX_InsnBitsReadTTLIO(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_Command = 0;
	unsigned char b_NumberOfPort =
		(unsigned char) (this_board->i_NbrTTLChannel / 8);
	unsigned char b_SelectedPort = CR_RANGE(insn->chanspec);
	unsigned char b_InputChannel = CR_CHAN(insn->chanspec);
	unsigned char *pb_Status;
	unsigned int dw_Status;

	
	
	

	if (insn->n >= 1) {
	   
		
		

		b_Command = (unsigned char) data[0];

	   
		
	   

		if ((b_Command == APCI16XX_TTL_READCHANNEL)
			|| (b_Command == APCI16XX_TTL_READPORT)) {
	      
			
	      

			if (b_SelectedPort < b_NumberOfPort) {
		 
				
		 

				if (((devpriv->ul_TTLPortConfiguration
							[b_SelectedPort /
								4] >> (8 *
								(b_SelectedPort
									%
									4))) &
						0xFF) == 0) {
		    
					
		    

					if ((b_Command ==
							APCI16XX_TTL_READCHANNEL)
						&& (b_InputChannel > 7)) {
		       
						
		       

						printk("\nChannel selection error");
						i_ReturnValue = -103;
					}
				} else {
		    
					
		    

					printk("\nPort selection error");
					i_ReturnValue = -102;
				}
			} else {
		 
				
		 

				printk("\nPort selection error");
				i_ReturnValue = -102;
			}
		} else {
	      
			
	      

			printk("\nCommand selection error");
			i_ReturnValue = -100;
		}
	} else {
	   
		
	   

		printk("\nBuffer size error");
		i_ReturnValue = -101;
	}

	
	
	

	if (i_ReturnValue >= 0) {
		pb_Status = (unsigned char *) &data[0];

	   
		
	   

		dw_Status =
			inl(devpriv->iobase + 8 + ((b_SelectedPort / 4) * 4));
		dw_Status = (dw_Status >> (8 * (b_SelectedPort % 4))) & 0xFF;

	   
		
	   

		*pb_Status = (unsigned char) dw_Status;

	   
		
	   

		if (b_Command == APCI16XX_TTL_READCHANNEL) {
			*pb_Status = (*pb_Status >> b_InputChannel) & 1;
		}
	}

	return i_ReturnValue;
}


int i_APCI16XX_InsnReadTTLIOAllPortValue(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	unsigned char b_Command = (unsigned char) CR_AREF(insn->chanspec);
	int i_ReturnValue = insn->n;
	unsigned char b_Cpt = 0;
	unsigned char b_NumberOfPort = 0;
	unsigned int *pls_ReadData = data;

	
	
	

	if ((b_Command == APCI16XX_TTL_READ_ALL_INPUTS)
		|| (b_Command == APCI16XX_TTL_READ_ALL_OUTPUTS)) {
	   
		
	   

		b_NumberOfPort =
			(unsigned char) (this_board->i_NbrTTLChannel / 32);
		if ((b_NumberOfPort * 32) <
			this_board->i_NbrTTLChannel) {
			b_NumberOfPort = b_NumberOfPort + 1;
		}

	   
		
	   

		if (insn->n >= b_NumberOfPort) {
			if (b_Command == APCI16XX_TTL_READ_ALL_INPUTS) {
		 
				
		 

				for (b_Cpt = 0; b_Cpt < b_NumberOfPort; b_Cpt++) {
		    
					
		    

					pls_ReadData[b_Cpt] =
						inl(devpriv->iobase + 8 +
						(b_Cpt * 4));

		    
					
		    

					pls_ReadData[b_Cpt] =
						pls_ReadData[b_Cpt] &
						(~devpriv->
						ul_TTLPortConfiguration[b_Cpt]);
				}
			} else {
		 
				
		 

				for (b_Cpt = 0; b_Cpt < b_NumberOfPort; b_Cpt++) {
		    
					
		    

					pls_ReadData[b_Cpt] =
						inl(devpriv->iobase + 20 +
						(b_Cpt * 4));

		    
					
		    

					pls_ReadData[b_Cpt] =
						pls_ReadData[b_Cpt] & devpriv->
						ul_TTLPortConfiguration[b_Cpt];
				}
			}
		} else {
	      
			
	      

			printk("\nBuffer size error");
			i_ReturnValue = -101;
		}
	} else {
	   
		
	   

		printk("\nCommand selection error");
		i_ReturnValue = -100;
	}

	return i_ReturnValue;
}



int i_APCI16XX_InsnBitsWriteTTLIO(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_Command = 0;
	unsigned char b_NumberOfPort =
		(unsigned char) (this_board->i_NbrTTLChannel / 8);
	unsigned char b_SelectedPort = CR_RANGE(insn->chanspec);
	unsigned char b_OutputChannel = CR_CHAN(insn->chanspec);
	unsigned int dw_Status = 0;

	
	
	

	if (insn->n >= 1) {
	   
		
		

		b_Command = (unsigned char) data[0];

	   
		
	   

		if ((b_Command == APCI16XX_TTL_WRITECHANNEL_ON) ||
			(b_Command == APCI16XX_TTL_WRITEPORT_ON) ||
			(b_Command == APCI16XX_TTL_WRITECHANNEL_OFF) ||
			(b_Command == APCI16XX_TTL_WRITEPORT_OFF)) {
	      
			
	      

			if (b_SelectedPort < b_NumberOfPort) {
		 
				
		 

				if (((devpriv->ul_TTLPortConfiguration
							[b_SelectedPort /
								4] >> (8 *
								(b_SelectedPort
									%
									4))) &
						0xFF) == 0xFF) {
		    
					
		    

					if (((b_Command == APCI16XX_TTL_WRITECHANNEL_ON) || (b_Command == APCI16XX_TTL_WRITECHANNEL_OFF)) && (b_OutputChannel > 7)) {
		       
						
		       

						printk("\nChannel selection error");
						i_ReturnValue = -103;
					}

					if (((b_Command == APCI16XX_TTL_WRITECHANNEL_OFF) || (b_Command == APCI16XX_TTL_WRITEPORT_OFF)) && (devpriv->b_OutputMemoryStatus == ADDIDATA_DISABLE)) {
		       
						
		       

						printk("\nOutput memory disabled");
						i_ReturnValue = -104;
					}

		    
					
		    

					if (((b_Command == APCI16XX_TTL_WRITEPORT_ON) || (b_Command == APCI16XX_TTL_WRITEPORT_OFF)) && (insn->n < 2)) {
		       
						
		       

						printk("\nBuffer size error");
						i_ReturnValue = -101;
					}
				} else {
		    
					
		    

					printk("\nPort selection error %lX",
						(unsigned long)devpriv->
						ul_TTLPortConfiguration[0]);
					i_ReturnValue = -102;
				}
			} else {
		 
				
		 

				printk("\nPort selection error %d %d",
					b_SelectedPort, b_NumberOfPort);
				i_ReturnValue = -102;
			}
		} else {
	      
			
	      

			printk("\nCommand selection error");
			i_ReturnValue = -100;
		}
	} else {
	   
		
	   

		printk("\nBuffer size error");
		i_ReturnValue = -101;
	}

	
	
	

	if (i_ReturnValue >= 0) {
	   
		
	   

		dw_Status =
			inl(devpriv->iobase + 20 + ((b_SelectedPort / 4) * 4));

	   
		
	   

		if (devpriv->b_OutputMemoryStatus == ADDIDATA_DISABLE) {
	      
			
	      

			dw_Status =
				dw_Status & (0xFFFFFFFFUL -
				(0xFFUL << (8 * (b_SelectedPort % 4))));
		}

	   
		
	   

		if (b_Command == APCI16XX_TTL_WRITECHANNEL_ON) {
			dw_Status =
				dw_Status | (1UL << ((8 * (b_SelectedPort %
							4)) + b_OutputChannel));
		}

	   
		
	   

		if (b_Command == APCI16XX_TTL_WRITEPORT_ON) {
			dw_Status =
				dw_Status | ((data[1] & 0xFF) << (8 *
					(b_SelectedPort % 4)));
		}

	   
		
	   

		if (b_Command == APCI16XX_TTL_WRITECHANNEL_OFF) {
			dw_Status =
				dw_Status & (0xFFFFFFFFUL -
				(1UL << ((8 * (b_SelectedPort % 4)) +
						b_OutputChannel)));
		}

	   
		
	   

		if (b_Command == APCI16XX_TTL_WRITEPORT_OFF) {
			dw_Status =
				dw_Status & (0xFFFFFFFFUL -
				((data[1] & 0xFF) << (8 * (b_SelectedPort %
							4))));
		}

		outl(dw_Status,
			devpriv->iobase + 20 + ((b_SelectedPort / 4) * 4));
	}

	return i_ReturnValue;
}


int i_APCI16XX_Reset(struct comedi_device *dev)
{
	return 0;
}

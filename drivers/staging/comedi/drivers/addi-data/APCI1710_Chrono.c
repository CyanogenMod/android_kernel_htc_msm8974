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

#include "APCI1710_Chrono.h"


int i_APCI1710_InsnConfigInitChrono(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int ul_TimerValue = 0;
	unsigned int ul_TimingInterval = 0;
	unsigned int ul_RealTimingInterval = 0;
	double d_RealTimingInterval = 0;
	unsigned int dw_ModeArray[8] =
		{ 0x01, 0x05, 0x00, 0x04, 0x02, 0x0E, 0x0A, 0x06 };
	unsigned char b_ModulNbr, b_ChronoMode, b_PCIInputClock, b_TimingUnit;

	b_ModulNbr = CR_AREF(insn->chanspec);
	b_ChronoMode = (unsigned char) data[0];
	b_PCIInputClock = (unsigned char) data[1];
	b_TimingUnit = (unsigned char) data[2];
	ul_TimingInterval = (unsigned int) data[3];
	i_ReturnValue = insn->n;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_CHRONOMETER) {
	      
			
	      

			if (b_ChronoMode <= 7) {
		 
				
		 

				if ((b_PCIInputClock == APCI1710_30MHZ) ||
					(b_PCIInputClock == APCI1710_33MHZ) ||
					(b_PCIInputClock == APCI1710_40MHZ)) {
		    
					
		    

					if (b_TimingUnit <= 4) {
		       
						
		       

						if (((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 0) && (ul_TimingInterval >= 66) && (ul_TimingInterval <= 0xFFFFFFFFUL)) || ((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 1) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 143165576UL)) || ((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 2) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 143165UL)) || ((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 3) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 143UL)) || ((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 4) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 2UL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 0) && (ul_TimingInterval >= 60) && (ul_TimingInterval <= 0xFFFFFFFFUL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 1) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 130150240UL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 2) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 130150UL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 3) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 130UL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 4) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 2UL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 0) && (ul_TimingInterval >= 50) && (ul_TimingInterval <= 0xFFFFFFFFUL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 1) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 107374182UL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 2) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 107374UL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 3) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 107UL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 4) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 1UL))) {
			  
							
			  

							if (((b_PCIInputClock == APCI1710_40MHZ) && (devpriv->s_BoardInfos.b_BoardVersion > 0)) || (b_PCIInputClock != APCI1710_40MHZ)) {
			     
								
			     

								if (((b_PCIInputClock == APCI1710_40MHZ) && ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF) >= 0x3131)) || (b_PCIInputClock != APCI1710_40MHZ)) {
									fpu_begin
										();

				
									
				

									switch (b_TimingUnit) {
				   
										
				   

									case 0:

					   
										
					   

										ul_TimerValue
											=
											(unsigned int)
											(ul_TimingInterval
											*
											(0.001 * b_PCIInputClock));

					   
										
					   

										if ((double)((double)ul_TimingInterval * (0.001 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
											ul_TimerValue
												=
												ul_TimerValue
												+
												1;
										}

					   
										
					   

										ul_RealTimingInterval
											=
											(unsigned int)
											(ul_TimerValue
											/
											(0.001 * (double)b_PCIInputClock));
										d_RealTimingInterval
											=
											(double)
											ul_TimerValue
											/
											(0.001
											*
											(double)
											b_PCIInputClock);

										if ((double)((double)ul_TimerValue / (0.001 * (double)b_PCIInputClock)) >= (double)((double)ul_RealTimingInterval + 0.5)) {
											ul_RealTimingInterval
												=
												ul_RealTimingInterval
												+
												1;
										}

										ul_TimingInterval
											=
											ul_TimingInterval
											-
											1;
										ul_TimerValue
											=
											ul_TimerValue
											-
											2;
										if (b_PCIInputClock != APCI1710_40MHZ) {
											ul_TimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_TimerValue)
												*
												0.99392);
										}

										break;

				   
										
				   

									case 1:

					   
										
					   

										ul_TimerValue
											=
											(unsigned int)
											(ul_TimingInterval
											*
											(1.0 * b_PCIInputClock));

					   
										
					   

										if ((double)((double)ul_TimingInterval * (1.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
											ul_TimerValue
												=
												ul_TimerValue
												+
												1;
										}

					   
										
					   

										ul_RealTimingInterval
											=
											(unsigned int)
											(ul_TimerValue
											/
											(1.0 * (double)b_PCIInputClock));
										d_RealTimingInterval
											=
											(double)
											ul_TimerValue
											/
											(
											(double)
											1.0
											*
											(double)
											b_PCIInputClock);

										if ((double)((double)ul_TimerValue / (1.0 * (double)b_PCIInputClock)) >= (double)((double)ul_RealTimingInterval + 0.5)) {
											ul_RealTimingInterval
												=
												ul_RealTimingInterval
												+
												1;
										}

										ul_TimingInterval
											=
											ul_TimingInterval
											-
											1;
										ul_TimerValue
											=
											ul_TimerValue
											-
											2;
										if (b_PCIInputClock != APCI1710_40MHZ) {
											ul_TimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_TimerValue)
												*
												0.99392);
										}

										break;

				   
										
				   

									case 2:

					   
										
					   

										ul_TimerValue
											=
											ul_TimingInterval
											*
											(1000
											*
											b_PCIInputClock);

					   
										
					   

										if ((double)((double)ul_TimingInterval * (1000.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
											ul_TimerValue
												=
												ul_TimerValue
												+
												1;
										}

					   
										
					   

										ul_RealTimingInterval
											=
											(unsigned int)
											(ul_TimerValue
											/
											(1000.0 * (double)b_PCIInputClock));
										d_RealTimingInterval
											=
											(double)
											ul_TimerValue
											/
											(1000.0
											*
											(double)
											b_PCIInputClock);

										if ((double)((double)ul_TimerValue / (1000.0 * (double)b_PCIInputClock)) >= (double)((double)ul_RealTimingInterval + 0.5)) {
											ul_RealTimingInterval
												=
												ul_RealTimingInterval
												+
												1;
										}

										ul_TimingInterval
											=
											ul_TimingInterval
											-
											1;
										ul_TimerValue
											=
											ul_TimerValue
											-
											2;
										if (b_PCIInputClock != APCI1710_40MHZ) {
											ul_TimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_TimerValue)
												*
												0.99392);
										}

										break;

				   
										
				   

									case 3:

					   
										
					   

										ul_TimerValue
											=
											(unsigned int)
											(ul_TimingInterval
											*
											(1000000.0
												*
												b_PCIInputClock));

					   
										
					   

										if ((double)((double)ul_TimingInterval * (1000000.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
											ul_TimerValue
												=
												ul_TimerValue
												+
												1;
										}

					   
										
					   

										ul_RealTimingInterval
											=
											(unsigned int)
											(ul_TimerValue
											/
											(1000000.0
												*
												(double)
												b_PCIInputClock));
										d_RealTimingInterval
											=
											(double)
											ul_TimerValue
											/
											(1000000.0
											*
											(double)
											b_PCIInputClock);

										if ((double)((double)ul_TimerValue / (1000000.0 * (double)b_PCIInputClock)) >= (double)((double)ul_RealTimingInterval + 0.5)) {
											ul_RealTimingInterval
												=
												ul_RealTimingInterval
												+
												1;
										}

										ul_TimingInterval
											=
											ul_TimingInterval
											-
											1;
										ul_TimerValue
											=
											ul_TimerValue
											-
											2;
										if (b_PCIInputClock != APCI1710_40MHZ) {
											ul_TimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_TimerValue)
												*
												0.99392);
										}

										break;

				   
										
				   

									case 4:

					   
										
					   

										ul_TimerValue
											=
											(unsigned int)
											(
											(ul_TimingInterval
												*
												60)
											*
											(1000000.0
												*
												b_PCIInputClock));

					   
										
					   

										if ((double)((double)(ul_TimingInterval * 60.0) * (1000000.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
											ul_TimerValue
												=
												ul_TimerValue
												+
												1;
										}

					   
										
					   

										ul_RealTimingInterval
											=
											(unsigned int)
											(ul_TimerValue
											/
											(1000000.0
												*
												(double)
												b_PCIInputClock))
											/
											60;
										d_RealTimingInterval
											=
											(
											(double)
											ul_TimerValue
											/
											(0.001 * (double)b_PCIInputClock)) / 60.0;

										if ((double)(((double)ul_TimerValue / (1000000.0 * (double)b_PCIInputClock)) / 60.0) >= (double)((double)ul_RealTimingInterval + 0.5)) {
											ul_RealTimingInterval
												=
												ul_RealTimingInterval
												+
												1;
										}

										ul_TimingInterval
											=
											ul_TimingInterval
											-
											1;
										ul_TimerValue
											=
											ul_TimerValue
											-
											2;
										if (b_PCIInputClock != APCI1710_40MHZ) {
											ul_TimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_TimerValue)
												*
												0.99392);
										}

										break;
									}

									fpu_end();

				
									
				

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_ChronoModuleInfo.
										b_PCIInputClock
										=
										b_PCIInputClock;

				
									
				

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_ChronoModuleInfo.
										b_TimingUnit
										=
										b_TimingUnit;

				
									
				

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_ChronoModuleInfo.
										d_TimingInterval
										=
										d_RealTimingInterval;

				
									
				

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_ChronoModuleInfo.
										dw_ConfigReg
										=
										dw_ModeArray
										[b_ChronoMode];

				
									
				

									if (b_PCIInputClock == APCI1710_40MHZ) {
										devpriv->
											s_ModuleInfo
											[b_ModulNbr].
											s_ChronoModuleInfo.
											dw_ConfigReg
											=
											devpriv->
											s_ModuleInfo
											[b_ModulNbr].
											s_ChronoModuleInfo.
											dw_ConfigReg
											|
											0x80;
									}

									outl(devpriv->s_ModuleInfo[b_ModulNbr].s_ChronoModuleInfo.dw_ConfigReg, devpriv->s_BoardInfos.ui_Address + 16 + (64 * b_ModulNbr));

				
									
				

									outl(ul_TimerValue, devpriv->s_BoardInfos.ui_Address + (64 * b_ModulNbr));

				
									
				

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_ChronoModuleInfo.
										b_ChronoInit
										=
										1;
								} else {
				
									
				

									DPRINTK("TOR version error for 40MHz clock selection\n");
									i_ReturnValue
										=
										-9;
								}
							} else {
			     
								
			     

								DPRINTK("You can not used the 40MHz clock selection with this board\n");
								i_ReturnValue =
									-8;
							}
						} else {
			  
							
			  

							DPRINTK("Base timing selection is wrong\n");
							i_ReturnValue = -7;
						}
					}	
					else {
		       
						
		       

						DPRINTK("Timing unity selection is wrong\n");
						i_ReturnValue = -6;
					}	
				}	
				else {
		    
					
		    

					DPRINTK("The selected PCI input clock is wrong\n");
					i_ReturnValue = -5;
				}	
			}	
			else {
		 
				
		 

				DPRINTK("Chronometer mode selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
	      
			
	      

			DPRINTK("The module is not a Chronometer module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}
	data[0] = ul_RealTimingInterval;
	return i_ReturnValue;
}


int i_APCI1710_InsnWriteEnableDisableChrono(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned char b_ModulNbr, b_CycleMode, b_InterruptEnable, b_Action;
	b_ModulNbr = CR_AREF(insn->chanspec);
	b_Action = (unsigned char) data[0];
	b_CycleMode = (unsigned char) data[1];
	b_InterruptEnable = (unsigned char) data[2];
	i_ReturnValue = insn->n;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_CHRONOMETER) {
	      
			
	      

			if (devpriv->s_ModuleInfo[b_ModulNbr].
				s_ChronoModuleInfo.b_ChronoInit == 1) {

				switch (b_Action) {

				case APCI1710_ENABLE:

		 
					
		 

					if ((b_CycleMode == APCI1710_SINGLE)
						|| (b_CycleMode ==
							APCI1710_CONTINUOUS)) {
		    
						
		    

						if ((b_InterruptEnable ==
								APCI1710_ENABLE)
							|| (b_InterruptEnable ==
								APCI1710_DISABLE))
						{

			  
							
			  

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_ChronoModuleInfo.
								b_InterruptMask
								=
								b_InterruptEnable;

			  
							
			  

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_ChronoModuleInfo.
								b_CycleMode =
								b_CycleMode;

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_ChronoModuleInfo.
								dw_ConfigReg =
								(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_ChronoModuleInfo.
								dw_ConfigReg &
								0x8F) | ((1 &
									b_InterruptEnable)
								<< 5) | ((1 &
									b_CycleMode)
								<< 6) | 0x10;

			  
							
			  

							if (b_InterruptEnable ==
								APCI1710_ENABLE)
							{
			     
								
			     

								outl(devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_ChronoModuleInfo.
									dw_ConfigReg,
									devpriv->
									s_BoardInfos.
									ui_Address
									+ 32 +
									(64 * b_ModulNbr));
								devpriv->tsk_Current = current;	
							}

			  
							
							
			  

							outl(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_ChronoModuleInfo.
								dw_ConfigReg,
								devpriv->
								s_BoardInfos.
								ui_Address +
								16 +
								(64 * b_ModulNbr));

			  
							
			  

							outl(0, devpriv->
								s_BoardInfos.
								ui_Address +
								36 +
								(64 * b_ModulNbr));

						}	
						else {
		       
							
		       

							DPRINTK("Interrupt parameter is wrong\n");
							i_ReturnValue = -6;
						}	
					}	
					else {
		    
						
		    

						DPRINTK("Chronometer acquisition mode cycle is wrong\n");
						i_ReturnValue = -5;
					}	
					break;

				case APCI1710_DISABLE:

					devpriv->s_ModuleInfo[b_ModulNbr].
						s_ChronoModuleInfo.
						b_InterruptMask = 0;

					devpriv->s_ModuleInfo[b_ModulNbr].
						s_ChronoModuleInfo.
						dw_ConfigReg =
						devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_ChronoModuleInfo.
						dw_ConfigReg & 0x2F;

		 
					
					
		 

					outl(devpriv->s_ModuleInfo[b_ModulNbr].
						s_ChronoModuleInfo.dw_ConfigReg,
						devpriv->s_BoardInfos.
						ui_Address + 16 +
						(64 * b_ModulNbr));

		 
					
		 

					if (devpriv->s_ModuleInfo[b_ModulNbr].
						s_ChronoModuleInfo.
						b_CycleMode ==
						APCI1710_CONTINUOUS) {
		    
						
		    

						outl(0, devpriv->s_BoardInfos.
							ui_Address + 36 +
							(64 * b_ModulNbr));
					}
					break;

				default:
					DPRINTK("Inputs wrong! Enable or Disable chrono\n");
					i_ReturnValue = -8;
				}	
			} else {
		 
				
		 

				DPRINTK("Chronometer not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
	      

			DPRINTK("The module is not a Chronometer module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InsnReadChrono(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned char b_ReadType;
	int i_ReturnValue = insn->n;

	b_ReadType = CR_CHAN(insn->chanspec);

	switch (b_ReadType) {
	case APCI1710_CHRONO_PROGRESS_STATUS:
		i_ReturnValue = i_APCI1710_GetChronoProgressStatus(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char *) &data[0]);
		break;

	case APCI1710_CHRONO_READVALUE:
		i_ReturnValue = i_APCI1710_ReadChronoValue(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned int) insn->unused[0],
			(unsigned char *) &data[0], (unsigned int *) &data[1]);
		break;

	case APCI1710_CHRONO_CONVERTVALUE:
		i_ReturnValue = i_APCI1710_ConvertChronoValue(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned int) insn->unused[0],
			(unsigned int *) &data[0],
			(unsigned char *) &data[1],
			(unsigned char *) &data[2],
			(unsigned int *) &data[3],
			(unsigned int *) &data[4], (unsigned int *) &data[5]);
		break;

	case APCI1710_CHRONO_READINTERRUPT:
		printk("In Chrono Read Interrupt\n");

		data[0] = devpriv->s_InterruptParameters.
			s_FIFOInterruptParameters[devpriv->
			s_InterruptParameters.ui_Read].b_OldModuleMask;
		data[1] = devpriv->s_InterruptParameters.
			s_FIFOInterruptParameters[devpriv->
			s_InterruptParameters.ui_Read].ul_OldInterruptMask;
		data[2] = devpriv->s_InterruptParameters.
			s_FIFOInterruptParameters[devpriv->
			s_InterruptParameters.ui_Read].ul_OldCounterLatchValue;

			     
		
			     

		devpriv->
			s_InterruptParameters.
			ui_Read = (devpriv->
			s_InterruptParameters.
			ui_Read + 1) % APCI1710_SAVE_INTERRUPT;
		break;

	default:
		printk("ReadType Parameter wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;

}


int i_APCI1710_GetChronoProgressStatus(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char *pb_ChronoStatus)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_CHRONOMETER) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_ChronoModuleInfo.b_ChronoInit == 1) {

				dw_Status = inl(devpriv->s_BoardInfos.
					ui_Address + 8 + (64 * b_ModulNbr));

		 
				
		 

				if ((dw_Status & 8) == 8) {
		    
					
		    

					*pb_ChronoStatus = 3;
				}	
				else {
		    
					
		    

					if ((dw_Status & 2) == 2) {
		       
						
		       

						*pb_ChronoStatus = 2;
					}	
					else {
		       
						
		       

						if ((dw_Status & 1) == 1) {
			  
							
			  

							*pb_ChronoStatus = 1;
						}	
						else {
			  
							
			  

							*pb_ChronoStatus = 0;
						}	
					}	
				}	
			} else {
		 
				
		 
				DPRINTK("Chronometer not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
	      
			DPRINTK("The module is not a Chronometer module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_ReadChronoValue(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned int ui_TimeOut, unsigned char *pb_ChronoStatus, unsigned int *pul_ChronoValue)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status;
	unsigned int dw_TimeOut = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_CHRONOMETER) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_ChronoModuleInfo.b_ChronoInit == 1) {
		 
				
		 

				if (ui_TimeOut <= 65535UL) {

					for (;;) {
			  
						
			  

						dw_Status =
							inl(devpriv->
							s_BoardInfos.
							ui_Address + 8 +
							(64 * b_ModulNbr));

			  
						
			  

						if ((dw_Status & 8) == 8) {
			     
							
			     

							*pb_ChronoStatus = 3;

			     
							
			     

							if (devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_ChronoModuleInfo.
								b_CycleMode ==
								APCI1710_CONTINUOUS)
							{
				
								
				

								outl(0, devpriv->s_BoardInfos.ui_Address + 36 + (64 * b_ModulNbr));
							}

							break;
						}	
						else {
			     
							
			     

							if ((dw_Status & 2) ==
								2) {
				
								
				

								*pb_ChronoStatus
									= 2;

				
								
				

								if (devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_ChronoModuleInfo.
									b_CycleMode
									==
									APCI1710_CONTINUOUS)
								{
				   
									
				   

									outl(0, devpriv->s_BoardInfos.ui_Address + 36 + (64 * b_ModulNbr));
								}
								break;
							}	
							else {
				
								
				

								if ((dw_Status & 1) == 1) {
				   
									
				   

									*pb_ChronoStatus
										=
										1;
								}	
								else {
				   
									
				   

									*pb_ChronoStatus
										=
										0;
								}	
							}	
						}	

						if (dw_TimeOut == ui_TimeOut) {
			     
							
			     

							break;
						} else {
			     
							
			     

							dw_TimeOut =
								dw_TimeOut + 1;
							mdelay(1000);

						}
					}	

		       
					
		       

					if (*pb_ChronoStatus == 2) {
			  
						
			  

						*pul_ChronoValue =
							inl(devpriv->
							s_BoardInfos.
							ui_Address + 4 +
							(64 * b_ModulNbr));

						if (*pul_ChronoValue != 0) {
							*pul_ChronoValue =
								*pul_ChronoValue
								- 1;
						}
					} else {
			  
						
			  

						if ((*pb_ChronoStatus != 3)
							&& (dw_TimeOut ==
								ui_TimeOut)
							&& (ui_TimeOut != 0)) {
			     
							
			     

							*pb_ChronoStatus = 4;
						}
					}

				} else {
		    
					
		    
					DPRINTK("Timeout parameter is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 
				
		 
				DPRINTK("Chronometer not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
	      
			DPRINTK("The module is not a Chronometer module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_ConvertChronoValue(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned int ul_ChronoValue,
	unsigned int *pul_Hour,
	unsigned char *pb_Minute,
	unsigned char *pb_Second,
	unsigned int *pui_MilliSecond, unsigned int *pui_MicroSecond, unsigned int *pui_NanoSecond)
{
	int i_ReturnValue = 0;
	double d_Hour;
	double d_Minute;
	double d_Second;
	double d_MilliSecond;
	double d_MicroSecond;
	double d_NanoSecond;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_CHRONOMETER) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_ChronoModuleInfo.b_ChronoInit == 1) {
				fpu_begin();

				d_Hour = (double)ul_ChronoValue *(double)
					devpriv->s_ModuleInfo[b_ModulNbr].
					s_ChronoModuleInfo.d_TimingInterval;

				switch (devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_ChronoModuleInfo.b_TimingUnit) {
				case 0:
					d_Hour = d_Hour / (double)1000.0;

				case 1:
					d_Hour = d_Hour / (double)1000.0;

				case 2:
					d_Hour = d_Hour / (double)1000.0;

				case 3:
					d_Hour = d_Hour / (double)60.0;

				case 4:
			    
					
			    

					d_Hour = d_Hour / (double)60.0;
					*pul_Hour = (unsigned int) d_Hour;

			    
					
			    

					d_Minute = d_Hour - *pul_Hour;
					d_Minute = d_Minute * 60;
					*pb_Minute = (unsigned char) d_Minute;

			    
					
			    

					d_Second = d_Minute - *pb_Minute;
					d_Second = d_Second * 60;
					*pb_Second = (unsigned char) d_Second;

			    
					
			    

					d_MilliSecond = d_Second - *pb_Second;
					d_MilliSecond = d_MilliSecond * 1000;
					*pui_MilliSecond = (unsigned int) d_MilliSecond;

			    
					
			    

					d_MicroSecond =
						d_MilliSecond -
						*pui_MilliSecond;
					d_MicroSecond = d_MicroSecond * 1000;
					*pui_MicroSecond = (unsigned int) d_MicroSecond;

			    
					
			    

					d_NanoSecond =
						d_MicroSecond -
						*pui_MicroSecond;
					d_NanoSecond = d_NanoSecond * 1000;
					*pui_NanoSecond = (unsigned int) d_NanoSecond;
					break;
				}

				fpu_end();
			} else {
		 
				
		 
				DPRINTK("Chronometer not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
	      
			DPRINTK("The module is not a Chronometer module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}





int i_APCI1710_InsnBitsChronoDigitalIO(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned char b_ModulNbr, b_OutputChannel, b_InputChannel, b_IOType;
	unsigned int dw_Status;
	unsigned char *pb_ChannelStatus;
	unsigned char *pb_PortValue;

	b_ModulNbr = CR_AREF(insn->chanspec);
	i_ReturnValue = insn->n;
	b_IOType = (unsigned char) data[0];

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_CHRONOMETER) {
	      
			
	      

			if (devpriv->s_ModuleInfo[b_ModulNbr].
				s_ChronoModuleInfo.b_ChronoInit == 1) {
		 
				
		 
				switch (b_IOType) {

				case APCI1710_CHRONO_SET_CHANNELOFF:

					b_OutputChannel =
						(unsigned char) CR_CHAN(insn->chanspec);
					if (b_OutputChannel <= 2) {

						outl(0, devpriv->s_BoardInfos.
							ui_Address + 20 +
							(b_OutputChannel * 4) +
							(64 * b_ModulNbr));
					}	
					else {
		    
						
		    

						DPRINTK("The selected digital output is wrong\n");
						i_ReturnValue = -4;

					}	

					break;

				case APCI1710_CHRONO_SET_CHANNELON:

					b_OutputChannel =
						(unsigned char) CR_CHAN(insn->chanspec);
					if (b_OutputChannel <= 2) {

						outl(1, devpriv->s_BoardInfos.
							ui_Address + 20 +
							(b_OutputChannel * 4) +
							(64 * b_ModulNbr));
					}	
					else {
		    
						
		    

						DPRINTK("The selected digital output is wrong\n");
						i_ReturnValue = -4;

					}	

					break;

				case APCI1710_CHRONO_READ_CHANNEL:
		 
					
		 
					pb_ChannelStatus = (unsigned char *) &data[0];
					b_InputChannel =
						(unsigned char) CR_CHAN(insn->chanspec);

					if (b_InputChannel <= 2) {

						dw_Status =
							inl(devpriv->
							s_BoardInfos.
							ui_Address + 12 +
							(64 * b_ModulNbr));

						*pb_ChannelStatus =
							(unsigned char) (((dw_Status >>
									b_InputChannel)
								& 1) ^ 1);
					}	
					else {
		    
						
		    

						DPRINTK("The selected digital input is wrong\n");
						i_ReturnValue = -4;
					}	

					break;

				case APCI1710_CHRONO_READ_PORT:

					pb_PortValue = (unsigned char *) &data[0];

					dw_Status =
						inl(devpriv->s_BoardInfos.
						ui_Address + 12 +
						(64 * b_ModulNbr));

					*pb_PortValue =
						(unsigned char) ((dw_Status & 0x7) ^ 7);
					break;
				}
			} else {
		 
				
		 

				DPRINTK("Chronometer not initialised\n");
				i_ReturnValue = -5;
			}
		} else {
	      
			
	      

			DPRINTK("The module is not a Chronometer module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

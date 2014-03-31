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


#include "APCI1710_Tor.h"


int i_APCI1710_InsnConfigInitTorCounter(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int ul_TimerValue = 0;
	unsigned int dw_Command;
	double d_RealTimingInterval = 0;
	unsigned char b_ModulNbr;
	unsigned char b_TorCounter;
	unsigned char b_PCIInputClock;
	unsigned char b_TimingUnit;
	unsigned int ul_TimingInterval;
	unsigned int ul_RealTimingInterval = 0;

	i_ReturnValue = insn->n;
	b_ModulNbr = (unsigned char) CR_AREF(insn->chanspec);

	b_TorCounter = (unsigned char) data[0];
	b_PCIInputClock = (unsigned char) data[1];
	b_TimingUnit = (unsigned char) data[2];
	ul_TimingInterval = (unsigned int) data[3];
	printk("INPUT clock %d\n", b_PCIInputClock);

		
	
		

	if (b_ModulNbr < 4) {
		
		
		

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_TOR_COUNTER) {
	      
			
	      

			if (b_TorCounter <= 1) {
		 
				
		 

				if ((b_PCIInputClock == APCI1710_30MHZ) ||
					(b_PCIInputClock == APCI1710_33MHZ) ||
					(b_PCIInputClock == APCI1710_40MHZ) ||
					(b_PCIInputClock ==
						APCI1710_GATE_INPUT)) {
		    
					
		    

					if ((b_TimingUnit <= 4)
						|| (b_PCIInputClock ==
							APCI1710_GATE_INPUT)) {
		       
						
		       

						if (((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 0) && (ul_TimingInterval >= 133) && (ul_TimingInterval <= 0xFFFFFFFFUL)) || ((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 1) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 571230650UL)) || ((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 2) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 571230UL)) || ((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 3) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 571UL)) || ((b_PCIInputClock == APCI1710_30MHZ) && (b_TimingUnit == 4) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 9UL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 0) && (ul_TimingInterval >= 121) && (ul_TimingInterval <= 0xFFFFFFFFUL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 1) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 519691043UL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 2) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 519691UL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 3) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 520UL)) || ((b_PCIInputClock == APCI1710_33MHZ) && (b_TimingUnit == 4) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 8UL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 0) && (ul_TimingInterval >= 100) && (ul_TimingInterval <= 0xFFFFFFFFUL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 1) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 429496729UL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 2) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 429496UL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 3) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 429UL)) || ((b_PCIInputClock == APCI1710_40MHZ) && (b_TimingUnit == 4) && (ul_TimingInterval >= 1) && (ul_TimingInterval <= 7UL)) || ((b_PCIInputClock == APCI1710_GATE_INPUT) && (ul_TimingInterval >= 2))) {
				
							
				

							if (((b_PCIInputClock == APCI1710_40MHZ) && (devpriv->s_BoardInfos.b_BoardVersion > 0)) || (b_PCIInputClock != APCI1710_40MHZ)) {
			     
								
			     

								if (((b_PCIInputClock == APCI1710_40MHZ) && ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF) >= 0x3131)) || ((b_PCIInputClock == APCI1710_GATE_INPUT) && ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF) >= 0x3132)) || (b_PCIInputClock == APCI1710_30MHZ) || (b_PCIInputClock == APCI1710_33MHZ)) {
				
									
				

									if (b_PCIInputClock != APCI1710_GATE_INPUT) {
										fpu_begin
											();
				   
										
				   

										switch (b_TimingUnit) {
				      
											
				      

										case 0:

					      
											
					      

											ul_TimerValue
												=
												(unsigned int)
												(ul_TimingInterval
												*
												(0.00025 * b_PCIInputClock));

					      
											
					      

											if ((double)((double)ul_TimingInterval * (0.00025 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
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
												(0.00025 * (double)b_PCIInputClock));
											d_RealTimingInterval
												=
												(double)
												ul_TimerValue
												/
												(0.00025
												*
												(double)
												b_PCIInputClock);

											if ((double)((double)ul_TimerValue / (0.00025 * (double)b_PCIInputClock)) >= (double)((double)ul_RealTimingInterval + 0.5)) {
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
													1.007752288);
											}

											break;

				      
											
				      

										case 1:

					      
											
					      

											ul_TimerValue
												=
												(unsigned int)
												(ul_TimingInterval
												*
												(0.25 * b_PCIInputClock));

					      
											
					      

											if ((double)((double)ul_TimingInterval * (0.25 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
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
												(0.25 * (double)b_PCIInputClock));
											d_RealTimingInterval
												=
												(double)
												ul_TimerValue
												/
												(
												(double)
												0.25
												*
												(double)
												b_PCIInputClock);

											if ((double)((double)ul_TimerValue / (0.25 * (double)b_PCIInputClock)) >= (double)((double)ul_RealTimingInterval + 0.5)) {
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
													1.007752288);
											}

											break;

				      
											
				      

										case 2:

					      
											
					      

											ul_TimerValue
												=
												ul_TimingInterval
												*
												(250.0
												*
												b_PCIInputClock);

					      
											
					      

											if ((double)((double)ul_TimingInterval * (250.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
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
												(250.0 * (double)b_PCIInputClock));
											d_RealTimingInterval
												=
												(double)
												ul_TimerValue
												/
												(250.0
												*
												(double)
												b_PCIInputClock);

											if ((double)((double)ul_TimerValue / (250.0 * (double)b_PCIInputClock)) >= (double)((double)ul_RealTimingInterval + 0.5)) {
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
													1.007752288);
											}

											break;

				      
											
				      

										case 3:

					      
											
					      

											ul_TimerValue
												=
												(unsigned int)
												(ul_TimingInterval
												*
												(250000.0
													*
													b_PCIInputClock));

					      
											
					      

											if ((double)((double)ul_TimingInterval * (250000.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
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
												(250000.0
													*
													(double)
													b_PCIInputClock));
											d_RealTimingInterval
												=
												(double)
												ul_TimerValue
												/
												(250000.0
												*
												(double)
												b_PCIInputClock);

											if ((double)((double)ul_TimerValue / (250000.0 * (double)b_PCIInputClock)) >= (double)((double)ul_RealTimingInterval + 0.5)) {
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
													1.007752288);
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
												(250000.0
													*
													b_PCIInputClock));

					      
											
					      

											if ((double)((double)(ul_TimingInterval * 60.0) * (250000.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
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
												(250000.0
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
												(250000.0
													*
													(double)
													b_PCIInputClock))
												/
												60.0;

											if ((double)(((double)ul_TimerValue / (250000.0 * (double)b_PCIInputClock)) / 60.0) >= (double)((double)ul_RealTimingInterval + 0.5)) {
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
													1.007752288);
											}

											break;
										}

										fpu_end();
									}	
									else {
				   
										
				   

										ul_TimerValue
											=
											ul_TimingInterval
											-
											2;
									}	

				
									
				
									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_TorCounterModuleInfo.
										b_PCIInputClock
										=
										b_PCIInputClock;

				
									
				

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_TorCounterModuleInfo.
										s_TorCounterInfo
										[b_TorCounter].
										b_TimingUnit
										=
										b_TimingUnit;

				
									
				
									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_TorCounterModuleInfo.
										s_TorCounterInfo
										[b_TorCounter].
										d_TimingInterval
										=
										d_RealTimingInterval;

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_TorCounterModuleInfo.
										s_TorCounterInfo
										[b_TorCounter].
										ul_RealTimingInterval
										=
										ul_RealTimingInterval;

				
									
				

									dw_Command
										=
										inl
										(devpriv->
										s_BoardInfos.
										ui_Address
										+
										4
										+
										(16 * b_TorCounter) + (64 * b_ModulNbr));

									dw_Command
										=
										(dw_Command
										>>
										4)
										&
										0xF;

				
									
				

									if (b_PCIInputClock == APCI1710_40MHZ) {
				   
										
				   

										dw_Command
											=
											dw_Command
											|
											0x10;
									}

				
									
				

									if (b_PCIInputClock == APCI1710_GATE_INPUT) {
				   
										
				   

										dw_Command
											=
											dw_Command
											|
											0x20;
									}

				
									
				

									outl(dw_Command, devpriv->s_BoardInfos.ui_Address + 4 + (16 * b_TorCounter) + (64 * b_ModulNbr));

				
									
				

									outl(0, devpriv->s_BoardInfos.ui_Address + 8 + (16 * b_TorCounter) + (64 * b_ModulNbr));
				
									
				

									outl(ul_TimerValue, devpriv->s_BoardInfos.ui_Address + 0 + (16 * b_TorCounter) + (64 * b_ModulNbr));

				
									
				

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_TorCounterModuleInfo.
										s_TorCounterInfo
										[b_TorCounter].
										b_TorCounterInit
										=
										1;
								} else {
				
									
				

									DPRINTK("TOR version error for 40MHz clock selection\n");
									i_ReturnValue
										=
										-9;
								}
							} else {
			     
								
			     

								DPRINTK("You can not used the 40MHz clock selection wich this board\n");
								i_ReturnValue =
									-8;
							}
						} else {
			  
							
			  

							DPRINTK("Base timing selection is wrong\n");
							i_ReturnValue = -7;
						}
					}	
					else {
		       
						
		       

						DPRINTK("Timing unit selection is wrong\n");
						i_ReturnValue = -6;
					}	
				}	
				else {
		    
					
		    

					DPRINTK("The selected PCI input clock is wrong\n");
					i_ReturnValue = -5;
				}	
			}	
			else {
		 
				
		 

				DPRINTK("Tor Counter selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
	      
			
	      

			DPRINTK("The module is not a tor counter module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}
	data[0] = (unsigned int) ul_RealTimingInterval;
	return i_ReturnValue;
}


int i_APCI1710_InsnWriteEnableDisableTorCounter(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status;
	unsigned int dw_DummyRead;
	unsigned int dw_ConfigReg;
	unsigned char b_ModulNbr, b_Action;
	unsigned char b_TorCounter;
	unsigned char b_InputMode;
	unsigned char b_ExternGate;
	unsigned char b_CycleMode;
	unsigned char b_InterruptEnable;

	b_ModulNbr = (unsigned char) CR_AREF(insn->chanspec);
	b_Action = (unsigned char) data[0];	
	b_TorCounter = (unsigned char) data[1];
	b_InputMode = (unsigned char) data[2];
	b_ExternGate = (unsigned char) data[3];
	b_CycleMode = (unsigned char) data[4];
	b_InterruptEnable = (unsigned char) data[5];
	i_ReturnValue = insn->n;
	devpriv->tsk_Current = current;	
	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_TOR_COUNTER) {
	      
			
	      

			if (b_TorCounter <= 1) {
				switch (b_Action)	
				{
				case APCI1710_ENABLE:
		 
					
		 

					dw_Status =
						inl(devpriv->s_BoardInfos.
						ui_Address + 8 +
						(16 * b_TorCounter) +
						(64 * b_ModulNbr));

					if (dw_Status & 0x10) {
		    
						
		    

						if (b_InputMode == 0 ||
							b_InputMode == 1 ||
							b_InputMode ==
							APCI1710_TOR_SIMPLE_MODE
							|| b_InputMode ==
							APCI1710_TOR_DOUBLE_MODE
							|| b_InputMode ==
							APCI1710_TOR_QUADRUPLE_MODE)
						{
		       
							
		       

							if (b_ExternGate == 0
								|| b_ExternGate
								== 1
								|| b_InputMode >
								1) {
			  
								
			  

								if ((b_CycleMode == APCI1710_SINGLE) || (b_CycleMode == APCI1710_CONTINUOUS)) {
			     
									
			     

									if ((b_InterruptEnable == APCI1710_ENABLE) || (b_InterruptEnable == APCI1710_DISABLE)) {

				   
										
				   

										devpriv->
											s_ModuleInfo
											[b_ModulNbr].
											s_TorCounterModuleInfo.
											s_TorCounterInfo
											[b_TorCounter].
											b_InterruptEnable
											=
											b_InterruptEnable;

				   
										
				   

										dw_ConfigReg
											=
											inl
											(devpriv->
											s_BoardInfos.
											ui_Address
											+
											4
											+
											(16 * b_TorCounter) + (64 * b_ModulNbr));

										dw_ConfigReg
											=
											(dw_ConfigReg
											>>
											4)
											&
											0x30;

				   
										
				   

										if (b_InputMode > 1) {
				      
											
				      

											b_ExternGate
												=
												0;

				      
											
				      

											dw_ConfigReg
												=
												dw_ConfigReg
												|
												0x40;

				      
											
				      

											if (b_InputMode == APCI1710_TOR_SIMPLE_MODE) {
					 
												
					 

												dw_ConfigReg
													=
													dw_ConfigReg
													|
													0x780;

											}	

				      
											
				      

											if (b_InputMode == APCI1710_TOR_DOUBLE_MODE) {
					 
												
					 

												dw_ConfigReg
													=
													dw_ConfigReg
													|
													0x180;

											}	

											b_InputMode
												=
												0;
										}	

				   
										
				   

										dw_ConfigReg
											=
											dw_ConfigReg
											|
											b_CycleMode
											|
											(b_InterruptEnable
											*
											2)
											|
											(b_InputMode
											*
											4)
											|
											(b_ExternGate
											*
											8);

				   
										
				   

										dw_DummyRead
											=
											inl
											(devpriv->
											s_BoardInfos.
											ui_Address
											+
											0
											+
											(16 * b_TorCounter) + (64 * b_ModulNbr));

				   
										
				   

										dw_DummyRead
											=
											inl
											(devpriv->
											s_BoardInfos.
											ui_Address
											+
											12
											+
											(16 * b_TorCounter) + (64 * b_ModulNbr));

				   
										
				   

										outl(dw_ConfigReg, devpriv->s_BoardInfos.ui_Address + 4 + (16 * b_TorCounter) + (64 * b_ModulNbr));

				   
										
				   

										outl(1, devpriv->s_BoardInfos.ui_Address + 8 + (16 * b_TorCounter) + (64 * b_ModulNbr));

									}	
									else {
				
										
				

										DPRINTK("Interrupt parameter is wrong\n");
										i_ReturnValue
											=
											-9;
									}	
								}	
								else {
			     
									
			     

									DPRINTK("Tor counter acquisition mode cycle is wrong\n");
									i_ReturnValue
										=
										-8;
								}	
							}	
							else {
			  
								
			  

								DPRINTK("Extern gate input mode is wrong\n");
								i_ReturnValue =
									-7;
							}	
						}	
						else {
		       
							
		       

							DPRINTK("Tor input signal selection is wrong\n");
							i_ReturnValue = -6;
						}
					} else {
		    
						
		    

						DPRINTK("Tor counter not initialised\n");
						i_ReturnValue = -5;
					}
					break;

				case APCI1710_DISABLE:
			 
					
		 

					dw_Status = inl(devpriv->s_BoardInfos.
						ui_Address + 8 +
						(16 * b_TorCounter) +
						(64 * b_ModulNbr));

		 
					
		 

					if (dw_Status & 0x10) {
		    
						
		    

						if (dw_Status & 0x1) {
		       
							
		       
							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_TorCounterModuleInfo.
								s_TorCounterInfo
								[b_TorCounter].
								b_InterruptEnable
								=
								APCI1710_DISABLE;

		       
							
		       

							outl(0, devpriv->
								s_BoardInfos.
								ui_Address + 8 +
								(16 * b_TorCounter) + (64 * b_ModulNbr));
						}	
						else {
		       
							
		       

							DPRINTK("Tor counter not enabled \n");
							i_ReturnValue = -6;
						}	
					}	
					else {
		    
						
		    

						DPRINTK("Tor counter not initialised\n");
						i_ReturnValue = -5;
					}	

				}	
			}	
			else {
		 
				
		 

				DPRINTK("Tor counter selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
	      
			
	      

			DPRINTK("The module is not a tor counter module \n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("Module number error \n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InsnReadGetTorCounterInitialisation(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status;
	unsigned char b_ModulNbr;
	unsigned char b_TorCounter;
	unsigned char *pb_TimingUnit;
	unsigned int *pul_TimingInterval;
	unsigned char *pb_InputMode;
	unsigned char *pb_ExternGate;
	unsigned char *pb_CycleMode;
	unsigned char *pb_Enable;
	unsigned char *pb_InterruptEnable;

	i_ReturnValue = insn->n;
	b_ModulNbr = CR_AREF(insn->chanspec);
	b_TorCounter = CR_CHAN(insn->chanspec);

	pb_TimingUnit = (unsigned char *) &data[0];
	pul_TimingInterval = (unsigned int *) &data[1];
	pb_InputMode = (unsigned char *) &data[2];
	pb_ExternGate = (unsigned char *) &data[3];
	pb_CycleMode = (unsigned char *) &data[4];
	pb_Enable = (unsigned char *) &data[5];
	pb_InterruptEnable = (unsigned char *) &data[6];

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_TOR_COUNTER) {
	      
			
	      

			if (b_TorCounter <= 1) {

		 
				
		 

				dw_Status = inl(devpriv->s_BoardInfos.
					ui_Address + 8 + (16 * b_TorCounter) +
					(64 * b_ModulNbr));

				if (dw_Status & 0x10) {
					*pb_Enable = dw_Status & 1;

		    
					
		    

					dw_Status = inl(devpriv->s_BoardInfos.
						ui_Address + 4 +
						(16 * b_TorCounter) +
						(64 * b_ModulNbr));

					*pb_CycleMode =
						(unsigned char) ((dw_Status >> 4) & 1);
					*pb_InterruptEnable =
						(unsigned char) ((dw_Status >> 5) & 1);

		    
					
		    

					if (dw_Status & 0x600) {
		       
						
		       

						if (dw_Status & 0x400) {
			  
							
			  

							if ((dw_Status & 0x7800)
								== 0x7800) {
								*pb_InputMode =
									APCI1710_TOR_SIMPLE_MODE;
							}

			  
							
			  

							if ((dw_Status & 0x7800)
								== 0x1800) {
								*pb_InputMode =
									APCI1710_TOR_DOUBLE_MODE;
							}

			  
							
			  

							if ((dw_Status & 0x7800)
								== 0x0000) {
								*pb_InputMode =
									APCI1710_TOR_QUADRUPLE_MODE;
							}
						}	
						else {
							*pb_InputMode = 1;
						}	

		       
						
		       

						*pb_ExternGate = 0;
					}	
					else {
						*pb_InputMode =
							(unsigned char) ((dw_Status >> 6)
							& 1);
						*pb_ExternGate =
							(unsigned char) ((dw_Status >> 7)
							& 1);
					}	

					*pb_TimingUnit =
						devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_TorCounterModuleInfo.
						s_TorCounterInfo[b_TorCounter].
						b_TimingUnit;

					*pul_TimingInterval =
						devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_TorCounterModuleInfo.
						s_TorCounterInfo[b_TorCounter].
						ul_RealTimingInterval;
				} else {
		    
					
		    

					DPRINTK("Tor counter not initialised\n");
					i_ReturnValue = -5;
				}

			}	
			else {
		 
				
		 

				DPRINTK("Tor counter selection is wrong \n");
				i_ReturnValue = -4;
			}	
		} else {
	      
			
	      

			DPRINTK("The module is not a tor counter module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InsnBitsGetTorCounterProgressStatusAndValue(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status;
	unsigned int dw_TimeOut = 0;

	unsigned char b_ModulNbr;
	unsigned char b_TorCounter;
	unsigned char b_ReadType;
	unsigned int ui_TimeOut;
	unsigned char *pb_TorCounterStatus;
	unsigned int *pul_TorCounterValue;

	i_ReturnValue = insn->n;
	b_ModulNbr = CR_AREF(insn->chanspec);
	b_ReadType = (unsigned char) data[0];
	b_TorCounter = (unsigned char) data[1];
	ui_TimeOut = (unsigned int) data[2];
	pb_TorCounterStatus = (unsigned char *) &data[0];
	pul_TorCounterValue = (unsigned int *) &data[1];

	
	
	

	if (b_ReadType == APCI1710_TOR_READINTERRUPT) {

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

		return insn->n;
	}

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_TOR_COUNTER) {
	      
			
	      

			if (b_TorCounter <= 1) {
		 
				
		 

				dw_Status = inl(devpriv->s_BoardInfos.
					ui_Address + 8 + (16 * b_TorCounter) +
					(64 * b_ModulNbr));

		 
				
		 

				if (dw_Status & 0x10) {
		    
					
		    

					if (dw_Status & 0x1) {

						switch (b_ReadType) {

						case APCI1710_TOR_GETPROGRESSSTATUS:
		       
							
		       

							dw_Status =
								inl(devpriv->
								s_BoardInfos.
								ui_Address + 4 +
								(16 * b_TorCounter) + (64 * b_ModulNbr));

							dw_Status =
								dw_Status & 0xF;

		       
							
		       

							if (dw_Status & 1) {
								if (dw_Status &
									2) {
									if (dw_Status & 4) {
				
										
				

										*pb_TorCounterStatus
											=
											3;
									} else {
				
										
				

										*pb_TorCounterStatus
											=
											2;
									}
								} else {
			     
									
			     

									*pb_TorCounterStatus
										=
										1;
								}
							} else {
			  
								
			  

								*pb_TorCounterStatus
									= 0;
							}
							break;

						case APCI1710_TOR_GETCOUNTERVALUE:

		       
							
		       

							if ((ui_TimeOut >= 0)
								&& (ui_TimeOut
									<=
									65535UL))
							{
								for (;;) {
			     
									
			     

									dw_Status
										=
										inl
										(devpriv->
										s_BoardInfos.
										ui_Address
										+
										4
										+
										(16 * b_TorCounter) + (64 * b_ModulNbr));
			     
									
			     

									if ((dw_Status & 4) == 4) {
				
										
				

										*pb_TorCounterStatus
											=
											3;

				
										
				

										*pul_TorCounterValue
											=
											inl
											(devpriv->
											s_BoardInfos.
											ui_Address
											+
											0
											+
											(16 * b_TorCounter) + (64 * b_ModulNbr));
										break;
									}	
									else {
				
										
				

										if ((dw_Status & 2) == 2) {
				   
											
				   

											*pb_TorCounterStatus
												=
												2;

				   
											
				   

											*pul_TorCounterValue
												=
												inl
												(devpriv->
												s_BoardInfos.
												ui_Address
												+
												0
												+
												(16 * b_TorCounter) + (64 * b_ModulNbr));

											break;
										}	
										else {
				   
											
				   

											if ((dw_Status & 1) == 1) {
				      
												
				      

												*pb_TorCounterStatus
													=
													1;
											}	
											else {
				      
												
				      

												*pb_TorCounterStatus
													=
													0;
											}	
										}	
									}	

									if (dw_TimeOut == ui_TimeOut) {
				
										
				

										break;
									} else {
				
										
				

										dw_TimeOut
											=
											dw_TimeOut
											+
											1;

										mdelay(1000);
									}
								}	

			  
								
			  

								if ((*pb_TorCounterStatus != 3) && (dw_TimeOut == ui_TimeOut) && (ui_TimeOut != 0)) {
			     
									
			     

									*pb_TorCounterStatus
										=
										4;
								}
							} else {
			  
								
			  

								DPRINTK("Timeout parameter is wrong\n");
								i_ReturnValue =
									-7;
							}
							break;

						default:
							printk("Inputs wrong\n");
						}	
					}	
					else {
		       
						
		       

						DPRINTK("Tor counter not enabled\n");
						i_ReturnValue = -6;
					}	
				} else {
		    
					
		    

					DPRINTK("Tor counter not initialised\n");
					i_ReturnValue = -5;
				}
			}	
			else {
		 
				
		 

				DPRINTK("Tor counter selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
	      
			
	      

			DPRINTK("The module is not a tor counter module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

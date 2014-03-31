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


#include "APCI1710_Pwm.h"


int i_APCI1710_InsnConfigPWM(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned char b_ConfigType;
	int i_ReturnValue = 0;
	b_ConfigType = CR_CHAN(insn->chanspec);

	switch (b_ConfigType) {
	case APCI1710_PWM_INIT:
		i_ReturnValue = i_APCI1710_InitPWM(dev, (unsigned char) CR_AREF(insn->chanspec),	
			(unsigned char) data[0],	
			(unsigned char) data[1],	
			(unsigned char) data[2],	
			(unsigned int) data[3],	
			(unsigned int) data[4],	
			(unsigned int *) &data[0],	
			(unsigned int *) &data[1]	
			);
		break;

	case APCI1710_PWM_GETINITDATA:
		i_ReturnValue = i_APCI1710_GetPWMInitialisation(dev, (unsigned char) CR_AREF(insn->chanspec),	
			(unsigned char) data[0],	
			(unsigned char *) &data[0],	
			(unsigned int *) &data[1],	
			(unsigned int *) &data[2],	
			(unsigned char *) &data[3],	
			(unsigned char *) &data[4],	
			(unsigned char *) &data[5],	
			(unsigned char *) &data[6],	
			(unsigned char *) &data[7],	
			(unsigned char *) &data[8]	
			);
		break;

	default:
		printk(" Config Parameter Wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;
}


int i_APCI1710_InitPWM(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_PWM,
	unsigned char b_ClockSelection,
	unsigned char b_TimingUnit,
	unsigned int ul_LowTiming,
	unsigned int ul_HighTiming,
	unsigned int *pul_RealLowTiming, unsigned int *pul_RealHighTiming)
{
	int i_ReturnValue = 0;
	unsigned int ul_LowTimerValue = 0;
	unsigned int ul_HighTimerValue = 0;
	unsigned int dw_Command;
	double d_RealLowTiming = 0;
	double d_RealHighTiming = 0;

	
	
	

	if (b_ModulNbr < 4) {
		
		
		

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_PWM) {
			
			
			

			if (b_PWM <= 1) {
				
				
				

				if ((b_ClockSelection == APCI1710_30MHZ) ||
					(b_ClockSelection == APCI1710_33MHZ) ||
					(b_ClockSelection == APCI1710_40MHZ)) {
					
					
					

					if (b_TimingUnit <= 4) {
						
						
						

						if (((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 0)
								&& (ul_LowTiming
									>= 266)
								&& (ul_LowTiming
									<=
									0xFFFFFFFFUL))
							|| ((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 1)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									571230650UL))
							|| ((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 2)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									571230UL))
							|| ((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 3)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									571UL))
							|| ((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 4)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<= 9UL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 0)
								&& (ul_LowTiming
									>= 242)
								&& (ul_LowTiming
									<=
									0xFFFFFFFFUL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 1)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									519691043UL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 2)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									519691UL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 3)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									520UL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 4)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<= 8UL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 0)
								&& (ul_LowTiming
									>= 200)
								&& (ul_LowTiming
									<=
									0xFFFFFFFFUL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 1)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									429496729UL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 2)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									429496UL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 3)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									429UL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 4)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									7UL))) {
							
							
							

							if (((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 0) && (ul_HighTiming >= 266) && (ul_HighTiming <= 0xFFFFFFFFUL)) || ((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 1) && (ul_HighTiming >= 1) && (ul_HighTiming <= 571230650UL)) || ((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 2) && (ul_HighTiming >= 1) && (ul_HighTiming <= 571230UL)) || ((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 3) && (ul_HighTiming >= 1) && (ul_HighTiming <= 571UL)) || ((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 4) && (ul_HighTiming >= 1) && (ul_HighTiming <= 9UL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 0) && (ul_HighTiming >= 242) && (ul_HighTiming <= 0xFFFFFFFFUL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 1) && (ul_HighTiming >= 1) && (ul_HighTiming <= 519691043UL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 2) && (ul_HighTiming >= 1) && (ul_HighTiming <= 519691UL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 3) && (ul_HighTiming >= 1) && (ul_HighTiming <= 520UL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 4) && (ul_HighTiming >= 1) && (ul_HighTiming <= 8UL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 0) && (ul_HighTiming >= 200) && (ul_HighTiming <= 0xFFFFFFFFUL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 1) && (ul_HighTiming >= 1) && (ul_HighTiming <= 429496729UL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 2) && (ul_HighTiming >= 1) && (ul_HighTiming <= 429496UL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 3) && (ul_HighTiming >= 1) && (ul_HighTiming <= 429UL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 4) && (ul_HighTiming >= 1) && (ul_HighTiming <= 7UL))) {
								
								
								

								if (((b_ClockSelection == APCI1710_40MHZ) && (devpriv->s_BoardInfos.b_BoardVersion > 0)) || (b_ClockSelection != APCI1710_40MHZ)) {

									
									
									

									fpu_begin
										();

									switch (b_TimingUnit) {
										
										
										

									case 0:

										
										
										

										ul_LowTimerValue
											=
											(unsigned int)
											(ul_LowTiming
											*
											(0.00025 * b_ClockSelection));

										
										
										

										if ((double)((double)ul_LowTiming * (0.00025 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
											ul_LowTimerValue
												=
												ul_LowTimerValue
												+
												1;
										}

										
										
										

										*pul_RealLowTiming
											=
											(unsigned int)
											(ul_LowTimerValue
											/
											(0.00025 * (double)b_ClockSelection));
										d_RealLowTiming
											=
											(double)
											ul_LowTimerValue
											/
											(0.00025
											*
											(double)
											b_ClockSelection);

										if ((double)((double)ul_LowTimerValue / (0.00025 * (double)b_ClockSelection)) >= (double)((double)*pul_RealLowTiming + 0.5)) {
											*pul_RealLowTiming
												=
												*pul_RealLowTiming
												+
												1;
										}

										ul_LowTiming
											=
											ul_LowTiming
											-
											1;
										ul_LowTimerValue
											=
											ul_LowTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_LowTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_LowTimerValue)
												*
												1.007752288);
										}

										break;

										
										
										

									case 1:

										
										
										

										ul_LowTimerValue
											=
											(unsigned int)
											(ul_LowTiming
											*
											(0.25 * b_ClockSelection));

										
										
										

										if ((double)((double)ul_LowTiming * (0.25 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
											ul_LowTimerValue
												=
												ul_LowTimerValue
												+
												1;
										}

										
										
										

										*pul_RealLowTiming
											=
											(unsigned int)
											(ul_LowTimerValue
											/
											(0.25 * (double)b_ClockSelection));
										d_RealLowTiming
											=
											(double)
											ul_LowTimerValue
											/
											(
											(double)
											0.25
											*
											(double)
											b_ClockSelection);

										if ((double)((double)ul_LowTimerValue / (0.25 * (double)b_ClockSelection)) >= (double)((double)*pul_RealLowTiming + 0.5)) {
											*pul_RealLowTiming
												=
												*pul_RealLowTiming
												+
												1;
										}

										ul_LowTiming
											=
											ul_LowTiming
											-
											1;
										ul_LowTimerValue
											=
											ul_LowTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_LowTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_LowTimerValue)
												*
												1.007752288);
										}

										break;

										
										
										

									case 2:

										
										
										

										ul_LowTimerValue
											=
											ul_LowTiming
											*
											(250.0
											*
											b_ClockSelection);

										
										
										

										if ((double)((double)ul_LowTiming * (250.0 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
											ul_LowTimerValue
												=
												ul_LowTimerValue
												+
												1;
										}

										
										
										

										*pul_RealLowTiming
											=
											(unsigned int)
											(ul_LowTimerValue
											/
											(250.0 * (double)b_ClockSelection));
										d_RealLowTiming
											=
											(double)
											ul_LowTimerValue
											/
											(250.0
											*
											(double)
											b_ClockSelection);

										if ((double)((double)ul_LowTimerValue / (250.0 * (double)b_ClockSelection)) >= (double)((double)*pul_RealLowTiming + 0.5)) {
											*pul_RealLowTiming
												=
												*pul_RealLowTiming
												+
												1;
										}

										ul_LowTiming
											=
											ul_LowTiming
											-
											1;
										ul_LowTimerValue
											=
											ul_LowTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_LowTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_LowTimerValue)
												*
												1.007752288);
										}

										break;

										
										
										

									case 3:
										
										
										

										ul_LowTimerValue
											=
											(unsigned int)
											(ul_LowTiming
											*
											(250000.0
												*
												b_ClockSelection));

										
										
										

										if ((double)((double)ul_LowTiming * (250000.0 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
											ul_LowTimerValue
												=
												ul_LowTimerValue
												+
												1;
										}

										
										
										

										*pul_RealLowTiming
											=
											(unsigned int)
											(ul_LowTimerValue
											/
											(250000.0
												*
												(double)
												b_ClockSelection));
										d_RealLowTiming
											=
											(double)
											ul_LowTimerValue
											/
											(250000.0
											*
											(double)
											b_ClockSelection);

										if ((double)((double)ul_LowTimerValue / (250000.0 * (double)b_ClockSelection)) >= (double)((double)*pul_RealLowTiming + 0.5)) {
											*pul_RealLowTiming
												=
												*pul_RealLowTiming
												+
												1;
										}

										ul_LowTiming
											=
											ul_LowTiming
											-
											1;
										ul_LowTimerValue
											=
											ul_LowTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_LowTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_LowTimerValue)
												*
												1.007752288);
										}

										break;

										
										
										

									case 4:

										
										
										

										ul_LowTimerValue
											=
											(unsigned int)
											(
											(ul_LowTiming
												*
												60)
											*
											(250000.0
												*
												b_ClockSelection));

										
										
										

										if ((double)((double)(ul_LowTiming * 60.0) * (250000.0 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
											ul_LowTimerValue
												=
												ul_LowTimerValue
												+
												1;
										}

										
										
										

										*pul_RealLowTiming
											=
											(unsigned int)
											(ul_LowTimerValue
											/
											(250000.0
												*
												(double)
												b_ClockSelection))
											/
											60;
										d_RealLowTiming
											=
											(
											(double)
											ul_LowTimerValue
											/
											(250000.0
												*
												(double)
												b_ClockSelection))
											/
											60.0;

										if ((double)(((double)ul_LowTimerValue / (250000.0 * (double)b_ClockSelection)) / 60.0) >= (double)((double)*pul_RealLowTiming + 0.5)) {
											*pul_RealLowTiming
												=
												*pul_RealLowTiming
												+
												1;
										}

										ul_LowTiming
											=
											ul_LowTiming
											-
											1;
										ul_LowTimerValue
											=
											ul_LowTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_LowTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_LowTimerValue)
												*
												1.007752288);
										}

										break;
									}

									
									
									

									switch (b_TimingUnit) {
										
										
										

									case 0:

										
										
										

										ul_HighTimerValue
											=
											(unsigned int)
											(ul_HighTiming
											*
											(0.00025 * b_ClockSelection));

										
										
										

										if ((double)((double)ul_HighTiming * (0.00025 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
											ul_HighTimerValue
												=
												ul_HighTimerValue
												+
												1;
										}

										
										
										

										*pul_RealHighTiming
											=
											(unsigned int)
											(ul_HighTimerValue
											/
											(0.00025 * (double)b_ClockSelection));
										d_RealHighTiming
											=
											(double)
											ul_HighTimerValue
											/
											(0.00025
											*
											(double)
											b_ClockSelection);

										if ((double)((double)ul_HighTimerValue / (0.00025 * (double)b_ClockSelection)) >= (double)((double)*pul_RealHighTiming + 0.5)) {
											*pul_RealHighTiming
												=
												*pul_RealHighTiming
												+
												1;
										}

										ul_HighTiming
											=
											ul_HighTiming
											-
											1;
										ul_HighTimerValue
											=
											ul_HighTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_HighTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_HighTimerValue)
												*
												1.007752288);
										}

										break;

										
										
										

									case 1:

										
										
										

										ul_HighTimerValue
											=
											(unsigned int)
											(ul_HighTiming
											*
											(0.25 * b_ClockSelection));

										
										
										

										if ((double)((double)ul_HighTiming * (0.25 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
											ul_HighTimerValue
												=
												ul_HighTimerValue
												+
												1;
										}

										
										
										

										*pul_RealHighTiming
											=
											(unsigned int)
											(ul_HighTimerValue
											/
											(0.25 * (double)b_ClockSelection));
										d_RealHighTiming
											=
											(double)
											ul_HighTimerValue
											/
											(
											(double)
											0.25
											*
											(double)
											b_ClockSelection);

										if ((double)((double)ul_HighTimerValue / (0.25 * (double)b_ClockSelection)) >= (double)((double)*pul_RealHighTiming + 0.5)) {
											*pul_RealHighTiming
												=
												*pul_RealHighTiming
												+
												1;
										}

										ul_HighTiming
											=
											ul_HighTiming
											-
											1;
										ul_HighTimerValue
											=
											ul_HighTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_HighTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_HighTimerValue)
												*
												1.007752288);
										}

										break;

										
										
										

									case 2:

										
										
										

										ul_HighTimerValue
											=
											ul_HighTiming
											*
											(250.0
											*
											b_ClockSelection);

										
										
										

										if ((double)((double)ul_HighTiming * (250.0 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
											ul_HighTimerValue
												=
												ul_HighTimerValue
												+
												1;
										}

										
										
										

										*pul_RealHighTiming
											=
											(unsigned int)
											(ul_HighTimerValue
											/
											(250.0 * (double)b_ClockSelection));
										d_RealHighTiming
											=
											(double)
											ul_HighTimerValue
											/
											(250.0
											*
											(double)
											b_ClockSelection);

										if ((double)((double)ul_HighTimerValue / (250.0 * (double)b_ClockSelection)) >= (double)((double)*pul_RealHighTiming + 0.5)) {
											*pul_RealHighTiming
												=
												*pul_RealHighTiming
												+
												1;
										}

										ul_HighTiming
											=
											ul_HighTiming
											-
											1;
										ul_HighTimerValue
											=
											ul_HighTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_HighTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_HighTimerValue)
												*
												1.007752288);
										}

										break;

										
										
										

									case 3:

										
										
										

										ul_HighTimerValue
											=
											(unsigned int)
											(ul_HighTiming
											*
											(250000.0
												*
												b_ClockSelection));

										
										
										

										if ((double)((double)ul_HighTiming * (250000.0 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
											ul_HighTimerValue
												=
												ul_HighTimerValue
												+
												1;
										}

										
										
										

										*pul_RealHighTiming
											=
											(unsigned int)
											(ul_HighTimerValue
											/
											(250000.0
												*
												(double)
												b_ClockSelection));
										d_RealHighTiming
											=
											(double)
											ul_HighTimerValue
											/
											(250000.0
											*
											(double)
											b_ClockSelection);

										if ((double)((double)ul_HighTimerValue / (250000.0 * (double)b_ClockSelection)) >= (double)((double)*pul_RealHighTiming + 0.5)) {
											*pul_RealHighTiming
												=
												*pul_RealHighTiming
												+
												1;
										}

										ul_HighTiming
											=
											ul_HighTiming
											-
											1;
										ul_HighTimerValue
											=
											ul_HighTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_HighTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_HighTimerValue)
												*
												1.007752288);
										}

										break;

										
										
										

									case 4:

										
										
										

										ul_HighTimerValue
											=
											(unsigned int)
											(
											(ul_HighTiming
												*
												60)
											*
											(250000.0
												*
												b_ClockSelection));

										
										
										

										if ((double)((double)(ul_HighTiming * 60.0) * (250000.0 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
											ul_HighTimerValue
												=
												ul_HighTimerValue
												+
												1;
										}

										
										
										

										*pul_RealHighTiming
											=
											(unsigned int)
											(ul_HighTimerValue
											/
											(250000.0
												*
												(double)
												b_ClockSelection))
											/
											60;
										d_RealHighTiming
											=
											(
											(double)
											ul_HighTimerValue
											/
											(250000.0
												*
												(double)
												b_ClockSelection))
											/
											60.0;

										if ((double)(((double)ul_HighTimerValue / (250000.0 * (double)b_ClockSelection)) / 60.0) >= (double)((double)*pul_RealHighTiming + 0.5)) {
											*pul_RealHighTiming
												=
												*pul_RealHighTiming
												+
												1;
										}

										ul_HighTiming
											=
											ul_HighTiming
											-
											1;
										ul_HighTimerValue
											=
											ul_HighTimerValue
											-
											2;

										if (b_ClockSelection != APCI1710_40MHZ) {
											ul_HighTimerValue
												=
												(unsigned int)
												(
												(double)
												(ul_HighTimerValue)
												*
												1.007752288);
										}

										break;
									}

									fpu_end();
									
									
									

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_PWMModuleInfo.
										b_ClockSelection
										=
										b_ClockSelection;

									
									
									

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_PWMModuleInfo.
										s_PWMInfo
										[b_PWM].
										b_TimingUnit
										=
										b_TimingUnit;

									
									
									

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_PWMModuleInfo.
										s_PWMInfo
										[b_PWM].
										d_LowTiming
										=
										d_RealLowTiming;

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_PWMModuleInfo.
										s_PWMInfo
										[b_PWM].
										ul_RealLowTiming
										=
										*pul_RealLowTiming;

									
									
									

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_PWMModuleInfo.
										s_PWMInfo
										[b_PWM].
										d_HighTiming
										=
										d_RealHighTiming;

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_PWMModuleInfo.
										s_PWMInfo
										[b_PWM].
										ul_RealHighTiming
										=
										*pul_RealHighTiming;

									
									
									

									outl(ul_LowTimerValue, devpriv->s_BoardInfos.ui_Address + 0 + (20 * b_PWM) + (64 * b_ModulNbr));

									
									
									

									outl(ul_HighTimerValue, devpriv->s_BoardInfos.ui_Address + 4 + (20 * b_PWM) + (64 * b_ModulNbr));

									
									
									

									dw_Command
										=
										inl
										(devpriv->
										s_BoardInfos.
										ui_Address
										+
										8
										+
										(20 * b_PWM) + (64 * b_ModulNbr));

									dw_Command
										=
										dw_Command
										&
										0x7F;

									if (b_ClockSelection == APCI1710_40MHZ) {
										dw_Command
											=
											dw_Command
											|
											0x80;
									}

									
									
									

									outl(dw_Command, devpriv->s_BoardInfos.ui_Address + 8 + (20 * b_PWM) + (64 * b_ModulNbr));

									
									
									
									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_PWMModuleInfo.
										s_PWMInfo
										[b_PWM].
										b_PWMInit
										=
										1;
								} else {
									
									
									
									
									DPRINTK("You can not used the 40MHz clock selection with this board\n");
									i_ReturnValue
										=
										-9;
								}
							} else {
								
								
								
								DPRINTK("High base timing selection is wrong\n");
								i_ReturnValue =
									-8;
							}
						} else {
							
							
							
							DPRINTK("Low base timing selection is wrong\n");
							i_ReturnValue = -7;
						}
					}	
					else {
						
						
						
						DPRINTK("Timing unit selection is wrong\n");
						i_ReturnValue = -6;
					}	
				}	
				else {
					
					
					
					DPRINTK("The selected clock is wrong\n");
					i_ReturnValue = -5;
				}	
			}	
			else {
				
				
				
				DPRINTK("Tor PWM selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
			
			
			
			DPRINTK("The module is not a PWM module\n");
			i_ReturnValue = -3;
		}
	} else {
		
		
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_GetPWMInitialisation(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_PWM,
	unsigned char *pb_TimingUnit,
	unsigned int *pul_LowTiming,
	unsigned int *pul_HighTiming,
	unsigned char *pb_StartLevel,
	unsigned char *pb_StopMode,
	unsigned char *pb_StopLevel,
	unsigned char *pb_ExternGate, unsigned char *pb_InterruptEnable, unsigned char *pb_Enable)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status;
	unsigned int dw_Command;

	
	
	

	if (b_ModulNbr < 4) {
		
		
		

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_PWM) {
			
			
			

			if (b_PWM <= 1) {
				
				
				

				dw_Status = inl(devpriv->s_BoardInfos.
					ui_Address + 12 + (20 * b_PWM) +
					(64 * b_ModulNbr));

				if (dw_Status & 0x10) {
					
					
					

					*pul_LowTiming =
						inl(devpriv->s_BoardInfos.
						ui_Address + 0 + (20 * b_PWM) +
						(64 * b_ModulNbr));

					
					
					

					*pul_HighTiming =
						inl(devpriv->s_BoardInfos.
						ui_Address + 4 + (20 * b_PWM) +
						(64 * b_ModulNbr));

					
					
					

					dw_Command = inl(devpriv->s_BoardInfos.
						ui_Address + 8 + (20 * b_PWM) +
						(64 * b_ModulNbr));

					*pb_StartLevel =
						(unsigned char) ((dw_Command >> 5) & 1);
					*pb_StopMode =
						(unsigned char) ((dw_Command >> 0) & 1);
					*pb_StopLevel =
						(unsigned char) ((dw_Command >> 1) & 1);
					*pb_ExternGate =
						(unsigned char) ((dw_Command >> 4) & 1);
					*pb_InterruptEnable =
						(unsigned char) ((dw_Command >> 3) & 1);

					if (*pb_StopLevel) {
						*pb_StopLevel =
							*pb_StopLevel +
							(unsigned char) ((dw_Command >>
								2) & 1);
					}

					
					
					

					dw_Command = inl(devpriv->s_BoardInfos.
						ui_Address + 8 + (20 * b_PWM) +
						(64 * b_ModulNbr));

					*pb_Enable =
						(unsigned char) ((dw_Command >> 0) & 1);

					*pb_TimingUnit = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_PWMModuleInfo.
						s_PWMInfo[b_PWM].b_TimingUnit;
				}	
				else {
					
					
					
					DPRINTK("PWM not initialised\n");
					i_ReturnValue = -5;
				}	
			}	
			else {
				
				
				
				DPRINTK("Tor PWM selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
			
			
			
			DPRINTK("The module is not a PWM module\n");
			i_ReturnValue = -3;
		}
	} else {
		
		
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InsnWritePWM(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned char b_WriteType;
	int i_ReturnValue = 0;
	b_WriteType = CR_CHAN(insn->chanspec);

	switch (b_WriteType) {
	case APCI1710_PWM_ENABLE:
		i_ReturnValue = i_APCI1710_EnablePWM(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1],
			(unsigned char) data[2],
			(unsigned char) data[3], (unsigned char) data[4], (unsigned char) data[5]);
		break;

	case APCI1710_PWM_DISABLE:
		i_ReturnValue = i_APCI1710_DisablePWM(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char) data[0]);
		break;

	case APCI1710_PWM_NEWTIMING:
		i_ReturnValue = i_APCI1710_SetNewPWMTiming(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1], (unsigned int) data[2], (unsigned int) data[3]);
		break;

	default:
		printk("Write Config Parameter Wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;
}


int i_APCI1710_EnablePWM(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_PWM,
	unsigned char b_StartLevel,
	unsigned char b_StopMode,
	unsigned char b_StopLevel, unsigned char b_ExternGate, unsigned char b_InterruptEnable)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status;
	unsigned int dw_Command;

	devpriv->tsk_Current = current;	
	
	
	

	if (b_ModulNbr < 4) {
		
		
		

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_PWM) {
			
			
			

			if (b_PWM <= 1) {
				
				
				

				dw_Status = inl(devpriv->s_BoardInfos.
					ui_Address + 12 + (20 * b_PWM) +
					(64 * b_ModulNbr));

				if (dw_Status & 0x10) {
					
					
					

					if (b_StartLevel <= 1) {
						
						
						

						if (b_StopMode <= 1) {
							
							
							

							if (b_StopLevel <= 2) {
								
								
								

								if (b_ExternGate
									<= 1) {
									
									
									

									if (b_InterruptEnable == APCI1710_ENABLE || b_InterruptEnable == APCI1710_DISABLE) {
										
										
										

										
										
										

										dw_Command
											=
											inl
											(devpriv->
											s_BoardInfos.
											ui_Address
											+
											8
											+
											(20 * b_PWM) + (64 * b_ModulNbr));

										dw_Command
											=
											dw_Command
											&
											0x80;

										
										
										

										dw_Command
											=
											dw_Command
											|
											b_StopMode
											|
											(b_InterruptEnable
											<<
											3)
											|
											(b_ExternGate
											<<
											4)
											|
											(b_StartLevel
											<<
											5);

										if (b_StopLevel & 3) {
											dw_Command
												=
												dw_Command
												|
												2;

											if (b_StopLevel & 2) {
												dw_Command
													=
													dw_Command
													|
													4;
											}
										}

										devpriv->
											s_ModuleInfo
											[b_ModulNbr].
											s_PWMModuleInfo.
											s_PWMInfo
											[b_PWM].
											b_InterruptEnable
											=
											b_InterruptEnable;

										
										
										

										outl(dw_Command, devpriv->s_BoardInfos.ui_Address + 8 + (20 * b_PWM) + (64 * b_ModulNbr));

										
										
										
										outl(1, devpriv->s_BoardInfos.ui_Address + 12 + (20 * b_PWM) + (64 * b_ModulNbr));
									}	
									else {
										
										
										
										DPRINTK("Interrupt parameter is wrong\n");
										i_ReturnValue
											=
											-10;
									}	
								}	
								else {
									
									
									
									DPRINTK("Extern gate signal selection is wrong\n");
									i_ReturnValue
										=
										-9;
								}	
							}	
							else {
								
								
								
								DPRINTK("PWM stop level selection is wrong\n");
								i_ReturnValue =
									-8;
							}	
						}	
						else {
							
							
							
							DPRINTK("PWM stop mode selection is wrong\n");
							i_ReturnValue = -7;
						}	
					}	
					else {
						
						
						
						DPRINTK("PWM start level selection is wrong\n");
						i_ReturnValue = -6;
					}	
				}	
				else {
					
					
					
					DPRINTK("PWM not initialised\n");
					i_ReturnValue = -5;
				}	
			}	
			else {
				
				
				
				DPRINTK("Tor PWM selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
			
			
			
			DPRINTK("The module is not a PWM module\n");
			i_ReturnValue = -3;
		}
	} else {
		
		
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_DisablePWM(struct comedi_device *dev, unsigned char b_ModulNbr, unsigned char b_PWM)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status;

	
	
	

	if (b_ModulNbr < 4) {
		
		
		

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_PWM) {
			
			
			

			if (b_PWM <= 1) {
				
				
				

				dw_Status = inl(devpriv->s_BoardInfos.
					ui_Address + 12 + (20 * b_PWM) +
					(64 * b_ModulNbr));

				if (dw_Status & 0x10) {
					
					
					

					if (dw_Status & 0x1) {
						
						
						
						outl(0, devpriv->s_BoardInfos.
							ui_Address + 12 +
							(20 * b_PWM) +
							(64 * b_ModulNbr));
					}	
					else {
						
						
						
						DPRINTK("PWM not enabled\n");
						i_ReturnValue = -6;
					}	
				}	
				else {
					
					
					
					DPRINTK(" PWM not initialised\n");
					i_ReturnValue = -5;
				}	
			}	
			else {
				
				
				
				DPRINTK("Tor PWM selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
			
			
			
			DPRINTK("The module is not a PWM module\n");
			i_ReturnValue = -3;
		}
	} else {
		
		
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_SetNewPWMTiming(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_PWM, unsigned char b_TimingUnit, unsigned int ul_LowTiming, unsigned int ul_HighTiming)
{
	unsigned char b_ClockSelection;
	int i_ReturnValue = 0;
	unsigned int ul_LowTimerValue = 0;
	unsigned int ul_HighTimerValue = 0;
	unsigned int ul_RealLowTiming = 0;
	unsigned int ul_RealHighTiming = 0;
	unsigned int dw_Status;
	unsigned int dw_Command;
	double d_RealLowTiming = 0;
	double d_RealHighTiming = 0;

	
	
	

	if (b_ModulNbr < 4) {
		
		
		

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_PWM) {
			
			
			

			if (b_PWM <= 1) {
				
				
				

				dw_Status = inl(devpriv->s_BoardInfos.
					ui_Address + 12 + (20 * b_PWM) +
					(64 * b_ModulNbr));

				if (dw_Status & 0x10) {
					b_ClockSelection = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_PWMModuleInfo.
						b_ClockSelection;

					
					
					

					if (b_TimingUnit <= 4) {
						
						
						

						if (((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 0)
								&& (ul_LowTiming
									>= 266)
								&& (ul_LowTiming
									<=
									0xFFFFFFFFUL))
							|| ((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 1)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									571230650UL))
							|| ((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 2)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									571230UL))
							|| ((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 3)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									571UL))
							|| ((b_ClockSelection ==
									APCI1710_30MHZ)
								&& (b_TimingUnit
									== 4)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<= 9UL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 0)
								&& (ul_LowTiming
									>= 242)
								&& (ul_LowTiming
									<=
									0xFFFFFFFFUL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 1)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									519691043UL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 2)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									519691UL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 3)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									520UL))
							|| ((b_ClockSelection ==
									APCI1710_33MHZ)
								&& (b_TimingUnit
									== 4)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<= 8UL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 0)
								&& (ul_LowTiming
									>= 200)
								&& (ul_LowTiming
									<=
									0xFFFFFFFFUL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 1)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									429496729UL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 2)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									429496UL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 3)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									429UL))
							|| ((b_ClockSelection ==
									APCI1710_40MHZ)
								&& (b_TimingUnit
									== 4)
								&& (ul_LowTiming
									>= 1)
								&& (ul_LowTiming
									<=
									7UL))) {
							
							
							

							if (((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 0) && (ul_HighTiming >= 266) && (ul_HighTiming <= 0xFFFFFFFFUL)) || ((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 1) && (ul_HighTiming >= 1) && (ul_HighTiming <= 571230650UL)) || ((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 2) && (ul_HighTiming >= 1) && (ul_HighTiming <= 571230UL)) || ((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 3) && (ul_HighTiming >= 1) && (ul_HighTiming <= 571UL)) || ((b_ClockSelection == APCI1710_30MHZ) && (b_TimingUnit == 4) && (ul_HighTiming >= 1) && (ul_HighTiming <= 9UL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 0) && (ul_HighTiming >= 242) && (ul_HighTiming <= 0xFFFFFFFFUL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 1) && (ul_HighTiming >= 1) && (ul_HighTiming <= 519691043UL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 2) && (ul_HighTiming >= 1) && (ul_HighTiming <= 519691UL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 3) && (ul_HighTiming >= 1) && (ul_HighTiming <= 520UL)) || ((b_ClockSelection == APCI1710_33MHZ) && (b_TimingUnit == 4) && (ul_HighTiming >= 1) && (ul_HighTiming <= 8UL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 0) && (ul_HighTiming >= 200) && (ul_HighTiming <= 0xFFFFFFFFUL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 1) && (ul_HighTiming >= 1) && (ul_HighTiming <= 429496729UL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 2) && (ul_HighTiming >= 1) && (ul_HighTiming <= 429496UL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 3) && (ul_HighTiming >= 1) && (ul_HighTiming <= 429UL)) || ((b_ClockSelection == APCI1710_40MHZ) && (b_TimingUnit == 4) && (ul_HighTiming >= 1) && (ul_HighTiming <= 7UL))) {
								
								
								

								fpu_begin();
								switch (b_TimingUnit) {
									
									
									

								case 0:

									
									
									

									ul_LowTimerValue
										=
										(unsigned int)
										(ul_LowTiming
										*
										(0.00025 * b_ClockSelection));

									
									
									

									if ((double)((double)ul_LowTiming * (0.00025 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
										ul_LowTimerValue
											=
											ul_LowTimerValue
											+
											1;
									}

									
									
									

									ul_RealLowTiming
										=
										(unsigned int)
										(ul_LowTimerValue
										/
										(0.00025 * (double)b_ClockSelection));
									d_RealLowTiming
										=
										(double)
										ul_LowTimerValue
										/
										(0.00025
										*
										(double)
										b_ClockSelection);

									if ((double)((double)ul_LowTimerValue / (0.00025 * (double)b_ClockSelection)) >= (double)((double)ul_RealLowTiming + 0.5)) {
										ul_RealLowTiming
											=
											ul_RealLowTiming
											+
											1;
									}

									ul_LowTiming
										=
										ul_LowTiming
										-
										1;
									ul_LowTimerValue
										=
										ul_LowTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_LowTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_LowTimerValue)
											*
											1.007752288);
									}

									break;

									
									
									

								case 1:

									
									
									

									ul_LowTimerValue
										=
										(unsigned int)
										(ul_LowTiming
										*
										(0.25 * b_ClockSelection));

									
									
									

									if ((double)((double)ul_LowTiming * (0.25 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
										ul_LowTimerValue
											=
											ul_LowTimerValue
											+
											1;
									}

									
									
									

									ul_RealLowTiming
										=
										(unsigned int)
										(ul_LowTimerValue
										/
										(0.25 * (double)b_ClockSelection));
									d_RealLowTiming
										=
										(double)
										ul_LowTimerValue
										/
										(
										(double)
										0.25
										*
										(double)
										b_ClockSelection);

									if ((double)((double)ul_LowTimerValue / (0.25 * (double)b_ClockSelection)) >= (double)((double)ul_RealLowTiming + 0.5)) {
										ul_RealLowTiming
											=
											ul_RealLowTiming
											+
											1;
									}

									ul_LowTiming
										=
										ul_LowTiming
										-
										1;
									ul_LowTimerValue
										=
										ul_LowTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_LowTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_LowTimerValue)
											*
											1.007752288);
									}

									break;

									
									
									

								case 2:

									
									
									

									ul_LowTimerValue
										=
										ul_LowTiming
										*
										(250.0
										*
										b_ClockSelection);

									
									
									

									if ((double)((double)ul_LowTiming * (250.0 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
										ul_LowTimerValue
											=
											ul_LowTimerValue
											+
											1;
									}

									
									
									

									ul_RealLowTiming
										=
										(unsigned int)
										(ul_LowTimerValue
										/
										(250.0 * (double)b_ClockSelection));
									d_RealLowTiming
										=
										(double)
										ul_LowTimerValue
										/
										(250.0
										*
										(double)
										b_ClockSelection);

									if ((double)((double)ul_LowTimerValue / (250.0 * (double)b_ClockSelection)) >= (double)((double)ul_RealLowTiming + 0.5)) {
										ul_RealLowTiming
											=
											ul_RealLowTiming
											+
											1;
									}

									ul_LowTiming
										=
										ul_LowTiming
										-
										1;
									ul_LowTimerValue
										=
										ul_LowTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_LowTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_LowTimerValue)
											*
											1.007752288);
									}

									break;

									
									
									

								case 3:

									
									
									

									ul_LowTimerValue
										=
										(unsigned int)
										(ul_LowTiming
										*
										(250000.0
											*
											b_ClockSelection));

									
									
									

									if ((double)((double)ul_LowTiming * (250000.0 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
										ul_LowTimerValue
											=
											ul_LowTimerValue
											+
											1;
									}

									
									
									

									ul_RealLowTiming
										=
										(unsigned int)
										(ul_LowTimerValue
										/
										(250000.0
											*
											(double)
											b_ClockSelection));
									d_RealLowTiming
										=
										(double)
										ul_LowTimerValue
										/
										(250000.0
										*
										(double)
										b_ClockSelection);

									if ((double)((double)ul_LowTimerValue / (250000.0 * (double)b_ClockSelection)) >= (double)((double)ul_RealLowTiming + 0.5)) {
										ul_RealLowTiming
											=
											ul_RealLowTiming
											+
											1;
									}

									ul_LowTiming
										=
										ul_LowTiming
										-
										1;
									ul_LowTimerValue
										=
										ul_LowTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_LowTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_LowTimerValue)
											*
											1.007752288);
									}

									break;

									
									
									

								case 4:

									
									
									

									ul_LowTimerValue
										=
										(unsigned int)
										(
										(ul_LowTiming
											*
											60)
										*
										(250000.0
											*
											b_ClockSelection));

									
									
									

									if ((double)((double)(ul_LowTiming * 60.0) * (250000.0 * (double)b_ClockSelection)) >= ((double)((double)ul_LowTimerValue + 0.5))) {
										ul_LowTimerValue
											=
											ul_LowTimerValue
											+
											1;
									}

									
									
									

									ul_RealLowTiming
										=
										(unsigned int)
										(ul_LowTimerValue
										/
										(250000.0
											*
											(double)
											b_ClockSelection))
										/
										60;
									d_RealLowTiming
										=
										(
										(double)
										ul_LowTimerValue
										/
										(250000.0
											*
											(double)
											b_ClockSelection))
										/
										60.0;

									if ((double)(((double)ul_LowTimerValue / (250000.0 * (double)b_ClockSelection)) / 60.0) >= (double)((double)ul_RealLowTiming + 0.5)) {
										ul_RealLowTiming
											=
											ul_RealLowTiming
											+
											1;
									}

									ul_LowTiming
										=
										ul_LowTiming
										-
										1;
									ul_LowTimerValue
										=
										ul_LowTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_LowTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_LowTimerValue)
											*
											1.007752288);
									}

									break;
								}

								
								
								

								switch (b_TimingUnit) {
									
									
									

								case 0:

									
									
									

									ul_HighTimerValue
										=
										(unsigned int)
										(ul_HighTiming
										*
										(0.00025 * b_ClockSelection));

									
									
									

									if ((double)((double)ul_HighTiming * (0.00025 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
										ul_HighTimerValue
											=
											ul_HighTimerValue
											+
											1;
									}

									
									
									

									ul_RealHighTiming
										=
										(unsigned int)
										(ul_HighTimerValue
										/
										(0.00025 * (double)b_ClockSelection));
									d_RealHighTiming
										=
										(double)
										ul_HighTimerValue
										/
										(0.00025
										*
										(double)
										b_ClockSelection);

									if ((double)((double)ul_HighTimerValue / (0.00025 * (double)b_ClockSelection)) >= (double)((double)ul_RealHighTiming + 0.5)) {
										ul_RealHighTiming
											=
											ul_RealHighTiming
											+
											1;
									}

									ul_HighTiming
										=
										ul_HighTiming
										-
										1;
									ul_HighTimerValue
										=
										ul_HighTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_HighTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_HighTimerValue)
											*
											1.007752288);
									}

									break;

									
									
									

								case 1:

									
									
									

									ul_HighTimerValue
										=
										(unsigned int)
										(ul_HighTiming
										*
										(0.25 * b_ClockSelection));

									
									
									

									if ((double)((double)ul_HighTiming * (0.25 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
										ul_HighTimerValue
											=
											ul_HighTimerValue
											+
											1;
									}

									
									
									

									ul_RealHighTiming
										=
										(unsigned int)
										(ul_HighTimerValue
										/
										(0.25 * (double)b_ClockSelection));
									d_RealHighTiming
										=
										(double)
										ul_HighTimerValue
										/
										(
										(double)
										0.25
										*
										(double)
										b_ClockSelection);

									if ((double)((double)ul_HighTimerValue / (0.25 * (double)b_ClockSelection)) >= (double)((double)ul_RealHighTiming + 0.5)) {
										ul_RealHighTiming
											=
											ul_RealHighTiming
											+
											1;
									}

									ul_HighTiming
										=
										ul_HighTiming
										-
										1;
									ul_HighTimerValue
										=
										ul_HighTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_HighTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_HighTimerValue)
											*
											1.007752288);
									}

									break;

									
									
									

								case 2:

									
									
									

									ul_HighTimerValue
										=
										ul_HighTiming
										*
										(250.0
										*
										b_ClockSelection);

									
									
									

									if ((double)((double)ul_HighTiming * (250.0 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
										ul_HighTimerValue
											=
											ul_HighTimerValue
											+
											1;
									}

									
									
									

									ul_RealHighTiming
										=
										(unsigned int)
										(ul_HighTimerValue
										/
										(250.0 * (double)b_ClockSelection));
									d_RealHighTiming
										=
										(double)
										ul_HighTimerValue
										/
										(250.0
										*
										(double)
										b_ClockSelection);

									if ((double)((double)ul_HighTimerValue / (250.0 * (double)b_ClockSelection)) >= (double)((double)ul_RealHighTiming + 0.5)) {
										ul_RealHighTiming
											=
											ul_RealHighTiming
											+
											1;
									}

									ul_HighTiming
										=
										ul_HighTiming
										-
										1;
									ul_HighTimerValue
										=
										ul_HighTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_HighTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_HighTimerValue)
											*
											1.007752288);
									}

									break;

									
									
									

								case 3:

									
									
									

									ul_HighTimerValue
										=
										(unsigned int)
										(ul_HighTiming
										*
										(250000.0
											*
											b_ClockSelection));

									
									
									

									if ((double)((double)ul_HighTiming * (250000.0 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
										ul_HighTimerValue
											=
											ul_HighTimerValue
											+
											1;
									}

									
									
									

									ul_RealHighTiming
										=
										(unsigned int)
										(ul_HighTimerValue
										/
										(250000.0
											*
											(double)
											b_ClockSelection));
									d_RealHighTiming
										=
										(double)
										ul_HighTimerValue
										/
										(250000.0
										*
										(double)
										b_ClockSelection);

									if ((double)((double)ul_HighTimerValue / (250000.0 * (double)b_ClockSelection)) >= (double)((double)ul_RealHighTiming + 0.5)) {
										ul_RealHighTiming
											=
											ul_RealHighTiming
											+
											1;
									}

									ul_HighTiming
										=
										ul_HighTiming
										-
										1;
									ul_HighTimerValue
										=
										ul_HighTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_HighTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_HighTimerValue)
											*
											1.007752288);
									}

									break;

									
									
									

								case 4:

									
									
									

									ul_HighTimerValue
										=
										(unsigned int)
										(
										(ul_HighTiming
											*
											60)
										*
										(250000.0
											*
											b_ClockSelection));

									
									
									

									if ((double)((double)(ul_HighTiming * 60.0) * (250000.0 * (double)b_ClockSelection)) >= ((double)((double)ul_HighTimerValue + 0.5))) {
										ul_HighTimerValue
											=
											ul_HighTimerValue
											+
											1;
									}

									
									
									

									ul_RealHighTiming
										=
										(unsigned int)
										(ul_HighTimerValue
										/
										(250000.0
											*
											(double)
											b_ClockSelection))
										/
										60;
									d_RealHighTiming
										=
										(
										(double)
										ul_HighTimerValue
										/
										(250000.0
											*
											(double)
											b_ClockSelection))
										/
										60.0;

									if ((double)(((double)ul_HighTimerValue / (250000.0 * (double)b_ClockSelection)) / 60.0) >= (double)((double)ul_RealHighTiming + 0.5)) {
										ul_RealHighTiming
											=
											ul_RealHighTiming
											+
											1;
									}

									ul_HighTiming
										=
										ul_HighTiming
										-
										1;
									ul_HighTimerValue
										=
										ul_HighTimerValue
										-
										2;

									if (b_ClockSelection != APCI1710_40MHZ) {
										ul_HighTimerValue
											=
											(unsigned int)
											(
											(double)
											(ul_HighTimerValue)
											*
											1.007752288);
									}

									break;
								}

								fpu_end();

								
								
								

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_PWMModuleInfo.
									s_PWMInfo
									[b_PWM].
									b_TimingUnit
									=
									b_TimingUnit;

								
								
								

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_PWMModuleInfo.
									s_PWMInfo
									[b_PWM].
									d_LowTiming
									=
									d_RealLowTiming;

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_PWMModuleInfo.
									s_PWMInfo
									[b_PWM].
									ul_RealLowTiming
									=
									ul_RealLowTiming;

								
								
								

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_PWMModuleInfo.
									s_PWMInfo
									[b_PWM].
									d_HighTiming
									=
									d_RealHighTiming;

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_PWMModuleInfo.
									s_PWMInfo
									[b_PWM].
									ul_RealHighTiming
									=
									ul_RealHighTiming;

								
								
								

								outl(ul_LowTimerValue, devpriv->s_BoardInfos.ui_Address + 0 + (20 * b_PWM) + (64 * b_ModulNbr));

								
								
								

								outl(ul_HighTimerValue, devpriv->s_BoardInfos.ui_Address + 4 + (20 * b_PWM) + (64 * b_ModulNbr));

								
								
								

								dw_Command =
									inl
									(devpriv->
									s_BoardInfos.
									ui_Address
									+ 8 +
									(20 * b_PWM) + (64 * b_ModulNbr));

								dw_Command =
									dw_Command
									& 0x7F;

								if (b_ClockSelection == APCI1710_40MHZ) {
									dw_Command
										=
										dw_Command
										|
										0x80;
								}

								
								
								

								outl(dw_Command,
									devpriv->
									s_BoardInfos.
									ui_Address
									+ 8 +
									(20 * b_PWM) + (64 * b_ModulNbr));
							} else {
								
								
								
								DPRINTK("High base timing selection is wrong\n");
								i_ReturnValue =
									-8;
							}
						} else {
							
							
							
							DPRINTK("Low base timing selection is wrong\n");
							i_ReturnValue = -7;
						}
					}	
					else {
						
						
						
						DPRINTK("Timing unit selection is wrong\n");
						i_ReturnValue = -6;
					}	
				}	
				else {
					
					
					
					DPRINTK("PWM not initialised\n");
					i_ReturnValue = -5;
				}	
			}	
			else {
				
				
				
				DPRINTK("Tor PWM selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
			
			
			
			DPRINTK("The module is not a PWM module\n");
			i_ReturnValue = -3;
		}
	} else {
		
		
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InsnReadGetPWMStatus(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status;

	unsigned char b_ModulNbr;
	unsigned char b_PWM;
	unsigned char *pb_PWMOutputStatus;
	unsigned char *pb_ExternGateStatus;

	i_ReturnValue = insn->n;
	b_ModulNbr = (unsigned char) CR_AREF(insn->chanspec);
	b_PWM = (unsigned char) CR_CHAN(insn->chanspec);
	pb_PWMOutputStatus = (unsigned char *) &data[0];
	pb_ExternGateStatus = (unsigned char *) &data[1];

	
	
	

	if (b_ModulNbr < 4) {
		
		
		

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_PWM) {
			
			
			

			if (b_PWM <= 1) {
				
				
				

				dw_Status = inl(devpriv->s_BoardInfos.
					ui_Address + 12 + (20 * b_PWM) +
					(64 * b_ModulNbr));

				if (dw_Status & 0x10) {
					
					
					

					if (dw_Status & 0x1) {
						*pb_PWMOutputStatus =
							(unsigned char) ((dw_Status >> 7)
							& 1);
						*pb_ExternGateStatus =
							(unsigned char) ((dw_Status >> 6)
							& 1);
					}	
					else {
						
						
						

						DPRINTK("PWM not enabled \n");
						i_ReturnValue = -6;
					}	
				}	
				else {
					
					
					

					DPRINTK("PWM not initialised\n");
					i_ReturnValue = -5;
				}	
			}	
			else {
				
				
				

				DPRINTK("Tor PWM selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
			
			
			

			DPRINTK("The module is not a PWM module\n");
			i_ReturnValue = -3;
		}
	} else {
		
		
		

		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

int i_APCI1710_InsnBitsReadPWMInterrupt(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
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
		s_InterruptParameters.ui_Read + 1) % APCI1710_SAVE_INTERRUPT;

	return insn->n;

}

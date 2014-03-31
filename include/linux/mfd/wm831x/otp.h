/*
 * include/linux/mfd/wm831x/otp.h -- OTP interface for WM831x
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_WM831X_OTP_H__
#define __MFD_WM831X_OTP_H__

int wm831x_otp_init(struct wm831x *wm831x);
void wm831x_otp_exit(struct wm831x *wm831x);

#define WM831X_UNIQUE_ID_MASK                   0xFFFF  
#define WM831X_UNIQUE_ID_SHIFT                       0  
#define WM831X_UNIQUE_ID_WIDTH                      16  

#define WM831X_UNIQUE_ID_MASK                   0xFFFF  
#define WM831X_UNIQUE_ID_SHIFT                       0  
#define WM831X_UNIQUE_ID_WIDTH                      16  

#define WM831X_UNIQUE_ID_MASK                   0xFFFF  
#define WM831X_UNIQUE_ID_SHIFT                       0  
#define WM831X_UNIQUE_ID_WIDTH                      16  

#define WM831X_UNIQUE_ID_MASK                   0xFFFF  
#define WM831X_UNIQUE_ID_SHIFT                       0  
#define WM831X_UNIQUE_ID_WIDTH                      16  

#define WM831X_UNIQUE_ID_MASK                   0xFFFF  
#define WM831X_UNIQUE_ID_SHIFT                       0  
#define WM831X_UNIQUE_ID_WIDTH                      16  

#define WM831X_UNIQUE_ID_MASK                   0xFFFF  
#define WM831X_UNIQUE_ID_SHIFT                       0  
#define WM831X_UNIQUE_ID_WIDTH                      16  

#define WM831X_UNIQUE_ID_MASK                   0xFFFF  
#define WM831X_UNIQUE_ID_SHIFT                       0  
#define WM831X_UNIQUE_ID_WIDTH                      16  

#define WM831X_UNIQUE_ID_MASK                   0xFFFF  
#define WM831X_UNIQUE_ID_SHIFT                       0  
#define WM831X_UNIQUE_ID_WIDTH                      16  

#define WM831X_OTP_FACT_ID_MASK                 0xFFFE  
#define WM831X_OTP_FACT_ID_SHIFT                     1  
#define WM831X_OTP_FACT_ID_WIDTH                    15  
#define WM831X_OTP_FACT_FINAL                   0x0001  
#define WM831X_OTP_FACT_FINAL_MASK              0x0001  
#define WM831X_OTP_FACT_FINAL_SHIFT                  0  
#define WM831X_OTP_FACT_FINAL_WIDTH                  1  

#define WM831X_DC3_TRIM_MASK                    0xF000  
#define WM831X_DC3_TRIM_SHIFT                       12  
#define WM831X_DC3_TRIM_WIDTH                        4  
#define WM831X_DC2_TRIM_MASK                    0x0FC0  
#define WM831X_DC2_TRIM_SHIFT                        6  
#define WM831X_DC2_TRIM_WIDTH                        6  
#define WM831X_DC1_TRIM_MASK                    0x003F  
#define WM831X_DC1_TRIM_SHIFT                        0  
#define WM831X_DC1_TRIM_WIDTH                        6  

#define WM831X_CHIP_ID_MASK                     0xFFFF  
#define WM831X_CHIP_ID_SHIFT                         0  
#define WM831X_CHIP_ID_WIDTH                        16  

#define WM831X_OSC_TRIM_MASK                    0x0780  
#define WM831X_OSC_TRIM_SHIFT                        7  
#define WM831X_OSC_TRIM_WIDTH                        4  
#define WM831X_BG_TRIM_MASK                     0x0078  
#define WM831X_BG_TRIM_SHIFT                         3  
#define WM831X_BG_TRIM_WIDTH                         4  
#define WM831X_LPBG_TRIM_MASK                   0x0007  
#define WM831X_LPBG_TRIM_SHIFT                       0  
#define WM831X_LPBG_TRIM_WIDTH                       3  

#define WM831X_CHILD_I2C_ADDR_MASK              0x00FE  
#define WM831X_CHILD_I2C_ADDR_SHIFT                  1  
#define WM831X_CHILD_I2C_ADDR_WIDTH                  7  
#define WM831X_CH_AW                            0x0001  
#define WM831X_CH_AW_MASK                       0x0001  
#define WM831X_CH_AW_SHIFT                           0  
#define WM831X_CH_AW_WIDTH                           1  

#define WM831X_CHARGE_TRIM_MASK                 0x003F  
#define WM831X_CHARGE_TRIM_SHIFT                     0  
#define WM831X_CHARGE_TRIM_WIDTH                     6  

#define WM831X_OTP_AUTO_PROG                    0x8000  
#define WM831X_OTP_AUTO_PROG_MASK               0x8000  
#define WM831X_OTP_AUTO_PROG_SHIFT                  15  
#define WM831X_OTP_AUTO_PROG_WIDTH                   1  
#define WM831X_OTP_CUST_ID_MASK                 0x7FFE  
#define WM831X_OTP_CUST_ID_SHIFT                     1  
#define WM831X_OTP_CUST_ID_WIDTH                    14  
#define WM831X_OTP_CUST_FINAL                   0x0001  
#define WM831X_OTP_CUST_FINAL_MASK              0x0001  
#define WM831X_OTP_CUST_FINAL_SHIFT                  0  
#define WM831X_OTP_CUST_FINAL_WIDTH                  1  

#define WM831X_DBE_VALID_DATA_MASK              0xFFFF  
#define WM831X_DBE_VALID_DATA_SHIFT                  0  
#define WM831X_DBE_VALID_DATA_WIDTH                 16  


#endif

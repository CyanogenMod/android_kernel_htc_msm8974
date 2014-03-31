/**
 *
 * Synaptics Register Mapped Interface (RMI4) Header File.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *#############################################################################
 * GPL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 *#############################################################################
 */

#ifndef _RMI_H
#define _RMI_H


struct rmi_function_descriptor {
	unsigned char queryBaseAddr;
	unsigned char commandBaseAddr;
	unsigned char controlBaseAddr;
	unsigned char dataBaseAddr;
	unsigned char interruptSrcCnt;
	unsigned char functionNum;
};

struct rmi_F01_query {
	
	unsigned char mfgid;

	
	unsigned char properties;

	
	unsigned char prod_info[2];

	
	unsigned char date_code[3];

	
	unsigned short tester_id;

	
	unsigned short serial_num;

	
	char prod_id[11];
};

struct rmi_F01_control {
    unsigned char deviceControl;
    unsigned char interruptEnable[1];
};

struct rmi_F01_data {
    unsigned char deviceStatus;
    unsigned char irqs[1];
};



struct rmi_F11_device_query {
    bool hasQuery9;
    unsigned char numberOfSensors;
};

struct rmi_F11_sensor_query {
    bool configurable;
    bool hasSensitivityAdjust;
    bool hasGestures;
    bool hasAbs;
    bool hasRel;
    unsigned char numberOfFingers;
    unsigned char numberOfXElectrodes;
    unsigned char numberOfYElectrodes;
    unsigned char maximumElectrodes;
    bool hasAnchoredFinger;
    unsigned char absDataSize;
};

struct rmi_F11_control {
    bool relativeBallistics;
    bool relativePositionFilter;
    bool absolutePositionFilter;
    unsigned char reportingMode;
    bool manuallyTrackedFinger;
    bool manuallyTrackedFingerEnable;
    unsigned char motionSensitivity;
    unsigned char palmDetectThreshold;
    unsigned char deltaXPosThreshold;
    unsigned char deltaYPosThreshold;
    unsigned char velocity;
    unsigned char acceleration;
    unsigned short sensorMaxXPos;
    unsigned short sensorMaxYPos;
};



struct rmi_F19_query {
	bool hasHysteresisThreshold;
	bool hasSensitivityAdjust;
	bool configurable;
	unsigned char buttonCount;
};

struct rmi_F19_control {
	unsigned char buttonUsage;
	unsigned char filterMode;
	unsigned char *intEnableRegisters;
	unsigned char *singleButtonControl;
	unsigned char *sensorMap;
	unsigned char *singleButtonSensitivity;
	unsigned char globalSensitivityAdjustment;
	unsigned char globalHysteresisThreshold;
};

#endif

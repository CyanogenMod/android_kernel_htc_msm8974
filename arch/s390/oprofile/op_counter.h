/**
 * arch/s390/oprofile/op_counter.h
 *
 *   Copyright (C) 2011 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *   Author(s): Andreas Krebbel (krebbel@linux.vnet.ibm.com)
 *
 * @remark Copyright 2011 OProfile authors
 */

#ifndef OP_COUNTER_H
#define OP_COUNTER_H

struct op_counter_config {
	
	
	
	unsigned long kernel;
	unsigned long user;
};

extern struct op_counter_config counter_config;

#endif 

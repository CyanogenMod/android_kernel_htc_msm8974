/*
 * dds.h - sysfs attributes associated with DDS devices
 *
 * Copyright (c) 2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */


#define IIO_DEV_ATTR_FREQ(_channel, _num, _mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(dds##_channel##_freq##_num,			\
			_mode, _show, _store, _addr)


#define IIO_CONST_ATTR_FREQ_SCALE(_channel, _string)			\
	IIO_CONST_ATTR(dds##_channel##_freq_scale, _string)


#define IIO_DEV_ATTR_FREQSYMBOL(_channel, _mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(dds##_channel##_freqsymbol,			\
			_mode, _show, _store, _addr);


#define IIO_DEV_ATTR_PHASE(_channel, _num, _mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(dds##_channel##_phase##_num,			\
			_mode, _show, _store, _addr)


#define IIO_CONST_ATTR_PHASE_SCALE(_channel, _string)			\
	IIO_CONST_ATTR(dds##_channel##_phase_scale, _string)


#define IIO_DEV_ATTR_PHASESYMBOL(_channel, _mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(dds##_channel##_phasesymbol,			\
			_mode, _show, _store, _addr);


#define IIO_DEV_ATTR_PINCONTROL_EN(_channel, _mode, _show, _store, _addr)\
	IIO_DEVICE_ATTR(dds##_channel##_pincontrol_en,			\
			_mode, _show, _store, _addr);


#define IIO_DEV_ATTR_PINCONTROL_FREQ_EN(_channel, _mode, _show, _store, _addr)\
	IIO_DEVICE_ATTR(dds##_channel##_pincontrol_freq_en,		\
			_mode, _show, _store, _addr);


#define IIO_DEV_ATTR_PINCONTROL_PHASE_EN(_channel, _mode, _show, _store, _addr)\
	IIO_DEVICE_ATTR(dds##_channel##_pincontrol_phase_en,		\
			_mode, _show, _store, _addr);


#define IIO_DEV_ATTR_OUT_ENABLE(_channel, _mode, _show, _store, _addr)	\
	IIO_DEVICE_ATTR(dds##_channel##_out_enable,			\
			_mode, _show, _store, _addr);


#define IIO_DEV_ATTR_OUTY_ENABLE(_channel, _output,			\
			_mode, _show, _store, _addr)			\
	IIO_DEVICE_ATTR(dds##_channel##_out##_output##_enable,		\
			_mode, _show, _store, _addr);


#define IIO_DEV_ATTR_OUT_WAVETYPE(_channel, _output, _store, _addr)	\
	IIO_DEVICE_ATTR(dds##_channel##_out##_output##_wavetype,	\
			S_IWUSR, NULL, _store, _addr);


#define IIO_CONST_ATTR_OUT_WAVETYPES_AVAILABLE(_channel, _output, _modes)\
	IIO_CONST_ATTR(dds##_channel##_out##_output##_wavetype_available,\
			_modes);

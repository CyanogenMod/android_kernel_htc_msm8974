/*
 *
 *
 *  Copyright (C) 2005 Mike Isely <isely@pobox.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef __PVRUSB2_HDW_INTERNAL_H
#define __PVRUSB2_HDW_INTERNAL_H


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include "pvrusb2-hdw.h"
#include "pvrusb2-io.h"
#include <media/v4l2-device.h>
#include <media/cx2341x.h>
#include <media/ir-kbd-i2c.h>
#include "pvrusb2-devattr.h"

#define PVR2_CVAL_HSM_FAIL 0
#define PVR2_CVAL_HSM_FULL 1
#define PVR2_CVAL_HSM_HIGH 2

#define PVR2_VID_ENDPOINT        0x84
#define PVR2_UNK_ENDPOINT        0x86    
#define PVR2_VBI_ENDPOINT        0x88

#define PVR2_CTL_BUFFSIZE        64

#define FREQTABLE_SIZE 500

#define LOCK_TAKE(x) do { mutex_lock(&x##_mutex); x##_held = !0; } while (0)
#define LOCK_GIVE(x) do { x##_held = 0; mutex_unlock(&x##_mutex); } while (0)

typedef int (*pvr2_ctlf_is_dirty)(struct pvr2_ctrl *);
typedef void (*pvr2_ctlf_clear_dirty)(struct pvr2_ctrl *);
typedef int (*pvr2_ctlf_check_value)(struct pvr2_ctrl *,int);
typedef int (*pvr2_ctlf_get_value)(struct pvr2_ctrl *,int *);
typedef int (*pvr2_ctlf_set_value)(struct pvr2_ctrl *,int msk,int val);
typedef int (*pvr2_ctlf_val_to_sym)(struct pvr2_ctrl *,int msk,int val,
				    char *,unsigned int,unsigned int *);
typedef int (*pvr2_ctlf_sym_to_val)(struct pvr2_ctrl *,
				    const char *,unsigned int,
				    int *mskp,int *valp);
typedef unsigned int (*pvr2_ctlf_get_v4lflags)(struct pvr2_ctrl *);

struct pvr2_ctl_info {
	
	const char *name;

	
	const char *desc;

	
	pvr2_ctlf_get_value get_value;      
	pvr2_ctlf_get_value get_def_value;  
	pvr2_ctlf_get_value get_min_value;  
	pvr2_ctlf_get_value get_max_value;  
	pvr2_ctlf_set_value set_value;      
	pvr2_ctlf_check_value check_value;  
	pvr2_ctlf_val_to_sym val_to_sym;    
	pvr2_ctlf_sym_to_val sym_to_val;    
	pvr2_ctlf_is_dirty is_dirty;        
	pvr2_ctlf_clear_dirty clear_dirty;  
	pvr2_ctlf_get_v4lflags get_v4lflags;

	
	enum pvr2_ctl_type type;

	
	int v4l_id;

	
	int internal_id;

	
	int skip_init;

	
	int default_value;

	
	union {
		struct { 
			long min_value; 
			long max_value; 
		} type_int;
		struct { 
			unsigned int count;       
			const char * const *value_names; 
		} type_enum;
		struct { 
			unsigned int valid_bits; 
			const char **bit_names;  
		} type_bitmask;
	} def;
};


#define PVR2_CTLD_INFO_DESC_SIZE 32
struct pvr2_ctld_info {
	struct pvr2_ctl_info info;
	char desc[PVR2_CTLD_INFO_DESC_SIZE];
};

struct pvr2_ctrl {
	const struct pvr2_ctl_info *info;
	struct pvr2_hdw *hdw;
};



#define FW1_STATE_UNKNOWN 0
#define FW1_STATE_MISSING 1
#define FW1_STATE_FAILED 2
#define FW1_STATE_RELOAD 3
#define FW1_STATE_OK 4

#define PVR2_PATHWAY_UNKNOWN 0
#define PVR2_PATHWAY_ANALOG 1
#define PVR2_PATHWAY_DIGITAL 2

typedef int (*pvr2_i2c_func)(struct pvr2_hdw *,u8,u8 *,u16,u8 *, u16);
#define PVR2_I2C_FUNC_CNT 128

struct pvr2_hdw {
	
	struct usb_device *usb_dev;
	struct usb_interface *usb_intf;

	
	struct v4l2_device v4l2_dev;
	const struct pvr2_device_desc *hdw_desc;

	
	struct workqueue_struct *workqueue;
	struct work_struct workpoll;     

	
	struct pvr2_stream *vid_stream;

	
	struct mutex big_lock_mutex;
	int big_lock_held;  

	char name[32];

	char identifier[32];

	
	struct i2c_adapter i2c_adap;
	struct i2c_algorithm i2c_algo;
	pvr2_i2c_func i2c_func[PVR2_I2C_FUNC_CNT];
	int i2c_cx25840_hack_state;
	int i2c_linked;

	
	unsigned int ir_scheme_active; 
	struct IR_i2c_init_data ir_init_data; 

	
	unsigned int freqTable[FREQTABLE_SIZE];
	unsigned int freqProgSlot;

	
	struct mutex ctl_lock_mutex;
	int ctl_lock_held;  
	struct urb *ctl_write_urb;
	struct urb *ctl_read_urb;
	unsigned char *ctl_write_buffer;
	unsigned char *ctl_read_buffer;
	int ctl_write_pend_flag;
	int ctl_read_pend_flag;
	int ctl_timeout_flag;
	struct completion ctl_done;
	unsigned char cmd_buffer[PVR2_CTL_BUFFSIZE];
	int cmd_debug_state;               
	unsigned char cmd_debug_code;      
	unsigned int cmd_debug_write_len;  
	unsigned int cmd_debug_read_len;   

	int state_pathway_ok;         
	int state_encoder_ok;         
	int state_encoder_run;        
	int state_encoder_config;     
	int state_encoder_waitok;     
	int state_encoder_runok;      
	int state_decoder_run;        
	int state_decoder_ready;      
	int state_usbstream_run;      
	int state_decoder_quiescent;  
	int state_pipeline_config;    
	int state_pipeline_req;       
	int state_pipeline_pause;     
	int state_pipeline_idle;      

	unsigned int master_state;

	
	int led_on;

	
	int state_stale;

	void (*state_func)(void *);
	void *state_data;

	struct timer_list quiescent_timer;

	struct timer_list decoder_stabilization_timer;

	
	struct timer_list encoder_wait_timer;

	
	struct timer_list encoder_run_timer;

	
	wait_queue_head_t state_wait_data;


	int force_dirty;        
	int flag_ok;            
	int flag_modulefail;    
	int flag_disconnected;  
	int flag_init_ok;       
	int fw1_state;          
	int pathway_state;      
	int flag_decoder_missed;
	int flag_tripped;       

	unsigned int decoder_client_id;

	
	char *fw_buffer;
	unsigned int fw_size;
	int fw_cpu_flag; 

	
	unsigned int tuner_type;
	int tuner_updated;
	unsigned int freqValTelevision;  
	unsigned int freqValRadio;       
	unsigned int freqSlotTelevision; 
	unsigned int freqSlotRadio;      
	unsigned int freqSelector;       
	int freqDirty;

	
	struct v4l2_tuner tuner_signal_info;
	int tuner_signal_stale;

	
	struct v4l2_cropcap cropcap_info;
	int cropcap_stale;

	
	v4l2_std_id std_mask_eeprom; 
	v4l2_std_id std_mask_avail;  
	v4l2_std_id std_mask_cur;    
	unsigned int std_enum_cnt;   
	int std_enum_cur;            
	int std_dirty;               
	struct pvr2_ctl_info std_info_enum;
	struct pvr2_ctl_info std_info_avail;
	struct pvr2_ctl_info std_info_cur;
	struct v4l2_standard *std_defs;
	const char **std_enum_names;

	
	const char *std_mask_ptrs[32];
	char std_mask_names[32][10];

	int unit_number;             
	unsigned long serial_number; 

	char bus_info[32]; 

	int v4l_minor_number_video;
	int v4l_minor_number_vbi;
	int v4l_minor_number_radio;

	
	unsigned int input_avail_mask;
	
	unsigned int input_allowed_mask;

	
	int eeprom_addr;

	enum pvr2_config active_stream_type;
	enum pvr2_config desired_stream_type;

	
	struct cx2341x_mpeg_params enc_cur_state;
	struct cx2341x_mpeg_params enc_ctl_state;
	
	int enc_stale;
	
	int enc_unsafe_stale;
	
	int enc_cur_valid;

	
#define VCREATE_DATA(lab) int lab##_val; int lab##_dirty
	VCREATE_DATA(brightness);
	VCREATE_DATA(contrast);
	VCREATE_DATA(saturation);
	VCREATE_DATA(hue);
	VCREATE_DATA(volume);
	VCREATE_DATA(balance);
	VCREATE_DATA(bass);
	VCREATE_DATA(treble);
	VCREATE_DATA(mute);
	VCREATE_DATA(cropl);
	VCREATE_DATA(cropt);
	VCREATE_DATA(cropw);
	VCREATE_DATA(croph);
	VCREATE_DATA(input);
	VCREATE_DATA(audiomode);
	VCREATE_DATA(res_hor);
	VCREATE_DATA(res_ver);
	VCREATE_DATA(srate);
#undef VCREATE_DATA

	struct pvr2_ctld_info *mpeg_ctrl_info;

	struct pvr2_ctrl *controls;
	unsigned int control_cnt;
};

unsigned long pvr2_hdw_get_cur_freq(struct pvr2_hdw *);

void pvr2_hdw_status_poll(struct pvr2_hdw *);

#endif 


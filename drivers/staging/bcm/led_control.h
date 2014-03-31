#ifndef _LED_CONTROL_H
#define _LED_CONTROL_H

#define NUM_OF_LEDS 4

#define DSD_START_OFFSET			0x0200
#define EEPROM_VERSION_OFFSET			0x020E
#define EEPROM_HW_PARAM_POINTER_ADDRESS		0x0218
#define EEPROM_HW_PARAM_POINTER_ADDRRES_MAP5	0x0220
#define GPIO_SECTION_START_OFFSET		0x03

#define COMPATIBILITY_SECTION_LENGTH         42
#define COMPATIBILITY_SECTION_LENGTH_MAP5    84


#define EEPROM_MAP5_MAJORVERSION             5
#define EEPROM_MAP5_MINORVERSION             0


#define MAX_NUM_OF_BLINKS			10
#define NUM_OF_GPIO_PINS			16

#define DISABLE_GPIO_NUM			0xFF
#define EVENT_SIGNALED				1

#define MAX_FILE_NAME_BUFFER_SIZE		100

#define TURN_ON_LED(GPIO, index) do { \
							UINT gpio_val = GPIO; \
							(Adapter->LEDInfo.LEDState[index].BitPolarity == 1) ? \
							wrmaltWithLock(Adapter, BCM_GPIO_OUTPUT_SET_REG, &gpio_val, sizeof(gpio_val)) : \
							wrmaltWithLock(Adapter, BCM_GPIO_OUTPUT_CLR_REG, &gpio_val, sizeof(gpio_val)); \
						} while (0);

#define TURN_OFF_LED(GPIO, index)  do { \
							UINT gpio_val = GPIO; \
							(Adapter->LEDInfo.LEDState[index].BitPolarity == 1) ? \
							wrmaltWithLock(Adapter, BCM_GPIO_OUTPUT_CLR_REG, &gpio_val, sizeof(gpio_val)) : \
							wrmaltWithLock(Adapter, BCM_GPIO_OUTPUT_SET_REG, &gpio_val, sizeof(gpio_val));  \
						} while (0);

#define B_ULONG32 unsigned long



typedef enum _LEDColors{
	RED_LED = 1,
	BLUE_LED = 2,
	YELLOW_LED = 3,
	GREEN_LED = 4
} LEDColors;	

typedef enum LedEvents {
	SHUTDOWN_EXIT = 0x00,
	DRIVER_INIT = 0x1,
	FW_DOWNLOAD = 0x2,
	FW_DOWNLOAD_DONE = 0x4,
	NO_NETWORK_ENTRY = 0x8,
	NORMAL_OPERATION = 0x10,
	LOWPOWER_MODE_ENTER = 0x20,
	IDLEMODE_CONTINUE = 0x40,
	IDLEMODE_EXIT = 0x80,
	LED_THREAD_INACTIVE = 0x100,  
	LED_THREAD_ACTIVE = 0x200,    
	DRIVER_HALT = 0xff
} LedEventInfo_t;	

typedef struct LedStateInfo_t {
	UCHAR LED_Type; 
	UCHAR LED_On_State; 
	UCHAR LED_Blink_State; 
	UCHAR GPIO_Num;
	UCHAR BitPolarity; 
} LEDStateInfo, *pLEDStateInfo;


typedef struct _LED_INFO_STRUCT {
	LEDStateInfo	LEDState[NUM_OF_LEDS];
	BOOLEAN		bIdleMode_tx_from_host; 
	BOOLEAN			bIdle_led_off;
	wait_queue_head_t   notify_led_event;
	wait_queue_head_t	idleModeSyncEvent;
	struct task_struct  *led_cntrl_threadid;
	int                 led_thread_running;
	BOOLEAN			bLedInitDone;

} LED_INFO_STRUCT, *PLED_INFO_STRUCT;
#define BCM_LED_THREAD_DISABLED		0 
#define BCM_LED_THREAD_RUNNING_ACTIVELY	1 
#define BCM_LED_THREAD_RUNNING_INACTIVELY 2 



#endif


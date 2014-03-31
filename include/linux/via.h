
#define VIA_FUNCTION_PARPORT_SPP     0x00
#define VIA_FUNCTION_PARPORT_ECP     0x01
#define VIA_FUNCTION_PARPORT_EPP     0x02
#define VIA_FUNCTION_PARPORT_DISABLE 0x03
#define VIA_FUNCTION_PROBE           0xFF /* Special magic value to be used in code, not to be written into chip */

#define VIA_PARPORT_ECPEPP 0X20
#define VIA_PARPORT_BIDIR  0x80

#define VIA_CONFIG_INDEX 0x3F0
#define VIA_CONFIG_DATA  0x3F1

#define VIA_IRQCONTROL_PARALLEL 0xF0
#define VIA_DMACONTROL_PARALLEL 0x0C

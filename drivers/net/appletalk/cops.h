
#ifndef __LINUX_COPSLTALK_H
#define __LINUX_COPSLTALK_H

#ifdef __KERNEL__

#define MAX_LLAP_SIZE		603

#define TANG_CARD_STATUS        1
#define TANG_CLEAR_INT          1
#define TANG_RESET              3

#define TANG_TX_READY           1
#define TANG_RX_READY           2

#define DAYNA_CMD_DATA          0
#define DAYNA_CLEAR_INT         1
#define DAYNA_CARD_STATUS       2
#define DAYNA_INT_CARD          3
#define DAYNA_RESET             4

#define DAYNA_RX_READY          0
#define DAYNA_TX_READY          1
#define DAYNA_RX_REQUEST        3

#define COPS_CLEAR_INT  1

#define LAP_INIT        1       
#define LAP_INIT_RSP    2       
#define LAP_WRITE       3       
#define DATA_READ       4       
#define LAP_RESPONSE    4       
#define LAP_GETSTAT     5       
#define LAP_RSPSTAT     6       

#endif

struct ltfirmware
{
        unsigned int length;
        const unsigned char *data;
};

#define DAYNA 1
#define TANGENT 2

#endif

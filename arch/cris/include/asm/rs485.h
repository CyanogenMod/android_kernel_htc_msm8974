
struct rs485_control {
        unsigned short rts_on_send;
        unsigned short rts_after_sent;
        unsigned long delay_rts_before_send;
        unsigned short enabled;
};

struct rs485_write {
        unsigned short outc_size;
        unsigned char *outc;
};


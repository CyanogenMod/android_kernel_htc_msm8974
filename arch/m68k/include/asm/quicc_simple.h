
#ifndef __SIMPLE_H
#define __SIMPLE_H


#define GLB_SCC_0   0
#define GLB_SCC_1   1
#define GLB_SCC_2   2
#define GLB_SCC_3   3

typedef void (int_routine)(unsigned short interrupt_event);
typedef int_routine *int_routine_ptr; 
typedef void *(alloc_routine)(int length);
typedef void (free_routine)(int scc_num, int channel_num, void *buf);
typedef void (store_rx_buffer_routine)(int scc_num, int channel_num, void *buff, int length);
typedef int  (handle_tx_error_routine)(int scc_num, int channel_num, QUICC_BD *tbd);
typedef void (handle_rx_error_routine)(int scc_num, int channel_num, QUICC_BD *rbd);
typedef void (handle_lost_error_routine)(int scc_num, int channel_num);

typedef void (handle_glob_overrun_routine)(int scc_number);
typedef void (handle_glob_underrun_routine)(int scc_number);
typedef void (glob_intr_q_overflow_routine)(int scc_number);

void quicc_issue_cmd (unsigned short cmd, int scc_num);
void quicc_init(void);
void quicc_scc_init(int scc_number, int number_of_rx_buf, int number_of_tx_buf);
void quicc_smc_init(int smc_number, int number_of_rx_buf, int number_of_tx_buf);
void quicc_scc_start(int scc_num);
void quicc_scc_loopback(int scc_num);

unsigned short  IntrDis(void);
void            IntrEna(unsigned short old_sr);

void print_rbd(int scc_num);
void print_tbd(int scc_num);

#endif

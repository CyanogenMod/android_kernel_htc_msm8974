#ifndef ISCSI_THREAD_QUEUE_H
#define ISCSI_THREAD_QUEUE_H

extern int iscsi_thread_set_force_reinstatement(struct iscsi_conn *);
extern void iscsi_add_ts_to_inactive_list(struct iscsi_thread_set *);
extern int iscsi_allocate_thread_sets(u32);
extern void iscsi_deallocate_thread_sets(void);
extern void iscsi_activate_thread_set(struct iscsi_conn *, struct iscsi_thread_set *);
extern struct iscsi_thread_set *iscsi_get_thread_set(void);
extern void iscsi_set_thread_clear(struct iscsi_conn *, u8);
extern void iscsi_set_thread_set_signal(struct iscsi_conn *, u8);
extern int iscsi_release_thread_set(struct iscsi_conn *);
extern struct iscsi_conn *iscsi_rx_thread_pre_handler(struct iscsi_thread_set *);
extern struct iscsi_conn *iscsi_tx_thread_pre_handler(struct iscsi_thread_set *);
extern int iscsi_thread_set_init(void);
extern void iscsi_thread_set_free(void);

extern int iscsi_target_tx_thread(void *);
extern int iscsi_target_rx_thread(void *);

#define TARGET_THREAD_SET_COUNT			4

#define ISCSI_RX_THREAD                         1
#define ISCSI_TX_THREAD                         2
#define ISCSI_RX_THREAD_NAME			"iscsi_trx"
#define ISCSI_TX_THREAD_NAME			"iscsi_ttx"
#define ISCSI_BLOCK_RX_THREAD			0x1
#define ISCSI_BLOCK_TX_THREAD			0x2
#define ISCSI_CLEAR_RX_THREAD			0x1
#define ISCSI_CLEAR_TX_THREAD			0x2
#define ISCSI_SIGNAL_RX_THREAD			0x1
#define ISCSI_SIGNAL_TX_THREAD			0x2

#define ISCSI_THREAD_SET_FREE			1
#define ISCSI_THREAD_SET_ACTIVE			2
#define ISCSI_THREAD_SET_DIE			3
#define ISCSI_THREAD_SET_RESET			4
#define ISCSI_THREAD_SET_DEALLOCATE_THREADS	5

#define ISCSI_TS_BITMAP_BITS			32768

struct iscsi_thread_set {
	
	int	blocked_threads;
	
	int	create_threads;
	
	int	delay_inactive;
	
	int	status;
	
	int	signal_sent;
	
	int	thread_clear;
	
	int	thread_count;
	
	u32	thread_id;
	
	struct iscsi_conn	*conn;
	
	spinlock_t	ts_state_lock;
	
	struct completion	rx_post_start_comp;
	
	struct completion	tx_post_start_comp;
	
	struct completion	rx_restart_comp;
	
	struct completion	tx_restart_comp;
	
	struct completion	rx_start_comp;
	
	struct completion	tx_start_comp;
	
	struct task_struct	*rx_thread;
	
	struct task_struct	*tx_thread;
	
	struct list_head	ts_list;
};

#endif   

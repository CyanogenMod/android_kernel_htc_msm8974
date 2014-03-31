#ifndef TARGET_CORE_BASE_H
#define TARGET_CORE_BASE_H

#include <linux/in.h>
#include <linux/configfs.h>
#include <linux/dma-mapping.h>
#include <linux/blkdev.h>
#include <scsi/scsi_cmnd.h>
#include <net/sock.h>
#include <net/tcp.h>

#define TARGET_CORE_MOD_VERSION		"v4.1.0-rc2-ml"
#define TARGET_CORE_VERSION		TARGET_CORE_MOD_VERSION

#define TRANSPORT_MAX_LUNS_PER_TPG		256
#define TCM_MAX_COMMAND_SIZE			32
#define TRANSPORT_SENSE_BUFFER			SCSI_SENSE_BUFFERSIZE
#define SPC_SENSE_KEY_OFFSET			2
#define SPC_ADD_SENSE_LEN_OFFSET		7
#define SPC_ASC_KEY_OFFSET			12
#define SPC_ASCQ_KEY_OFFSET			13
#define TRANSPORT_IQN_LEN			224
#define LU_GROUP_NAME_BUF			256
#define TG_PT_GROUP_NAME_BUF			256
#define VPD_TMP_BUF_SIZE			128
#define READ_BLOCK_LEN          		6
#define READ_CAP_LEN            		8
#define READ_POSITION_LEN       		20
#define INQUIRY_LEN				36
#define INQUIRY_VPD_SERIAL_LEN			254
#define INQUIRY_VPD_DEVICE_IDENTIFIER_LEN	254

#define PYX_TRANSPORT_WINDOW_CLOSED_THRESHOLD	3
#define PYX_TRANSPORT_WINDOW_CLOSED_WAIT_SHORT	3  
#define PYX_TRANSPORT_WINDOW_CLOSED_WAIT_LONG	10 

#define PYX_TRANSPORT_STATUS_INTERVAL		5 

#define SDF_FIRMWARE_VPD_UNIT_SERIAL		0x00000001
#define SDF_EMULATED_VPD_UNIT_SERIAL		0x00000002
#define SDF_USING_UDEV_PATH			0x00000004
#define SDF_USING_ALIAS				0x00000008

#define DF_READ_ONLY				0x00000001
#define DF_SPC2_RESERVATIONS			0x00000002
#define DF_SPC2_RESERVATIONS_WITH_ISID		0x00000004

#define DA_MAX_UNMAP_LBA_COUNT			0
#define DA_MAX_UNMAP_BLOCK_DESC_COUNT		0
#define DA_UNMAP_GRANULARITY_DEFAULT		0
#define DA_UNMAP_GRANULARITY_ALIGNMENT_DEFAULT	0
#define DA_FABRIC_MAX_SECTORS			8192
#define DA_EMULATE_DPO				0
#define DA_EMULATE_FUA_WRITE			1
#define DA_EMULATE_FUA_READ			0
#define DA_EMULATE_WRITE_CACHE			0
#define DA_EMULATE_UA_INTLLCK_CTRL		0
#define DA_EMULATE_TAS				1
#define DA_EMULATE_TPU				0
#define DA_EMULATE_TPWS				0
#define DA_EMULATE_RESERVATIONS			0
#define DA_EMULATE_ALUA				0
#define DA_ENFORCE_PR_ISIDS			1
#define DA_STATUS_MAX_SECTORS_MIN		16
#define DA_STATUS_MAX_SECTORS_MAX		8192
#define DA_IS_NONROT				0
#define DA_EMULATE_REST_REORD			0

#define SE_INQUIRY_BUF				512
#define SE_MODE_PAGE_BUF			512

enum hba_flags_table {
	HBA_FLAGS_INTERNAL_USE	= 0x01,
	HBA_FLAGS_PSCSI_MODE	= 0x02,
};

enum transport_lun_status_table {
	TRANSPORT_LUN_STATUS_FREE = 0,
	TRANSPORT_LUN_STATUS_ACTIVE = 1,
};

enum transport_tpg_type_table {
	TRANSPORT_TPG_TYPE_NORMAL = 0,
	TRANSPORT_TPG_TYPE_DISCOVERY = 1,
};

enum se_task_flags {
	TF_ACTIVE		= (1 << 0),
	TF_SENT			= (1 << 1),
	TF_REQUEST_STOP		= (1 << 2),
	TF_HAS_SENSE		= (1 << 3),
};

enum transport_state_table {
	TRANSPORT_NO_STATE	= 0,
	TRANSPORT_NEW_CMD	= 1,
	TRANSPORT_WRITE_PENDING	= 3,
	TRANSPORT_PROCESS_WRITE	= 4,
	TRANSPORT_PROCESSING	= 5,
	TRANSPORT_COMPLETE	= 6,
	TRANSPORT_PROCESS_TMR	= 9,
	TRANSPORT_ISTATE_PROCESSING = 11,
	TRANSPORT_NEW_CMD_MAP	= 16,
	TRANSPORT_COMPLETE_QF_WP = 18,
	TRANSPORT_COMPLETE_QF_OK = 19,
};

enum se_cmd_flags_table {
	SCF_SUPPORTED_SAM_OPCODE	= 0x00000001,
	SCF_TRANSPORT_TASK_SENSE	= 0x00000002,
	SCF_EMULATED_TASK_SENSE		= 0x00000004,
	SCF_SCSI_DATA_SG_IO_CDB		= 0x00000008,
	SCF_SCSI_CONTROL_SG_IO_CDB	= 0x00000010,
	SCF_SCSI_NON_DATA_CDB		= 0x00000020,
	SCF_SCSI_TMR_CDB		= 0x00000040,
	SCF_SCSI_CDB_EXCEPTION		= 0x00000080,
	SCF_SCSI_RESERVATION_CONFLICT	= 0x00000100,
	SCF_FUA				= 0x00000200,
	SCF_SE_LUN_CMD			= 0x00000800,
	SCF_SE_ALLOW_EOO		= 0x00001000,
	SCF_BIDI			= 0x00002000,
	SCF_SENT_CHECK_CONDITION	= 0x00004000,
	SCF_OVERFLOW_BIT		= 0x00008000,
	SCF_UNDERFLOW_BIT		= 0x00010000,
	SCF_SENT_DELAYED_TAS		= 0x00020000,
	SCF_ALUA_NON_OPTIMIZED		= 0x00040000,
	SCF_DELAYED_CMD_FROM_SAM_ATTR	= 0x00080000,
	SCF_UNUSED			= 0x00100000,
	SCF_PASSTHROUGH_SG_TO_MEM_NOALLOC = 0x00200000,
	SCF_ACK_KREF			= 0x00400000,
};

enum transport_lunflags_table {
	TRANSPORT_LUNFLAGS_NO_ACCESS		= 0x00,
	TRANSPORT_LUNFLAGS_INITIATOR_ACCESS	= 0x01,
	TRANSPORT_LUNFLAGS_READ_ONLY		= 0x02,
	TRANSPORT_LUNFLAGS_READ_WRITE		= 0x04,
};

enum transport_device_status_table {
	TRANSPORT_DEVICE_ACTIVATED		= 0x01,
	TRANSPORT_DEVICE_DEACTIVATED		= 0x02,
	TRANSPORT_DEVICE_QUEUE_FULL		= 0x04,
	TRANSPORT_DEVICE_SHUTDOWN		= 0x08,
	TRANSPORT_DEVICE_OFFLINE_ACTIVATED	= 0x10,
	TRANSPORT_DEVICE_OFFLINE_DEACTIVATED	= 0x20,
};

enum tcm_sense_reason_table {
	TCM_NON_EXISTENT_LUN			= 0x01,
	TCM_UNSUPPORTED_SCSI_OPCODE		= 0x02,
	TCM_INCORRECT_AMOUNT_OF_DATA		= 0x03,
	TCM_UNEXPECTED_UNSOLICITED_DATA		= 0x04,
	TCM_SERVICE_CRC_ERROR			= 0x05,
	TCM_SNACK_REJECTED			= 0x06,
	TCM_SECTOR_COUNT_TOO_MANY		= 0x07,
	TCM_INVALID_CDB_FIELD			= 0x08,
	TCM_INVALID_PARAMETER_LIST		= 0x09,
	TCM_LOGICAL_UNIT_COMMUNICATION_FAILURE	= 0x0a,
	TCM_UNKNOWN_MODE_PAGE			= 0x0b,
	TCM_WRITE_PROTECTED			= 0x0c,
	TCM_CHECK_CONDITION_ABORT_CMD		= 0x0d,
	TCM_CHECK_CONDITION_UNIT_ATTENTION	= 0x0e,
	TCM_CHECK_CONDITION_NOT_READY		= 0x0f,
	TCM_RESERVATION_CONFLICT		= 0x10,
};

enum target_sc_flags_table {
	TARGET_SCF_BIDI_OP		= 0x01,
	TARGET_SCF_ACK_KREF		= 0x02,
	TARGET_SCF_UNKNOWN_SIZE		= 0x04,
};

enum tcm_tmreq_table {
	TMR_ABORT_TASK		= 1,
	TMR_ABORT_TASK_SET	= 2,
	TMR_CLEAR_ACA		= 3,
	TMR_CLEAR_TASK_SET	= 4,
	TMR_LUN_RESET		= 5,
	TMR_TARGET_WARM_RESET	= 6,
	TMR_TARGET_COLD_RESET	= 7,
	TMR_FABRIC_TMR		= 255,
};

enum tcm_tmrsp_table {
	TMR_FUNCTION_COMPLETE		= 0,
	TMR_TASK_DOES_NOT_EXIST		= 1,
	TMR_LUN_DOES_NOT_EXIST		= 2,
	TMR_TASK_STILL_ALLEGIANT	= 3,
	TMR_TASK_FAILOVER_NOT_SUPPORTED	= 4,
	TMR_TASK_MGMT_FUNCTION_NOT_SUPPORTED	= 5,
	TMR_FUNCTION_AUTHORIZATION_FAILED = 6,
	TMR_FUNCTION_REJECTED		= 255,
};

struct se_obj {
	atomic_t obj_access_count;
};

typedef enum {
	SPC_ALUA_PASSTHROUGH,
	SPC2_ALUA_DISABLED,
	SPC3_ALUA_EMULATED
} t10_alua_index_t;

typedef enum {
	SAM_TASK_ATTR_PASSTHROUGH,
	SAM_TASK_ATTR_UNTAGGED,
	SAM_TASK_ATTR_EMULATED
} t10_task_attr_index_t;

typedef enum {
	SCSI_INST_INDEX,
	SCSI_DEVICE_INDEX,
	SCSI_AUTH_INTR_INDEX,
	SCSI_INDEX_TYPE_MAX
} scsi_index_t;

struct se_cmd;

struct t10_alua {
	t10_alua_index_t alua_type;
	
	u16	alua_tg_pt_gps_counter;
	u32	alua_tg_pt_gps_count;
	spinlock_t tg_pt_gps_lock;
	struct se_subsystem_dev *t10_sub_dev;
	
	struct t10_alua_tg_pt_gp *default_tg_pt_gp;
	
	struct config_group alua_tg_pt_gps_group;
	int (*alua_state_check)(struct se_cmd *, unsigned char *, u8 *);
	struct list_head tg_pt_gps_list;
};

struct t10_alua_lu_gp {
	u16	lu_gp_id;
	int	lu_gp_valid_id;
	u32	lu_gp_members;
	atomic_t lu_gp_ref_cnt;
	spinlock_t lu_gp_lock;
	struct config_group lu_gp_group;
	struct list_head lu_gp_node;
	struct list_head lu_gp_mem_list;
};

struct t10_alua_lu_gp_member {
	bool lu_gp_assoc;
	atomic_t lu_gp_mem_ref_cnt;
	spinlock_t lu_gp_mem_lock;
	struct t10_alua_lu_gp *lu_gp;
	struct se_device *lu_gp_mem_dev;
	struct list_head lu_gp_mem_list;
};

struct t10_alua_tg_pt_gp {
	u16	tg_pt_gp_id;
	int	tg_pt_gp_valid_id;
	int	tg_pt_gp_alua_access_status;
	int	tg_pt_gp_alua_access_type;
	int	tg_pt_gp_nonop_delay_msecs;
	int	tg_pt_gp_trans_delay_msecs;
	int	tg_pt_gp_pref;
	int	tg_pt_gp_write_metadata;
	
#define ALUA_MD_BUF_LEN				1024
	u32	tg_pt_gp_md_buf_len;
	u32	tg_pt_gp_members;
	atomic_t tg_pt_gp_alua_access_state;
	atomic_t tg_pt_gp_ref_cnt;
	spinlock_t tg_pt_gp_lock;
	struct mutex tg_pt_gp_md_mutex;
	struct se_subsystem_dev *tg_pt_gp_su_dev;
	struct config_group tg_pt_gp_group;
	struct list_head tg_pt_gp_list;
	struct list_head tg_pt_gp_mem_list;
};

struct t10_alua_tg_pt_gp_member {
	bool tg_pt_gp_assoc;
	atomic_t tg_pt_gp_mem_ref_cnt;
	spinlock_t tg_pt_gp_mem_lock;
	struct t10_alua_tg_pt_gp *tg_pt_gp;
	struct se_port *tg_pt;
	struct list_head tg_pt_gp_mem_list;
};

struct t10_vpd {
	unsigned char device_identifier[INQUIRY_VPD_DEVICE_IDENTIFIER_LEN];
	int protocol_identifier_set;
	u32 protocol_identifier;
	u32 device_identifier_code_set;
	u32 association;
	u32 device_identifier_type;
	struct list_head vpd_list;
};

struct t10_wwn {
	char vendor[8];
	char model[16];
	char revision[4];
	char unit_serial[INQUIRY_VPD_SERIAL_LEN];
	spinlock_t t10_vpd_lock;
	struct se_subsystem_dev *t10_sub_dev;
	struct config_group t10_wwn_group;
	struct list_head t10_vpd_list;
};


typedef enum {
	SPC_PASSTHROUGH,
	SPC2_RESERVATIONS,
	SPC3_PERSISTENT_RESERVATIONS
} t10_reservations_index_t;

struct t10_pr_registration {
	
#define PR_REG_ISID_LEN				16
	
#define PR_REG_ISID_ID_LEN			(PR_REG_ISID_LEN + 5)
	char pr_reg_isid[PR_REG_ISID_LEN];
	
#define PR_APTPL_MAX_IPORT_LEN			256
	unsigned char pr_iport[PR_APTPL_MAX_IPORT_LEN];
	
#define PR_APTPL_MAX_TPORT_LEN			256
	unsigned char pr_tport[PR_APTPL_MAX_TPORT_LEN];
	
	unsigned char *pr_aptpl_buf;
	u16 pr_aptpl_rpti;
	u16 pr_reg_tpgt;
	
	int pr_reg_all_tg_pt;
	
	int pr_reg_aptpl;
	int pr_res_holder;
	int pr_res_type;
	int pr_res_scope;
	
	bool isid_present_at_reg;
	u32 pr_res_mapped_lun;
	u32 pr_aptpl_target_lun;
	u32 pr_res_generation;
	u64 pr_reg_bin_isid;
	u64 pr_res_key;
	atomic_t pr_res_holders;
	struct se_node_acl *pr_reg_nacl;
	struct se_dev_entry *pr_reg_deve;
	struct se_lun *pr_reg_tg_pt_lun;
	struct list_head pr_reg_list;
	struct list_head pr_reg_abort_list;
	struct list_head pr_reg_aptpl_list;
	struct list_head pr_reg_atp_list;
	struct list_head pr_reg_atp_mem_list;
};

struct t10_reservation_ops {
	int (*t10_reservation_check)(struct se_cmd *, u32 *);
	int (*t10_seq_non_holder)(struct se_cmd *, unsigned char *, u32);
	int (*t10_pr_register)(struct se_cmd *);
	int (*t10_pr_clear)(struct se_cmd *);
};

struct t10_reservation {
	
	int pr_all_tg_pt;
	int pr_aptpl_active;
	
#define PR_APTPL_BUF_LEN			8192
	u32 pr_aptpl_buf_len;
	u32 pr_generation;
	t10_reservations_index_t res_type;
	spinlock_t registration_lock;
	spinlock_t aptpl_reg_lock;
	struct se_node_acl *pr_res_holder;
	struct list_head registration_list;
	struct list_head aptpl_reg_list;
	struct t10_reservation_ops pr_ops;
};

struct se_queue_obj {
	atomic_t		queue_cnt;
	spinlock_t		cmd_queue_lock;
	struct list_head	qobj_list;
	wait_queue_head_t	thread_wq;
};

struct se_task {
	unsigned long long	task_lba;
	u32			task_sectors;
	u32			task_size;
	struct se_cmd		*task_se_cmd;
	struct scatterlist	*task_sg;
	u32			task_sg_nents;
	u16			task_flags;
	u8			task_scsi_status;
	enum dma_data_direction	task_data_direction;
	struct list_head	t_list;
	struct list_head	t_execute_list;
	struct list_head	t_state_list;
	bool			t_state_active;
	struct completion	task_stop_comp;
};

struct se_tmr_req {
	
	u8			function;
	
	u8			response;
	int			call_transport;
	
	u32			ref_task_tag;
	
	u64			ref_task_lun;
	void 			*fabric_tmr_ptr;
	struct se_cmd		*task_cmd;
	struct se_cmd		*ref_cmd;
	struct se_device	*tmr_dev;
	struct se_lun		*tmr_lun;
	struct list_head	tmr_list;
};

struct se_cmd {
	
	u8			scsi_status;
	u8			scsi_asc;
	u8			scsi_ascq;
	u8			scsi_sense_reason;
	u16			scsi_sense_length;
	
	int			alua_nonop_delay;
	
	enum dma_data_direction	data_direction;
	
	int			sam_task_attr;
	
	enum transport_state_table t_state;
	
	unsigned		check_release:1;
	unsigned		cmd_wait_set:1;
	unsigned		unknown_data_length:1;
	
	u32			se_cmd_flags;
	u32			se_ordered_id;
	
	u32			data_length;
	
	u32			cmd_spdtl;
	u32			residual_count;
	u32			orig_fe_lun;
	
	u64			pr_res_key;
	
	void			*sense_buffer;
	struct list_head	se_delayed_node;
	struct list_head	se_lun_node;
	struct list_head	se_qf_node;
	struct se_device      *se_dev;
	struct se_dev_entry   *se_deve;
	struct se_lun		*se_lun;
	
	struct se_session	*se_sess;
	struct se_tmr_req	*se_tmr_req;
	struct list_head	se_queue_node;
	struct list_head	se_cmd_list;
	struct completion	cmd_wait_comp;
	struct kref		cmd_kref;
	struct target_core_fabric_ops *se_tfo;
	int (*execute_task)(struct se_task *);
	void (*transport_complete_callback)(struct se_cmd *);

	unsigned char		*t_task_cdb;
	unsigned char		__t_task_cdb[TCM_MAX_COMMAND_SIZE];
	unsigned long long	t_task_lba;
	u32			t_tasks_sg_chained_no;
	atomic_t		t_fe_count;
	atomic_t		t_se_count;
	atomic_t		t_task_cdbs_left;
	atomic_t		t_task_cdbs_ex_left;
	atomic_t		t_task_cdbs_sent;
	unsigned int		transport_state;
#define CMD_T_ABORTED		(1 << 0)
#define CMD_T_ACTIVE		(1 << 1)
#define CMD_T_COMPLETE		(1 << 2)
#define CMD_T_QUEUED		(1 << 3)
#define CMD_T_SENT		(1 << 4)
#define CMD_T_STOP		(1 << 5)
#define CMD_T_FAILED		(1 << 6)
#define CMD_T_LUN_STOP		(1 << 7)
#define CMD_T_LUN_FE_STOP	(1 << 8)
#define CMD_T_DEV_ACTIVE	(1 << 9)
	spinlock_t		t_state_lock;
	struct completion	t_transport_stop_comp;
	struct completion	transport_lun_fe_stop_comp;
	struct completion	transport_lun_stop_comp;
	struct scatterlist	*t_tasks_sg_chained;

	struct work_struct	work;

	struct scatterlist	*t_data_sg;
	unsigned int		t_data_nents;
	void			*t_data_vmap;
	struct scatterlist	*t_bidi_data_sg;
	unsigned int		t_bidi_data_nents;

	
	struct list_head	t_task_list;
	u32			t_task_list_num;

};

struct se_ua {
	u8			ua_asc;
	u8			ua_ascq;
	struct se_node_acl	*ua_nacl;
	struct list_head	ua_dev_list;
	struct list_head	ua_nacl_list;
};

struct se_node_acl {
	char			initiatorname[TRANSPORT_IQN_LEN];
	
	bool			dynamic_node_acl;
	bool			acl_stop:1;
	u32			queue_depth;
	u32			acl_index;
	u64			num_cmds;
	u64			read_bytes;
	u64			write_bytes;
	spinlock_t		stats_lock;
	
	atomic_t		acl_pr_ref_count;
	struct se_dev_entry	**device_list;
	struct se_session	*nacl_sess;
	struct se_portal_group *se_tpg;
	spinlock_t		device_list_lock;
	spinlock_t		nacl_sess_lock;
	struct config_group	acl_group;
	struct config_group	acl_attrib_group;
	struct config_group	acl_auth_group;
	struct config_group	acl_param_group;
	struct config_group	acl_fabric_stat_group;
	struct config_group	*acl_default_groups[5];
	struct list_head	acl_list;
	struct list_head	acl_sess_list;
	struct completion	acl_free_comp;
	struct kref		acl_kref;
};

struct se_session {
	unsigned		sess_tearing_down:1;
	u64			sess_bin_isid;
	struct se_node_acl	*se_node_acl;
	struct se_portal_group *se_tpg;
	void			*fabric_sess_ptr;
	struct list_head	sess_list;
	struct list_head	sess_acl_list;
	struct list_head	sess_cmd_list;
	struct list_head	sess_wait_list;
	spinlock_t		sess_cmd_lock;
	struct kref		sess_kref;
};

struct se_device;
struct se_transform_info;
struct scatterlist;

struct se_ml_stat_grps {
	struct config_group	stat_group;
	struct config_group	scsi_auth_intr_group;
	struct config_group	scsi_att_intr_port_group;
};

struct se_lun_acl {
	char			initiatorname[TRANSPORT_IQN_LEN];
	u32			mapped_lun;
	struct se_node_acl	*se_lun_nacl;
	struct se_lun		*se_lun;
	struct list_head	lacl_list;
	struct config_group	se_lun_group;
	struct se_ml_stat_grps	ml_stat_grps;
};

struct se_dev_entry {
	bool			def_pr_registered;
	
	u32			lun_flags;
	u32			deve_cmds;
	u32			mapped_lun;
	u32			average_bytes;
	u32			last_byte_count;
	u32			total_cmds;
	u32			total_bytes;
	u64			pr_res_key;
	u64			creation_time;
	u32			attach_count;
	u64			read_bytes;
	u64			write_bytes;
	atomic_t		ua_count;
	
	atomic_t		pr_ref_count;
	struct se_lun_acl	*se_lun_acl;
	spinlock_t		ua_lock;
	struct se_lun		*se_lun;
	struct list_head	alua_port_list;
	struct list_head	ua_list;
};

struct se_dev_limits {
	
	u32		hw_queue_depth;
	
	u32		queue_depth;
	
	struct queue_limits limits;
};

struct se_dev_attrib {
	int		emulate_dpo;
	int		emulate_fua_write;
	int		emulate_fua_read;
	int		emulate_write_cache;
	int		emulate_ua_intlck_ctrl;
	int		emulate_tas;
	int		emulate_tpu;
	int		emulate_tpws;
	int		emulate_reservations;
	int		emulate_alua;
	int		enforce_pr_isids;
	int		is_nonrot;
	int		emulate_rest_reord;
	u32		hw_block_size;
	u32		block_size;
	u32		hw_max_sectors;
	u32		max_sectors;
	u32		fabric_max_sectors;
	u32		optimal_sectors;
	u32		hw_queue_depth;
	u32		queue_depth;
	u32		max_unmap_lba_count;
	u32		max_unmap_block_desc_count;
	u32		unmap_granularity;
	u32		unmap_granularity_alignment;
	struct se_subsystem_dev *da_sub_dev;
	struct config_group da_group;
};

struct se_dev_stat_grps {
	struct config_group stat_group;
	struct config_group scsi_dev_group;
	struct config_group scsi_tgt_dev_group;
	struct config_group scsi_lu_group;
};

struct se_subsystem_dev {
#define SE_DEV_ALIAS_LEN		512
	unsigned char	se_dev_alias[SE_DEV_ALIAS_LEN];
#define SE_UDEV_PATH_LEN		512
	unsigned char	se_dev_udev_path[SE_UDEV_PATH_LEN];
	u32		su_dev_flags;
	struct se_hba *se_dev_hba;
	struct se_device *se_dev_ptr;
	struct se_dev_attrib se_dev_attrib;
	
	struct t10_alua	t10_alua;
	
	struct t10_wwn	t10_wwn;
	
	struct t10_reservation t10_pr;
	spinlock_t      se_dev_lock;
	void            *se_dev_su_ptr;
	struct config_group se_dev_group;
	
	struct config_group se_dev_pr_group;
	
	struct se_dev_stat_grps dev_stat_grps;
};

struct se_device {
	
	u16			dev_rpti_counter;
	
	u32			dev_cur_ordered_id;
	u32			dev_flags;
	u32			dev_port_count;
	
	u32			dev_status;
	
	u32			queue_depth;
	
	u64			dev_res_bin_isid;
	t10_task_attr_index_t	dev_task_attr_type;
	
	void 			*dev_ptr;
	u32			dev_index;
	u64			creation_time;
	u32			num_resets;
	u64			num_cmds;
	u64			read_bytes;
	u64			write_bytes;
	spinlock_t		stats_lock;
	
	atomic_t		simple_cmds;
	atomic_t		dev_ordered_id;
	atomic_t		execute_tasks;
	atomic_t		dev_ordered_sync;
	atomic_t		dev_qf_count;
	struct se_obj		dev_obj;
	struct se_obj		dev_access_obj;
	struct se_obj		dev_export_obj;
	struct se_queue_obj	dev_queue_obj;
	spinlock_t		delayed_cmd_lock;
	spinlock_t		execute_task_lock;
	spinlock_t		dev_reservation_lock;
	spinlock_t		dev_status_lock;
	spinlock_t		se_port_lock;
	spinlock_t		se_tmr_lock;
	spinlock_t		qf_cmd_lock;
	
	struct se_node_acl	*dev_reserved_node_acl;
	
	struct t10_alua_lu_gp_member *dev_alua_lu_gp_mem;
	
	struct t10_pr_registration *dev_pr_res_holder;
	struct list_head	dev_sep_list;
	struct list_head	dev_tmr_list;
	
	struct task_struct	*process_thread;
	struct work_struct	qf_work_queue;
	struct list_head	delayed_cmd_list;
	struct list_head	execute_task_list;
	struct list_head	state_task_list;
	struct list_head	qf_cmd_list;
	
	struct se_hba		*se_hba;
	struct se_subsystem_dev *se_sub_dev;
	
	struct se_subsystem_api *transport;
	
	struct list_head	dev_list;
};

struct se_hba {
	u16			hba_tpgt;
	u32			hba_id;
	
	u32			hba_flags;
	
	u32			dev_count;
	u32			hba_index;
	
	void			*hba_ptr;
	
	struct list_head	hba_dev_list;
	struct list_head	hba_node;
	spinlock_t		device_lock;
	struct config_group	hba_group;
	struct mutex		hba_access_mutex;
	struct se_subsystem_api *transport;
};

struct se_port_stat_grps {
	struct config_group stat_group;
	struct config_group scsi_port_group;
	struct config_group scsi_tgt_port_group;
	struct config_group scsi_transport_group;
};

struct se_lun {
	
	enum transport_lun_status_table lun_status;
	u32			lun_access;
	u32			lun_flags;
	u32			unpacked_lun;
	atomic_t		lun_acl_count;
	spinlock_t		lun_acl_lock;
	spinlock_t		lun_cmd_lock;
	spinlock_t		lun_sep_lock;
	struct completion	lun_shutdown_comp;
	struct list_head	lun_cmd_list;
	struct list_head	lun_acl_list;
	struct se_device	*lun_se_dev;
	struct se_port		*lun_sep;
	struct config_group	lun_group;
	struct se_port_stat_grps port_stat_grps;
};

struct scsi_port_stats {
       u64     cmd_pdus;
       u64     tx_data_octets;
       u64     rx_data_octets;
};

struct se_port {
	
	u16		sep_rtpi;
	int		sep_tg_pt_secondary_stat;
	int		sep_tg_pt_secondary_write_md;
	u32		sep_index;
	struct scsi_port_stats sep_stats;
	
	atomic_t	sep_tg_pt_secondary_offline;
	
	atomic_t	sep_tg_pt_ref_cnt;
	spinlock_t	sep_alua_lock;
	struct mutex	sep_tg_pt_md_mutex;
	struct t10_alua_tg_pt_gp_member *sep_alua_tg_pt_gp_mem;
	struct se_lun *sep_lun;
	struct se_portal_group *sep_tpg;
	struct list_head sep_alua_list;
	struct list_head sep_list;
};

struct se_tpg_np {
	struct se_portal_group *tpg_np_parent;
	struct config_group	tpg_np_group;
};

struct se_portal_group {
	
	enum transport_tpg_type_table se_tpg_type;
	
	u32			num_node_acls;
	
	atomic_t		tpg_pr_ref_count;
	
	spinlock_t		acl_node_lock;
	
	spinlock_t		session_lock;
	spinlock_t		tpg_lun_lock;
	
	void			*se_tpg_fabric_ptr;
	struct list_head	se_tpg_node;
	
	struct list_head	acl_node_list;
	struct se_lun		**tpg_lun_list;
	struct se_lun		tpg_virt_lun0;
	
	struct list_head	tpg_sess_list;
	
	struct target_core_fabric_ops *se_tpg_tfo;
	struct se_wwn		*se_tpg_wwn;
	struct config_group	tpg_group;
	struct config_group	*tpg_default_groups[6];
	struct config_group	tpg_lun_group;
	struct config_group	tpg_np_group;
	struct config_group	tpg_acl_group;
	struct config_group	tpg_attrib_group;
	struct config_group	tpg_param_group;
};

struct se_wwn {
	struct target_fabric_configfs *wwn_tf;
	struct config_group	wwn_group;
	struct config_group	*wwn_default_groups[2];
	struct config_group	fabric_stat_group;
};

#endif 

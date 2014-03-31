#define TCM_LOOP_VERSION		"v2.1-rc2"
#define TL_WWN_ADDR_LEN			256
#define TL_TPGS_PER_HBA			32

#define TL_SCSI_MAX_CMD_LEN		32

struct tcm_loop_cmd {
	
	u32 sc_cmd_state;
	
	struct scsi_cmnd *sc;
	
	struct se_cmd tl_se_cmd;
	struct work_struct work;
	
	unsigned char tl_sense_buf[TRANSPORT_SENSE_BUFFER];
};

struct tcm_loop_tmr {
	atomic_t tmr_complete;
	wait_queue_head_t tl_tmr_wait;
};

struct tcm_loop_nexus {
	int it_nexus_active;
	struct scsi_host *sh;
	struct se_session *se_sess;
};

struct tcm_loop_nacl {
	struct se_node_acl se_node_acl;
};

struct tcm_loop_tpg {
	unsigned short tl_tpgt;
	atomic_t tl_tpg_port_count;
	struct se_portal_group tl_se_tpg;
	struct tcm_loop_hba *tl_hba;
};

struct tcm_loop_hba {
	u8 tl_proto_id;
	unsigned char tl_wwn_address[TL_WWN_ADDR_LEN];
	struct se_hba_s *se_hba;
	struct se_lun *tl_hba_lun;
	struct se_port *tl_hba_lun_sep;
	struct se_device_s *se_dev_hba_ptr;
	struct tcm_loop_nexus *tl_nexus;
	struct device dev;
	struct Scsi_Host *sh;
	struct tcm_loop_tpg tl_hba_tpgs[TL_TPGS_PER_HBA];
	struct se_wwn tl_hba_wwn;
};

/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/


#ifndef	_SMT_
#define _SMT_

#define SMT6_10
#define SMT7_20

#define	OPT_PMF		
#define	OPT_SRF		


#define SMT_VID	0x0001			
#define SMT_VID_2 0x0002		

struct smt_sid {
	u_char	sid_oem[2] ;			
	struct fddi_addr sid_node ;		
} ;

typedef u_char	t_station_id[8] ;

_packed struct smt_header {
	struct fddi_addr    	smt_dest ;	
	struct fddi_addr	smt_source ;	
	u_char			smt_class ;	
	u_char			smt_type ;	
	u_short			smt_version ;	
	u_int			smt_tid ;	
	struct smt_sid		smt_sid ;	
	u_short			smt_pad ;	
	u_short			smt_len ;	
} ;
#define SWAP_SMTHEADER	"662sl8ss"

#if	0
#define FC_SMT_INFO	0x41		
#define FC_SMT_NSA	0x4f		
#endif


#define SMT_ANNOUNCE	0x01		
#define SMT_REQUEST	0x02		
#define SMT_REPLY	0x03		

#define SMT_NIF		0x01		
#define SMT_SIF_CONFIG	0x02		
#define SMT_SIF_OPER	0x03		
#define SMT_ECF		0x04		
#define SMT_RAF		0x05		
#define SMT_RDF		0x06		
#define SMT_SRF		0x07		
#define SMT_PMF_GET	0x08		
#define SMT_PMF_SET	0x09		
#define SMT_ESF		0xff		

#define SMT_MAX_ECHO_LEN	4458	
#if	defined(CONC) || defined(CONC_II)
#define SMT_TEST_ECHO_LEN	50	
#else
#define SMT_TEST_ECHO_LEN	SMT_MAX_ECHO_LEN	
#endif

#define SMT_MAX_INFO_LEN	(4352-20)	



struct smt_para {
	u_short	p_type ;		
	u_short	p_len ;			
} ;

#define PARA_LEN	(sizeof(struct smt_para))

#define SMTSETPARA(p,t)		(p)->para.p_type = (t),\
				(p)->para.p_len = sizeof(*(p)) - PARA_LEN

#define SMT_P_UNA	0x0001		
#define SWAP_SMT_P_UNA	"s6"

struct smt_p_una {
	struct smt_para	para ;		
	u_short	una_pad ;
	struct fddi_addr una_node ;	
} ;

#define SMT_P_SDE	0x0002		
#define SWAP_SMT_P_SDE	"1111"

#define SMT_SDE_STATION		0	
#define SMT_SDE_CONCENTRATOR	1	

struct smt_p_sde {
	struct smt_para	para ;		
	u_char	sde_type ;		
	u_char	sde_mac_count ;		
	u_char	sde_non_master ;	
	u_char	sde_master ;		
} ;

#define SMT_P_STATE	0x0003		
#define SWAP_SMT_P_STATE	"scc"

struct smt_p_state {
	struct smt_para	para ;		
	u_short	st_pad ;
	u_char	st_topology ;		
	u_char	st_dupl_addr ;		
} ;
#define SMT_ST_WRAPPED		(1<<0)	
#define SMT_ST_UNATTACHED	(1<<1)	
#define SMT_ST_TWISTED_A	(1<<2)	
#define SMT_ST_TWISTED_B	(1<<3)	
#define SMT_ST_ROOTED_S		(1<<4)	
#define SMT_ST_SRF		(1<<5)	
#define SMT_ST_SYNC_SERVICE	(1<<6)	

#define SMT_ST_MY_DUPA		(1<<0)	
#define SMT_ST_UNA_DUPA		(1<<1)	

#define SMT_P_TIMESTAMP	0x0004		
#define SWAP_SMT_P_TIMESTAMP	"8"
struct smt_p_timestamp {
	struct smt_para	para ;		
	u_char	ts_time[8] ;		
} ;

#define SMT_P_POLICY	0x0005		
#define SWAP_SMT_P_POLICY	"ss"

struct smt_p_policy {
	struct smt_para	para ;		
	u_short	pl_config ;
	u_short pl_connect ;		
} ;
#define SMT_PL_HOLD		1	

#define SMT_P_LATENCY	0x0006		
#define SWAP_SMT_P_LATENCY	"ssss"

struct smt_p_latency {
	struct smt_para	para ;		
	u_short	lt_phyout_idx1 ;	
	u_short	lt_latency1 ;		
	u_short	lt_phyout_idx2 ;	
	u_short	lt_latency2 ;		
} ;

#define SMT_P_NEIGHBORS	0x0007		
#define SWAP_SMT_P_NEIGHBORS	"ss66"

struct smt_p_neighbor {
	struct smt_para	para ;		
	u_short	nb_mib_index ;		
	u_short	nb_mac_index ;		
	struct fddi_addr nb_una ;	
	struct fddi_addr nb_dna ;	
} ;

#define SMT_PHY_A	0		
#define SMT_PHY_B	1		
#define SMT_PHY_S	2		
#define SMT_PHY_M	3		

#define SMT_CS_DISABLED	0		
#define SMT_CS_CONNECTING	1	
#define SMT_CS_STANDBY	2		
#define SMT_CS_ACTIVE	3		

#define SMT_RM_NONE	0
#define SMT_RM_MAC	1

struct smt_phy_rec {
	u_short	phy_mib_index ;		
	u_char	phy_type ;		
	u_char	phy_connect_state ;	
	u_char	phy_remote_type ;	
	u_char	phy_remote_mac ;	
	u_short	phy_resource_idx ;	
} ;

struct smt_mac_rec {
	struct fddi_addr mac_addr ;		
	u_short		mac_resource_idx ;	
} ;

#define SMT_P_PATH	0x0008			
#define SWAP_SMT_P_PATH	"[6s]"

struct smt_p_path {
	struct smt_para	para ;		
	struct smt_phy_rec	pd_phy[2] ;	
	struct smt_mac_rec	pd_mac ;	
} ;

#define SMT_P_MAC_STATUS	0x0009		
#define SWAP_SMT_P_MAC_STATUS	"sslllllllll"

struct smt_p_mac_status {
	struct smt_para	para ;		
	u_short st_mib_index ;		
	u_short	st_mac_index ;		
	u_int	st_t_req ;		
	u_int	st_t_neg ;		
	u_int	st_t_max ;		
	u_int	st_tvx_value ;		
	u_int	st_t_min ;		
	u_int	st_sba ;		
	u_int	st_frame_ct ;		
	u_int	st_error_ct ;		
	u_int	st_lost_ct ;		
} ;

#define SMT_P_LEM	0x000a		
#define SWAP_SMT_P_LEM	"ssccccll"
struct smt_p_lem {
	struct smt_para	para ;		
	u_short	lem_mib_index ;		
	u_short	lem_phy_index ;		
	u_char	lem_pad2 ;		
	u_char	lem_cutoff ;		
	u_char	lem_alarm ;		
	u_char	lem_estimate ;		
	u_int	lem_reject_ct ;		
	u_int	lem_ct ;		
} ;

#define SMT_P_MAC_COUNTER 0x000b	
#define SWAP_SMT_P_MAC_COUNTER	"ssll"

struct smt_p_mac_counter {
	struct smt_para	para ;		
	u_short	mc_mib_index ;		
	u_short	mc_index ;		
	u_int	mc_receive_ct ;		
	u_int	mc_transmit_ct ;	
} ;

#define SMT_P_MAC_FNC	0x000c		
#define SWAP_SMT_P_MAC_FNC	"ssl"

struct smt_p_mac_fnc {
	struct smt_para	para ;		
	u_short	nc_mib_index ;		
	u_short	nc_index ;		
	u_int	nc_counter ;		
} ;


#define SMT_P_PRIORITY	0x000d		
#define SWAP_SMT_P_PRIORITY	"ssl"

struct smt_p_priority {
	struct smt_para	para ;		
	u_short	pr_mib_index ;		
	u_short	pr_index ;		
	u_int	pr_priority[7] ;	
} ;

#define SMT_P_EB	0x000e		
#define SWAP_SMT_P_EB	"ssl"

struct smt_p_eb {
	struct smt_para	para ;		
	u_short	eb_mib_index ;		
	u_short	eb_index ;		
	u_int	eb_error_ct ;		
} ;

#define SMT_P_MANUFACTURER	0x000f	
#define SWAP_SMT_P_MANUFACTURER	""

struct smp_p_manufacturer {
	struct smt_para	para ;		
	u_char mf_data[32] ;		
} ;

#define SMT_P_USER		0x0010	
#define SWAP_SMT_P_USER	""

struct smp_p_user {
	struct smt_para	para ;		
	u_char us_data[32] ;		
} ;



#define SMT_P_ECHODATA	0x0011		
#define SWAP_SMT_P_ECHODATA	""

struct smt_p_echo {
	struct smt_para	para ;		
	u_char	ec_data[SMT_MAX_ECHO_LEN-4] ;	
} ;

#define SMT_P_REASON	0x0012		
#define SWAP_SMT_P_REASON	"l"

struct smt_p_reason {
	struct smt_para	para ;		
	u_int	rdf_reason ;		
} ;
#define SMT_RDF_CLASS	0x00000001	
#define SMT_RDF_VERSION	0x00000002	
#define SMT_RDF_SUCCESS	0x00000003	
#define SMT_RDF_BADSET	0x00000004	
#define SMT_RDF_ILLEGAL 0x00000005	
#define SMT_RDF_NOPARAM	0x6		
#define SMT_RDF_RANGE	0x8		
#define SMT_RDF_AUTHOR	0x9		
#define SMT_RDF_LENGTH	0x0a		
#define SMT_RDF_TOOLONG	0x0b		
#define SMT_RDF_SBA	0x0d		

#define SMT_P_REFUSED	0x0013		
#define SWAP_SMT_P_REFUSED	"l"

struct smt_p_refused {
	struct smt_para	para ;		
	u_int	ref_fc ;		
	struct smt_header	ref_header ;	
} ;

#define SMT_P_VERSION	0x0014		
#define SWAP_SMT_P_VERSION	"sccss"

struct smt_p_version {
	struct smt_para	para ;		
	u_short	v_pad ;
	u_char	v_n ;			
	u_char	v_index ;		
	u_short	v_version[1] ;		
	u_short	v_pad2 ;		
} ;

#define	SWAP_SMT_P0015		"l"

struct smt_p_0015 {
	struct smt_para	para ;		
	u_int		res_type ;	
} ;

#define	SYNC_BW		0x00000001L	

#define	SWAP_SMT_P0016		"l"

struct smt_p_0016 {
	struct smt_para	para ;		
	u_int		sba_cmd ;	
} ;

#define	REQUEST_ALLOCATION	0x1	
#define	REPORT_ALLOCATION	0x2	
#define	CHANGE_ALLOCATION	0x3	
					
					

#define	SWAP_SMT_P0017		"l"

struct smt_p_0017 {
	struct smt_para	para ;		
	int		sba_pl_req ;	
} ;					

#define	SWAP_SMT_P0018		"l"

struct smt_p_0018 {
	struct smt_para	para ;		
	int		sba_ov_req ;	
} ;					

#define	SWAP_SMT_P0019		"s6"

struct smt_p_0019 {
	struct smt_para	para ;		
	u_short		sba_pad ;
	struct fddi_addr alloc_addr ;	
} ;

#define	SWAP_SMT_P001A		"l"

struct smt_p_001a {
	struct smt_para	para ;		
	u_int		category ;	
} ;

#define	SWAP_SMT_P001B		"l"

struct smt_p_001b {
	struct smt_para	para ;		
	u_int		max_t_neg ;	
} ;

#define	SWAP_SMT_P001C		"l"

struct smt_p_001c {
	struct smt_para	para ;		
	u_int		min_seg_siz ;	
} ;

#define	SWAP_SMT_P001D		"l"

struct smt_p_001d {
	struct smt_para	para ;		
	u_int		allocatable ;	
} ;

#define SMT_P_FSC	0x200b

struct smt_p_fsc {
	struct smt_para	para ;		
	u_short	fsc_pad0 ;
	u_short	fsc_mac_index ;		
	u_short	fsc_pad1 ;
	u_short	fsc_value ;		
} ;

#define FSC_TYPE0	0		
#define FSC_TYPE1	1		
#define FSC_TYPE2	2		

#define SMT_P_AUTHOR	0x0021

#define SWAP_SMT_P1048	"ll"
struct smt_p_1048 {
	u_int p1048_flag ;
	u_int p1048_cf_state ;
} ;

#define SWAP_SMT_P208C	"4lss66"
struct smt_p_208c {
	u_int			p208c_flag ;
	u_short			p208c_pad ;
	u_short			p208c_dupcondition ;
	struct	fddi_addr	p208c_fddilong ;
	struct	fddi_addr	p208c_fddiunalong ;
} ;

#define SWAP_SMT_P208D	"4lllll"
struct smt_p_208d {
	u_int			p208d_flag ;
	u_int			p208d_frame_ct ;
	u_int			p208d_error_ct ;
	u_int			p208d_lost_ct ;
	u_int			p208d_ratio ;
} ;

#define SWAP_SMT_P208E	"4llll"
struct smt_p_208e {
	u_int			p208e_flag ;
	u_int			p208e_not_copied ;
	u_int			p208e_copied ;
	u_int			p208e_not_copied_ratio ;
} ;

#define SWAP_SMT_P208F	"4ll6666s6"

struct smt_p_208f {
	u_int			p208f_multiple ;
	u_int			p208f_nacondition ;
	struct fddi_addr	p208f_old_una ;
	struct fddi_addr	p208f_new_una ;
	struct fddi_addr	p208f_old_dna ;
	struct fddi_addr	p208f_new_dna ;
	u_short			p208f_curren_path ;
	struct fddi_addr	p208f_smt_address ;
} ;

#define SWAP_SMT_P2090	"4lssl"

struct smt_p_2090 {
	u_int			p2090_multiple ;
	u_short			p2090_availablepaths ;
	u_short			p2090_currentpath ;
	u_int			p2090_requestedpaths ;
} ;

#ifdef	LITTLE_ENDIAN
#define SBAPATHINDEX	(0x01000000L)
#else
#define SBAPATHINDEX	(0x01L)
#endif

#define	SWAP_SMT_P320B	"42s"

struct	smt_p_320b {
	struct smt_para para ;	
	u_int	mib_index ;
	u_short path_pad ;
	u_short	path_index ;
} ;

#define	SWAP_SMT_P320F	"4l"

struct	smt_p_320f {
	struct smt_para para ;	
	u_int	mib_index ;
	u_int	mib_payload ;
} ;

#define	SWAP_SMT_P3210	"4l"

struct	smt_p_3210 {
	struct smt_para para ;	
	u_int	mib_index ;
	u_int	mib_overhead ;
} ;

#define SWAP_SMT_P4050	"4l1111ll"

struct smt_p_4050 {
	u_int			p4050_flag ;
	u_char			p4050_pad ;
	u_char			p4050_cutoff ;
	u_char			p4050_alarm ;
	u_char			p4050_estimate ;
	u_int			p4050_reject_ct ;
	u_int			p4050_ct ;
} ;

#define SWAP_SMT_P4051	"4lssss"
struct smt_p_4051 {
	u_int			p4051_multiple ;
	u_short			p4051_porttype ;
	u_short			p4051_connectstate ;
	u_short			p4051_pc_neighbor ;
	u_short			p4051_pc_withhold ;
} ;

#define SWAP_SMT_P4052	"4ll"
struct smt_p_4052 {
	u_int			p4052_flag ;
	u_int			p4052_eberrorcount ;
} ;

#define SWAP_SMT_P4053	"4lsslss"

struct smt_p_4053 {
	u_int			p4053_multiple ;
	u_short			p4053_availablepaths ;
	u_short			p4053_currentpath ;
	u_int			p4053_requestedpaths ;
	u_short			p4053_mytype ;
	u_short			p4053_neighbortype ;
} ;


#define SMT_P_SETCOUNT	0x1035
#define SWAP_SMT_P_SETCOUNT	"l8"

struct smt_p_setcount {
	struct smt_para	para ;		
	u_int		count ;
	u_char		timestamp[8] ;
} ;


struct smt_nif {
	struct smt_header	smt ;		
	struct smt_p_una	una ;		
	struct smt_p_sde	sde ;		
	struct smt_p_state	state ;		
#ifdef	SMT6_10
	struct smt_p_fsc	fsc ;		
#endif
} ;

struct smt_sif_config {
	struct smt_header	smt ;		
	struct smt_p_timestamp	ts ;		
	struct smt_p_sde	sde ;		
	struct smt_p_version	version ;	
	struct smt_p_state	state ;		
	struct smt_p_policy	policy ;	
	struct smt_p_latency	latency ;	
	struct smt_p_neighbor	neighbor ;	
#ifdef	OPT_PMF
	struct smt_p_setcount	setcount ;	 
#endif
	
	struct smt_p_path	path ;		
} ;
#define SIZEOF_SMT_SIF_CONFIG	(sizeof(struct smt_sif_config)- \
				 sizeof(struct smt_p_path))

struct smt_sif_operation {
	struct smt_header	smt ;		
	struct smt_p_timestamp	ts ;		
	struct smt_p_mac_status	status ;	
	struct smt_p_mac_counter mc ;		
	struct smt_p_mac_fnc 	fnc ;		
	struct smp_p_manufacturer man ;		
	struct smp_p_user	user ;		
#ifdef	OPT_PMF
	struct smt_p_setcount	setcount ;	 
#endif
	
	struct smt_p_lem	lem[1] ;	
} ;
#define SIZEOF_SMT_SIF_OPERATION	(sizeof(struct smt_sif_operation)- \
					 sizeof(struct smt_p_lem))

struct smt_ecf {
	struct smt_header	smt ;		
	struct smt_p_echo	ec_echo ;	
} ;
#define SMT_ECF_LEN	(sizeof(struct smt_header)+sizeof(struct smt_para))

struct smt_rdf {
	struct smt_header	smt ;		
	struct smt_p_reason	reason ;	
	struct smt_p_version	version ;	
	struct smt_p_refused	refused ;	
} ;

struct smt_sba_alc_res {
	struct smt_header	smt ;		
	struct smt_p_0015	s_type ;	
	struct smt_p_0016	cmd ;		
	struct smt_p_reason	reason ;	
	struct smt_p_320b	path ;		
	struct smt_p_320f	payload ;	
	struct smt_p_3210	overhead ;	
	struct smt_p_0019	a_addr ;	
	struct smt_p_001a	cat ;		
	struct smt_p_001d	alloc ;		
} ;

struct smt_sba_alc_req {
	struct smt_header	smt ;		
	struct smt_p_0015	s_type ;	
	struct smt_p_0016	cmd ;		
	struct smt_p_320b	path ;		
	struct smt_p_0017	pl_req ;	
	struct smt_p_0018	ov_req ;	
	struct smt_p_320f	payload ;	
	struct smt_p_3210	overhead ;	
	struct smt_p_0019	a_addr ;	
	struct smt_p_001a	cat ;		
	struct smt_p_001b	tneg ;		
	struct smt_p_001c	segm ;		
} ;

struct smt_sba_chg {
	struct smt_header	smt ;		
	struct smt_p_0015	s_type ;	
	struct smt_p_0016	cmd ;		
	struct smt_p_320b	path ;		
	struct smt_p_320f	payload ;	
	struct smt_p_3210	overhead ;	
	struct smt_p_001a	cat ;		
} ;

struct smt_sba_rep_req {
	struct smt_header	smt ;		
	struct smt_p_0015	s_type ;	
	struct smt_p_0016	cmd ;		
} ;

struct smt_sba_rep_res {
	struct smt_header	smt ;		
	struct smt_p_0015	s_type ;	
	struct smt_p_0016	cmd ;		
	struct smt_p_320b	path ;		
	struct smt_p_320f	payload ;	
	struct smt_p_3210	overhead ;	
} ;

#define SMT_STATION_ACTION	1
#define SMT_STATION_ACTION_CONNECT	0
#define SMT_STATION_ACTION_DISCONNECT	1
#define SMT_STATION_ACTION_PATHTEST	2
#define SMT_STATION_ACTION_SELFTEST	3
#define SMT_STATION_ACTION_DISABLE_A	4
#define SMT_STATION_ACTION_DISABLE_B	5
#define SMT_STATION_ACTION_DISABLE_M	6

#define SMT_PORT_ACTION		2
#define SMT_PORT_ACTION_MAINT	0
#define SMT_PORT_ACTION_ENABLE	1
#define SMT_PORT_ACTION_DISABLE	2
#define SMT_PORT_ACTION_START	3
#define SMT_PORT_ACTION_STOP	4

#endif	

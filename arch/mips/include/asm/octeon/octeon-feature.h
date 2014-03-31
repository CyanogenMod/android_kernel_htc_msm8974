/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2008 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/


#ifndef __OCTEON_FEATURE_H__
#define __OCTEON_FEATURE_H__
#include <asm/octeon/cvmx-mio-defs.h>
#include <asm/octeon/cvmx-rnm-defs.h>

enum octeon_feature {
        
	OCTEON_FEATURE_PKND,
	
	OCTEON_FEATURE_CN68XX_WQE,
	OCTEON_FEATURE_SAAD,
	
	OCTEON_FEATURE_ZIP,
	
	OCTEON_FEATURE_CRYPTO,
	OCTEON_FEATURE_DORM_CRYPTO,
	
	OCTEON_FEATURE_PCIE,
        
	OCTEON_FEATURE_SRIO,
	
	OCTEON_FEATURE_ILK,
	OCTEON_FEATURE_KEY_MEMORY,
	
	OCTEON_FEATURE_LED_CONTROLLER,
	
	OCTEON_FEATURE_TRA,
	
	OCTEON_FEATURE_MGMT_PORT,
	
	OCTEON_FEATURE_RAID,
	
	OCTEON_FEATURE_USB,
	
	OCTEON_FEATURE_NO_WPTR,
	
	OCTEON_FEATURE_DFA,
	OCTEON_FEATURE_MDIO_CLAUSE_45,
	OCTEON_FEATURE_NPEI,
	OCTEON_FEATURE_HFA,
	OCTEON_FEATURE_DFM,
	OCTEON_FEATURE_CIU2,
	OCTEON_MAX_FEATURE
};

static inline int cvmx_fuse_read(int fuse);

static inline int octeon_has_feature(enum octeon_feature feature)
{
	switch (feature) {
	case OCTEON_FEATURE_SAAD:
		return !OCTEON_IS_MODEL(OCTEON_CN3XXX);

	case OCTEON_FEATURE_ZIP:
		if (OCTEON_IS_MODEL(OCTEON_CN30XX)
		    || OCTEON_IS_MODEL(OCTEON_CN50XX)
		    || OCTEON_IS_MODEL(OCTEON_CN52XX))
			return 0;
		else if (OCTEON_IS_MODEL(OCTEON_CN38XX_PASS1))
			return 1;
		else
			return !cvmx_fuse_read(121);

	case OCTEON_FEATURE_CRYPTO:
		if (OCTEON_IS_MODEL(OCTEON_CN6XXX)) {
			union cvmx_mio_fus_dat2 fus_2;
			fus_2.u64 = cvmx_read_csr(CVMX_MIO_FUS_DAT2);
			if (fus_2.s.nocrypto || fus_2.s.nomul) {
				return 0;
			} else if (!fus_2.s.dorm_crypto) {
				return 1;
			} else {
				union cvmx_rnm_ctl_status st;
				st.u64 = cvmx_read_csr(CVMX_RNM_CTL_STATUS);
				return st.s.eer_val;
			}
		} else {
			return !cvmx_fuse_read(90);
		}

	case OCTEON_FEATURE_DORM_CRYPTO:
		if (OCTEON_IS_MODEL(OCTEON_CN6XXX)) {
			union cvmx_mio_fus_dat2 fus_2;
			fus_2.u64 = cvmx_read_csr(CVMX_MIO_FUS_DAT2);
			return !fus_2.s.nocrypto && !fus_2.s.nomul && fus_2.s.dorm_crypto;
		} else {
			return 0;
		}

	case OCTEON_FEATURE_PCIE:
		return OCTEON_IS_MODEL(OCTEON_CN56XX)
			|| OCTEON_IS_MODEL(OCTEON_CN52XX)
			|| OCTEON_IS_MODEL(OCTEON_CN6XXX);

	case OCTEON_FEATURE_SRIO:
		return OCTEON_IS_MODEL(OCTEON_CN63XX)
			|| OCTEON_IS_MODEL(OCTEON_CN66XX);

	case OCTEON_FEATURE_ILK:
		return (OCTEON_IS_MODEL(OCTEON_CN68XX));

	case OCTEON_FEATURE_KEY_MEMORY:
		return OCTEON_IS_MODEL(OCTEON_CN38XX)
			|| OCTEON_IS_MODEL(OCTEON_CN58XX)
			|| OCTEON_IS_MODEL(OCTEON_CN56XX)
			|| OCTEON_IS_MODEL(OCTEON_CN6XXX);

	case OCTEON_FEATURE_LED_CONTROLLER:
		return OCTEON_IS_MODEL(OCTEON_CN38XX)
			|| OCTEON_IS_MODEL(OCTEON_CN58XX)
			|| OCTEON_IS_MODEL(OCTEON_CN56XX);

	case OCTEON_FEATURE_TRA:
		return !(OCTEON_IS_MODEL(OCTEON_CN30XX)
			 || OCTEON_IS_MODEL(OCTEON_CN50XX));
	case OCTEON_FEATURE_MGMT_PORT:
		return OCTEON_IS_MODEL(OCTEON_CN56XX)
			|| OCTEON_IS_MODEL(OCTEON_CN52XX)
			|| OCTEON_IS_MODEL(OCTEON_CN6XXX);

	case OCTEON_FEATURE_RAID:
		return OCTEON_IS_MODEL(OCTEON_CN56XX)
			|| OCTEON_IS_MODEL(OCTEON_CN52XX)
			|| OCTEON_IS_MODEL(OCTEON_CN6XXX);

	case OCTEON_FEATURE_USB:
		return !(OCTEON_IS_MODEL(OCTEON_CN38XX)
			 || OCTEON_IS_MODEL(OCTEON_CN58XX));

	case OCTEON_FEATURE_NO_WPTR:
		return (OCTEON_IS_MODEL(OCTEON_CN56XX)
			|| OCTEON_IS_MODEL(OCTEON_CN52XX)
			|| OCTEON_IS_MODEL(OCTEON_CN6XXX))
			  && !OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X)
			  && !OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X);

	case OCTEON_FEATURE_DFA:
		if (!OCTEON_IS_MODEL(OCTEON_CN38XX)
		    && !OCTEON_IS_MODEL(OCTEON_CN31XX)
		    && !OCTEON_IS_MODEL(OCTEON_CN58XX))
			return 0;
		else if (OCTEON_IS_MODEL(OCTEON_CN3020))
			return 0;
		else
			return !cvmx_fuse_read(120);

	case OCTEON_FEATURE_HFA:
		if (!OCTEON_IS_MODEL(OCTEON_CN6XXX))
			return 0;
		else
			return !cvmx_fuse_read(90);

	case OCTEON_FEATURE_DFM:
		if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)
		      || OCTEON_IS_MODEL(OCTEON_CN66XX)))
			return 0;
		else
			return !cvmx_fuse_read(90);

	case OCTEON_FEATURE_MDIO_CLAUSE_45:
		return !(OCTEON_IS_MODEL(OCTEON_CN3XXX)
			 || OCTEON_IS_MODEL(OCTEON_CN58XX)
			 || OCTEON_IS_MODEL(OCTEON_CN50XX));

	case OCTEON_FEATURE_NPEI:
		return OCTEON_IS_MODEL(OCTEON_CN56XX)
			|| OCTEON_IS_MODEL(OCTEON_CN52XX);

	case OCTEON_FEATURE_PKND:
		return OCTEON_IS_MODEL(OCTEON_CN68XX);

	case OCTEON_FEATURE_CN68XX_WQE:
		return OCTEON_IS_MODEL(OCTEON_CN68XX);

	case OCTEON_FEATURE_CIU2:
		return OCTEON_IS_MODEL(OCTEON_CN68XX);

	default:
		break;
	}
	return 0;
}

#endif 

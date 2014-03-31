#ifndef _SCSI_SCSI_SCAN_H
#define _SCSI_SCSI_SCAN_H

#ifdef CONFIG_SCSI
extern int scsi_complete_async_scans(void);
#else
static inline int scsi_complete_async_scans(void) { return 0; }
#endif

#endif 

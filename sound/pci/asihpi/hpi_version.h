
#ifndef _HPI_VERSION_H
#define _HPI_VERSION_H

#define HPI_VER HPI_VERSION_CONSTRUCTOR(4, 10, 1)

#define HPI_VER_STRING "4.10.01"

#define HPI_LIB_VER  HPI_VERSION_CONSTRUCTOR(10, 2, 0)

#define HPI_VERSION_CONSTRUCTOR(maj, min, r) ((maj << 16) + (min << 8) + r)

#define HPI_VER_MAJOR(v) ((int)(v >> 16))
#define HPI_VER_MINOR(v) ((int)((v >> 8) & 0xFF))
#define HPI_VER_RELEASE(v) ((int)(v & 0xFF))

#endif

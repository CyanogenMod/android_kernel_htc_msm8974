/*
 *  Copyright (c) 2000-2007 LSI Corporation.
 *
 *
 *           Name:  mpi2_type.h
 *          Title:  MPI basic type definitions
 *  Creation Date:  August 16, 2006
 *
 *    mpi2_type.h Version:  02.00.00
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  04-30-07  02.00.00  Corresponds to Fusion-MPT MPI Specification Rev A.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI2_TYPE_H
#define MPI2_TYPE_H


#ifndef MPI2_POINTER
#define MPI2_POINTER     *
#endif

#ifndef MPI_TYPE_H

typedef u8 U8;
typedef __le16 U16;
typedef __le32 U32;
typedef __le64 U64 __attribute__((aligned(4)));


typedef U8      *PU8;
typedef U16     *PU16;
typedef U32     *PU32;
typedef U64     *PU64;

#endif

#endif


#ifndef INC_PARAM_H
#define INC_PARAM_H

#include "config.h"
#define NPROC        64  // maximum number of processes
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG        8  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1500  // size of file system in blocks
#define MAXPATH      128   // maximum file path name

#ifndef INTERVAL
#define INTERVAL     1000000UL  // QEMU: 1ppps = 100MHz
#define US_INTERVAL  10UL
#endif

#endif

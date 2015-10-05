#define NPROC        64  // maximum number of processes
#define NPRIORITIES   5  // number of total possible priorities
#define LOWEST_PRIORITY     NPRIORITIES-1  // Lowest priority but highest index (corresponds to 2)
#define HIGHEST_PRIORITY    0 // Highest priority  but lowest index (corresponds to -2)
#define DEFAULT_PRIORITY    NPRIORITIES/2 // Default priority (corresponds to 0)
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks

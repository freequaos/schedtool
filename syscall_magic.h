/*
 this sticks around for documentation issues only and will be removed in the next
 release - it documents the direct syscalls for affinity, without going thru glibc
 */

#if 0

/* 
 I don't know where this was exactly taken from, but I think I found it
 in glibc.
 */
#include <linux/unistd.h>
#define __NR_sched_setaffinity	241
#define __NR_sched_getaffinity	242

/*
 a nice macro to define the following:
 it's a syscall with 3 args,
 it returns int,
 it's named sched_....,
 the next arg is of type pid_t,
 has the local name pid,
 next is unsigned int,
 with name len,
 then an unsigned long *,
 named user_mask_ptr
 */
_syscall3 (int, sched_setaffinity, pid_t, pid, unsigned int, len, unsigned long *, user_mask_ptr)
_syscall3 (int, sched_getaffinity, pid_t, pid, unsigned int, len, unsigned long *, user_mask_ptr)

#endif

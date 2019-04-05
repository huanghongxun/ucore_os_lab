#ifndef __KERN_DEBUG_MONITOR_H__
#define __KERN_DEBUG_MONITOR_H__

#include <trap.h>

void kmonitor(struct trapframe *tf);

/* print the information about mon_* functions */
int mon_help(int argc, char **argv, struct trapframe *tf);

/**
 * call print_kerninfo in kern/debug/kdebug.c to
 * print the memory occupancy in kernel.
 */
int mon_kerninfo(int argc, char **argv, struct trapframe *tf);

/**
 * call print_stackframe in kern/debug/kdebug.c to
 * print a backtrace of the stack.
 */
int mon_backtrace(int argc, char **argv, struct trapframe *tf);

#endif /* !__KERN_DEBUG_MONITOR_H__ */


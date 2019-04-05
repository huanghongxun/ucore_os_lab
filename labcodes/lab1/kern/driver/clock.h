#ifndef __KERN_DRIVER_CLOCK_H__
#define __KERN_DRIVER_CLOCK_H__

#include <defs.h>

extern volatile size_t ticks;

/**
 * initialize 8253 clock to interrupt 100 times per second,
 * and then enable IRQ_TIMER.
 */
void clock_init(void);

#endif /* !__KERN_DRIVER_CLOCK_H__ */


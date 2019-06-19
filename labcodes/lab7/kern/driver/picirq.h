#ifndef __KERN_DRIVER_PICIRQ_H__
#define __KERN_DRIVER_PICIRQ_H__

/* initialize the 8259A interrupt controllers */
void pic_init(void);
void pic_enable(unsigned int irq);

#define IRQ_OFFSET      32

#endif /* !__KERN_DRIVER_PICIRQ_H__ */


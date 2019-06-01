#ifndef __KERN_DRIVER_INTR_H__
#define __KERN_DRIVER_INTR_H__

/* 启用中断 */
void intr_enable(void);

/* 禁用中断 */
void intr_disable(void);

#endif /* !__KERN_DRIVER_INTR_H__ */


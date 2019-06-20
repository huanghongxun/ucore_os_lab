#ifndef __KERN_SYNC_SEM_H__
#define __KERN_SYNC_SEM_H__

#include <defs.h>
#include <atomic.h>
#include <wait.h>

/**
 * 信号量结构体
 */
typedef struct {
    // 信号量的值
    int value;

    // 该信号量的等待队列
    wait_queue_t wait_queue;
} semaphore_t;

/**
 * 初始化信号量的值和等待队列
 */
void sem_init(semaphore_t *sem, int value);

/**
 * 给信号量的值 + 1
 */
void up(semaphore_t *sem);

/**
 * 给信号量的值 - 1
 */
void down(semaphore_t *sem);

/**
 * @brief 尝试给信号量的值 - 1
 * @return 操作是否成功，非零值为成功
 */
bool try_down(semaphore_t *sem);

#endif /* !__KERN_SYNC_SEM_H__ */


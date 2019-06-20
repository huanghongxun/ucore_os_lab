#include <defs.h>
#include <wait.h>
#include <atomic.h>
#include <kmalloc.h>
#include <sem.h>
#include <proc.h>
#include <sync.h>
#include <assert.h>

void
sem_init(semaphore_t *sem, int value) {
    sem->value = value;
    wait_queue_init(&(sem->wait_queue));
}

static __noinline void __up(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    // up 操作必须是原子操作，关闭中断以保证函数操作的原子性
    local_intr_save(intr_flag);
    {
        wait_t *wait;
        if ((wait = wait_queue_first(&(sem->wait_queue))) == NULL) {
            // 首先检查是否存在进程在等待这个信号量，如果没有进程持有这个
            // 信号量，则这个信号量是空闲的，可以直接 + 1
            sem->value ++;
        }
        else {
            assert(wait->proc->wait_state == wait_state);
            // 否则唤醒等待队列中的最早的进程，且因为这个进程会消耗掉当前
            // 的这个增量，因此不需要 + 1
            wakeup_wait(&(sem->wait_queue), wait, wait_state, 1);
        }
    }
    local_intr_restore(intr_flag);
}

static __noinline uint32_t __down(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    // down 操作必须是原子操作，关闭中断以保证函数操作的原子性
    local_intr_save(intr_flag);
    // 检查信号量值是否可以直接减（是否还有表示的资源？）
    if (sem->value > 0) {
        // 如果有则直接减即可
        sem->value --;
        local_intr_restore(intr_flag);
        return 0;
    }
    wait_t __wait, *wait = &__wait;
    // 将当前进程标记为 PROC_SLEEPING 并加入等待队列中
    wait_current_set(&(sem->wait_queue), wait, wait_state);
    local_intr_restore(intr_flag);

    // 发起调度交出 CPU 执行权（和 Thread.yield 比较相似）
    // 由于 schedule 执行时会关闭中断，因此我们这里需要先
    // 恢复中断，调度完成恢复执行后再关闭中断。
    schedule();

    // schedule 函数执行结束后 CPU 执行权将回到该进程
    // 对信号量的操作以及等待队列的操作必须是原子操作
    local_intr_save(intr_flag);
    {
        // 当前进程被唤醒是因为此时发生了一次 up 操作，
        // 由于 up 操作中唤醒其他进程时没有为 value + 1，
        // 这里也不需要执行 value - 1
        wait_current_del(&(sem->wait_queue), wait);
    }
    local_intr_restore(intr_flag);

    // 如果唤醒的原因不是因为信号量减操作，则返回唤醒的原因
    if (wait->wakeup_flags != wait_state) {
        return wait->wakeup_flags;
    }
    // 否则我们是因为信号量减操作才被唤醒的，返回 0 表示正常
    return 0;
}

void
up(semaphore_t *sem) {
    __up(sem, WT_KSEM);
}

void
down(semaphore_t *sem) {
    uint32_t flags = __down(sem, WT_KSEM);
    assert(flags == 0);
}

bool
try_down(semaphore_t *sem) {
    bool intr_flag, ret = 0;
    // down 操作必须是原子操作，关闭中断以保证函数操作的原子性
    local_intr_save(intr_flag);
    // 检查值是否可以直接减（是否还有表示的资源？）
    if (sem->value > 0) {
        // 返回减操作成功执行
        sem->value --, ret = 1;
    }
    local_intr_restore(intr_flag);
    return ret;
}


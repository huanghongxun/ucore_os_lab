#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

/**
 * 调度程序，选择一个可以运行的进程抢占 CPU
 */
void schedule(void) {
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    // 调度代码是关键代码，一旦发生中断会导致数据错乱，因此关闭中断
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        // 在进程列表中寻找一个可执行的进程
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE) {
                    break;
                }
            }
        } while (le != last);
        // 如果不存在可执行的进程，则执行系统空闲进程等待下一次调度
        if (next == NULL || next->state != PROC_RUNNABLE) {
            next = idleproc;
        }
        next->runs ++;
        // 切换上下文给该进程
        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}


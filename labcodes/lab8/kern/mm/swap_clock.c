#include <defs.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_clock.h>
#include <list.h>

static int _clock_init_mm(struct mm_struct *mm)
{
    list_init(&mm->free_page_list);
    mm->sm_priv = &mm->free_page_list;
    return 0;
}

static int
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head = (list_entry_t *) mm->sm_priv;
    list_entry_t *entry = &(page->pra_page_link);
 
    assert(entry != NULL && head != NULL);
    list_add_before(head, entry);
    return 0;
}

static int
_clock_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
    struct proc_struct *least = NULL; 
    for (list_entry_t * le = list_next(&proc_list); le != &proc_list; le = list_next(le)) {
        struct proc_struct *proc = le2proc(le, list_link);
        if (!proc->mm || list_empty((list_entry_t *)proc->mm->sm_priv)) continue;
        if (!least || proc->page_fault < least->page_fault)
            least = proc;
    }
    if (least) mm = least->mm;
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    struct Page *victim = NULL;

    for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
        struct Page *page = le2page(le, pra_page_link);
        pte_t *ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);
        // not accessed and not dirty
        if (!(*ptep & PTE_A) && !(*ptep & PTE_D)) {
            victim = page;
            break;
        }
    }
    if (victim) goto found;

    for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
        struct Page *page = le2page(le, pra_page_link);
        pte_t *ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);
        // not accessed and dirty
        if (!(*ptep & PTE_A) && (*ptep & PTE_D)) {
            victim = page;
            break;
        }
        *ptep &= ~PTE_A;
        tlb_invalidate(mm->pgdir, page->pra_vaddr);
    }
    if (victim) goto found;

    for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
        struct Page *page = le2page(le, pra_page_link);
        pte_t *ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);
        // not accessed and not dirty
        if (!(*ptep & PTE_A) && !(*ptep & PTE_D)) {
            victim = page;
            break;
        }
    }
    if (victim) goto found;

    for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
        struct Page *page = le2page(le, pra_page_link);
        pte_t *ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);
        // not accessed and dirty
        if (!(*ptep & PTE_A) && (*ptep & PTE_D)) {
            victim = page;
            break;
        }
        *ptep &= ~PTE_A;
        tlb_invalidate(mm->pgdir, page->pra_vaddr);
    }
    if (victim) goto found;
    assert(victim != NULL);
found:
    list_del(&victim->pra_page_link);
    *ptr_page = victim;
    return 0;
}

static int
_clock_check_swap(void)
{
    return 0;
}


static int
_clock_init(void)
{
    return 0;
}

static int
_clock_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_clock_tick_event(struct mm_struct *mm)
{ return 0; }


struct swap_manager swap_manager_clock =
{
    .name            = "clock swap manager",
    .init            = &_clock_init,
    .init_mm         = &_clock_init_mm,
    .tick_event      = &_clock_tick_event,
    .map_swappable   = &_clock_map_swappable,
    .set_unswappable = &_clock_set_unswappable,
    .swap_out_victim = &_clock_swap_out_victim,
    .check_swap      = &_clock_check_swap,
};

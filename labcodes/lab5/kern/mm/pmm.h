#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <atomic.h>
#include <assert.h>

// pmm_manager is a physical memory management class. A special pmm manager - XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
/**
 * 物理内存管理类，
 */
struct pmm_manager {
    const char *name;                                 // 物理内存页管理器的名字
    void (*init)(void);                               // 初始化内存管理器
    void (*init_memmap)(struct Page *base, size_t n); // 初始化管理空闲内存页的数据结构
    struct Page *(*alloc_pages)(size_t n);            // 分配 n 个物理内存页
    void (*free_pages)(struct Page *base, size_t n);  // 释放 n 个物理内存页
    size_t (*nr_free_pages)(void);                    // 返回当前剩余的空闲页数 
    void (*check)(void);                              // 用于检测分配/释放实现是否正确的辅助函数 
};

// 物理内存管理器
extern const struct pmm_manager *pmm_manager;
extern pde_t *boot_pgdir;
extern uintptr_t boot_cr3;

// 初始化物理内存管理器。 setup a pmm to manage physical memory, build PDT&PT to setup paging mechanism 
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void);

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

/**
 * 根据线性地址以及页表获取页表项
 * @param pgdir 内核页表的地址（内核虚地址）
 * @param la 需要查找所在页的线性地址
 * @param create 是否需要分配新的页给二级页表
 * @return 页表项的内核虚拟地址
 */
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);

// get_page - get related Page struct for linear address la using PDT pgdir
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store);
void page_remove(pde_t *pgdir, uintptr_t la);

/**
 * 建立页面到线性地址的映射.
 * 
 * @param pgdir PDT 的内核虚拟地址
 * @param page 需要映射的页
 * @param la 需要映射的线性地址
 * @param perm 页权限
 * @return 0 表示正常，<0 为错误码
 * @note 该函数会修改页表，因此需要更新 TLB.
 */
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);

/**
 * 修改当前的 TSS 中的 esp0。允许我们从内核调到内核态时使用不同的内核栈。
 */
void load_esp0(uintptr_t esp0);

/**
 * 标记刷新 TLB 项。仅标记刷新当前正在使用的页表项
 */
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm);
void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end);
void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end);

/**
 * @brief 将进程 A 中的用户内存段 [start,end) 复制给进程 B。
 * @param to 目标进程 B 的页目录表
 * @param from 源进程 A 的页目录表
 * @param start 进程 A 的待复制用户内存段起始地址，必须页对齐
 * @param end 进程 A 的待复制用户内存段结束地址，必须页对齐
 * @param share 为真时将共享页面，否则新建页面并复制内存内容。
 * @call_chain copy_mm-->dup_mmap-->copy_range
 */
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share);

void print_pgdir(void);

/**
 * 将内核虚拟地址 (+KERNBASE) 转换为物理地址
 */
#define PADDR(kva) ({                                                   \
            uintptr_t __m_kva = (uintptr_t)(kva);                       \
            if (__m_kva < KERNBASE) {                                   \
                panic("PADDR called with invalid kva %08lx", __m_kva);  \
            }                                                           \
            __m_kva - KERNBASE;                                         \
        })

/**
 * 将物理地址转换为内核虚拟地址 (+KERNBASE)
 */
#define KADDR(pa) ({                                                    \
            uintptr_t __m_pa = (pa);                                    \
            size_t __m_ppn = PPN(__m_pa);                               \
            if (__m_ppn >= npage) {                                     \
                panic("KADDR called with invalid pa %08lx", __m_pa);    \
            }                                                           \
            (void *) (__m_pa + KERNBASE);                               \
        })

// 物理内存页数组（虚拟地址）
extern struct Page *pages;
// 物理内存页数
extern size_t npage;

/**
 * 返回页面的页面号。
 * 由于内核页表是按顺序存放页面的，因此页面下标就是页面号。
 */
static inline ppn_t
page2ppn(struct Page *page) {
    return page - pages;
}

/**
 * 返回页面的物理内存首地址。
 */
static inline uintptr_t
page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}

/**
 * 返回物理地址所在的页面的结构体
 */
static inline struct Page *
pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa)];
}

static inline void *
page2kva(struct Page *page) {
    return KADDR(page2pa(page));
}

static inline struct Page *
kva2page(void *kva) {
    return pa2page(PADDR(kva));
}

static inline struct Page *
pte2page(pte_t pte) {
    if (!(pte & PTE_P)) {
        panic("pte2page called with invalid pte");
    }
    return pa2page(PTE_ADDR(pte));
}

static inline struct Page *
pde2page(pde_t pde) {
    return pa2page(PDE_ADDR(pde));
}

static inline int
page_ref(struct Page *page) {
    return page->ref;
}

static inline void
set_page_ref(struct Page *page, int val) {
    page->ref = val;
}

static inline int
page_ref_inc(struct Page *page) {
    page->ref += 1;
    return page->ref;
}

static inline int
page_ref_dec(struct Page *page) {
    page->ref -= 1;
    return page->ref;
}

extern char bootstack[], bootstacktop[];

#endif /* !__KERN_MM_PMM_H__ */


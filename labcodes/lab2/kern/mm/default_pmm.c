#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* First Fit 算法：分配器维护空闲块列表（free list）。一旦收到了内存分配请求，
 * ff 算法在 free list 中顺序查找第一个足够大的块并分配。如果选中的块大小大于申请
 * 的大小，这个块将会被切分成两部分，剩余部分留在 free_list 中。
 * 
 * 本算法将 Page->property 用于记录连续内存空闲块大小。
 */

/**
 * 存储空闲内存块的链表
 */ 
free_area_t free_area;

// free_list 用于记录空闲物理内存页面（page frame block）。
#define free_list (free_area.free_list)
// 空闲页面总数
#define nr_free (free_area.nr_free)
/**
 * 你可以复用默认的 default_init 函数实现来初始化 free_list，设置 nr_free=0。
 */
static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

/**
 * 将一段连续的物理内存加入到空闲块表中。
 * 将初始化所有页面。
 * 
 * @param base 空闲块的头页面
 * @param n 空闲块的页面数，在页表中连续
 * 
 * 调用链：
 * kern_init -> pmm_init -> page_init -> init_memmap -> pmm_manager -> pmm_manager.init_memmap
 */
static void
default_init_memmap(struct Page *base, size_t n) {
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p)); // 我们在 pmm.c 中将所有页面设置为保留态待分配
        p->flags = p->property = 0; // ClearPageReserved && ClearPageProperty
        set_page_ref(p, 0); // 这些页面都未被使用，因此引用数为 0
    }
    // base 页为该连续空闲页面的头页面
    base->property = n;
    SetPageProperty(base); // 将 base 标记为头页面
    nr_free += n;
    list_add_before(&free_list, &(base->page_link));
}

/**
 * 在空闲页面表中寻找第一个大小足够大(>=n)的块，
 * 修改块大小成剩余部分的大小。
 * @return 分配的内存的首地址，若分配失败返回 NULL
 */
static struct Page *
default_alloc_pages(size_t n) {
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL; // 选中的块
    list_entry_t *le = &free_list;
    // 遍历所有的空闲块，找到第一个至少有 n 个页面的块
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) { // 如果成功选中某个块
        if (page->property > n) {
            // 分裂块
            struct Page *p = page + n;
            p->property = page->property - n;
            // 在原处插入新节点以保证 free_list 是按照页地址顺序存储的
            list_add(&(page->page_link), &(p->page_link));
        }
        list_del(&(page->page_link));
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}

/**
 * 释放 base 开始的连续 n 个页面。
 * 释放时需要检查 free_list 是否存在相邻的块，如果存在则需要合并。
 */
static void
default_free_pages(struct Page *base, size_t n) {
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    list_entry_t *le = list_next(&free_list);
    // 遍历列表，查找是否存在可以合并的块
    // 可以合并的块至多两个，一个在 base 前面，一个在 base 后面
    list_entry_t *entry = &free_list;
    for (; le != &free_list; le = list_next(le)) {
        p = le2page(le, page_link);
        if (base + base->property == p) { // 向后合并
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
        else if (p + p->property == base) { // 向前合并
            p->property += base->property;
            ClearPageProperty(base);
            base = p;
            list_del(&(p->page_link));
        }
        // 寻找插入的位置，我们需要保证 free_list 内元素是按照地址顺序排列的
        else if (base + base->property < p) {
            entry = &p->page_link;
            break;
        }
    }
    nr_free += n;
    list_add_before(entry, &(base->page_link));
}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3); // Now we have 3 free pages
    assert(alloc_pages(4) == NULL); // So we cannot alloc 4 pages
    assert(PageProperty(p0 + 2) && p0[2].property == 3); // p0 + 2 is the head page, property
    assert((p1 = alloc_pages(3)) != NULL); // We can alloc 3 pages
    assert(alloc_page() == NULL); // And we cannot alloc extra one page
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    // Since [p0], [p1], [p1], [p1], [p1]. Two blocks cannot be joint
    // We have two list item in free list.
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    // 这里的测试要求先被释放的页面排在列表前面
    // 因此 free_page 中插入元素时需要注意这点
    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};


#include <defs.h>
#include <x86.h>
#include <stab.h>
#include <stdio.h>
#include <string.h>
#include <kdebug.h>

#define STACKFRAME_DEPTH 20

extern const struct stab __STAB_BEGIN__[];  // beginning of stabs table
extern const struct stab __STAB_END__[];    // end of stabs table
extern const char __STABSTR_BEGIN__[];      // beginning of string table
extern const char __STABSTR_END__[];        // end of string table

/* debug information about a particular instruction pointer */
struct eipdebuginfo {
    const char *eip_file;                   // source code filename for eip
    int eip_line;                           // source code line number for eip
    const char *eip_fn_name;                // name of function containing eip
    int eip_fn_namelen;                     // length of function's name
    uintptr_t eip_fn_addr;                  // start address of function
    int eip_fn_narg;                        // number of function arguments
};

/**
 * 在 [*@region_left, *@region_right] 范围内寻找包含地址 addr 且类型为 type
 * 的 stab entry, 保存边界
 * and then save its boundary to the locations that pointed
 * by @region_left and @region_right.
 *
 * Some stab types are arranged in increasing order by instruction address.
 * For example, N_FUN stabs (stab entries with n_type == N_FUN), which
 * mark functions, and N_SO stabs, which mark source files.
 *
 * Given an instruction address, this function finds the single stab entry
 * of type @type that contains that address.
 *
 * The search takes place within the range [*@region_left, *@region_right].
 * Thus, to search an entire set of N stabs, you might do:
 *
 *      left = 0;
 *      right = N - 1;    (rightmost stab)
 *      stab_binsearch(stabs, &left, &right, type, addr);
 *
 * The search modifies *region_left and *region_right to bracket the @addr.
 * *@region_left points to the matching stab that contains @addr,
 * and *@region_right points just before the next stab.
 * If *@region_left > *region_right, then @addr is not contained in any
 * matching stab.
 *
 * For example, given these N_SO stabs:
 *      Index  Type   Address
 *      0      SO     f0100000
 *      13     SO     f0100040
 *      117    SO     f0100176
 *      118    SO     f0100178
 *      555    SO     f0100652
 *      556    SO     f0100654
 *      657    SO     f0100849
 * this code:
 *      left = 0, right = 657;
 *      stab_binsearch(stabs, &left, &right, N_SO, 0xf0100184);
 * will exit setting left = 118, right = 554.
 * */
static void stab_binsearch(const struct stab *stabs, int *region_left, int *region_right,
           int type, uintptr_t addr) {
    int l = *region_left, r = *region_right, any_matches = 0;

    while (l <= r) {
        int true_m = (l + r) / 2, m = true_m;

        // search for earliest stab with right type
        while (m >= l && stabs[m].n_type != type) {
            m --;
        }
        if (m < l) {    // no match in [l, m]
            l = true_m + 1;
            continue;
        }

        // actual binary search
        any_matches = 1;
        if (stabs[m].n_value < addr) {
            *region_left = m;
            l = true_m + 1;
        } else if (stabs[m].n_value > addr) {
            *region_right = m - 1;
            r = m - 1;
        } else {
            // exact match for 'addr', but continue loop to find
            // *region_right
            *region_left = m;
            l = m;
            addr ++;
        }
    }

    if (!any_matches) {
        *region_right = *region_left - 1;
    }
    else {
        // find rightmost region containing 'addr'
        l = *region_right;
        for (; l > *region_left && stabs[l].n_type != type; l --)
            /* do nothing */;
        *region_left = l;
    }
}

/**
 * 根据指令地址 addr 输出调试信息。
 * @return 0 if information was found, otherwise if not.
 */
int debuginfo_eip(uintptr_t addr, struct eipdebuginfo *info) {
    const struct stab *stabs, *stab_end;
    const char *stabstr, *stabstr_end;

    info->eip_file = "<unknown>";
    info->eip_line = 0;
    info->eip_fn_name = "<unknown>";
    info->eip_fn_namelen = 9;
    info->eip_fn_addr = addr;
    info->eip_fn_narg = 0;

    stabs = __STAB_BEGIN__;
    stab_end = __STAB_END__;
    stabstr = __STABSTR_BEGIN__;
    stabstr_end = __STABSTR_END__;

    // String table validity checks
    if (stabstr_end <= stabstr || stabstr_end[-1] != 0) {
        return -1;
    }

    // Now we find the right stabs that define the function containing
    // 'eip'.  First, we find the basic source file containing 'eip'.
    // Then, we look in that source file for the function.  Then we look
    // for the line number.

    // Search the entire set of stabs for the source file (type N_SO).
    int lfile = 0, rfile = (stab_end - stabs) - 1;
    stab_binsearch(stabs, &lfile, &rfile, N_SO, addr);
    if (lfile == 0)
        return -1;

    // Search within that file's stabs for the function definition
    // (N_FUN).
    int lfun = lfile, rfun = rfile;
    int lline, rline;
    stab_binsearch(stabs, &lfun, &rfun, N_FUN, addr);

    if (lfun <= rfun) {
        // stabs[lfun] points to the function name
        // in the string table, but check bounds just in case.
        if (stabs[lfun].n_strx < stabstr_end - stabstr) {
            info->eip_fn_name = stabstr + stabs[lfun].n_strx;
        }
        info->eip_fn_addr = stabs[lfun].n_value;
        addr -= info->eip_fn_addr;
        // Search within the function definition for the line number.
        lline = lfun;
        rline = rfun;
    } else {
        // Couldn't find function stab!  Maybe we're in an assembly
        // file.  Search the whole file for the line number.
        info->eip_fn_addr = addr;
        lline = lfile;
        rline = rfile;
    }
    info->eip_fn_namelen = strfind(info->eip_fn_name, ':') - info->eip_fn_name;

    // Search within [lline, rline] for the line number stab.
    // If found, set info->eip_line to the right line number.
    // If not found, return -1.
    stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
    if (lline <= rline) {
        info->eip_line = stabs[rline].n_desc;
    } else {
        return -1;
    }

    // Search backwards from the line number for the relevant filename stab.
    // We can't just use the "lfile" stab because inlined functions
    // can interpolate code from a different file!
    // Such included source files use the N_SOL stab type.
    while (lline >= lfile
           && stabs[lline].n_type != N_SOL
           && (stabs[lline].n_type != N_SO || !stabs[lline].n_value)) {
        lline --;
    }
    if (lline >= lfile && stabs[lline].n_strx < stabstr_end - stabstr) {
        info->eip_file = stabstr + stabs[lline].n_strx;
    }

    // Set eip_fn_narg to the number of arguments taken by the function,
    // or 0 if there was no containing function.
    if (lfun < rfun) {
        for (lline = lfun + 1;
             lline < rfun && stabs[lline].n_type == N_PSYM;
             lline ++) {
            info->eip_fn_narg ++;
        }
    }
    return 0;
}

void print_kerninfo(void) {
    extern char etext[], edata[], end[], kern_init[];
    cprintf("Special kernel symbols:\n");
    cprintf("  entry  0x%08x (phys)\n", kern_init);
    cprintf("  etext  0x%08x (phys)\n", etext);
    cprintf("  edata  0x%08x (phys)\n", edata);
    cprintf("  end    0x%08x (phys)\n", end);
    cprintf("Kernel executable memory footprint: %dKB\n", (end - kern_init + 1023)/1024);
}

void print_debuginfo(uintptr_t eip) {
    struct eipdebuginfo info;
    if (debuginfo_eip(eip, &info) != 0) {
        cprintf("    <unknow>: -- 0x%08x --\n", eip);
    } else {
        char fnname[256];
        int j;
        for (j = 0; j < info.eip_fn_namelen; j ++) {
            fnname[j] = info.eip_fn_name[j];
        }
        fnname[j] = '\0';
        cprintf("    %s:%d: %s+%d\n", info.eip_file, info.eip_line,
                fnname, eip - info.eip_fn_addr);
    }
}

/**
 * non-inline function read_eip() is useful, it can read the value of current eip,
 * since while calling this function, read_eip() can read the caller's eip from
 * stack easily.
 */
static __noinline uint32_t read_eip(void) {
    uint32_t eip;
    asm volatile("movl 4(%%ebp), %0" : "=r" (eip));
    return eip;
}

void print_stackframe(void) {
    /**
     * 堆栈指针寄存器 esp 指向栈的栈顶（内存布局中的最低处），比 esp 低的堆栈段都是可用的栈地址。
     * 向堆栈压入元素将导致 esp 寄存器减小，并再次指向栈顶。弹出元素时正好相反。
     * 基址指针寄存器 ebp 是通用寄存器，在 C 语言程序中被用来指向函数内栈的栈底。初次进入 C 语言
     * 函数时，会向栈中压入调用者的 ebp 值，同时将 ebp 置为现在的 esp 值，也就是说，对于一个 C
     * 语言函数，执行时 ebp 指向的地址会保存调用者的 ebp 地址。如果所有的函数都遵循该原则，那么
     * 我们可以利用这个约定追踪程序的调用堆栈。比如如果某个函数抛出了异常，我们可以根据调用堆栈帮
     * 助你进行调试。当然，为了知道我们调用的是什么函数，我们在压入 ebp 的时候，还会压入当前的
     * eip 寄存器的值，也就是当前指令的地址，使我们根据这个地址找到函数定义。
     *
     * 如：我们调用 printf 的步骤是（压入参数时需要：
     * pushl arg2           # arg2, %ebp+4*4
     * pushl arg1           # arg1, %ebp+3*4
     * pushl format_string  # arg0，%ebp+2*4
     * call printf
     *   pushl %eip # 此时的 eip 是该函数的 eip, %ebp+1*4
     *   pushl %ebp # 此时的 ebp 是调用者的 ebp, %ebp+0*4
     *   movl %esp, %ebp # 该函数的栈从当前的 esp 往下增长，此时 ebp 指向刚才压入的 %ebp
     *   ...
     *   popl %ebp # 恢复 %ebp
     *   popl # 弹出 %eip
     *   ret
     * addl $12, %esp
     */

    /* LAB1 YOUR CODE : STEP 1 */
    // (1) call read_ebp() to get the value of ebp. the type is (uint32_t);
    uint32_t ebp = read_ebp();
    // (2) call read_eip() to get the value of eip. the type is (uint32_t);
    uint32_t eip = read_eip();
    // (3) from 0 .. STACKFRAME_DEPTH

    int i, j;
    uint32_t *args;

    /**
     * Note that, the length of ebp-chain is limited. In boot/bootasm.S, before jumping
     * to the kernel entry, the value of ebp has been set to zero, that's the boundary.
     */
    for (i = 0; ebp && i < STACKFRAME_DEPTH; ++i) {
        // (3.1) printf value of ebp, eip
        cprintf("ebp: 0x%08x, eip: 0x%08x, args", ebp, eip);
        // (3.2) (uint32_t)calling arguments [0..4] = the contents in address (uint32_t)ebp +2 [0..4]
        args = (uint32_t *)ebp + 2;
        for (j = 0; j < 4; ++j) cprintf("%c 0x%08x", j == 0 ? ':' : ',', args[j]);
        // (3.3) cprintf("\n");
        cprintf("\n");
        // (3.4) call print_debuginfo(eip-1) to print the C calling function name and line number, etc.
        print_debuginfo(eip - 1);
        // (3.5) popup a calling stackframe
        //        NOTICE: the calling funciton's return addr eip  = ss:[ebp+4]
        eip = *((uint32_t *)ebp + 1); // 堆栈段寄存器值不变
        //                the calling funciton's ebp = ss:[ebp]
        ebp = *((uint32_t *)ebp); // 堆栈段寄存器值不变
        //
    }
}


#include <os/mm.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:

    PTE *pg_base = (PTE *)pgdir;

    uintptr_t kva = allocPage(1);

    uintptr_t pa = kva2pa(kva);

    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;
    
    if (pg_base[vpn2] == 0)
    {
        set_pfn(&pg_base[vpn2], allocPage(1) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pg_base[vpn2], _PAGE_PRESENT | _PAGE_USER);
        clear_pgdir(kva2pa(get_pa(pg_base[vpn2])));
    }
    
    PTE *pmd = (PTE *)pa2kva(get_pa(pg_base[vpn2]));
    if (pmd[vpn1] == 0)
    {
        set_pfn(&pmd[vpn1], allocPage(1) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT | _PAGE_USER);
        clear_pgdir(kva2pa(get_pa(pmd[vpn1])));
    }

    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_USER | _PAGE_DIRTY | _PAGE_ACCESSED
                      | _PAGE_EXEC | _PAGE_WRITE | _PAGE_READ);

    return kva;
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}
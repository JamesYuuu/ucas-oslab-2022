#include <os/mm.h>
#include <os/list.h>
#include <os/sched.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

mm_page_t free_page[PAGE_NUM];

LIST_HEAD(free_mm_list);

mm_page_t* allocPage()
{
    // align PAGE_SIZE
    list_node_t *free_mem_list = free_mm_list.prev;
    list_del(free_mem_list);
    return list_to_mm(free_mem_list);
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

void freePage(list_node_t *mm_list)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    list_node_t *pgdir_list = mm_list->prev;
    list_del(pgdir_list);
    list_add(&free_mm_list,pgdir_list);
    PTE* pgdir = (PTE*)list_to_mm(pgdir_list)->kva;
    while (!list_empty(mm_list))
    {
        // recycle memory
        list_node_t *temp_list = mm_list->prev;
        list_del(temp_list);
        list_add(&free_mm_list, temp_list);
        mm_page_t *temp_mm = list_to_mm(temp_list);
        uintptr_t va = temp_mm->va;
        // set pgdir to invalid
        uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
        uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
        uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;
        del_attribute(&pgdir[vpn2], _PAGE_PRESENT);  
        PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
        del_attribute(&pmd[vpn1], _PAGE_PRESENT);
        PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
        del_attribute(&pte[vpn0], _PAGE_PRESENT);
    }
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    memcpy((void*)dest_pgdir, (void*)src_pgdir, PAGE_SIZE);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, pcb_t *pcb)
{
    // TODO [P4-task1] alloc_page_helper:

    PTE *pg_base = (PTE *)pcb->pgdir;

    mm_page_t* free_page = allocPage();
    free_page->va = va;
    list_add(&pcb->mm_list,&free_page->list);

    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;
    
    if (pg_base[vpn2] == 0)
    {
        mm_page_t* free_page_table = allocPage();
        set_pfn(&pg_base[vpn2], kva2pa(free_page_table->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pg_base[vpn2])));
    }
    set_attribute(&pg_base[vpn2], _PAGE_PRESENT | _PAGE_USER);  
    PTE *pmd = (PTE *)pa2kva(get_pa(pg_base[vpn2]));

    if (pmd[vpn1] == 0)
    {
        mm_page_t* free_page_table = allocPage();
        set_pfn(&pmd[vpn1], kva2pa(free_page_table->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    set_attribute(&pmd[vpn1], _PAGE_PRESENT | _PAGE_USER);
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));

    set_pfn(&pte[vpn0], kva2pa(free_page->kva) >> NORMAL_PAGE_SHIFT);
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_USER | _PAGE_EXEC | _PAGE_WRITE | _PAGE_READ);

    return free_page->kva;
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}

void init_memory()
{
    for (int i=0;i<PAGE_NUM;i++)
    {
        uint64_t addr = FREEMEM_KERNEL + i * NORMAL_PAGE_SIZE;
        free_page[i].kva = addr;
        list_add(&free_mm_list,&free_page[i].list);
    }
}

extern mm_page_t* list_to_mm(list_node_t *list)
{
    return (mm_page_t*)((uint64_t)list - 2 * sizeof(uintptr_t));
}
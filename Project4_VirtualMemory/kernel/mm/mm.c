#include <os/mm.h>
#include <os/list.h>
#include <os/sched.h>
#include <os/task.h>
#include <common.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

mm_page_t free_page[FREE_PAGE_NUM];
mm_page_t free_disk[FREE_DISK_NUM];

LIST_HEAD(free_mm_list);
LIST_HEAD(free_disk_list);

mm_page_t *allocPage()
{
    // align PAGE_SIZE
    list_node_t *free_mem_list;
    // if there is no free page , swap from disk
    if (list_empty(&free_mm_list)) return swap_out();
    // alloc a new page
    free_mem_list = free_mm_list.prev;
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
    list_add(&free_mm_list, pgdir_list);
    PTE *pgdir = (PTE *)list_to_mm(pgdir_list)->kva;
    while (!list_empty(mm_list))
    {
        // recycle memory
        list_node_t *temp_list = mm_list->prev;
        list_del(temp_list);
        mm_page_t *temp_mm = list_to_mm(temp_list);
        if (temp_mm->page_type == PAGE_MEM) list_add(&free_mm_list, temp_list);
            else list_add(&free_disk_list, temp_list);
        uintptr_t va = temp_mm->va;
        // set pgdir to invalid
        uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
        uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
        uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;
        PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
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
    memcpy((void *)dest_pgdir, (void *)src_pgdir, PAGE_SIZE);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, pcb_t *pcb)
{
    // TODO [P4-task1] alloc_page_helper:

    PTE *pg_base = (PTE *)pcb->pgdir;

    mm_page_t *free_page = allocPage();
    free_page->va = va;
    list_add(&pcb->mm_list, &free_page->list);

    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;

    if (pg_base[vpn2] == 0)
    {
        mm_page_t *free_page_table = allocPage();
        set_pfn(&pg_base[vpn2], kva2pa(free_page_table->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pg_base[vpn2])));
    }
    set_attribute(&pg_base[vpn2], _PAGE_PRESENT | _PAGE_USER);
    PTE *pmd = (PTE *)pa2kva(get_pa(pg_base[vpn2]));

    if (pmd[vpn1] == 0)
    {
        mm_page_t *free_page_table = allocPage();
        set_pfn(&pmd[vpn1], kva2pa(free_page_table->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    set_attribute(&pmd[vpn1], _PAGE_PRESENT | _PAGE_USER);
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));

    set_pfn(&pte[vpn0], kva2pa(free_page->kva) >> NORMAL_PAGE_SHIFT);
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_USER | _PAGE_EXEC | _PAGE_WRITE | _PAGE_READ | _PAGE_ACCESSED | _PAGE_DIRTY);

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


mm_page_t * swap_out()
{
    printl("swap out\n");
    int cpu_id = get_current_cpu_id();
    list_node_t *disk_list = free_disk_list.prev;
    list_del(disk_list);
    mm_page_t *free_disk = list_to_mm(disk_list);
    // FIFO and RANDOM Alogrithm
    for (int i = 1;i < NUM_MAX_TASK; i++)
    {
        if (pcb[i].is_used && pcb[i].status != TASK_RUNNING && &pcb[i]!=current_running[cpu_id])
        {
            list_node_t *temp_list = pcb[i].mm_list.prev; // we don't swap pg_dir
            temp_list = temp_list->prev;  // we don't swap stack
            while (temp_list->prev != &pcb[i].mm_list)
            {
                // we make sure we don't swap disk_page
                temp_list = temp_list->prev;
                mm_page_t *temp_mm = list_to_mm(temp_list);
                if (temp_mm->page_type == PAGE_DISK) continue;
                // we decided to swap temp_list->prev
                list_del(temp_list);
                list_add(&pcb[i].mm_list,disk_list);
                // copy va for swap in
                uintptr_t va = temp_mm->va;
                free_disk->va = va;
                sd_write(kva2pa(temp_mm->kva),BLOCK_NUM,free_disk->block_id);
                // set va pg_dir to invalid
                PTE* pg_base = (PTE*)pcb[i].pgdir;
                uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
                uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
                uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;      
                PTE *pmd = (PTE *)pa2kva(get_pa(pg_base[vpn2]));          
                PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
                del_attribute(&pte[vpn0], _PAGE_PRESENT);
                return temp_mm;
            }
        }
    }
    // FIXME: we should think if their is no free_disk_page or no page to swap out
    printl("No page to swap out!\n");
}

void swap_in(mm_page_t *disk_page)
{
    printl("swap in\n");
    int cpu_id = get_current_cpu_id();
    // copy disk back to memory
    list_del(&disk_page->list);
    list_add(&free_disk_list,&disk_page->list);
    uintptr_t va = disk_page->va;
    uintptr_t kva = alloc_page_helper(va,current_running[cpu_id]);
    sd_read(kva2pa(kva),BLOCK_NUM,disk_page->block_id);
}

mm_page_t *list_to_mm(list_node_t *list)
{
    return (mm_page_t *)((uint64_t)list - sizeof(uintptr_t));
}

void init_memory()
{
    for (uint32_t i = 0; i < FREE_PAGE_NUM; i++)
    {
        uint64_t addr = FREEMEM_KERNEL + i * NORMAL_PAGE_SIZE;
        free_page[i].page_type = PAGE_MEM;
        free_page[i].kva = addr;
        list_add(&free_mm_list, &free_page[i].list);
    }
}

void init_disk()
{
    uint32_t final_block_num;
    for (uint32_t i = 0; i < NUM_MAX_TASK; i++)
        if (tasks[i].entry_point==0)
        {
            final_block_num = (tasks[i-1].start_addr + tasks[i-1].filesz) / BLOCK_SIZE + 1;
            break;
        }
    for (uint32_t i = 0; i < FREE_DISK_NUM; i++)
    {
        uint32_t block_id = final_block_num + i * BLOCK_NUM;
        free_disk[i].page_type = PAGE_DISK;
        free_disk[i].block_id = block_id;
        list_add(&free_disk_list, &free_disk[i].list);
    }
}
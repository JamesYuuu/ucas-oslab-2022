#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

static void map_page_net(uint64_t va, uint64_t pa, PTE* pgdir)
{
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;

    if (pgdir[vpn2]==0)
    {
        set_pfn(&pgdir[vpn2], kva2pa(allocPage()->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pgdir[vpn2])));
    }
    set_attribute(&pgdir[vpn2], _PAGE_PRESENT);

    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
    if (pmd[vpn1]==0)
    {
        set_pfn(&pmd[vpn1], kva2pa(allocPage()->kva) >> NORMAL_PAGE_SHIFT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    set_attribute(&pmd[vpn1], _PAGE_PRESENT);

    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);

    return;
}

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    uintptr_t vaddr = io_base;
    while (size > 0)
    {
        map_page_net(io_base, phys_addr, (PTE*)PGDIR_KVA);
        io_base += NORMAL_PAGE_SIZE;
        phys_addr += NORMAL_PAGE_SIZE;
        size -= NORMAL_PAGE_SIZE;
    }
    local_flush_tlb_all();
    return vaddr;
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
    uint64_t va = (uint64_t)io_addr;
    PTE* pgdir = (PTE*)PGDIR_KVA;

    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & VPN_MASK;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & VPN_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN_MASK;

    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    del_attribute(&pte[vpn0], _PAGE_PRESENT);

    local_flush_tlb_all();
}

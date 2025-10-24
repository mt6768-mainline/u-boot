#include <config.h>
#include <asm/global_data.h>
#include <linux/bitops.h>
#include <linux/sizes.h>
#include <linux/libfdt.h>
#include <fdtdec.h>
#include <init.h>
#include <vsprintf.h>
#include <asm/armv8/mmu.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
    int ret = fdtdec_setup_mem_size_base();
    if (ret)
        return ret;

    /* Hardcode 2 GiB of DRAM (safe minimum) to avoid any potential issues */
    gd->ram_size = SZ_2G;
    return 0;
}

int dram_init_banksize(void)
{
    gd->bd->bi_dram[0].start = gd->ram_base;
    gd->bd->bi_dram[0].size = gd->ram_size;
    return 0;
}

int print_cpuinfo(void) 
{
    return 0;
}

int board_early_init_f(void)
{
    /*
     * Find the simplefb node, then put status = "okay" to enable it
     * based on the current config option enabled, since
     * simplefb parameters differ for each device
     */
    void *fdt = (void *)gd->fdt_blob;
    int node;

#ifdef CONFIG_XIAOMI_MERLIN
    node = fdt_path_offset(fdt, "/merlin_fb");
    if (node >= 0) 
        fdt_setprop_string(fdt, node, "status", "okay");
#endif
    // TODO: other devices (more ifdefs)

    return 0;
}

#ifdef CONFIG_XIAOMI_MERLIN
static struct mm_region merlin_mem_map[] = {
    {
        /* DRAM */
        .virt = 0x40000000UL,
        .phys = 0x40000000UL,
        .size = 0x80000000UL,
        .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE,
    }, {
        /* Peripherals */
        .virt = 0x00000000UL,
        .phys = 0x00000000UL,
        .size = 0x40000000UL,
        .attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
        PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN
    }, {
        /* Framebuffer */
        .virt = 0x7d9b0000UL,
        .phys = 0x7d9b0000UL,
        .size = 0x2250000UL,
        .attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) | 
            PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN
    }, {
        /* List terminator */
        0,
    }
};
struct mm_region *mem_map = merlin_mem_map;
#endif
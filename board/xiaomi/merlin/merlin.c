#include <config.h>
#include <init.h>
#include <asm/global_data.h>
#include <asm/armv8/mmu.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
  gd->bd->bi_boot_params = CFG_SYS_SDRAM_BASE + 0x100;
  return 0;
}

static struct mm_region merlin_mem_map[] = {
  {
    /* DDR */
    .virt = 0x40000000UL,
    .phys = 0x40000000UL,
    .size = 0x100000000UL,
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
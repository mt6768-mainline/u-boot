#include <config.h>
#include <dm.h>
#include <init.h>
#include <asm/global_data.h>
#include <asm/armv8/mmu.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
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
    /* Framebuffer */
    .virt = 0x7d9b0000UL,
    .phys = 0x7d9b0000UL,
    .size = 0x2250000UL,
    .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE,
  }, {
    /* List terminator */
  }
};

struct mm_region *mem_map = merlin_mem_map;

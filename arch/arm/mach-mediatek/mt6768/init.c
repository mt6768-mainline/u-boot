#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <fdtdec.h>
#include <init.h>

DECLARE_GLOBAL_DATA_PTR;

/*
#define MTK_WDT_BASE            0x10007000
#define MTK_WDT_MODE            0x00
#define MTK_WDT_MODE_ENABLE     BIT(0)
#define MTK_WDT_MODE_KEY        (0x22 << 24)

static void mtk_wdt_disable(void)
{
  u32 tmp;

  tmp = readl(MTK_WDT_BASE + MTK_WDT_MODE);
  tmp &= ~MTK_WDT_MODE_ENABLE;
  tmp |= MTK_WDT_MODE_KEY;
  writel(tmp, MTK_WDT_BASE + MTK_WDT_MODE);
}
*/

static int lk_fdt_fix_model(void *fdt)
{
  int offset = fdt_path_offset(fdt, "/");
  /* merlin is the only supported device for now */
  fdt_setprop_string(fdt, offset, "model", "Xiaomi Redmi Note 9");

  return 0;
}

static int lk_fdt_clean(void *fdt)
{
  int offset, region;

  /*
   * Remove compatible prop in mblock-* nodes
   * so that U-Boot doesn't try to probe them
   */
  offset = fdt_path_offset(fdt, "/reserved-memory");
  debug("Removing compatible prop in mblock-* nodes\n");
  fdt_for_each_subnode(region, fdt, offset)
    fdt_delprop(fdt, region, "compatible");

  /* Clean up garbage required to pass LK checks from U-Boot's FDT */
  offset = fdt_path_offset(fdt, "/chosen");
  debug("Removing /chosen/bootargs prop\n");
  fdt_delprop(fdt, offset, "bootargs");

  offset = fdt_path_offset(fdt, "/scp@10500000");
  debug("Removing /scp@10500000\n");
  fdt_del_node(fdt, offset);

  offset = fdt_path_offset(fdt, "/dtbo-dummy");
  debug("Removing /dtbo-dummy\n");
  fdt_del_node(fdt, offset);

  offset = fdt_path_offset(fdt, "/__symbols__");
  debug("Removing /__symbols__\n");
  fdt_del_node(fdt, offset);

  offset = fdt_path_offset(fdt, "/firmware");
  debug("Removing /firmware\n");
  fdt_del_node(fdt, offset);

  offset = fdt_path_offset(fdt, "/bootprof");
  debug("Removing /bootprof\n");
  fdt_del_node(fdt, offset);

  offset = fdt_path_offset(fdt, "/md_attr_node");
  debug("Removing /md_attr_node\n");
  fdt_del_node(fdt, offset);

  return 0;
}

int board_fdt_blob_setup(void **fdtp)
{
  /* FDT address is in the x0 register */
  *fdtp = (void*)get_prev_bl_fdt_addr();
  debug("Using FDT by previous bootloader\n");

  debug("Fixing up FDT...\n");
  lk_fdt_clean(*fdtp);
  debug("FDT fixup done!\n\n");

  return 0;
}

int dram_init(void)
{
  fdtdec_setup_mem_size_base();
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
  /*
   * LK forcefully sets /model to "MT6769Z" so use
   * that to print the exact SoC used in the device
   */
  void *fdt = (void*)gd->fdt_blob;
  int offset = fdt_path_offset(fdt, "/");
  const char *model = fdt_getprop(fdt, offset, "model", NULL);
  if (model)
    printf("SoC:   MediaTek %s\n", model);
  else
    printf("SoC:   MediaTek MT6768\n");

  /*
   * Since model print happens later than cpuinfo,
   * fix /model with actual device name
   */
  lk_fdt_fix_model(fdt);
  return 0;
}
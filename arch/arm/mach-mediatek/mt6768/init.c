#include <config.h>
#include <asm/global_data.h>
#include <asm/armv8/mmu.h>
#include <init.h>

#include <linux/bitops.h>
#include <linux/sizes.h>
#include <linux/libfdt.h>
#include <fdtdec.h>
#include <fdt_support.h>
#include <vsprintf.h>
#include <asm/io.h>
#include <dm.h>
#include <env.h>
#include <command.h>

DECLARE_GLOBAL_DATA_PTR;

static struct mm_region mt6768_mem_map[] = {
	{
		/* Peripherals */
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x40000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) | PTE_BLOCK_NON_SHARE |
		         PTE_BLOCK_PXN | PTE_BLOCK_UXN },
	{
		/* DDR */
		.virt = 0x40000000UL,
		.phys = 0x40000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE,
	},
	{
		/* Framebuffer */
		/* virt/phys get updated in dram_init()*/
		.size = 0x00c00000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) | PTE_BLOCK_INNER_SHARE |
		         PTE_BLOCK_PXN | PTE_BLOCK_UXN },
	{
		/* sentinel*/
		0,
	}
};
struct mm_region *mem_map = mt6768_mem_map;

static const void *get_prevbl_fdt_addr(void)
{
	const char *fdt_addr_str = env_get("prevbl_fdt_addr");

	if (!fdt_addr_str)
		return NULL;

	return (const void *)simple_strtoul(fdt_addr_str, NULL, 16);
}

static const char *get_cmdline(void)
{
	const void *fdt_blob = get_prevbl_fdt_addr();
	int node;

	if (!fdt_blob)
		return NULL;

	if (fdt_check_header(fdt_blob))
		return NULL;

	node = fdt_path_offset(fdt_blob, "/chosen");
	return fdt_getprop(fdt_blob, node, "bootargs", NULL);
}

static int get_cmdline_option(const char *cmdline, const char *key, char *out,
			      int out_len)
{
	const char *p, *p_end;
	int len;

	p = strstr(cmdline, key);
	if (!p)
		return -ENOENT;

	p += strlen(key);
	p_end = strstr(p, " ");
	if (!p_end)
		return -ENOENT;

	len = p_end - p;
	if (len > out_len)
		len = out_len;

	strncpy(out, p, len);
	out[len] = '\0';

	return 0;
}

int dram_init(void)
{
	int ret = fdtdec_setup_mem_size_base();
	if (ret) {
		printf("%s: failed (err: %d)\n", __func__, ret);
		return ret;
	}

	gd->ram_size = mt6768_mem_map[1].size;

	gd->ram_size = get_ram_size((long *)CFG_SYS_SDRAM_BASE, SZ_8G);
	/* build the memmap */
	int simplefb = fdt_path_offset(gd->fdt_blob, "/framebuffer");
	if (simplefb >= 0) {
		void *base =
			(void *)fdtdec_get_addr(gd->fdt_blob, simplefb, "reg");
		mt6768_mem_map[2].virt = (u64)base;
		mt6768_mem_map[2].phys = (u64)base;
	} else {
		printf("%s: no simplefb node in fdt\n", __func__);
	}

	mt6768_mem_map[1].size = gd->ram_size;
	fdt_fixup_memory((void *)gd->fdt_blob, CFG_SYS_SDRAM_BASE,
			 gd->ram_size);
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = gd->ram_base;
	gd->bd->bi_dram[0].size = gd->ram_size;
	return 0;
}

void reset_cpu(void)
{
	printf("resetting ...\n");
	/* reset counter */
	writel(0x1971, 0x10007000 + 0x8);
	/* SW reset */
	writel(0x1209, 0x10007000 + 0x14);
}

int board_init(void) {
	return 0;
}

int board_late_init(void)
{
	struct udevice *dev;
	const char *cmdline = get_cmdline();
	char serial[48];
	int ret;

	/* Trigger MUSB probe */
	ret = uclass_get_device(UCLASS_USB_GADGET_GENERIC, 0, &dev);
	if (ret) {
		printf("%s: Failed to find USB device (err: %d)\n", __func__,
		       ret);
		return ret;
	}

	/*
     * Set our custom kernel/FDT/ramdisk addresses
     * because we have CONFIG_ANDROID_BOOT_IMAGE_IGNORE_BLOB_ADDR enabled that fixes:
     * [    0.000000] [Firmware Bug]: Kernel image misaligned at boot, please fix your bootloader!
     * by ignoring boot.img's kernel address (LK expects kernel to be at 0x40080000, not aligned)
     * However, normally enabling this option and not setting these in env
     * causes bootm to malloc a buffer at 0x0 which makes it crash, we fix this.
     *
     * Fastboot buffer addr remains 0x45000000, we don't overlap with anything.
     */
	env_set("kernel_addr_r", "40000000");
	env_set("fdt_addr_r", "41000000");
	env_set("ramdisk_addr_r", "42000000");
	env_set("fastboot_addr_r", "45000000");
	/* fastboot getvar stuff */
	env_set("platform", "mt6768");
	get_cmdline_option(cmdline, "androidboot.serialno=", serial,
			   sizeof(serial));
	if (serial[0] != '\0') {
		env_set("serial#", serial);
	} else {
		printf("%s: serialno not found in cmdline\n", __func__);
		env_set("serial#", "unknown");
	}

#ifdef CONFIG_XIAOMI_MERLIN
	env_set("board", "merlin");
#else
	env_set("board", "generic");
#endif

	return 0;
}

int fdt_copy_resv_mem_node(const void *src, void *dst)
{
	u32 phandle;
	struct fdt_memory pmp_mem;
	fdt_addr_t addr;
	fdt_size_t size;
	int offset, node, err, rmem_offset;
	char basename[32] = {0};
	int bname_len;
	int max_len = sizeof(basename);
	const char *name;
	char *temp;

	offset = fdt_path_offset(src, "/reserved-memory");
	if (offset < 0) {
		log_debug("No reserved memory region found in source FDT\n");
		return 0;
	}

	/*
	 * Extend the FDT by the following estimated size:
	 *
	 * Each PMP memory region entry occupies 64 bytes.
	 * With 16 PMP memory regions we need 64 * 16 = 1024 bytes.
	 */

	fdt_for_each_subnode(node, src, offset) {
		name = fdt_get_name(src, node, NULL);
		printf("setup node: %s\n", name);

		addr = fdtdec_get_addr_size_auto_parent(src, offset, node,
							"reg", 0, &size,
							false);
		if (addr == FDT_ADDR_T_NONE) {
			printf("bad node: %s\n", name);
			continue;
		}

		strncpy(basename, name, max_len);
		temp = strchr(basename, '@');
		if (temp) {
			bname_len = strnlen(basename, max_len) - strnlen(temp,
								       max_len);
			*(basename + bname_len) = '\0';
		}
		pmp_mem.start = addr;
		pmp_mem.end = addr + size - 1;
		err = fdtdec_add_reserved_memory(dst, basename, &pmp_mem,
						 NULL, 0, &phandle, 0);
		if (err < 0 && err != -FDT_ERR_EXISTS) {
			log_err("failed to add reserved memory: %d\n", err);
			return err;
		}
		if (fdt_getprop(src, node, "no-map", NULL)) {
			rmem_offset = fdt_node_offset_by_phandle(dst, phandle);
			fdt_setprop_empty(dst, rmem_offset, "no-map");
		}
	}

	return 0;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
    const void *lk_fdt = get_prevbl_fdt_addr();
    int lk_node;
    fdt_addr_t addr;
    fdt_size_t size;

    if (!lk_fdt || fdt_check_header(lk_fdt))
    	return -EINVAL;

    fdt_set_totalsize(blob, fdt_totalsize(blob) + 4096);

    lk_node = fdt_path_offset(lk_fdt, "/memory");
    if (lk_node > 0) {
    	addr = fdtdec_get_addr_size_auto_noparent(lk_fdt, lk_node, "reg", 0, &size, true);
    	if (addr == FDT_ADDR_T_NONE) {
    		panic("failed to setup /memory address + size");
    	}

    	fdt_fixup_memory(blob, addr, size);
    }

    return fdt_copy_resv_mem_node(lk_fdt, blob);
}

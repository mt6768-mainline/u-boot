# "Das U-Boot" for MediaTek MT6768
## Supported devices
* Xiaomi Redmi Note 9 (xiaomi-merlin)

## Status
**Y** = works;\
**P** = partially works;\
**N** = doesn't work.

* Booting - **N**;
* UART - **N**;
* Display - **N** (simple-framebuffer);
* Internal storage / eMMC - **N**;
* External storage / SD card - **N**;
* Buttons - **N**;
* USB - **N**;

## Building
Firstly, build `U-Boot` itself:
```
ARCH=arm CROSS_COMPILE=aarch64-linux-gnu- make merlin_defconfig
ARCH=arm CROSS_COMPILE=aarch64-linux-gnu- make -j$(nproc)
```

Then, build the Android boot.img:
```
mkbootimg \
  --kernel u-boot-dtb.bin \
  --dtb <stock boot.img dtb> \
  --cmdline "bootopt=64S3,32N2,64N2" \
  --base 0x40078000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x07c08000 \
  --second_offset 0xbff88000 \
  --tags_offset 0x0bc08000 \
  --pagesize 2048 \
  --os_version 13.0.0 \
  --os_patch_level 2025-04 \
  --header_version 2 \
  --output boot.img
```

## Flashing
TBD

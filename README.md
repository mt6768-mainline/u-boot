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
```
make ARCH=arm CROSS_COMPILE=aarch64-linux-gnu- merlin_defconfig
make ARCH=arm CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
mkdtboimg create minimal-dtbo.img arch/arm/dts/mt6769z-xiaomi-merlin-minimal-dtbo.dtb
gzip -c u-boot-dtb.bin > u-boot.gz
dd if=/dev/random of=dummy-ramdisk bs=2048 count=9
mkbootimg \
  --kernel u-boot.gz \
  --ramdisk dummy-ramdisk \
  --dtb arch/arm/dts/mt6769z-xiaomi-merlin.dtb \
  --cmdline "bootopt=64S3,32N2,64N2" \
  --base 0x40078000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x07c08000 \
  --second_offset 0xbff88000 \
  --tags_offset 0x0bc08000 \
  --pagesize 2048 \
  --header_version 2 \
  --output boot.img
```

## Flashing
TBD

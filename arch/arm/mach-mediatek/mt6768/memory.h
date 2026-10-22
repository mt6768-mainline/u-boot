/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2026 Roman Vivchar <rva333@protonmail.com> */

#ifndef __MT6768_MEMORY_H
#define __MT6768_MEMORY_H

#include "preloader.h"

extern struct mt6768_dram_info mt6768_memory;

int mt6768_load_bootargs(const void *args);

#endif

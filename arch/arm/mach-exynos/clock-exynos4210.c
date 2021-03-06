/*
 * linux/arch/arm/mach-exynos/clock-exynos4210.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4210 - Clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>

#include <plat/cpu-freq.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/pll.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>
#include <plat/exynos4.h>
#include <plat/pm.h>

#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/exynos-clock.h>
#include <mach/dev-sysmmu.h>

static struct clksrc_clk *sysclks[] = {
	/* nothing here yet */
};

#ifdef CONFIG_PM
static struct sleep_save exynos4210_clock_save[] = {
	/* CMU side */
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_LCD1),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_IMAGE_4210),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_LCD1),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_PERIR_4210),
	SAVE_ITEM(EXYNOS4_CLKDIV_IMAGE),
	SAVE_ITEM(EXYNOS4_CLKDIV_LCD1),
	SAVE_ITEM(EXYNOS4_CLKSRC_IMAGE),
	SAVE_ITEM(EXYNOS4_CLKSRC_LCD1),
};
#endif

static struct clk init_clocks_off[] = {
	{
		.name		= "sataphy",
		.parent		= &exynos4_clk_aclk_133.clk,
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 3),
	}, {
		.name		= "sata",
		.parent		= &exynos4_clk_aclk_133.clk,
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 10),
	}, {
		.name		= "ppmuimage",
		.enable		= exynos4_clk_ip_image_ctrl,
		.ctrlbit	= (1 << 9),
	}, {
		.name		= "ppmulcd1",
		.enable		= exynos4_clk_ip_lcd1_ctrl,
		.ctrlbit	= (1 << 5),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(fimd1, 7),
		.enable		= exynos4_clk_ip_lcd1_ctrl,
		.ctrlbit	= (1 << 4),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(pcie, 8),
		.enable		= exynos4_clk_ip_fsys_ctrl,
		.ctrlbit	= (1 << 18),
	}, {
		.name		= "sysmmu",
		.devname	= SYSMMU_CLOCK_NAME(2d, 9),
		.enable		= exynos4_clk_ip_image_ctrl,
		.ctrlbit	= (1 << 3),
	},
};

static struct clksrc_clk clksrcs[] = {
	{
		.clk	= {
			.name		= "sclk_sata",
			.enable		= exynos4_clksrc_mask_fsys_ctrl,
			.ctrlbit	= (1 << 24),
		},
		.sources = &exynos4_clkset_mout_corebus,
		.reg_src = { .reg = EXYNOS4_CLKSRC_FSYS, .shift = 24, .size = 1 },
		.reg_div = { .reg = EXYNOS4_CLKDIV_FSYS0, .shift = 20, .size = 4 },
	},
};

static struct vpll_div_data vpll_div_4210[] = {
	{54000000, 3, 53, 3, 1024, 0, 17, 0},
	{108000000, 3, 53, 2, 1024, 0, 17, 0},
	{260000000, 3, 63, 1, 1950, 0, 20, 1},
	{330000000, 2, 53, 1, 2048, 1,  1, 1},
#ifdef CONFIG_EXYNOS4_MSHC_VPLL_46MHZ
	{370882812, 3, 44, 0, 2417, 0, 14, 0},
#endif
};

static unsigned long exynos4210_vpll_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int exynos4210_vpll_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int vpll_con0, vpll_con1;
	unsigned int i;

	/* Return if nothing changed */
	if (clk->rate == rate)
		return 0;

	vpll_con0 = __raw_readl(EXYNOS4_VPLL_CON0);
	vpll_con0 &= ~(0x1 << 27 |					\
			PLL90XX_MDIV_MASK << PLL90XX_MDIV_SHIFT |	\
			PLL90XX_PDIV_MASK << PLL90XX_PDIV_SHIFT |	\
			PLL90XX_SDIV_MASK << PLL90XX_SDIV_SHIFT);

	vpll_con1 = __raw_readl(EXYNOS4_VPLL_CON1);
	vpll_con1 &= ~(0x1f << 24 |	\
			0x3f << 16 |	\
			0xfff << 0);

	for (i = 0; i < ARRAY_SIZE(vpll_div_4210); i++) {
		if (vpll_div_4210[i].rate == rate) {
			vpll_con0 |= vpll_div_4210[i].vsel << 27;
			vpll_con0 |= vpll_div_4210[i].pdiv << PLL90XX_PDIV_SHIFT;
			vpll_con0 |= vpll_div_4210[i].mdiv << PLL90XX_MDIV_SHIFT;
			vpll_con0 |= vpll_div_4210[i].sdiv << PLL90XX_SDIV_SHIFT;
			vpll_con1 |= vpll_div_4210[i].mrr << 24;
			vpll_con1 |= vpll_div_4210[i].mfr << 16;
			vpll_con1 |= vpll_div_4210[i].k << 0;
			break;
		}
	}

	if (i == ARRAY_SIZE(vpll_div_4210)) {
		printk(KERN_ERR "%s: Invalid Clock VPLL Frequency\n",
				__func__);
		return -EINVAL;
	}

	__raw_writel(vpll_con0, EXYNOS4_VPLL_CON0);
	__raw_writel(vpll_con1, EXYNOS4_VPLL_CON1);

	clk->rate = rate;

	return 0;
}

#ifdef CONFIG_PM
static int exynos4210_clock_suspend(void)
{
	s3c_pm_do_save(exynos4210_clock_save, ARRAY_SIZE(exynos4210_clock_save));

	return 0;
}

static void exynos4210_clock_resume(void)
{
	s3c_pm_do_restore_core(exynos4210_clock_save, ARRAY_SIZE(exynos4210_clock_save));
}
#else
#define exynos4210_clock_suspend NULL
#define exynos4210_clock_resume NULL
#endif

struct syscore_ops exynos4210_clock_syscore_ops = {
	.suspend        = exynos4210_clock_suspend,
	.resume         = exynos4210_clock_resume,
};

void __init exynos4210_register_clocks(void)
{
	int ptr;

	exynos4_clk_mout_mpll.reg_src.reg = EXYNOS4_CLKSRC_CPU;
	exynos4_clk_mout_mpll.reg_src.shift = 8;
	exynos4_clk_mout_mpll.reg_src.size = 1;

	exynos4_clk_aclk_200.sources = &exynos4_clkset_aclk;
	exynos4_clk_aclk_200.reg_src.reg = EXYNOS4_CLKSRC_TOP0;
	exynos4_clk_aclk_200.reg_src.shift = 12;
	exynos4_clk_aclk_200.reg_src.size = 1;
	exynos4_clk_aclk_200.reg_div.reg = EXYNOS4_CLKDIV_TOP;
	exynos4_clk_aclk_200.reg_div.shift = 0;
	exynos4_clk_aclk_200.reg_div.size = 3;

	exynos4_clk_fimg2d.enable = exynos4_clk_ip_image_ctrl;
	exynos4_clk_fimg2d.ctrlbit = (1 << 0);

	exynos4_clk_mout_g2d0.reg_src.reg = EXYNOS4_CLKSRC_IMAGE;
	exynos4_clk_mout_g2d0.reg_src.shift = 0;
	exynos4_clk_mout_g2d0.reg_src.size = 1;

	exynos4_clk_mout_g2d1.reg_src.reg = EXYNOS4_CLKSRC_IMAGE;
	exynos4_clk_mout_g2d1.reg_src.shift = 4;
	exynos4_clk_mout_g2d1.reg_src.size = 1;

	exynos4_clk_sclk_fimg2d.reg_src.reg = EXYNOS4_CLKSRC_IMAGE;
	exynos4_clk_sclk_fimg2d.reg_src.shift = 8;
	exynos4_clk_sclk_fimg2d.reg_src.size = 1;
	exynos4_clk_sclk_fimg2d.reg_div.reg = EXYNOS4_CLKDIV_IMAGE;
	exynos4_clk_sclk_fimg2d.reg_div.shift = 0;
	exynos4_clk_sclk_fimg2d.reg_div.size = 4;

	exynos4_init_dmaclocks[2].parent = &exynos4_init_dmaclocks[1];

	exynos4_vpll_ops.get_rate = exynos4210_vpll_get_rate;
	exynos4_vpll_ops.set_rate = exynos4210_vpll_set_rate;

	for (ptr = 0; ptr < ARRAY_SIZE(sysclks); ptr++)
		s3c_register_clksrc(sysclks[ptr], 1);

	s3c_register_clksrc(clksrcs, ARRAY_SIZE(clksrcs));

	s3c_register_clocks(init_clocks_off, ARRAY_SIZE(init_clocks_off));
	s3c_disable_clocks(init_clocks_off, ARRAY_SIZE(init_clocks_off));
	register_syscore_ops(&exynos4210_clock_syscore_ops);
}

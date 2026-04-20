// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/net/phy/motorcomm.c
 *
 * Driver for Motorcomm PHYs
 *
 * Author: yinghong.zhang<yinghong.zhang@motor-comm.com>
 *
 * Copyright (c) 2024 Motorcomm, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * Support : Motorcomm Phys:
 * Giga phys: yt8511, yt8521, yt8531, yt8543, yt8614, yt8618
 * 100/10M Phys: yt8510, yt8512, yt8522
 */
#include <phy.h>
#include <linux/compat.h>
#include <malloc.h>

/* PS: below(#include <linux/delay.h>) will be updated to "#include <common.h>"
 * when uboot ver is 2017.1
 */
// #include <linux/delay.h>
#include <common.h>
#include <net.h>
#include <version.h>

#undef U_BOOT_PHY_DRIVER_MACRO
#if defined(U_BOOT_VERSION_NUM) && defined(U_BOOT_VERSION_NUM_PATCH)
#if ((U_BOOT_VERSION_NUM * 10000 + U_BOOT_VERSION_NUM_PATCH) >= (2023 * 10000 + 7))
#define U_BOOT_PHY_DRIVER_MACRO
#endif
#endif

#define YT_UBOOT_MAJOR			2
#define YT_UBOOT_MINOR			2
#define YT_UBOOT_SUBVERSION		63626
#define YT_UBOOT_VERSIONID		"2.2.63626"

#define PHY_ID_YT8510			0x00000109
#define PHY_ID_YT8511			0x0000010a
#define PHY_ID_YT8512			0x00000118
#define PHY_ID_YT8512B			0x00000128
#define PHY_ID_YT8522			0x4f51e928
#define PHY_ID_YT8521S			0x0000011a
#define PHY_ID_YT8531			0x4f51e91b
/* YT8543 phy driver disable(default) */
/* #define YTPHY_YT8543_ENABLE */
#ifdef YTPHY_YT8543_ENABLE
#define PHY_ID_YT8543			0x0008011b
#endif
#define PHY_ID_YT8531S			0x4f51e91a
#define PHY_ID_YT8614			0x4F51E899
#define PHY_ID_YT8618			0x4f51e889
#define PHY_ID_YT8821			0x4f51ea19
#define PHY_ID_YT8831			0x4f51e938
#define PHY_ID_YT8111			0x4f51e9e8
#define PHY_ID_YT8628			0x4f51e8c8
#define PHY_ID_YT8824			0x4f51e8b8
#define PHY_ID_MASK			0xffffffff

/* for YT8531 package A xtal init config */
#define YT8531A_XTAL_INIT		0
/* some GMAC need clock input from PHY, for eg., 125M,
 * please enable this macro
 * by degault, it is set to 0
 * NOTE: this macro will need macro SYS_WAKEUP_BASED_ON_ETH_PKT to set to 1
 */
#define GMAC_CLOCK_INPUT_NEEDED		0
#define YT_861X_AB_VER			0
#if (YT_861X_AB_VER)
static int yt8614_get_port_from_phydev(struct phy_device *phydev);
#endif

#define REG_PHY_SPEC_STATUS		0x11

#define REG_DEBUG_ADDR_OFFSET		0x1e
#define REG_DEBUG_DATA			0x1f
#define YT_SPEC_STATUS			0x11
#define YT_UTP_INTR_REG			0x12
#define YT_SOFT_RESET			0x8000
#define YT_SPEED_MODE			0xc000
#define YT_SPEED_MODE_BIT		14
#define YT_DUPLEX			0x2000
#define YT_DUPLEX_BIT			13
#define YT_LINK_STATUS_BIT		10
#define YT_REG_SPACE_UTP		0
#define YT_REG_SPACE_FIBER		2
#define YT_REG_SMI_MUX			0xa000
#define YT8512_CLOCK_INIT_NEED		0
#define YT8512_EXTREG_AFE_PLL		0x50
#define YT8512_EXTREG_EXTEND_COMBO	0x4000
#define YT8512_EXTREG_LED0		0x40c0
#define YT8512_EXTREG_LED1		0x40c3
#define YT8512_EXTREG_SLEEP_CONTROL1	0x2027
#define YT8512_CONFIG_PLL_REFCLK_SEL_EN	0x0040
#define YT8512_CONTROL1_RMII_EN		0x0001
#define YT8512_LED0_ACT_BLK_IND		0x1000
#define YT8512_LED0_DIS_LED_AN_TRY	0x0001
#define YT8512_LED0_BT_BLK_EN		0x0002
#define YT8512_LED0_HT_BLK_EN		0x0004
#define YT8512_LED0_COL_BLK_EN		0x0008
#define YT8512_LED0_BT_ON_EN		0x0010
#define YT8512_LED1_BT_ON_EN		0x0010
#define YT8512_LED1_TXACT_BLK_EN	0x0100
#define YT8512_LED1_RXACT_BLK_EN	0x0200
#define YT8512_10BT_DEBUG_LPBKS		0x200A
#define YT8522_TX_CLK_DELAY		0x4210
#define YT8522_ANAGLOG_IF_CTRL		0x4008
#define YT8522_DAC_CTRL			0x2057
#define YT8522_INTERPOLATOR_FILTER_1	0x14
#define YT8522_INTERPOLATOR_FILTER_2	0x15
#define YT8522_EXTENDED_COMBO_CTRL_1	0x4000
#define YT8522_TX_DELAY_CONTROL		0x19
#define YT8522_EXTENDED_PAD_CONTROL	0x4001
#define YT8512_EN_SLEEP_SW_BIT		15
#define YT8521_EXTREG_SLEEP_CONTROL1	0x27
#define YT8521_EN_SLEEP_SW_BIT		15
#define YT8628_CHIP_MODE_REG		0xa008
#define YT8628_CHIP_MODE		(BIT(7) | BIT(6))
#define YT8628_CHIP_MODE_OFFSET		6
#define YT8628_CHIP_MODE_BASE_ADDR (BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0))
#define YT8628_REG_SPACE_UTP 		0
#define YT8628_REG_SPACE_QSGMII_OUSGMII 1
#define YT8824_REG_SPACE_UTP		0
#define YT8824_REG_SPACE_SERDES		1
#define REG_MII_MMD_CTRL		0x0D
#define REG_MII_MMD_DATA		0x0E

/* to enable system WOL feature of phy, please define this macro to 1
 * otherwise, define it to 0.
 */
#define YT_WOL_FEATURE_ENABLE		0
/* WOL Feature Event Interrupt Enable */
#define YT_WOL_FEATURE_INTR		BIT(6)

/* Magic Packet MAC address registers */
#define YT_WOL_FEATURE_MACADDR2_4_MAGIC_PACKET	0xa007
#define YT_WOL_FEATURE_MACADDR1_4_MAGIC_PACKET	0xa008
#define YT_WOL_FEATURE_MACADDR0_4_MAGIC_PACKET	0xa009

#define YT_WOL_FEATURE_REG_CFG		0xa00a
/* WOL TYPE Config */
#define YT_WOL_FEATURE_TYPE_CFG		BIT(0)
/* WOL Enable Config */
#define YT_WOL_FEATURE_ENABLE_CFG	BIT(3)
/* WOL Event Interrupt Enable Config */
#define YT_WOL_FEATURE_INTR_SEL_CFG	BIT(6)
/* WOL Pulse Width Config */
#define YT_WOL_FEATURE_WIDTH1_CFG	BIT(1)
/* WOL Pulse Width Config */
#define YT_WOL_FEATURE_WIDTH2_CFG	BIT(2)

enum yt_wol_feature_trigger_type_e {
	YT_WOL_FEATURE_PULSE_TRIGGER,
	YT_WOL_FEATURE_LEVEL_TRIGGER,
	YT_WOL_FEATURE_TRIGGER_TYPE_MAX
};

enum yt_wol_feature_pulse_width_e {
	YT_WOL_FEATURE_672MS_PULSE_WIDTH,
	YT_WOL_FEATURE_336MS_PULSE_WIDTH,
	YT_WOL_FEATURE_168MS_PULSE_WIDTH,
	YT_WOL_FEATURE_84MS_PULSE_WIDTH,
	YT_WOL_FEATURE_PULSE_WIDTH_MAX
};

struct yt_wol_feature_cfg {
	bool enable;
	int type;
	int width;
};

/* polling mode */
#define PHY_MODE_FIBER		1	/* fiber mode only */
#define PHY_MODE_UTP		2	/* utp mode only */
#define PHY_MODE_POLL		3	/* fiber and utp, poll mode */

#define msleep(n)		udelay(n * 1000)

struct yt8xxx_priv {
	int strap_polling;
	int chip_mode;
	u8 phy_base_addr;
	u8 top_phy_addr;
	u16 data;
};

static int ytphy_read_ext(struct phy_device *phydev, u32 regnum)
{
	int ret;
	int val;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, REG_DEBUG_DATA);

	return val;
}

static int ytphy_write_ext(struct phy_device *phydev, u32 regnum, u16 val)
{
	int ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		return ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_DEBUG_DATA, val);

	return ret;
}

int phy_top_read(struct phy_device *phydev, int devad, int regnum)
{
	struct yt8xxx_priv *priv = phydev->priv;
	struct mii_dev *bus = phydev->bus;

	if (!bus || !bus->read) {
		debug("%s: No bus configured\n", __func__);
		return -1;
	}

	return bus->read(bus, priv->top_phy_addr, devad, regnum);
}

int phy_top_write(struct phy_device *phydev, int devad, int regnum, u16 val)
{
	struct yt8xxx_priv *priv = phydev->priv;
	struct mii_dev *bus = phydev->bus;

	if (!bus || !bus->write) {
		debug("%s: No bus configured\n", __func__);
		return -1;
	}

	return bus->write(bus, priv->top_phy_addr, devad, regnum, val);
}

static int ytphy_top_write_ext(struct phy_device *phydev, u32 regnum, u16 val)
{
	int ret;

	ret = phy_top_write(phydev, MDIO_DEVAD_NONE, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		return ret;

	ret = phy_top_write(phydev, MDIO_DEVAD_NONE, REG_DEBUG_DATA, val);

	return ret;
}

__attribute__((unused)) static int ytphy_top_read_ext(struct phy_device *phydev, u32 regnum)
{
	int ret;
	int val;

	ret = phy_top_write(phydev, MDIO_DEVAD_NONE, REG_DEBUG_ADDR_OFFSET, regnum);
	if (ret < 0)
		return ret;

	val = phy_top_read(phydev, MDIO_DEVAD_NONE, REG_DEBUG_DATA);

	return val;
}

__attribute__((unused)) static int ytphy_read_mmd(struct phy_device* phydev, u16 device, u16 reg)
{
	int val;
	int ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_MII_MMD_CTRL, device);
	if (ret < 0)
		return ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_MII_MMD_DATA, reg);
	if (ret < 0)
		return ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_MII_MMD_CTRL,
			device | 0x4000);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, REG_MII_MMD_DATA);

	return val;
}

static int ytphy_write_mmd(struct phy_device* phydev,
						   u16 device, u16 reg,
						   u16 value)
{
	int ret = 0;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_MII_MMD_CTRL, device);
	if (ret < 0)
		return ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_MII_MMD_DATA, reg);
	if (ret < 0)
		return ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_MII_MMD_CTRL,
			device | 0x4000);
	if (ret < 0)
		return ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, REG_MII_MMD_DATA, value);

	return ret;
}

static int ytxxxx_soft_reset(struct phy_device *phydev)
{
	int ret, val;

	val = ytphy_read_ext(phydev, 0xa001);
	ytphy_write_ext(phydev, 0xa001, (val & ~0x8000));

	ret = ytphy_write_ext(phydev, 0xa000, 0);

	return ret;
}

static int ytphy_soft_reset(struct phy_device *phydev)
{
	int ret = 0, val = 0;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	if (val < 0)
		return val;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val | BMCR_RESET);
	if (ret < 0)
		return ret;

	return ret;
}

#if YT8512_CLOCK_INIT_NEED
static int yt8512_clk_init(struct phy_device *phydev)
{
	int ret;
	int val;

	val = ytphy_read_ext(phydev, YT8512_EXTREG_AFE_PLL);
	if (val < 0)
		return val;

	val |= YT8512_CONFIG_PLL_REFCLK_SEL_EN;

	ret = ytphy_write_ext(phydev, YT8512_EXTREG_AFE_PLL, val);
	if (ret < 0)
		return ret;

	val = ytphy_read_ext(phydev, YT8512_EXTREG_EXTEND_COMBO);
	if (val < 0)
		return val;

	val |= YT8512_CONTROL1_RMII_EN;

	ret = ytphy_write_ext(phydev, YT8512_EXTREG_EXTEND_COMBO, val);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	if (val < 0)
		return val;

	val |= YT_SOFT_RESET;
	ret = phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val);

	return ret;
}
#endif

static int yt8512_led_init(struct phy_device *phydev)
{
	int mask;
	int ret;
	int val;

	val = ytphy_read_ext(phydev, YT8512_EXTREG_LED0);
	if (val < 0)
		return val;

	val |= YT8512_LED0_ACT_BLK_IND;

	mask = YT8512_LED0_DIS_LED_AN_TRY | YT8512_LED0_BT_BLK_EN |
	YT8512_LED0_HT_BLK_EN | YT8512_LED0_COL_BLK_EN |
	YT8512_LED0_BT_ON_EN;
	val &= ~mask;

	ret = ytphy_write_ext(phydev, YT8512_EXTREG_LED0, val);
	if (ret < 0)
		return ret;

	val = ytphy_read_ext(phydev, YT8512_EXTREG_LED1);
	if (val < 0)
		return val;

	val |= YT8512_LED1_BT_ON_EN;

	mask = YT8512_LED1_TXACT_BLK_EN | YT8512_LED1_RXACT_BLK_EN;
	val &= ~mask;

	ret = ytphy_write_ext(phydev, YT8512_LED1_BT_ON_EN, val);

	return ret;
}

#if (YT8531A_XTAL_INIT)
static int yt8531a_xtal_init(struct phy_device *phydev)
{
	int ret = 0;
	int val = 0;

	udelay(50000);	/* delay 50ms */

	do {
		ret = ytphy_write_ext(phydev, 0xa012, 0x88);
		if (ret < 0)
			return ret;

		udelay(100000);	/* delay 100ms */

		val = ytphy_read_ext(phydev, 0xa012);
		if (val < 0)
			return val;
	} while (val != 0x88);

	ret = ytphy_write_ext(phydev, 0xa012, 0xc8);
	if (ret < 0)
		return ret;

	return ret;
}
#endif

#if (YT_WOL_FEATURE_ENABLE)
static int yt_switch_reg_space(struct phy_device *phydev, int space)
{
	int ret;

	if (space == YT_REG_SPACE_UTP)
		ret = ytphy_write_ext(phydev, 0xa000, 0);
	else
		ret = ytphy_write_ext(phydev, 0xa000, 2);

	return ret;
}

static int yt_wol_feature_enable_cfg(struct phy_device *phydev,
				     struct yt_wol_feature_cfg wol_cfg)
{
	int ret = 0;
	int val = 0;

	val = ytphy_read_ext(phydev, YT_WOL_FEATURE_REG_CFG);
	if (val < 0)
		return val;

	if (wol_cfg.enable) {
		val |= YT_WOL_FEATURE_ENABLE_CFG;

		if (wol_cfg.type == YT_WOL_FEATURE_LEVEL_TRIGGER) {
			val &= ~YT_WOL_FEATURE_TYPE_CFG;
			val &= ~YT_WOL_FEATURE_INTR_SEL_CFG;
		} else if (wol_cfg.type == YT_WOL_FEATURE_PULSE_TRIGGER) {
			val |= YT_WOL_FEATURE_TYPE_CFG;
			val |= YT_WOL_FEATURE_INTR_SEL_CFG;

			if (wol_cfg.width == YT_WOL_FEATURE_84MS_PULSE_WIDTH) {
				val &= ~YT_WOL_FEATURE_WIDTH1_CFG;
				val &= ~YT_WOL_FEATURE_WIDTH2_CFG;
			} else if (wol_cfg.width == YT_WOL_FEATURE_168MS_PULSE_WIDTH) {
				val |= YT_WOL_FEATURE_WIDTH1_CFG;
				val &= ~YT_WOL_FEATURE_WIDTH2_CFG;
			} else if (wol_cfg.width == YT_WOL_FEATURE_336MS_PULSE_WIDTH) {
				val &= ~YT_WOL_FEATURE_WIDTH1_CFG;
				val |= YT_WOL_FEATURE_WIDTH2_CFG;
			} else if (wol_cfg.width == YT_WOL_FEATURE_672MS_PULSE_WIDTH) {
				val |= YT_WOL_FEATURE_WIDTH1_CFG;
				val |= YT_WOL_FEATURE_WIDTH2_CFG;
			}
		}
	} else {
		val &= ~YT_WOL_FEATURE_ENABLE_CFG;
		val &= ~YT_WOL_FEATURE_INTR_SEL_CFG;
	}

	ret = ytphy_write_ext(phydev, YT_WOL_FEATURE_REG_CFG, val);
	if (ret < 0)
		return ret;

	return 0;
}

static int yt_wol_feature_set(struct phy_device *phydev,
			      struct ethtool_wolinfo *wol)
{
	/* u-boot ver < v2023.04(not included)
	 * struct phy_device {
	 *	...
	 * 	#ifdef CONFIG_DM_ETH
	 * 		struct udevice *dev;
	 * 		ofnode node;
	 * 	#else
	 * 		struct eth_device *dev;
	 * 	#endif
	 *	...
	 * };
	 * u-boot ver >= v2023.04(included)
	 * struct phy_device {
	 *	...
	 * 		struct udevice *dev;
	 *	...
	 * };
	 * -------------------------------------
	 * u-boot-2021.04
	 * #define U_BOOT_VERSION_NUM 2021
	 * #define U_BOOT_VERSION_NUM_PATCH 4
	 * -------------------------------------
	 * u-boot-2021.07
	 * #define U_BOOT_VERSION_NUM 2021
	 * #define U_BOOT_VERSION_NUM_PATCH 7
	 * -------------------------------------
	 * u-boot-2022.01-rc1
	 * #define U_BOOT_VERSION_NUM 2022
	 * #define U_BOOT_VERSION_NUM_PATCH 1
	 * -------------------------------------
	 * u-boot-2023.04
	 * #define U_BOOT_VERSION_NUM 2023
	 * #define U_BOOT_VERSION_NUM_PATCH 4
	 * -------------------------------------
	 * u-boot-2023.07-rc1
	 * #define U_BOOT_VERSION_NUM 2023
	 * #define U_BOOT_VERSION_NUM_PATCH 7
	 * -------------------------------------
	 * u-boot-2024.10-rc1
	 * #define U_BOOT_VERSION_NUM 2024
	 * #define U_BOOT_VERSION_NUM_PATCH 10
	 * -------------------------------------
	 */
	/* if defined CONFIG_DM_ETH || u-boot ver >= v2023.04 ==> struct udevice */
#if defined(CONFIG_DM_ETH) || (defined(U_BOOT_VERSION_NUM) && defined(U_BOOT_VERSION_NUM_PATCH) && ((U_BOOT_VERSION_NUM * 10000 + U_BOOT_VERSION_NUM_PATCH) >= (2023 * 10000 + 4)))
	struct udevice *dev = phydev->dev;
	struct eth_pdata *pdata = dev_get_plat(dev);
	u8 *hwaddr = pdata->enetaddr;
#else
	struct eth_device *p_attached_dev = phydev->dev;
	u8 *hwaddr = p_attached_dev->enetaddr;
#endif
	struct yt_wol_feature_cfg wol_cfg;
	int ret, curr_reg_space, val;

	memset(&wol_cfg, 0, sizeof(struct yt_wol_feature_cfg));
	curr_reg_space = ytphy_read_ext(phydev, 0xa000);
	if (curr_reg_space < 0)
		return curr_reg_space;

	/* Switch to phy UTP page */
	ret = yt_switch_reg_space(phydev, YT_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	if (wol->wolopts & WAKE_MAGIC) {
		/* Enable the WOL feature interrupt */
		val = phy_read(phydev, MDIO_DEVAD_NONE, YT_UTP_INTR_REG);
		val |= YT_WOL_FEATURE_INTR;
		ret = phy_write(phydev, MDIO_DEVAD_NONE, YT_UTP_INTR_REG, val);
		if (ret < 0)
			return ret;

		/* Set the WOL feature config */
		wol_cfg.enable = true;
		wol_cfg.type = YT_WOL_FEATURE_PULSE_TRIGGER;
		wol_cfg.width = YT_WOL_FEATURE_672MS_PULSE_WIDTH;
		ret = yt_wol_feature_enable_cfg(phydev, wol_cfg);
		if (ret < 0)
			return ret;

		/* Store the device address for the magic packet */
		ret = ytphy_write_ext(phydev,
				      YT_WOL_FEATURE_MACADDR2_4_MAGIC_PACKET,
				      ((hwaddr[0] << 8) | hwaddr[1]));
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev,
				      YT_WOL_FEATURE_MACADDR1_4_MAGIC_PACKET,
				      ((hwaddr[2] << 8) | hwaddr[3]));
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev,
				      YT_WOL_FEATURE_MACADDR0_4_MAGIC_PACKET,
				      ((hwaddr[4] << 8) | hwaddr[5]));
		if (ret < 0)
			return ret;
	} else {
		wol_cfg.enable = false;
		wol_cfg.type = YT_WOL_FEATURE_TRIGGER_TYPE_MAX;
		wol_cfg.width = YT_WOL_FEATURE_PULSE_WIDTH_MAX;
		ret = yt_wol_feature_enable_cfg(phydev, wol_cfg);
		if (ret < 0)
			return ret;
	}

	/* Recover to previous register space page */
	ret = yt_switch_reg_space(phydev, curr_reg_space);
	if (ret < 0)
		return ret;

	return 0;
}
#endif	/*(YT_WOL_FEATURE_ENABLE)*/

static int yt_parse_status(struct phy_device *phydev)
{
	int speed, speed_mode, duplex;
	int val;

	val = phy_read(phydev, MDIO_DEVAD_NONE, YT_SPEC_STATUS);
	if (val < 0)
		return val;

	duplex = (val & YT_DUPLEX) >> YT_DUPLEX_BIT;
	speed_mode = (val & YT_SPEED_MODE) >> YT_SPEED_MODE_BIT;
	switch (speed_mode) {
	case 2:
		speed = SPEED_1000;
		break;
	case 1:
		speed = SPEED_100;
		break;
	default:
		speed = SPEED_10;
		break;
	}

	phydev->speed = speed;
	phydev->duplex = duplex;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_LPA);
	if (val < 0)
		return val;

	phydev->pause = val & LPA_PAUSE_CAP;
	phydev->asym_pause = val & LPA_PAUSE_ASYM;

	return 0;
}

static int yt8821_parse_status(struct phy_device *phydev)
{
	int speed_mode_bit15_14, speed_mode_bit9;
	int speed, speed_mode, duplex;
	int val;

	val = phy_read(phydev, MDIO_DEVAD_NONE, YT_SPEC_STATUS);
	if (val < 0)
		return val;

	duplex = (val & YT_DUPLEX) >> YT_DUPLEX_BIT;

	/* Bit9-Bit15-Bit14 speed mode 100---2.5G; 010---1000M;
	 * 001---100M; 000---10M
	 */
	speed_mode_bit15_14 = (val & YT_SPEED_MODE) >> YT_SPEED_MODE_BIT;
	speed_mode_bit9 = (val & BIT(9)) >> 9;
	speed_mode = (speed_mode_bit9 << 2) | speed_mode_bit15_14;

	switch (speed_mode) {
	case 4:
		speed = SPEED_2500;
		break;
	case 2:
		speed = SPEED_1000;
		break;
	case 1:
		speed = SPEED_100;
		break;
	default:
		speed = SPEED_10;
		break;
	}

	phydev->speed = speed;
	phydev->duplex = duplex;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_LPA);
	if (val < 0)
		return val;

	phydev->pause = val & LPA_PAUSE_CAP;
	phydev->asym_pause = val & LPA_PAUSE_ASYM;

	return 0;
}

static int yt_startup(struct phy_device *phydev)
{
	msleep(1000);
	genphy_update_link(phydev);
	yt_parse_status(phydev);

	return 0;
}

static int yt8510_config(struct phy_device *phydev)
{
	ytphy_soft_reset(phydev);

	return 0;
}

static int yt8511_config(struct phy_device *phydev)
{
	genphy_config_aneg(phydev);

	ytphy_soft_reset(phydev);

	return 0;
}

static int yt8512_probe(struct phy_device *phydev)
{
	struct yt8xxx_priv *yt8512;
	int chip_config;

	if (!phydev->priv) {
		yt8512 = kzalloc(sizeof(*yt8512), GFP_KERNEL);
		if (!yt8512)
			return -ENOMEM;	/* ref: net/phy/ti.c */

		phydev->priv = yt8512;
		chip_config = ytphy_read_ext(phydev,
					     YT8512_EXTREG_EXTEND_COMBO);
		yt8512->chip_mode = (chip_config & (BIT(1) | BIT(0)));
	} else {
		yt8512 = (struct yt8xxx_priv *)phydev->priv;
	}

	return 0;
}

static int yt8512_config(struct phy_device *phydev)
{
	struct yt8xxx_priv *priv = phydev->priv;
	int ret = 0, val = 0;

	val = ytphy_read_ext(phydev, YT8512_10BT_DEBUG_LPBKS);
	if (val < 0)
		return val;

	val &= ~BIT(10);
	ret = ytphy_write_ext(phydev, YT8512_10BT_DEBUG_LPBKS, val);
	if (ret < 0)
		return ret;

	if (!(priv->chip_mode)) {	/* MII mode */
		val &= ~BIT(15);
		ret = ytphy_write_ext(phydev, YT8512_10BT_DEBUG_LPBKS, val);
		if (ret < 0)
			return ret;
	}

#if YT8512_CLOCK_INIT_NEED
	ret = yt8512_clk_init(phydev);
	if (ret < 0)
		return ret;
#endif

	ret = yt8512_led_init(phydev);

	/* disable auto sleep */
	val = ytphy_read_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1);
	if (val < 0)
		return val;

	val &= (~BIT(YT8512_EN_SLEEP_SW_BIT));

	ret = ytphy_write_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1, val);
	if (ret < 0)
		return ret;

	genphy_config_aneg(phydev);

	ytphy_soft_reset(phydev);

	return 0;
}

static int yt8522_probe(struct phy_device *phydev)
{
	struct yt8xxx_priv *yt8522;
	int chip_config;

	if (!phydev->priv) {
		yt8522 = kzalloc(sizeof(*yt8522), GFP_KERNEL);
		if (!yt8522)
			return -ENOMEM;	/* ref: net/phy/ti.c */

		phydev->priv = yt8522;
		chip_config = ytphy_read_ext(phydev,
					     YT8522_EXTENDED_COMBO_CTRL_1);
		yt8522->chip_mode = (chip_config & (BIT(1) | BIT(0)));
	} else {
		yt8522 = (struct yt8xxx_priv *)phydev->priv;
	}

	return 0;
}

static int yt8522_config(struct phy_device *phydev)
{
	struct yt8xxx_priv *priv = phydev->priv;
	int ret;
	int val;

	val = ytphy_read_ext(phydev, YT8522_EXTENDED_COMBO_CTRL_1);
	if (val < 0)
		return val;

	if (0x2 == (priv->chip_mode)) {	/* RMII2 mode */
		val |= BIT(4);
		ret = ytphy_write_ext(phydev, YT8522_EXTENDED_COMBO_CTRL_1,
				      val);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(phydev, YT8522_TX_DELAY_CONTROL, 0x9f);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(phydev, YT8522_EXTENDED_PAD_CONTROL,
				      0x81d4);
		if (ret < 0)
			return ret;
	}
	else if (0x3 == (priv->chip_mode)) {	/* RMII1 mode */
		val |= BIT(4);
		ret = ytphy_write_ext(phydev, YT8522_EXTENDED_COMBO_CTRL_1,
				      val);
		if (ret < 0)
			return ret;
	}

	ret = ytphy_write_ext(phydev, YT8522_TX_CLK_DELAY, 0);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, YT8522_ANAGLOG_IF_CTRL, 0xbf2a);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, YT8522_DAC_CTRL, 0x297f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, YT8522_INTERPOLATOR_FILTER_1, 0x1FE);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, YT8522_INTERPOLATOR_FILTER_2, 0x1FE);
	if (ret < 0)
		return ret;

	/* disable auto sleep */
	val = ytphy_read_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1);
	if (val < 0)
		return val;

	val &= (~BIT(YT8512_EN_SLEEP_SW_BIT));

	ret = ytphy_write_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1, val);
	if (ret < 0)
		return ret;

	genphy_config_aneg(phydev);

	ytphy_soft_reset(phydev);

	return 0;
}

static int yt8522_startup(struct phy_device *phydev)
{
	int speed, speed_mode, duplex;
	int val;

	msleep(1000);

	val = phy_read(phydev, MDIO_DEVAD_NONE, REG_PHY_SPEC_STATUS);
	if (val < 0)
		return val;

	if ((val & BIT(10)) >> YT_LINK_STATUS_BIT) {	/* link up */
		duplex = (val & BIT(13)) >> YT_DUPLEX_BIT;
		speed_mode = (val & (BIT(15) | BIT(14))) >> YT_SPEED_MODE_BIT;
		switch (speed_mode) {
		case 0:
			speed = SPEED_10;
			break;
		case 1:
			speed = SPEED_100;
			break;
		default:
			speed = SPEED_10;
			break;
		}

		phydev->link = 1;
		phydev->speed = speed;
		phydev->duplex = duplex;

		val = phy_read(phydev, MDIO_DEVAD_NONE, MII_LPA);
		if (val < 0)
			return val;

		phydev->pause = val & LPA_PAUSE_CAP;
		phydev->asym_pause = val & LPA_PAUSE_ASYM;

		return 0;
	}

	phydev->link = 0;

	return 0;
}

static int yt8521_hw_strap_polling(struct phy_device *phydev)
{
	int val = 0;

	val = ytphy_read_ext(phydev, 0xa001) & 0x7;
	switch (val) {
	case 1:
	case 4:
	case 5:
		return PHY_MODE_FIBER;
	case 2:
	case 6:
	case 7:
		return PHY_MODE_POLL;
	case 3:
	case 0:
	default:
		return PHY_MODE_UTP;
	}
}

static int yt8521S_probe(struct phy_device *phydev)
{
	struct yt8xxx_priv *yt8521S;

	if (!phydev->priv) {
		yt8521S = kzalloc(sizeof(*yt8521S), GFP_KERNEL);
		if (!yt8521S)
			return -ENOMEM;	/* ref: net/phy/ti.c */

		phydev->priv = yt8521S;
		yt8521S->strap_polling = yt8521_hw_strap_polling(phydev);
	} else {
		yt8521S = (struct yt8xxx_priv *)phydev->priv;
	}

	return 0;
}

static int yt8521S_config(struct phy_device *phydev)
{
	int val, hw_strap_mode;
	int ret;

#if (YT_WOL_FEATURE_ENABLE)
	struct ethtool_wolinfo wol;

	/* set phy wol enable */
	memset(&wol, 0x0, sizeof(struct ethtool_wolinfo));
	wol.wolopts |= WAKE_MAGIC;
	yt_wol_feature_set(phydev, &wol);
#endif

	hw_strap_mode = ytphy_read_ext(phydev, 0xa001) & 0x7;
	printf("hw_strap_mode: 0x%x\n", hw_strap_mode);

	ytphy_write_ext(phydev, 0xa000, 0);

	/* disable auto sleep */
	val = ytphy_read_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1);
	if (val < 0)
		return val;

	val &= (~BIT(YT8521_EN_SLEEP_SW_BIT));

	ret = ytphy_write_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1, val);
	if (ret < 0)
		return ret;

	/* enable RXC clock when no wire plug */
	val = ytphy_read_ext(phydev, 0xc);
	if (val < 0)
		return val;
	val &= ~(1 << 12);
	ret = ytphy_write_ext(phydev, 0xc, val);
	if (ret < 0)
		return ret;

	genphy_config_aneg(phydev);

	ret = ytxxxx_soft_reset(phydev);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa000, 0x0);

	return ret;
}

/* for fiber mode, there is no 10M speed mode and
 * this function is for this purpose.
 */
static int yt8521_adjust_status(struct phy_device *phydev, int val, int is_utp)
{
	int speed_mode, duplex, speed;

	if (is_utp)
		duplex = (val & YT_DUPLEX) >> YT_DUPLEX_BIT;
	else
		duplex = 1;
	speed_mode = (val & YT_SPEED_MODE) >> YT_SPEED_MODE_BIT;
	switch (speed_mode) {
	case 2:
		speed = SPEED_1000;
		break;
	case 1:
		speed = SPEED_100;
		break;
	default:
		speed = SPEED_10;
		break;
	}

	if (!is_utp) {
		if (speed == SPEED_10)
			speed = SPEED_100;
	}

	phydev->speed = speed;
	phydev->duplex = duplex;

	return 0;
}

static int yt8521S_startup(struct phy_device *phydev)
{
	struct yt8xxx_priv *yt8521S = phydev->priv;
	int link_utp = 0, link_fiber = 0;
	int val_utp = 0, val_fiber = 0;
	int yt8521_fiber_latch_val;
	int yt8521_fiber_curr_val;
	int ret = 0;
	int link;

	msleep(1000);
	if (yt8521S->strap_polling != PHY_MODE_FIBER) {
		/* reading UTP */
		ret = ytphy_write_ext(phydev, 0xa000, 0);
		if (ret < 0)
			return ret;

		val_utp = phy_read(phydev, MDIO_DEVAD_NONE,
				   REG_PHY_SPEC_STATUS);
		if (val_utp < 0)
			return val_utp;

		link = val_utp & (BIT(YT_LINK_STATUS_BIT));
		if (link) {
			link_utp = 1;
			yt8521_adjust_status(phydev, val_utp, 1);
		} else {
			link_utp = 0;
		}
	}

	if (yt8521S->strap_polling != PHY_MODE_UTP) {
		/* reading Fiber */
		ret = ytphy_write_ext(phydev, 0xa000, 2);
		if (ret < 0)
			return ret;

		val_fiber = phy_read(phydev, MDIO_DEVAD_NONE,
				     REG_PHY_SPEC_STATUS);
		if (val_fiber < 0)
			return val_fiber;

		/* for fiber, from 1000m to 100m,
		 * there is not link down from 0x11,
		 * and check reg 1 to identify such case this is important
		 * for Linux kernel for that, missing linkdown event
		 * will cause problem.
		 */
		yt8521_fiber_latch_val = phy_read(phydev, MDIO_DEVAD_NONE,
						  MII_BMSR);
		yt8521_fiber_curr_val = phy_read(phydev, MDIO_DEVAD_NONE,
						 MII_BMSR);
		link = val_fiber & (BIT(YT_LINK_STATUS_BIT));
		if (link && yt8521_fiber_latch_val != yt8521_fiber_curr_val) {
			link = 0;
			printf("%s, phy addr: %d, fiber link down detect, latch = %04x, curr = %04x\n",
			       __func__, phydev->addr,
			       yt8521_fiber_latch_val,
			       yt8521_fiber_curr_val);
		}

		if (link) {
			link_fiber = 1;
			yt8521_adjust_status(phydev, val_fiber, 0);
		} else {
			link_fiber = 0;
		}
	}

	if (link_utp || link_fiber) {
		if (phydev->link == 0)
			printf("%s, phy addr: %d, link up, media: %s, mii reg 0x11 = 0x%x\n",
			       __func__, phydev->addr,
			       (link_utp && link_fiber) ? "UNKONWN MEDIA" :
			       (link_utp ? "UTP" : "Fiber"),
			       (link_utp ? (unsigned int)val_utp :
			       (unsigned int)val_fiber));
		phydev->link = 1;
	} else {
		if (phydev->link == 1)
			printf("%s, phy addr: %d, link down\n", __func__,
			       phydev->addr);
		phydev->link = 0;
	}

	if (yt8521S->strap_polling != PHY_MODE_FIBER) {
		if (link_fiber)
			ytphy_write_ext(phydev, 0xa000, 2);
		if (link_utp)
			ytphy_write_ext(phydev, 0xa000, 0);
	}

	return 0;
}

static int yt8531_config(struct phy_device *phydev)
{
	int ret = 0, val = 0;

#if (YT8531A_XTAL_INIT)
	ret = yt8531a_xtal_init(phydev);
	if (ret < 0)
		return ret;
#endif

#if (YT_WOL_FEATURE_ENABLE)
	struct ethtool_wolinfo wol;

	/* set phy wol enable */
	memset(&wol, 0x0, sizeof(struct ethtool_wolinfo));
	wol.wolopts |= WAKE_MAGIC;
	yt_wol_feature_set(phydev, &wol);
#endif

	val = ytphy_read_ext(phydev, 0xf);
	if(0x31 != val && 0x32 != val) {
		ret = ytphy_write_ext(phydev, 0x52, 0x231d);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(phydev, 0x51, 0x04a9);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(phydev, 0x57, 0x274c);
		if (ret < 0)
			return ret;

		if(0x500 == val) {
			val = ytphy_read_ext(phydev, 0xa001);
			if((0x30 == (val & 0x30)) || (0x20 == (val & 0x30))) {
				ret = ytphy_write_ext(phydev, 0xa010, 0xabff);
				if (ret < 0)
					return ret;
			}
		}
	}

	genphy_config_aneg(phydev);

	ytphy_soft_reset(phydev);

	return 0;
}

#ifdef YTPHY_YT8543_ENABLE
static int yt8543_config(struct phy_device *phydev)
{
	int ret = 0, val = 0;

#if (YT_WOL_FEATURE_ENABLE)
	struct ethtool_wolinfo wol;

	/* set phy wol enable */
	memset(&wol, 0x0, sizeof(struct ethtool_wolinfo));
	wol.wolopts |= WAKE_MAGIC;
	yt_wol_feature_set(phydev, &wol);
#endif

	ret = ytphy_write_ext(phydev, 0x403c, 0x286);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xdc, 0x855c);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xdd, 0x6040);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x40e, 0xf00);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x40f, 0xf00);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x411, 0x5030);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x1f, 0x110a);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20, 0xc06);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x40d, 0x1f);
	if (ret < 0)
		return ret;

	val = ytphy_read_ext(phydev, 0xa088);
	if (val < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa088,
			      ((val & 0xfff0) | BIT(4)) |
			      (((ytphy_read_ext(phydev, 0xa015) & 0x3c)) >> 2));
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa183, 0x1918);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa184, 0x1818);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa186, 0x2018);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa189, 0x3894);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa187, 0x3838);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa18b, 0x1918);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa18c, 0x1818);
	if (ret < 0)
		return ret;

	genphy_config_aneg(phydev);

	ytphy_soft_reset(phydev);

	return 0;
}
#endif

static int yt8531S_config(struct phy_device *phydev)
{
	int ret = 0, val = 0;

#if (YT8531A_XTAL_INIT)
	ret = yt8531a_xtal_init(phydev);
	if (ret < 0)
		return ret;
#endif

	ret = ytphy_write_ext(phydev, 0xa023, 0x4031); 
	if (ret < 0)
		return ret;   

	ytphy_write_ext(phydev, 0xa000, 0x0);
	val = ytphy_read_ext(phydev, 0xf);

	if (0x31 != val && 0x32 != val) {
		ret = ytphy_write_ext(phydev, 0xa071, 0x9007);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(phydev, 0x52, 0x231d);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(phydev, 0x51, 0x04a9);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(phydev, 0x57, 0x274c);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(phydev, 0xa006, 0x10d);
		if (ret < 0)
			return ret;

		if (0x500 == val) {
			val = ytphy_read_ext(phydev, 0xa001);
			if((0x30 == (val & 0x30)) || (0x20 == (val & 0x30))) {
				ret = ytphy_write_ext(phydev, 0xa010, 0xabff);
				if (ret < 0)
					return ret;
			}
		}
	}

	ret = yt8521S_config(phydev);

	if (ret < 0)
		return ret;

	ytphy_write_ext(phydev, 0xa000, 0x0);

	return 0;
}

static int yt8614_hw_strap_polling(struct phy_device *phydev)
{
	int val = 0;

	/* b3:0 are work mode, but b3 is always 1 */
	val = ytphy_read_ext(phydev, 0xa007) & 0xf;
	switch (val) {
	case 8:
	case 12:
	case 13:
	case 15:
		return (PHY_MODE_FIBER | PHY_MODE_UTP);
	case 14:
	case 11:
		return PHY_MODE_FIBER;
	case 9:
	case 10:
	default:
		return PHY_MODE_UTP;
	}
}

static int yt8614_probe(struct phy_device *phydev)
{
	struct yt8xxx_priv *yt8614;

	if (!phydev->priv) {
		yt8614 = kzalloc(sizeof(*yt8614), GFP_KERNEL);
		if (!yt8614)
			return -ENOMEM;

		phydev->priv = yt8614;
		yt8614->strap_polling = yt8614_hw_strap_polling(phydev);
	} else {
		yt8614 = (struct yt8xxx_priv *)phydev->priv;
	}

	return 0;
}

int yt8614_soft_reset(struct phy_device *phydev)
{
	int ret;

	/* qsgmii */
	ytphy_write_ext(phydev, 0xa000, 2);
	ret = ytphy_soft_reset(phydev);
	if (ret < 0) {
		ytphy_write_ext(phydev, 0xa000, 0);
		return ret;
	}

	/* sgmii */
	ytphy_write_ext(phydev, 0xa000, 3);
	ret = ytphy_soft_reset(phydev);
	if (ret < 0) {
		ytphy_write_ext(phydev, 0xa000, 0);
		return ret;
	}

	/* utp */
	ytphy_write_ext(phydev, 0xa000, 0);
	ret = ytphy_soft_reset(phydev);
	if (ret < 0)
		return ret;

	return 0;
}
#if (YT_861X_AB_VER)
static int yt8614_get_port_from_phydev(struct phy_device *phydev)
{
	int tmp = ytphy_read_ext(phydev, 0xa0ff);
	int phy_addr = (unsigned int)phydev->addr;

	if ((phy_addr - tmp) < 0) {
		ytphy_write_ext(phydev, 0xa0ff, phy_addr);
		tmp = phy_addr;
	}

	return (phy_addr - tmp);
}
#endif

static int yt8614_config(struct phy_device *phydev)
{
	unsigned int retries = 12;
	int val, hw_strap_mode;
#if (YT_861X_AB_VER)
	int port = 0;
#endif
	int ret = 0;

	/* NOTE: this function should not be called more than one for each chip. */
	hw_strap_mode = ytphy_read_ext(phydev, 0xa007) & 0xf;

#if (YT_861X_AB_VER)
	port = yt8614_get_port_from_phydev(phydev);
#endif

	ytphy_write_ext(phydev, 0xa000, 0);

	/* for utp to optimize signal */
	ret = ytphy_write_ext(phydev, 0x41, 0x33);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0x42, 0x66);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0x43, 0xaa);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0x44, 0xd0d);
	if (ret < 0)
		return ret;

#if (YT_861X_AB_VER)
	if (port == 2) {
		ret = ytphy_write_ext(phydev, 0x57, 0x2929);
		if (ret < 0)
			return ret;
	}
#endif

	/* soft reset to take config effect */
	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val | BMCR_RESET);
	do {
		msleep(50);
		ret = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
		if (ret < 0)
			return ret;
	} while ((ret & BMCR_RESET) && --retries);
	if (ret & BMCR_RESET)
		return -ETIMEDOUT;

	/* for QSGMII optimization */
	ytphy_write_ext(phydev, 0xa000, 0x02);
	ret = ytphy_write_ext(phydev, 0x3, 0x4F80);
	if (ret < 0) {
		ytphy_write_ext(phydev, 0xa000, 0);
		return ret;
	}
	ret = ytphy_write_ext(phydev, 0xe, 0x4F80);
	if (ret < 0) {
		ytphy_write_ext(phydev, 0xa000, 0);
		return ret;
	}

	/* for SGMII optimization */
	ytphy_write_ext(phydev, 0xa000, 0x03);
	ret = ytphy_write_ext(phydev, 0x3, 0x2420);
	if (ret < 0) {
		ytphy_write_ext(phydev, 0xa000, 0);
		return ret;
	}
	ret = ytphy_write_ext(phydev, 0xe, 0x24a0);
	if (ret < 0) {
		ytphy_write_ext(phydev, 0xa000, 0);
		return ret;
	}

	/* back up to utp*/
	ytphy_write_ext(phydev, 0xa000, 0);

	printf("%s done, phy addr: %d, chip mode: %d\n", __func__, phydev->addr,
	       hw_strap_mode);

	genphy_config_aneg(phydev);

	yt8614_soft_reset(phydev);

	return ret;
}

static int yt8614_startup(struct phy_device *phydev)
{
	int val, yt8614_fiber_latch_val, yt8614_fiber_curr_val;
	struct yt8xxx_priv *yt8614 = phydev->priv;
	int link_utp = 0, link_fiber = 0;
	int link;
	int ret;

	msleep(1000);

	if (yt8614->strap_polling & PHY_MODE_UTP) {
		/* switch to utp and reading regs  */
		ret = ytphy_write_ext(phydev, 0xa000, 0);
		if (ret < 0)
			return ret;

		val = phy_read(phydev, MDIO_DEVAD_NONE, REG_PHY_SPEC_STATUS);
		if (val < 0)
			return val;

		link = val & (BIT(YT_LINK_STATUS_BIT));
		if (link) {
			if (phydev->link == 0)
				printf("%s, phy addr: %d, link up, UTP, reg 0x11 = 0x%x\n",
				       __func__, phydev->addr,
				       (unsigned int)val);
			link_utp = 1;
			/* here is same as 8521 and re-use the function; */
			yt8521_adjust_status(phydev, val, 1);
		} else {
			link_utp = 0;
		}
	}

	if (yt8614->strap_polling & PHY_MODE_FIBER) {
		/* reading Fiber/sgmii */
		ret = ytphy_write_ext(phydev, 0xa000, 3);
		if (ret < 0)
			return ret;

		val = phy_read(phydev, MDIO_DEVAD_NONE, REG_PHY_SPEC_STATUS);
		if (val < 0)
			return val;

		/* for fiber, from 1000m to 100m, there is not link down from
		 * 0x11, and check reg 1 to identify such case
		 */
		yt8614_fiber_latch_val = phy_read(phydev, MDIO_DEVAD_NONE,
						  MII_BMSR);
		yt8614_fiber_curr_val = phy_read(phydev, MDIO_DEVAD_NONE,
						 MII_BMSR);
		link = val & (BIT(YT_LINK_STATUS_BIT));
		if (link && yt8614_fiber_latch_val != yt8614_fiber_curr_val) {
			link = 0;
			printf("%s, phy addr: %d, fiber link down detect, latch = %04x, curr = %04x\n",
			       __func__, phydev->addr, yt8614_fiber_latch_val,
			       yt8614_fiber_curr_val);
		}

		if (link) {
			if (phydev->link == 0)
				printf("%s, phy addr: %d, link up, Fiber, reg 0x11 = 0x%x\n",
				       __func__, phydev->addr,
				       (unsigned int)val);
			link_fiber = 1;
			yt8521_adjust_status(phydev, val, 0);
		} else {
			link_fiber = 0;
		}
	}

	if (link_utp || link_fiber) {
		if (phydev->link == 0)
			printf("%s, phy addr: %d, link up, media %s\n",
			       __func__, phydev->addr,
			       (link_utp && link_fiber) ? "both UTP and Fiber" :
			       (link_utp ? "UTP" : "Fiber"));
		phydev->link = 1;
	} else {
		if (phydev->link == 1)
			printf("%s, phy addr: %d, link down\n", __func__,
			       phydev->addr);
		phydev->link = 0;
	}

	if (yt8614->strap_polling & PHY_MODE_UTP) {
		if (link_utp)
			ytphy_write_ext(phydev, 0xa000, 0);
	}

	return 0;
}

int yt8618_soft_reset(struct phy_device *phydev)
{
	int ret;

	/* qsgmii */
	ytphy_write_ext(phydev, 0xa000, 2);
	ret = ytphy_soft_reset(phydev);
	if (ret < 0) {
		ytphy_write_ext(phydev, 0xa000, 0);
		return ret;
	}

	ytphy_write_ext(phydev, 0xa000, 0);
	ret = ytphy_soft_reset(phydev);
	if (ret < 0)
		return ret;

	return 0;
}

static int yt8618_config(struct phy_device *phydev)
{
	unsigned int retries = 12;
#if (YT_861X_AB_VER)
	int port = 0;
#endif
	int ret;
	int val;

#if (YT_861X_AB_VER)
	port = yt8614_get_port_from_phydev(phydev);
#endif

	ytphy_write_ext(phydev, 0xa000, 0);

	/* for utp to optimize signal */
	ret = ytphy_write_ext(phydev, 0x41, 0x33);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0x42, 0x66);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0x43, 0xaa);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0x44, 0xd0d);
	if (ret < 0)
		return ret;

#if (YT_861X_AB_VER)
	if ((port == 2) || (port == 5)) {
		ret = ytphy_write_ext(phydev, 0x57, 0x2929);
		if (ret < 0)
			return ret;
	}
#endif

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val | BMCR_RESET);
	do {
		msleep(50);
		ret = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
		if (ret < 0)
			return ret;
	} while ((ret & BMCR_RESET) && --retries);
	if (ret & BMCR_RESET)
		return -ETIMEDOUT;

	/* for QSGMII optimization */
	ytphy_write_ext(phydev, 0xa000, 0x02);

	ret = ytphy_write_ext(phydev, 0x3, 0x4F80);
	if (ret < 0) {
		ytphy_write_ext(phydev, 0xa000, 0);
		return ret;
	}
	ret = ytphy_write_ext(phydev, 0xe, 0x4F80);
	if (ret < 0) {
		ytphy_write_ext(phydev, 0xa000, 0);
		return ret;
	}

	ytphy_write_ext(phydev, 0xa000, 0);

	printf("%s done, phy addr: %d\n", __func__, phydev->addr);

	genphy_config_aneg(phydev);

	yt8618_soft_reset(phydev);

	return ret;
}

static int yt8618_startup(struct phy_device *phydev)
{
	int link_utp = 0;
	int link;
	int ret;
	int val;

	msleep(1000);

	/* switch to utp and reading regs  */
	ret = ytphy_write_ext(phydev, 0xa000, 0);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, REG_PHY_SPEC_STATUS);
	if (val < 0)
		return val;

	link = val & (BIT(YT_LINK_STATUS_BIT));
	if (link) {
		link_utp = 1;
		yt8521_adjust_status(phydev, val, 1);
	} else {
		link_utp = 0;
	}

	if (link_utp)
		phydev->link = 1;
	else
		phydev->link = 0;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_LPA);
	if (val < 0)
		return val;

	phydev->pause = val & LPA_PAUSE_CAP;
	phydev->asym_pause = val & LPA_PAUSE_ASYM;

	return 0;
}

static int yt8824_startup(struct phy_device *phydev)
{
	int ret;

	msleep(1000);
	
	ret = ytphy_top_write_ext(phydev, YT_REG_SMI_MUX,
				  YT8824_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	genphy_update_link(phydev);
	yt8821_parse_status(phydev);

	return 0;
}

static int yt8824_shutdown(struct phy_device *phydev)
{
	int ret;
	
	ret = ytphy_top_write_ext(phydev, YT_REG_SMI_MUX,
				  YT8824_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	genphy_shutdown(phydev);

	return 0;
}

static int yt8821_init(struct phy_device *phydev)
{
	int ret = 0;

	ret = ytphy_write_ext(phydev, 0xa000, 0x2);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x23, 0x8605);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa000, 0x0);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x34e, 0x8080);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x4d2, 0x5200);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x4d3, 0x5200);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x372, 0x5a3c);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x374, 0x7c6c);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x336, 0xaa0a);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x340, 0x3022);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x36a, 0x8000);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x4b3, 0x7711);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x4b5, 0x2211);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x56, 0x20);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x56, 0x3f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x97, 0x380c);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x660, 0x112a);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x450, 0xe9);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x466, 0x6464);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x467, 0x6464);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x468, 0x6464);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x469, 0x6464);
	if (ret < 0)
		return ret;

	return ret;
}

#define YT8821_CHIP_MODE_AUTO_BX2500_SGMII	(1)
#define YT8821_CHIP_MODE_FORCE_BX2500		(0)
#define YT8821_CHIP_MODE_UTP_TO_FIBER_FORCE	(0)
static int yt8821_config(struct phy_device *phydev)
{
	int ret;
	int val;

	val = ytphy_read_ext(phydev, 0xa001);
/* ext reg 0xa001 bit2:0 3'b000 */
#if (YT8821_CHIP_MODE_AUTO_BX2500_SGMII)
	val &= ~(BIT(0));
	val &= ~(BIT(1));
	val &= ~(BIT(2));
	ret = ytphy_write_ext(phydev, 0xa001, val);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa000, 2);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	val |= BIT(12);
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val);

	ret = ytphy_write_ext(phydev, 0xa000, 0x0);
	if (ret < 0)
		return ret;
/* ext reg 0xa001 bit2:0 3'b001 */
#elif (YT8821_CHIP_MODE_FORCE_BX2500)
	val |= BIT(0);
	val &= ~(BIT(1));
	val &= ~(BIT(2));
	ret = ytphy_write_ext(phydev, 0xa001, val);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa000, 0x0);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE);
	val |= 1 << 10;
	val |= 1 << 11;
	phy_write(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE, val);

	ret = ytphy_write_ext(phydev, 0xa000, 2);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE);
	val |= 1 << 7;
	val |= 1 << 8;
	phy_write(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE, val);

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	val &= ~(BIT(12));
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val);

	ret = ytphy_write_ext(phydev, 0xa000, 0x0);
	if (ret < 0)
		return ret;
/* ext reg 0xa001 bit2:0 3'b101 */
#elif (YT8821_CHIP_MODE_UTP_TO_FIBER_FORCE)
	val |= BIT(0);
	val &= ~(BIT(1));
	val |= BIT(2);
	ret = ytphy_write_ext(phydev, 0xa001, val);
	if (ret < 0)
		return ret;
#endif

#if (YT_WOL_FEATURE_ENABLE)
	struct ethtool_wolinfo wol;

	/* set phy wol enable */
	memset(&wol, 0x0, sizeof(struct ethtool_wolinfo));
	wol.wolopts |= WAKE_MAGIC;
	yt_wol_feature_set(phydev, &wol);
#endif

	ret = yt8821_init(phydev);
	if (ret < 0)
		return ret;

	/* disable auto sleep */
	val = ytphy_read_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1);
	if (val < 0)
		return val;

	val &= (~BIT(YT8521_EN_SLEEP_SW_BIT));

	ret = ytphy_write_ext(phydev, YT8521_EXTREG_SLEEP_CONTROL1, val);
	if (ret < 0)
		return ret;

	genphy_config_aneg(phydev);

	/* soft reset whole chip */
	ret = ytxxxx_soft_reset(phydev);

	printf("%s done, phy addr: %d\n", __func__, phydev->addr);

	return ret;
}

static int yt8821_startup(struct phy_device *phydev)
{
	msleep(1000);
	genphy_update_link(phydev);
	yt8821_parse_status(phydev);

	return 0;
}

#define YT8831_PHYMODE_SEL		0x8802
#define YT8831_REG_SMI_MUX		0x8020
#define YT8831_REG_SPACE_UTP		1
#define YT8831_REG_SPACE_SDS		2

static int yt8831_soft_reset(struct phy_device *phydev)
{
	int ret;

	/* sds */
	ret = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_SDS);
	if (ret < 0)
		return ret;

	ret = ytphy_soft_reset(phydev);
	if (ret < 0) {
		ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8628_REG_SPACE_UTP);
		return ret;
	}

	/* utp */
	ret = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	return ytphy_soft_reset(phydev);
}

static int yt8831_init(struct phy_device *phydev)
{
	int ret = 0;

	ret = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x1, 0x3);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x0, 0xc8d);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x4, 0x8009);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x723, 0x6);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x727, 0x1001);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x339, 0x7c52);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xc3, 0x3848);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x32, 0xa8e8);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xff, 0x5200);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x372, 0x5849);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x336, 0xb00a);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x340, 0x3022);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x1e58, 0x1f10);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x209e, 0x22c8);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20a4, 0x2437);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20a9, 0x22c8);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20af, 0x2437);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20b4, 0x22c8);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20ba, 0x2437);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20bf, 0x22c8);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20c5, 0x2437);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20ef, 0x3f3f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20f0, 0x3f3f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x20ff, 0x1111);
	if (ret < 0)
		return ret;

	return ytphy_write_ext(phydev, 0x2100, 0x1111);
}

#define YT8831_CHIP_MODE_AUTO_BX2500_SGMII	(1)
#define YT8831_CHIP_MODE_FORCE_BX2500		(0)
#define YT8831_CHIP_MODE_UTP_TO_FIBER_FORCE	(0)
static int yt8831_config(struct phy_device *phydev)
{
	int ret;
	int val;

	val = ytphy_read_ext(phydev, YT8831_PHYMODE_SEL);
/* ext reg YT8831_PHYMODE_SEL bit2:0 3'b000 */
#if (YT8831_CHIP_MODE_AUTO_BX2500_SGMII)
	val &= ~(BIT(0));
	val &= ~(BIT(1));
	val &= ~(BIT(2));
	ret = ytphy_write_ext(phydev, YT8831_PHYMODE_SEL, val);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_SDS);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	val |= BIT(12);
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val);

	ret = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_UTP);
	if (ret < 0)
		return ret;
/* ext reg YT8831_PHYMODE_SEL bit2:0 3'b001 */
#elif (YT8831_CHIP_MODE_FORCE_BX2500)
	val |= BIT(0);
	val &= ~(BIT(1));
	val &= ~(BIT(2));
	ret = ytphy_write_ext(phydev, YT8831_PHYMODE_SEL, val);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE);
	val |= 1 << 10;
	val |= 1 << 11;
	phy_write(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE, val);

	ret = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_SDS);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE);
	val |= 1 << 7;
	val |= 1 << 8;
	phy_write(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE, val);

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	val &= ~(BIT(12));
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val);

	ret = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_UTP);
	if (ret < 0)
		return ret;
/* ext reg YT8831_PHYMODE_SEL bit2:0 3'b101 */
#elif (YT8831_CHIP_MODE_UTP_TO_FIBER_FORCE)
	val |= BIT(0);
	val &= ~(BIT(1));
	val |= BIT(2);
	ret = ytphy_write_ext(phydev, YT8831_PHYMODE_SEL, val);
	if (ret < 0)
		return ret;
#endif

	ret = yt8831_init(phydev);
	if (ret < 0)
		return ret;

	genphy_config_aneg(phydev);

	/* soft reset whole chip */
	ret = yt8831_soft_reset(phydev);

	printf("%s done, phy addr: %d\n", __func__, phydev->addr);

	return ret;
}

static int yt8831_parse_status(struct phy_device *phydev)
{
	int speed_mode_bit15_14, speed_mode_bit9;
	int speed, speed_mode, duplex;
	int val;

	val = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_UTP);
	if (val < 0)
		return val;

	val = phy_read(phydev, MDIO_DEVAD_NONE, YT_SPEC_STATUS);
	if (val < 0)
		return val;

	duplex = (val & YT_DUPLEX) >> YT_DUPLEX_BIT;

	/* Bit9-Bit15-Bit14 speed mode 100---2.5G; 010---1000M;
	 * 001---100M; 000---10M
	 */
	speed_mode_bit15_14 = (val & YT_SPEED_MODE) >> YT_SPEED_MODE_BIT;
	speed_mode_bit9 = (val & BIT(9)) >> 9;
	speed_mode = (speed_mode_bit9 << 2) | speed_mode_bit15_14;

	switch (speed_mode) {
	case 4:
		speed = SPEED_2500;
		break;
	case 2:
		speed = SPEED_1000;
		break;
	case 1:
		speed = SPEED_100;
		break;
	default:
		speed = SPEED_10;
		break;
	}

	phydev->speed = speed;
	phydev->duplex = duplex;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_LPA);
	if (val < 0)
		return val;

	phydev->pause = val & LPA_PAUSE_CAP;
	phydev->asym_pause = val & LPA_PAUSE_ASYM;

	return 0;
}

static int yt8831_startup(struct phy_device *phydev)
{
	int val;

	msleep(1000);

	val = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_UTP);
	if (val < 0)
		return val;
	genphy_update_link(phydev);
	yt8831_parse_status(phydev);

	return 0;
}

static int yt8831_shutdown(struct phy_device *phydev)
{
	int ret;

	ret = ytphy_write_ext(phydev, YT8831_REG_SMI_MUX, YT8831_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	genphy_shutdown(phydev);

	return 0;
}

static int yt8111_config(struct phy_device *phydev)
{
	int ret;
	
	ret = ytphy_write_ext(phydev, 0x221, 0x1f1f);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0x2506, 0x3f5);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0x510, 0x64f0);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x511, 0x70f0);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x512, 0x78f0);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0x507, 0xff80);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa401, 0xa04);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa400, 0xa04);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa108, 0x300);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa109, 0x800);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa304, 0x4);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa301, 0x810);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0x500, 0x2f);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa206, 0x1500);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa203, 0x1414);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa208, 0x1515);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa209, 0x1714);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa20b, 0x2d04);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0xa20c, 0x1500);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0x522, 0xfff);
	if (ret < 0)
		return ret;
	
	ret = ytphy_write_ext(phydev, 0x403, 0xff4);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa51f, 0x1070);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0x51a, 0x6f0);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0xa403, 0x1c00);
	if (ret < 0)
		return ret;
	ret = ytphy_write_ext(phydev, 0x506, 0xffe0);
	if (ret < 0)
		return ret;

	/* soft reset */
	ytphy_soft_reset(phydev);

	printf("%s done, phy addr: %d\n", __func__, phydev->addr);

	return ret;
}

static int yt8111_startup(struct phy_device *phydev)
{
	int val;

	msleep(1000);

	val = phy_read(phydev, MDIO_DEVAD_NONE, YT_SPEC_STATUS);
	if (val < 0)
		return val;

	phydev->link = (val & BIT(10)) > 0 ? 1 : 0;
	phydev->speed = SPEED_10;
	phydev->duplex = 1;

	return 0;
}

static int yt8628_probe(struct phy_device *phydev)
{
	struct yt8xxx_priv *priv;
	int chip_config;

	if (!phydev->priv) {
		priv = kzalloc(sizeof(*priv), GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		phydev->priv = priv;

		chip_config = ytphy_read_ext(phydev, YT8628_CHIP_MODE_REG);
		if (chip_config < 0)
			return chip_config;

		priv->chip_mode = (chip_config & YT8628_CHIP_MODE) >>
					YT8628_CHIP_MODE_OFFSET;

		priv->phy_base_addr = chip_config & YT8628_CHIP_MODE_BASE_ADDR;
		priv->data = (~ytphy_read_ext(phydev, 0xa032)) & 0x003f;
	} else {
		priv = (struct yt8xxx_priv *)phydev->priv;
	}

	return 0;
}

static int yt8821_probe(struct phy_device *phydev)
{
	phydev->advertising = PHY_GBIT_FEATURES |
				SUPPORTED_2500baseX_Full |
				SUPPORTED_Pause |
				SUPPORTED_Asym_Pause;
	phydev->supported = phydev->advertising;

	return 0;
}

/* config YT8824 base addr according to Power On Strapping in dts
 * eg,
 * ethernet-phy@4 {
 * 	...
 * 	motorcomm,base-address = <1>;
 * 	...
 * };
 * or
 * #define PHY_BASE_ADDR (1)
 */
#define PHY_BASE_ADDR (1)
static int yt8824_probe(struct phy_device *phydev)
{
	struct yt8xxx_priv *priv;

	if (!phydev->priv) {
		priv = kzalloc(sizeof(*priv), GFP_KERNEL);
		if (!priv)
			return -ENOMEM;	/* ref: net/phy/ti.c */

		phydev->priv = priv;
		priv->phy_base_addr = PHY_BASE_ADDR;

#if (defined(U_BOOT_VERSION_NUM) && defined(U_BOOT_VERSION_NUM_PATCH) && ((U_BOOT_VERSION_NUM * 10000 + U_BOOT_VERSION_NUM_PATCH) >= (2023 * 10000 + 4)))
		priv->phy_base_addr =
			ofnode_read_u32_default(phydev->node,
						"motorcomm,base-address",
						PHY_BASE_ADDR);
#endif
		priv->top_phy_addr = priv->phy_base_addr + 4;
	} else {
		priv = (struct yt8xxx_priv *)phydev->priv;
	}

	phydev->advertising = PHY_GBIT_FEATURES |
				SUPPORTED_2500baseX_Full |
				SUPPORTED_Pause |
				SUPPORTED_Asym_Pause;
	phydev->supported = phydev->advertising;

	return 0;
}

static int yt8628_utp_soft_reset(struct phy_device *phydev)
{
	int ret = 0, val = 0;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_CTRL1000);
	if (val < 0)
		return val;
	
	val = phy_write(phydev, MDIO_DEVAD_NONE, MII_CTRL1000,
		val | (BIT(15) | BIT(14) | BIT(13)));
	if (val < 0)
		return val;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	if (val < 0)
		return val;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val | BMCR_RESET);
	if (ret < 0)
		return ret;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_CTRL1000);
	if (val < 0)
		return val;
	
	val = phy_write(phydev, MDIO_DEVAD_NONE, MII_CTRL1000,
		val & (~(BIT(15) | BIT(14) | BIT(13))));
	if (val < 0)
		return val;

	return ret;
}

static int yt8628_soft_reset(struct phy_device *phydev)
{
	int ret;

	/* OUSGMII */
	ytphy_write_ext(phydev, YT_REG_SMI_MUX,
			YT8628_REG_SPACE_QSGMII_OUSGMII);
	ret = ytphy_soft_reset(phydev);
	if (ret < 0) {
		ytphy_write_ext(phydev, YT_REG_SMI_MUX, YT8628_REG_SPACE_UTP);
		return ret;
	}

	/* utp */
	ytphy_write_ext(phydev, YT_REG_SMI_MUX, YT8628_REG_SPACE_UTP);
	return yt8628_utp_soft_reset(phydev);
}

static int yt8628_config_init_paged(struct phy_device *phydev, int reg_space)
{
	struct yt8xxx_priv *priv = phydev->priv;
	u16 cali_out;
	int port;
	u16 temp;
	int ret;

	port = phydev->addr - priv->phy_base_addr;
	cali_out = priv->data;

	if (reg_space == YT8628_REG_SPACE_QSGMII_OUSGMII) {
		ret = ytphy_write_ext(phydev, YT_REG_SMI_MUX,
				      YT8628_REG_SPACE_QSGMII_OUSGMII);
		if (ret < 0)
			return ret;

		if (port == 0) {
			ret = ytphy_read_ext(phydev, 0x1872);
			if (ret < 0)
				return ret;
			ret &= (~(BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8)));
			ret |= (cali_out << 8);
			ret |= BIT(15);
			ret = ytphy_write_ext(phydev, 0x1872, ret);
			if (ret < 0)
				return ret;

			ret = ytphy_read_ext(phydev, 0x1871);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1871, ret | BIT(1));
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1865, 0xa73);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1820, 0xa0b);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1860, 0x40c8);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1860, 0x4048);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1873, 0x4844);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1860, 0x40c8);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1860, 0x4048);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x0028, 0x0cc0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x002a, 0x0cc8);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x0040, 0x00fe);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x0005, 0x0838);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x0005, 0x8838);
			if (ret < 0)
				return ret;
		} else if (port == 4) {
			ret = ytphy_read_ext(phydev, 0x1872);
			if (ret < 0)
				return ret;
			ret &= (~(BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8)));
			ret |= (cali_out << 8);
			ret |= BIT(15);
			ret = ytphy_write_ext(phydev, 0x1872, ret);
			if (ret < 0)
				return ret;

			ret = ytphy_read_ext(phydev, 0x1871);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1871, ret | BIT(1));
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1873, 0x4844);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1860, 0x40c8);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1860, 0x4048);
			if (ret < 0)
				return ret;
		}
	} else if (reg_space == YT8628_REG_SPACE_UTP) {
		ret = ytphy_write_ext(phydev, YT_REG_SMI_MUX,
				      YT8628_REG_SPACE_UTP);
		if (ret < 0)
			return ret;
		
		ret = ytphy_write_ext(phydev, 0x50c, 0x8787);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x50d, 0x8787);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x506, 0xc3c3);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x507, 0xc3c3);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x41, 0x4a50);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x42, 0x0da7);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x43, 0x7fff);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x508, 0x12);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x528, 0x8000);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x47, 0x2f);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x500, 0x8080);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x501, 0x8080);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x4035, 0x1007);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x20, 0xa05);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0xde, 0x5050);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x409d, 0x0000);
		if (ret < 0)
			return ret;
		temp = ytphy_read_mmd(phydev, 0x3, 0x0);
		if (temp < 0)
			return ret;
		ret = ytphy_write_mmd(phydev, 0x3, 0x0, temp | BIT(10));
		if (ret < 0)
			return ret;
		ret = ytphy_write_mmd(phydev, 0x3, 0x0, temp & (~BIT(10)));
		if (ret < 0)
			return ret;
		ret = ytphy_read_ext(phydev, 0x00dc);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x00dc, ret & (~BIT(7)));
		if (ret < 0)
			return ret;
		ret = ytphy_read_ext(phydev, 0x0307);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x0307, ret | BIT(9));
		if (ret < 0)
			return ret;

		if (port == 0) {
			ret = ytphy_write_ext(phydev, 0x1228, 0x0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1042, 0xf);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1004, 0x12);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x101b, 0x2fff);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x101b, 0x3fff);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1201 + port * 0x2d, 0x634a);
			if (ret < 0)
				return ret;
		} else if (port == 1) {
			ret = ytphy_write_ext(phydev, 0x1255, 0x0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1201 + port * 0x2d, 0x634a);
			if (ret < 0)
				return ret;
		} else if (port == 2) {
			ret = ytphy_write_ext(phydev, 0x1282, 0x0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1201 + port * 0x2d, 0x634a);
			if (ret < 0)
				return ret;
		} else if (port == 3) {
			ret = ytphy_write_ext(phydev, 0x12af, 0x0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x1201 + port * 0x2d, 0x634a);
			if (ret < 0)
				return ret;
		} else if (port == 4) {
			ret = ytphy_write_ext(phydev, 0x2228, 0x0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x2042, 0xf);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x2004, 0x12);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x201b, 0x2fff);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x201b, 0x3fff);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x2201 + (port - 4) * 0x2d, 0x634a);
			if (ret < 0)
				return ret;
		} else if (port == 5) {
			ret = ytphy_write_ext(phydev, 0x2255, 0x0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x2201 + (port - 4) * 0x2d, 0x634a);
			if (ret < 0)
				return ret;
		} else if (port == 6) {
			ret = ytphy_write_ext(phydev, 0x2282, 0x0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x2201 + (port - 4) * 0x2d, 0x634a);
			if (ret < 0)
				return ret;
		} else if (port == 7) { 
			ret = ytphy_write_ext(phydev, 0x22af, 0x0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x2201 + (port - 4) * 0x2d, 0x634a);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int yt8824_config_paged(struct phy_device *phydev, int reg_space)
{
	struct yt8xxx_priv *priv = phydev->priv;
	u16 var_A, var_B;
	int port;
	int ret;
	
	ret = ytphy_top_write_ext(phydev, YT_REG_SMI_MUX, reg_space);
	if (ret < 0)
		return ret;
	
	port = phydev->addr - priv->phy_base_addr;

	if (reg_space == YT8824_REG_SPACE_SERDES) {
		if (port == 0) {
			/* Serdes optimization */
			ret = ytphy_write_ext(phydev, 0x4be, 0x5);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x406, 0x800);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x48e, 0x3900);
			if (ret < 0)
				return ret;

			/* pll calibration */
			ret = ytphy_write_ext(phydev, 0x400, 0x70);
			if (ret < 0)
				return ret;
			/* expect bit3 1'b1 */
			ret = ytphy_read_ext(phydev, 0x4bc);
			if (ret < 0)
				return ret;
			ret = var_B = ytphy_read_ext(phydev, 0x57);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x449, 0x73e0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x4ca, 0x330);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x444, 0x8);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x4c5, 0x6409);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x4cc, 0x1100);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x4c1, 0x664);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x442, 0x9881);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x44b, 0x11);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x442, 0x9885);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x442, 0x988d);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x44b, 0x13);
			if (ret < 0)
				return ret;
			/* expect bit3 1'b1 */
			ret = ytphy_read_ext(phydev, 0x4bc);
			if (ret < 0)
				return ret;
			ret = var_A = ytphy_read_ext(phydev, 0x57);
			if (ret < 0)
				return ret;

			/* config band val(var_A, var_B) calibrated */
			ret = ytphy_write_ext(phydev, 0x436, var_A);
			if (ret < 0)
				return ret;
			/* sds_ext_0x4cc[8:0] var_B */
			ret = ytphy_read_ext(phydev, 0x4cc);
			if (ret < 0)
				return ret;
			ret = (ret & 0xfe00) | (var_B & 0x01ff);
			ret = ytphy_write_ext(phydev, 0x4cc, ret);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x4c1, 0x264);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x442, 0x9885);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x442, 0x988d);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x4c1, 0x64);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x400, 0x70);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x49e, 0x200);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x49e, 0x0);
			if (ret < 0)
				return ret;
			ret = ytphy_write_ext(phydev, 0x38, 0x1777);
			if (ret < 0)
				return ret;

			/* read pll lock state 3 times with interval 1s
			 * last val is 0x4777 expected.
			 */
			msleep(1000);
			ret = ytphy_read_ext(phydev, 0x38);
			if (ret < 0)
				return ret;
			msleep(1000);
			ret = ytphy_read_ext(phydev, 0x38);
			if (ret < 0)
				return ret;
			msleep(1000);
			ret = ytphy_read_ext(phydev, 0x38);
			if (ret < 0)
				return ret;
			if (ret != 0x4777)
				return ret;

			/* configuration for fib chip(support 2.5G)
			 * ret = ytphy_write_ext(phydev, 0xa086, 0x8139);
			 * if (ret < 0)
			 * 	return ret;
			 */
		}

	} else if (reg_space == YT8824_REG_SPACE_UTP) {
		if (port == 3) {
			ret = ytphy_write_ext(phydev, 0xa218, 0x6e);
			if (ret < 0)
				return ret;
		}

		ret = ytphy_write_ext(phydev, 0x1, 0x3);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0xa291, 0xffff);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0xa292, 0xffff);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x4e2, 0x16c);
		if (ret < 0)
			return ret;

		/* 100M template optimization */
		ret = ytphy_write_ext(phydev, 0x46e, 0x4545);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x46f, 0x4545);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x470, 0x4545);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x471, 0x4545);
		if (ret < 0)
			return ret;

		/* 10M template optimization */
		ret = ytphy_write_ext(phydev, 0x46b, 0x1818);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x46c, 0x1818);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0xa2db, 0x7737);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0xa2dc, 0x3737);
		if (ret < 0)
			return ret;

		/* link signal optimization */
		ret = ytphy_write_ext(phydev, 0xa003, 0x3);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x34a, 0xff03);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0xf8, 0xb3ff);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x32c, 0x5094);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x32d, 0xd094);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x322, 0x6440);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x4d3, 0x5220);
		if (ret < 0)
			return ret;
		ret = ytphy_write_ext(phydev, 0x4d2, 0x5220);
		if (ret < 0)
			return ret;
		/* ret = phy_write(phydev, 0x0, 0x9000);
		 * if (ret < 0)
		 * 	return ret;
		 */
	}

	return ret;
}

static int yt8628_config(struct phy_device *phydev)
{
	struct yt8xxx_priv *priv = phydev->priv;
	int ret;

	ret = ytphy_write_ext(phydev, 0xa005, 0x4080);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, 0xa00f, 0xffff);
	if (ret < 0)
		return ret;

	ret = yt8628_config_init_paged(phydev,
				       YT8628_REG_SPACE_QSGMII_OUSGMII);
	if (ret < 0)
		return ret;

	ret = yt8628_config_init_paged(phydev, YT8628_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	printf("%s done, phy addr: %d, chip mode: %d, base addr: %d\n",
	       __func__, phydev->addr, priv->chip_mode, priv->phy_base_addr);

	genphy_config_aneg(phydev);

	return yt8628_soft_reset(phydev);
}

static int yt8824_soft_reset_paged(struct phy_device *phydev, int reg_space)
{
	int ret;
	
	ret = ytphy_top_write_ext(phydev, YT_REG_SMI_MUX, reg_space);
	if (ret < 0)
		return ret;
	
	return ytphy_soft_reset(phydev);
}

static int yt8824_soft_reset(struct phy_device *phydev)
{
	int ret;

	ret = yt8824_soft_reset_paged(phydev, YT8824_REG_SPACE_UTP);
	if (ret < 0) 
		return ret;

	ret = yt8824_soft_reset_paged(phydev, YT8824_REG_SPACE_SERDES);
	if (ret < 0)
		return ret;

	return 0;
}

static int yt8824_config(struct phy_device *phydev)
{
	int ret;

	ret = yt8824_config_paged(phydev, YT8824_REG_SPACE_SERDES);
	if (ret < 0)
		return ret;

	ret = yt8824_config_paged(phydev, YT8824_REG_SPACE_UTP);
	if (ret < 0)
		return ret;

	ret = yt8824_soft_reset(phydev);
	if (ret < 0)
		return ret;

	return 0;
}

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8510_driver) = {
#else
static struct phy_driver YT8510_driver = {
#endif
	.name		= "8510 100/10Mb Ethernet",
	.uid		= PHY_ID_YT8510,
	.mask		= PHY_ID_MASK,
	.features	= PHY_BASIC_FEATURES,
	.config		= &yt8510_config,
	.startup	= &yt_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8511_driver) = {
#else
static struct phy_driver YT8511_driver = {
#endif
	.name		= "YT8511 Gigabit Ethernet",
	.uid		= PHY_ID_YT8511,
	.mask		= PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.config		= &yt8511_config,
	.startup	= &yt_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8512_driver) = {
#else
static struct phy_driver YT8512_driver = {
#endif
	.name		= "YT8512 100/10Mb Ethernet",
	.uid		= PHY_ID_YT8512,
	.mask		= PHY_ID_MASK,
	.features	= PHY_BASIC_FEATURES,
	.probe		= &yt8512_probe,
	.config		= &yt8512_config,
	.startup	= &yt_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8522_driver) = {
#else
static struct phy_driver YT8522_driver = {
#endif
	.name		= "YT8522 100/10Mb Ethernet",
	.uid		= PHY_ID_YT8522,
	.mask		= PHY_ID_MASK,
	.features	= PHY_BASIC_FEATURES,
	.probe		= &yt8522_probe,
	.config		= &yt8522_config,
	.startup	= &yt8522_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8512B_driver) = {
#else
static struct phy_driver YT8512B_driver = {
#endif
	.name		= "YT8512B 100/10Mb Ethernet",
	.uid		= PHY_ID_YT8512B,
	.mask		= PHY_ID_MASK,
	.features	= PHY_BASIC_FEATURES,
	.probe		= &yt8512_probe,
	.config		= &yt8512_config,
	.startup	= &yt_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8521S_driver) = {
#else
static struct phy_driver YT8521S_driver = {
#endif
	.name		= "YT8521S Gigabit Ethernet",
	.uid		= PHY_ID_YT8521S,
	.mask		= PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.probe		= &yt8521S_probe,
	.config		= &yt8521S_config,
	.startup	= &yt8521S_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8531_driver) = {
#else
static struct phy_driver YT8531_driver = {
#endif
	.name		= "YT8531 Gigabit Ethernet",
	.uid		= PHY_ID_YT8531,
	.mask		= PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.config		= &yt8531_config,
	.startup	= &yt_startup,
	.shutdown	= &genphy_shutdown,
};

#ifdef YTPHY_YT8543_ENABLE
#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8543_driver) = {
#else
static struct phy_driver YT8543_driver = {
#endif
	.name		= "YT8543 dual port Gigabit Ethernet",
	.uid		= PHY_ID_YT8543,
	.mask		= PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.config		= &yt8543_config,
	.startup	= &yt_startup,
	.shutdown	= &genphy_shutdown,
};
#endif

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8531S_driver) = {
#else
static struct phy_driver YT8531S_driver = {
#endif
	.name		= "YT8531S Gigabit Ethernet",
	.uid		= PHY_ID_YT8531S,
	.mask		= PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.probe		= &yt8521S_probe,
	.config		= &yt8531S_config,
	.startup	= &yt8521S_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8614_driver) = {
#else
static struct phy_driver YT8614_driver = {
#endif
	.name		= "YT8614 Gigabit Ethernet",
	.uid		= PHY_ID_YT8614,
	.mask		= PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.probe		= &yt8614_probe,
	.config		= &yt8614_config,
	.startup	= &yt8614_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8618_driver) = {
#else
static struct phy_driver YT8618_driver = {
#endif
	.name		= "YT8618 Gigabit Ethernet",
	.uid		= PHY_ID_YT8618,
	.mask		= PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.config		= &yt8618_config,
	.startup	= &yt8618_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8821_driver) = {
#else
static struct phy_driver YT8821_driver = {
#endif
	.name		= "YT8821 2.5Gb Ethernet",
	.uid		= PHY_ID_YT8821,
	.mask		= PHY_ID_MASK,
	.probe		= &yt8821_probe,
	.config		= &yt8821_config,
	.startup	= &yt8821_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8831_driver) = {
#else
static struct phy_driver YT8831_driver = {
#endif
	.name		= "YT8831 2.5Gb Ethernet",
	.uid		= PHY_ID_YT8831,
	.mask		= PHY_ID_MASK,
	.probe		= &yt8821_probe,
	.config		= &yt8831_config,
	.startup	= &yt8831_startup,
	.shutdown	= &yt8831_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8111_driver) = {
#else
static struct phy_driver YT8111_driver = {
#endif
	.name		= "YT8111 10base-T1L Ethernet",
	.uid		= PHY_ID_YT8111,
	.mask		= PHY_ID_MASK,
	.features	= PHY_10BT_FEATURES,
	.config		= &yt8111_config,
	.startup	= &yt8111_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8628_driver) = {
#else
static struct phy_driver YT8628_driver = {
#endif
	.name		= "YT8628 Octal Ports Gigabit Ethernet",
	.uid		= PHY_ID_YT8628,
	.mask		= PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.probe		= &yt8628_probe,
	.config		= &yt8628_config,
	.startup	= &yt8618_startup,
	.shutdown	= &genphy_shutdown,
};

#if defined(U_BOOT_PHY_DRIVER_MACRO)
U_BOOT_PHY_DRIVER(YT8824_driver) = {
#else
static struct phy_driver YT8824_driver = {
#endif
	.name		= "YT8824 Quad Ports 2.5Gbps Ethernet",
	.uid		= PHY_ID_YT8824,
	.mask		= PHY_ID_MASK,
	.probe		= &yt8824_probe,
	.config		= &yt8824_config,
	.startup	= &yt8824_startup,
	.shutdown	= &yt8824_shutdown,
};

#if !defined(U_BOOT_PHY_DRIVER_MACRO)
int phy_motorcomm_init(void)
{
	phy_register(&YT8510_driver);
	phy_register(&YT8511_driver);
	phy_register(&YT8512_driver);
	phy_register(&YT8512B_driver);
	phy_register(&YT8522_driver);
	phy_register(&YT8521S_driver);
	phy_register(&YT8531_driver);
#ifdef YTPHY_YT8543_ENABLE
	phy_register(&YT8543_driver);
#endif
	phy_register(&YT8531S_driver);
	phy_register(&YT8614_driver);
	phy_register(&YT8618_driver);
	phy_register(&YT8821_driver);
	phy_register(&YT8831_driver);
	phy_register(&YT8111_driver);
	phy_register(&YT8628_driver);
	phy_register(&YT8824_driver);

	return 0;
}
#endif

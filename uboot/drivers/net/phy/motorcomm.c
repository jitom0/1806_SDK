// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Motorcomm YT8522/YT8522C PHYs
 */

#include <common.h>
#include <phy.h>
#include <linux/mii.h>

#define PHY_ID_YT8522			0x4f51e928
#define PHY_ID_MASK			0xffffffff

#define REG_PHY_SPEC_STATUS		0x11
#define REG_DEBUG_ADDR_OFFSET		0x1e
#define REG_DEBUG_DATA			0x1f

#define YT_SOFT_RESET			0x8000
#define YT_SPEED_MODE_BIT		14
#define YT_DUPLEX_BIT			13
#define YT_LINK_STATUS_BIT		10

#define YT8512_EXTREG_SLEEP_CONTROL1	0x2027
#define YT8512_EN_SLEEP_SW_BIT		15

#define YT8522_TX_CLK_DELAY		0x4210
#define YT8522_ANAGLOG_IF_CTRL		0x4008
#define YT8522_DAC_CTRL			0x2057
#define YT8522_INTERPOLATOR_FILTER_1	0x14
#define YT8522_INTERPOLATOR_FILTER_2	0x15
#define YT8522_EXTENDED_COMBO_CTRL_1	0x4000
#define YT8522_TX_DELAY_CONTROL		0x19
#define YT8522_EXTENDED_PAD_CONTROL	0x4001

#define YT8522_CHIP_MODE_MASK		(BIT(1) | BIT(0))
#define YT8522_CHIP_MODE_MII		0x0
#define YT8522_CHIP_MODE_REMII		0x1
#define YT8522_CHIP_MODE_RMII2		0x2
#define YT8522_CHIP_MODE_RMII1		0x3

#define YT_SOFT_RESET_POLL_US		1000
#define YT_SOFT_RESET_TIMEOUT		1000

#define msleep(n)			udelay((n) * 1000)

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

static int ytphy_soft_reset(struct phy_device *phydev)
{
	int ret;
	int val;
	int timeout = YT_SOFT_RESET_TIMEOUT;

	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	if (val < 0)
		return val;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, val | YT_SOFT_RESET);
	if (ret < 0)
		return ret;

	while (timeout--) {
		val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
		if (val < 0)
			return val;
		if (!(val & YT_SOFT_RESET))
			return 0;
		udelay(YT_SOFT_RESET_POLL_US);
	}

	return -1;
}

static int yt8522_config_init(struct phy_device *phydev)
{
	int ret;
	int val;
	int chip_mode;
	int reg1a;

	chip_mode = ytphy_read_ext(phydev, YT8522_EXTENDED_COMBO_CTRL_1);
	if (chip_mode < 0)
		return chip_mode;

	chip_mode &= YT8522_CHIP_MODE_MASK;

	val = ytphy_read_ext(phydev, YT8522_EXTENDED_COMBO_CTRL_1);
	if (val < 0)
		return val;

	reg1a = phy_read(phydev, MDIO_DEVAD_NONE, 0x1a);
	printf("yt8522: addr=%d config_init combo=0x%x chip_mode=%d reg1a=0x%x\n",
	       phydev->addr, val, chip_mode, reg1a);

	if (chip_mode == YT8522_CHIP_MODE_RMII2) {
		val |= BIT(4);
		ret = ytphy_write_ext(phydev, YT8522_EXTENDED_COMBO_CTRL_1, val);
		if (ret < 0)
			return ret;
		printf("yt8522: addr=%d apply RMII2 combo=0x%x\n", phydev->addr,
		       val);

		ret = ytphy_write_ext(phydev, YT8522_TX_DELAY_CONTROL, 0x9f);
		if (ret < 0)
			return ret;
		printf("yt8522: addr=%d write ext 0x19=0x9f\n", phydev->addr);

		ret = ytphy_write_ext(phydev, YT8522_EXTENDED_PAD_CONTROL, 0x81d4);
		if (ret < 0)
			return ret;
		printf("yt8522: addr=%d write ext 0x4001=0x81d4\n", phydev->addr);
	} else if (chip_mode == YT8522_CHIP_MODE_RMII1) {
		val |= BIT(4);
		ret = ytphy_write_ext(phydev, YT8522_EXTENDED_COMBO_CTRL_1, val);
		if (ret < 0)
			return ret;
		printf("yt8522: addr=%d apply RMII1 combo=0x%x\n", phydev->addr,
		       val);
	} else {
		printf("yt8522: addr=%d non-rmii chip_mode=%d\n", phydev->addr,
		       chip_mode);
	}

	if (chip_mode == YT8522_CHIP_MODE_MII || chip_mode == YT8522_CHIP_MODE_REMII) {
		ret = ytphy_write_ext(phydev, YT8522_TX_CLK_DELAY, 0);
		if (ret < 0)
			return ret;
		printf("yt8522: addr=%d write ext 0x4210=0x0\n", phydev->addr);
	}

	ret = ytphy_write_ext(phydev, YT8522_ANAGLOG_IF_CTRL, 0xbf2a);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, YT8522_DAC_CTRL, 0x297f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, YT8522_INTERPOLATOR_FILTER_1, 0x1fe);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(phydev, YT8522_INTERPOLATOR_FILTER_2, 0x1fe);
	if (ret < 0)
		return ret;

	val = ytphy_read_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1);
	if (val < 0)
		return val;
	printf("yt8522: addr=%d sleep_ctrl before=0x%x\n", phydev->addr, val);

	val &= ~BIT(YT8512_EN_SLEEP_SW_BIT);
	ret = ytphy_write_ext(phydev, YT8512_EXTREG_SLEEP_CONTROL1, val);
	if (ret < 0)
		return ret;
	printf("yt8522: addr=%d sleep_ctrl after=0x%x\n", phydev->addr, val);

	val = ytphy_read_ext(phydev, YT8522_EXTENDED_COMBO_CTRL_1);
	reg1a = phy_read(phydev, MDIO_DEVAD_NONE, 0x1a);
	printf("yt8522: addr=%d final combo=0x%x reg1a=0x%x\n", phydev->addr,
	       val, reg1a);

	ret = genphy_config_aneg(phydev);
	if (ret < 0)
		return ret;

	ret = ytphy_soft_reset(phydev);
	if (ret < 0)
		return ret;
	printf("yt8522: addr=%d soft reset done\n", phydev->addr);

	return 0;
}

static int yt8522_startup(struct phy_device *phydev)
{
	int speed;
	int speed_mode;
	int duplex;
	int val;

	msleep(1000);

	val = phy_read(phydev, MDIO_DEVAD_NONE, REG_PHY_SPEC_STATUS);
	if (val < 0)
		return val;

	if ((val & BIT(YT_LINK_STATUS_BIT)) >> YT_LINK_STATUS_BIT) {
		duplex = (val & BIT(YT_DUPLEX_BIT)) >> YT_DUPLEX_BIT;
		speed_mode = (val & (BIT(15) | BIT(14))) >> YT_SPEED_MODE_BIT;

		switch (speed_mode) {
		case 1:
			speed = SPEED_100;
			break;
		case 0:
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

static struct phy_driver YT8522_driver = {
	.name		= "Motorcomm YT8522/YT8522C",
	.uid		= PHY_ID_YT8522,
	.mask		= PHY_ID_MASK,
	.features	= PHY_BASIC_FEATURES,
	.config		= &yt8522_config_init,
	.startup	= &yt8522_startup,
	.shutdown	= &genphy_shutdown,
};

int phy_motorcomm_init(void)
{
	phy_register(&YT8522_driver);

	return 0;
}

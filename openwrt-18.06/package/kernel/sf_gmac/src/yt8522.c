/*
 * Minimal YT8522C bring-up helper for sf_gmac.
 */

#include "sf_gmac.h"
#include "yt8521.h"
#include "yt8522.h"

int yt8522_config_init(struct mii_bus *bus, struct phy_device *phydev)
{
	int chip_mode;
	int ret;
	int val;

	val = ytphy_read_ext(bus, phydev, YT8522_EXTENDED_COMBO_CTRL_1);
	if (val < 0)
		return val;
	chip_mode = val & YT8522_CHIP_MODE_MASK;

	if (chip_mode == YT8522_CHIP_MODE_RMII_2) {
		val |= YT8522_RMII_MODE_EN;
		ret = ytphy_write_ext(bus, phydev, YT8522_EXTENDED_COMBO_CTRL_1, val);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(bus, phydev, YT8522_TX_DELAY_CONTROL, 0x9f);
		if (ret < 0)
			return ret;

		ret = ytphy_write_ext(bus, phydev, YT8522_EXTENDED_PAD_CONTROL, 0x81d4);
		if (ret < 0)
			return ret;
	} else if (chip_mode == YT8522_CHIP_MODE_RMII_1) {
		val |= YT8522_RMII_MODE_EN;
		ret = ytphy_write_ext(bus, phydev, YT8522_EXTENDED_COMBO_CTRL_1, val);
		if (ret < 0)
			return ret;
	}

	ret = ytphy_write_ext(bus, phydev, YT8522_TX_CLK_DELAY, 0x0);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(bus, phydev, YT8522_ANAGLOG_IF_CTRL, 0xbf2a);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(bus, phydev, YT8522_DAC_CTRL, 0x297f);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(bus, phydev, YT8522_INTERPOLATOR_FILTER_1, 0x1fe);
	if (ret < 0)
		return ret;

	ret = ytphy_write_ext(bus, phydev, YT8522_INTERPOLATOR_FILTER_2, 0x1fe);
	if (ret < 0)
		return ret;

	/* disable auto sleep */
	val = ytphy_read_ext(bus, phydev, YT8521_EXTREG_SLEEP_CONTROL1);
	if (val < 0)
		return val;

	val &= (~BIT(YT8521_EN_SLEEP_SW_BIT));
	ret = ytphy_write_ext(bus, phydev, YT8521_EXTREG_SLEEP_CONTROL1, val);
	if (ret < 0)
		return ret;

	/* trigger software reset to take effect */
	val = bus->read(bus, phydev->mdio.addr, 0);
	if (val < 0)
		return val;

	val |= BIT(15);
	ret = bus->write(bus, phydev->mdio.addr, 0, val);
	if (ret < 0)
		return ret;

	printk("end %s\n", __func__);
	return 0;
}

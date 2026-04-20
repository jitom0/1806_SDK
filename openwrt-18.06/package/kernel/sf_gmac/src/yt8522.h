#ifndef _YT8522_H_
#define _YT8522_H_

struct mii_bus;
struct phy_device;

#define PHY_ID_YT8522				0x4f51e928
#define PHY_ID_YT8522_MASKED			0xe928
#define YT8522_TX_CLK_DELAY			0x4210
#define YT8522_ANAGLOG_IF_CTRL			0x4008
#define YT8522_DAC_CTRL				0x2057
#define YT8522_INTERPOLATOR_FILTER_1		0x14
#define YT8522_INTERPOLATOR_FILTER_2		0x15
#define YT8522_EXTENDED_COMBO_CTRL_1		0x4000
#define YT8522_CHIP_MODE_MASK			(BIT(1) | BIT(0))
#define YT8522_CHIP_MODE_RMII_2			0x2
#define YT8522_CHIP_MODE_RMII_1			0x3
#define YT8522_RMII_MODE_EN			BIT(4)
#define YT8522_TX_DELAY_CONTROL			0x19
#define YT8522_EXTENDED_PAD_CONTROL		0x4001

int yt8522_config_init(struct mii_bus *bus, struct phy_device *phydev);

#endif

/*
 * imx219_tables.h - sensor mode tables for imx219 HDR sensor.
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IMX219_I2C_TABLES
#define IMX219_I2C_TABLES

#define IMX219_TABLE_WAIT_MS 0
#define IMX219_TABLE_END 1
#define IMX219_MAX_RETRIES 3
#define IMX219_WAIT_MS 3

static struct reg_8 mode_3280x2464[] = {
	{IMX219_TABLE_WAIT_MS, 10},
	/* software reset */
	{0x0103, 0x01},
	/* global settings */
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012A, 0x0C},
	{0x012B, 0x00},
	{0x0160, 0x09},
	{0x0161, 0xC3},
	{0x0162, 0x0D},
	{0x0163, 0x78},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016A, 0x09},
	{0x016B, 0x9F},
	{0x016C, 0x0C},
	{0x016D, 0xD0},
	{0x016E, 0x09},
	{0x016F, 0xA0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x4C},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x98},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x47B4, 0x14},
	{0x5041, 0x00},
	/* stream on */
	{0x0100, 0x01},
	{IMX219_TABLE_WAIT_MS, IMX219_WAIT_MS},
	{IMX219_TABLE_END, 0x00}
};

static struct reg_8 mode_3280x2460[] = {
	{IMX219_TABLE_WAIT_MS, 10},
	/* software reset */
	{0x0103, 0x01},
	/* global settings */
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x03},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	/* Bank A Settings */
	{0x0157, 0x00},
	{0x015A, 0x08},
	{0x015B, 0x8F},
	{0x0160, 0x0A},
	{0x0161, 0x83},
	{0x0162, 0x0D},
	{0x0163, 0x78},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016A, 0x09},
	{0x016B, 0x9F},
	{0x016C, 0x0C},
	{0x016D, 0xD0},
	{0x016E, 0x09},
	{0x016F, 0x9C},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	/* Bank B Settings */
	{0x0257, 0x00},
	{0x025A, 0x08},
	{0x025B, 0x8F},
	{0x0260, 0x0A},
	{0x0261, 0x83},
	{0x0262, 0x0D},
	{0x0263, 0x78},
	{0x0264, 0x00},
	{0x0265, 0x00},
	{0x0266, 0x0C},
	{0x0267, 0xCF},
	{0x0268, 0x00},
	{0x0269, 0x00},
	{0x026A, 0x09},
	{0x026B, 0x9F},
	{0x026C, 0x0C},
	{0x026D, 0xD0},
	{0x026E, 0x09},
	{0x026F, 0x9C},
	{0x0270, 0x01},
	{0x0271, 0x01},
	{0x0274, 0x00},
	{0x0275, 0x00},
	{0x028C, 0x0A},
	{0x028D, 0x0A},
	/* clock setting */
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x57},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x5A},
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x5041, 0x00},
	/* stream on */
	{0x0100, 0x01},
	{IMX219_TABLE_WAIT_MS, IMX219_WAIT_MS},
	{IMX219_TABLE_END, 0x00}
};
static struct reg_8 mode_3280x1846[] = {
	{IMX219_TABLE_WAIT_MS, 10},
	/* software reset */
	{0x0103, 0x01},
	/* global settings */
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x03},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	/* Bank A Settings */
	{0x0157, 0x00},
	{0x015A, 0x08},
	{0x015B, 0x8F},
	{0x0160, 0x07},
	{0x0161, 0x5E},
	{0x0162, 0x0D},
	{0x0163, 0x78},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x01},
	{0x0169, 0x36},
	{0x016A, 0x08},
	{0x016B, 0x6B},
	{0x016C, 0x0C},
	{0x016D, 0xD0},
	{0x016E, 0x07},
	{0x016F, 0x36},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	/* Bank B Settings */
	{0x0257, 0x00},
	{0x025A, 0x08},
	{0x025B, 0x8F},
	{0x0260, 0x07},
	{0x0261, 0x5E},
	{0x0262, 0x0D},
	{0x0263, 0x78},
	{0x0264, 0x00},
	{0x0265, 0x00},
	{0x0266, 0x0C},
	{0x0267, 0xCF},
	{0x0268, 0x01},
	{0x0269, 0x36},
	{0x026A, 0x08},
	{0x026B, 0x6B},
	{0x026C, 0x0C},
	{0x026D, 0xD0},
	{0x026E, 0x07},
	{0x026F, 0x36},
	{0x0270, 0x01},
	{0x0271, 0x01},
	{0x0274, 0x00},
	{0x0275, 0x00},
	{0x028C, 0x0A},
	{0x028D, 0x0A},
	/* clock setting */
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x57},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x5A},
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x5041, 0x00},
	/* stream on */
	{0x0100, 0x01},
	{IMX219_TABLE_WAIT_MS, IMX219_WAIT_MS},
	{IMX219_TABLE_END, 0x00}
};

static struct reg_8 mode_1280x720[] = {
	{IMX219_TABLE_WAIT_MS, 10},
	/* software reset */
	{0x0103, 0x01},
	/* global settings */
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x03},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	/* Bank A Settings */
	{0x0160, 0x02},
	{0x0161, 0x8C},
	{0x0162, 0x0D},
	{0x0163, 0xE8},
	{0x0164, 0x01},
	{0x0165, 0x68},
	{0x0166, 0x0B},
	{0x0167, 0x67},
	{0x0168, 0x02},
	{0x0169, 0x00},
	{0x016A, 0x07},
	{0x016B, 0x9F},
	{0x016C, 0x05},
	{0x016D, 0x00},
	{0x016E, 0x02},
	{0x016F, 0xD0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x03},
	{0x0175, 0x03},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	/* Bank B Settings */
	{0x0260, 0x02},
	{0x0261, 0x8C},
	{0x0262, 0x0D},
	{0x0263, 0xE8},
	{0x0264, 0x01},
	{0x0265, 0x68},
	{0x0266, 0x0B},
	{0x0267, 0x67},
	{0x0268, 0x02},
	{0x0269, 0x00},
	{0x026A, 0x07},
	{0x026B, 0x9F},
	{0x026C, 0x05},
	{0x026D, 0x00},
	{0x026E, 0x02},
	{0x026F, 0xD0},
	{0x0270, 0x01},
	{0x0271, 0x01},
	{0x0274, 0x03},
	{0x0275, 0x03},
	{0x028C, 0x0A},
	{0x028D, 0x0A},
	/* clock setting */
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x57},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x5A},
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x5041, 0x00},
	/* stream on */
	{0x0100, 0x01},
	{IMX219_TABLE_WAIT_MS, IMX219_WAIT_MS},
	{IMX219_TABLE_END, 0x00}
};
enum {
	IMX219_MODE_3280x2464,
	IMX219_MODE_3280x2460,
	IMX219_MODE_3280x1846,
	IMX219_MODE_1280x720,
};

static struct reg_8 *mode_table[] = {
	[IMX219_MODE_3280x2464] = mode_3280x2464,
	[IMX219_MODE_3280x2460] = mode_3280x2460,
	[IMX219_MODE_3280x1846] = mode_3280x1846,
	[IMX219_MODE_1280x720]  = mode_1280x720,
};

static const struct camera_common_frmfmt imx219_frmfmt[] = {
	{{3280, 2464},	0, IMX219_MODE_3280x2464},
	{{3280, 2460},	0, IMX219_MODE_3280x2460},
	{{3280, 1846},	0, IMX219_MODE_3280x1846},
	{{1280, 720},	0, IMX219_MODE_1280x720},
};

#endif

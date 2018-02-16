/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

        Module Name:

        Abstract:

        Revision History:
        Who             When                    What
        --------        ----------              ----------------------------------------------
*/


#include "rt_config.h"


void mt7612u_bbp_set_txdac(struct rtmp_adapter *pAd, int tx_dac)
{
	uint32_t txbe, txbe_r5 = 0;

	txbe_r5 = mt76u_reg_read(pAd, TXBE_R5);
	txbe = txbe_r5 & (~0x3);

	switch (tx_dac) {
	case 2:
		txbe |= 0x3;
		break;
	case 1:
	case 0:
	default:
		txbe &= (~0x3);
		break;
	}

	if (txbe != txbe_r5)
		mt76u_reg_write(pAd, TXBE_R5, txbe);
}


void mt7612u_bbp_set_rxpath(struct rtmp_adapter *pAd, int rxpath)
{
	uint32_t agc, agc_r0 = 0;

	agc_r0 = mt76u_reg_read(pAd, AGC1_R0);
	agc = agc_r0 & (~0x18);
	if(rxpath == 2)
		agc |= (0x8);
	else if(rxpath == 1)
		agc |= (0x0);

	if (agc != agc_r0)
		mt76u_reg_write(pAd, AGC1_R0, agc);

//DBGPRINT(RT_DEBUG_OFF, ("%s(): rxpath=%d, Set AGC1_R0=0x%x, agc_r0=0x%x\n", __FUNCTION__, rxpath, agc, agc_r0));
//		mt76u_reg_read(pAd, AGC1_R0, &agc);
//DBGPRINT(RT_DEBUG_OFF, ("%s(): rxpath=%d, After write, Get AGC1_R0=0x%x,\n", __FUNCTION__, rxpath, agc));
}

void mt7612u_bbp_set_ctrlch(struct rtmp_adapter *pAd, u8 ext_ch)
{
	uint32_t agc, agc_r0 = 0;
	uint32_t be, be_r0 = 0;

	agc_r0 = mt76u_reg_read(pAd, AGC1_R0);
	agc = agc_r0 & (~0x300);
	be_r0 = mt76u_reg_read(pAd, TXBE_R0);
	be = (be_r0 & (~0x03));
	if (pAd->CommonCfg.BBPCurrentBW == BW_80 &&
	    pAd->CommonCfg.Channel >= 36 &&
	    pAd->CommonCfg.vht_cent_ch) {
		if (pAd->CommonCfg.Channel < pAd->CommonCfg.vht_cent_ch) {
			switch (pAd->CommonCfg.vht_cent_ch - pAd->CommonCfg.Channel) {
			case 6:
				be |= 0;
				agc |=0x000;
				break;
			case 2:
				be |= 1;
				agc |=0x100;
				break;

			}
		} else if (pAd->CommonCfg.Channel > pAd->CommonCfg.vht_cent_ch) {
			switch (pAd->CommonCfg.Channel - pAd->CommonCfg.vht_cent_ch) {
			case 6:
				be |= 0x3;
				agc |=0x300;
				break;
			case 2:
				be |= 0x2;
				agc |=0x200;
				break;
			}
		}
	} else {
		switch (ext_ch) {
		case EXTCHA_BELOW:
			agc |= 0x100;
			be |= 0x01;
			break;
		case EXTCHA_ABOVE:
			agc &= (~0x300);
			be &= (~0x03);
			break;
		case EXTCHA_NONE:
		default:
			agc &= (~0x300);
			be &= (~0x03);
			break;
		}
	}
	if (agc != agc_r0)
		mt76u_reg_write(pAd, AGC1_R0, agc);

	if (be != be_r0)
		mt76u_reg_write(pAd, TXBE_R0, be);

//DBGPRINT(RT_DEBUG_OFF, ("%s(): ext_ch=%d, Set AGC1_R0=0x%x, agc_r0=0x%x\n", __FUNCTION__, ext_ch, agc, agc_r0));
//		mt76u_reg_read(pAd, AGC1_R0, &agc);
//DBGPRINT(RT_DEBUG_OFF, ("%s(): ext_ch=%d, After write, Get AGC1_R0=0x%x,\n", __FUNCTION__, ext_ch, agc));
}


/*
	<<Gamma2.1 Control Registers Rev1.3.pdf>>
	BBP bandwidth (CORE_R1[4:3]) change procedure:
	1. Hold BBP in reset by setting CORE_R4[0] to '1'
	2. Wait 0.5 us to ensure BBP is in the idle State
	3. Change BBP bandwidth with CORE_R1[4:3]
		CORE_R1 (Bit4:3)
		0: 20MHz
		1: 10MHz (11J)
		2: 40MHz
		3: 80MHz
	4. Wait 0.5 us for BBP clocks to settle
	5. Release BBP from reset by clearing CORE_R4[0]
*/
void mt7612u_bbp_set_bw(struct rtmp_adapter *pAd, u8 bw)
{
	uint32_t core, core_r1 = 0;
	uint32_t agc, agc_r0 = 0;

	uint32_t ret;


	ret = down_interruptible(&pAd->hw_atomic);
	if (ret != 0) {
		DBGPRINT(RT_DEBUG_ERROR, ("reg_atomic get failed(ret=%d)\n", ret));
		return;
	}

	core_r1 = mt76u_reg_read(pAd, CORE_R1);
		core = (core_r1 & (~0x18));
	agc_r0 = mt76u_reg_read(pAd, AGC1_R0);
	agc = agc_r0 & (~0x7000);
	switch (bw) {
	case BW_80:
		core |= 0x18;
		agc |= 0x7000;
		break;
	case BW_40:
		core |= 0x10;
		agc |= 0x3000;
		break;
	case BW_20:
		core &= (~0x18);
		agc |= 0x1000;
		break;
	case BW_10:
		core |= 0x08;
		agc |= 0x1000;
		break;
	}

	if (core != core_r1)
		mt76u_reg_write(pAd, CORE_R1, core);

	if (agc != agc_r0) {
		mt76u_reg_write(pAd, AGC1_R0, agc);
//DBGPRINT(RT_DEBUG_OFF, ("%s(): bw=%d, Set AGC1_R0=0x%x, agc_r0=0x%x\n", __FUNCTION__, bw, agc, agc_r0));
//		mt76u_reg_read(pAd, AGC1_R0, &agc);
//DBGPRINT(RT_DEBUG_OFF, ("%s(): bw=%d, After write, Get AGC1_R0=0x%x,\n", __FUNCTION__, bw, agc));
	}

	pAd->CommonCfg.BBPCurrentBW = bw;

	up(&pAd->hw_atomic);

	return;
}

static u8 rlt_bbp_get_random_seed(struct rtmp_adapter *pAd)
{
	uint32_t value, value2;
	u8 seed;

	value = mt76u_reg_read(pAd, AGC1_R16);
	seed = (u8)((value & 0xff) ^ ((value & 0xff00) >> 8)^
					((value & 0xff0000) >> 16));
	value2 = mt76u_reg_read(pAd, RXO_R9);

	return (u8)(seed ^ (value2 & 0xff)^ ((value2 & 0xff00) >> 8));
}


static struct phy_ops rlt_phy_ops = {
	.get_random_seed_by_phy = rlt_bbp_get_random_seed,
};


INT rlt_phy_probe(struct rtmp_adapter *pAd)
{
	pAd->phy_op = &rlt_phy_ops;

	return true;
}



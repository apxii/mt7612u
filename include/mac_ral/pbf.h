/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering	the source code	is stricitly prohibited, unless	the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	pbf.h

	Abstract:
	Ralink Wireless Chip MAC related definition & structures

	Revision History:
	Who			When		  What
	--------	----------	  ----------------------------------------------
*/

#ifndef __PBF_H__
#define __PBF_H__


#include "mac_ral/nmac/ral_nmac_pbf.h"

/* ================================================================================= */
/* Register format  for PBF                                                                                                                                                     */
/* ================================================================================= */


#define WPDMA_GLO_CFG 	0x208
#ifdef __BIG_ENDIAN
typedef	union _WPDMA_GLO_CFG_STRUC	{
	struct	{
		uint32_t rx_2b_offset:1;
		uint32_t clk_gate_dis:1;
		uint32_t rsv:14;
		uint32_t HDR_SEG_LEN:8;
		uint32_t BigEndian:1;
		uint32_t EnTXWriteBackDDONE:1;
		uint32_t WPDMABurstSIZE:2;
		uint32_t RxDMABusy:1;
		uint32_t EnableRxDMA:1;
		uint32_t TxDMABusy:1;
		uint32_t EnableTxDMA:1;
	}	field;
	uint32_t word;
}WPDMA_GLO_CFG_STRUC, *PWPDMA_GLO_CFG_STRUC;
#else
typedef	union _WPDMA_GLO_CFG_STRUC	{
	struct {
		uint32_t EnableTxDMA:1;
		uint32_t TxDMABusy:1;
		uint32_t EnableRxDMA:1;
		uint32_t RxDMABusy:1;
		uint32_t WPDMABurstSIZE:2;
		uint32_t EnTXWriteBackDDONE:1;
		uint32_t BigEndian:1;
		uint32_t HDR_SEG_LEN:8;
		uint32_t rsv:14;
		uint32_t clk_gate_dis:1;
		uint32_t rx_2b_offset:1;
	} field;
	uint32_t word;
} WPDMA_GLO_CFG_STRUC, *PWPDMA_GLO_CFG_STRUC;
#endif

#define PBF_SYS_CTRL 	 0x0400
//#define PBF_CFG         0x0404

#define PBF_CTRL			0x0410
#define MCU_INT_STA		0x0414
#define MCU_INT_ENA	0x0418
#define TXRXQ_PCNT		0x0438
#define PBF_DBG			0x043c


#endif /* __PBF_H__ */


/****************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ****************************************************************************

    Module Name:
    dot11_base.h

    Abstract:
	Defined IE/frame structures of 802.11 base line

    Revision History:
    Who        When          What
    ---------  ----------    ----------------------------------------------
*/


#ifndef _DOT11_BASE_H_
#define _DOT11_BASE_H_

#include "rtmp_type.h"


/* value domain of 802.11 header FC.Tyte, which is b3..b2 of the 1st-byte of MAC header */
#define FC_TYPE_MGMT	0
#define FC_TYPE_CNTL	1
#define FC_TYPE_DATA	2
#define FC_TYPE_RSVED	3

/* value domain of 802.11 MGMT frame's FC.subtype, which is b7..4 of the 1st-byte of MAC header */
#define SUBTYPE_ASSOC_REQ           0
#define SUBTYPE_ASSOC_RSP           1
#define SUBTYPE_REASSOC_REQ         2
#define SUBTYPE_REASSOC_RSP         3
#define SUBTYPE_PROBE_REQ           4
#define SUBTYPE_PROBE_RSP           5
#define SUBTYPE_TIMING_ADV		6
#define SUBTYPE_BEACON              8
#define SUBTYPE_ATIM                9
#define SUBTYPE_DISASSOC            10
#define SUBTYPE_AUTH                11
#define SUBTYPE_DEAUTH              12
#define SUBTYPE_ACTION              13
#define SUBTYPE_ACTION_NO_ACK              14

/* value domain of 802.11 CNTL frame's FC.subtype, which is b7..4 of the 1st-byte of MAC header */
#define SUBTYPE_VHT_NDPA		5
#define SUBTYPE_WRAPPER       	7
#define SUBTYPE_BLOCK_ACK_REQ       8
#define SUBTYPE_BLOCK_ACK           9
#define SUBTYPE_PS_POLL             10
#define SUBTYPE_RTS                 11
#define SUBTYPE_CTS                 12
#define SUBTYPE_ACK                 13
#define SUBTYPE_CFEND               14
#define SUBTYPE_CFEND_CFACK         15

/* value domain of 802.11 DATA frame's FC.subtype, which is b7..4 of the 1st-byte of MAC header */
#define SUBTYPE_DATA                0
#define SUBTYPE_DATA_CFACK          1
#define SUBTYPE_DATA_CFPOLL         2
#define SUBTYPE_DATA_CFACK_CFPOLL   3
#define SUBTYPE_DATA_NULL           4
#define SUBTYPE_CFACK               5
#define SUBTYPE_CFPOLL              6
#define SUBTYPE_CFACK_CFPOLL        7
#define SUBTYPE_QDATA               8
#define SUBTYPE_QDATA_CFACK         9
#define SUBTYPE_QDATA_CFPOLL        10
#define SUBTYPE_QDATA_CFACK_CFPOLL  11
#define SUBTYPE_QOS_NULL            12
#define SUBTYPE_QOS_CFACK           13
#define SUBTYPE_QOS_CFPOLL          14
#define SUBTYPE_QOS_CFACK_CFPOLL    15

/* 2-byte Frame control field */
typedef struct GNU_PACKED {
#ifdef __BIG_ENDIAN
	uint16_t Order:1;		/* Strict order expected */
	uint16_t Wep:1;		/* Wep data */
	uint16_t MoreData:1;	/* More data bit */
	uint16_t PwrMgmt:1;	/* Power management bit */
	uint16_t Retry:1;		/* Retry status bit */
	uint16_t MoreFrag:1;	/* More fragment bit */
	uint16_t FrDs:1;		/* From DS indication */
	uint16_t ToDs:1;		/* To DS indication */
	uint16_t SubType:4;	/* MSDU subtype */
	uint16_t Type:2;		/* MSDU type */
	uint16_t Ver:2;		/* Protocol version */
#else
        uint16_t Ver:2;		/* Protocol version */
	uint16_t Type:2;		/* MSDU type, refer to FC_TYPE_XX */
	uint16_t SubType:4;	/* MSDU subtype, refer to  SUBTYPE_XXX */
	uint16_t ToDs:1;		/* To DS indication */
	uint16_t FrDs:1;		/* From DS indication */
	uint16_t MoreFrag:1;	/* More fragment bit */
	uint16_t Retry:1;		/* Retry status bit */
	uint16_t PwrMgmt:1;	/* Power management bit */
	uint16_t MoreData:1;	/* More data bit */
	uint16_t Wep:1;		/* Wep data */
	uint16_t Order:1;		/* Strict order expected */
#endif	/* !__BIG_ENDIAN */
} FRAME_CONTROL, *PFRAME_CONTROL;


typedef struct GNU_PACKED _HEADER_802_11 {
        FRAME_CONTROL   FC;
        uint16_t          Duration;
        u8           Addr1[6];
        u8           Addr2[6];
	u8 	Addr3[6];
#ifdef __BIG_ENDIAN
	uint16_t 	Sequence:12;
	uint16_t 	Frag:4;
#else
	uint16_t 	Frag:4;
	uint16_t 	Sequence:12;
#endif /* !__BIG_ENDIAN */
	u8 	Octet[0];
}HEADER_802_11, *PHEADER_802_11;

#endif /* _DOT11_BASE_H_ */


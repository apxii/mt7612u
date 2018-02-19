/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2006, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	wsc.h

	Abstract:

	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
	Name		Date			Modification logs
	Paul Lin	06-08-08		Initial
*/

#ifndef	__WSC_H__
#define	__WSC_H__

/* WSC OUI SMI */
#define WSC_OUI				0x0050f204
#define	WSC_SMI				0x00372A
#define	WSC_VENDOR_TYPE		0x00000001

/* EAP code */
#define	EAP_CODE_REQ		0x01
#define	EAP_CODE_RSP		0x02
#define	EAP_CODE_FAIL		0x04
#define EAP_TYPE_ID			0x01
#define EAP_TYPE_NOTIFY		0x02
#define	EAP_TYPE_WSC		0xfe

/* structure to store Simple Config Attributes Info */
typedef struct GNU_PACKED _WSC_LV_INFO {
    unsigned short  ValueLen;
    u8   Value[512];
} WSC_LV_INFO;

typedef struct GNU_PACKED _WSC_IE_HEADER {
	u8 elemId;
	u8 length;
	u8 oui[4];
} WSC_IE_HEADER;

/* WSC IE structure */
typedef	struct GNU_PACKED _WSC_IE
{
	unsigned short Type;
	unsigned short Length;
	u8 Data[1];	/* variable length data */
}	WSC_IE, *PWSC_IE;

/* WSC fixed information within EAP */
typedef	struct GNU_PACKED _WSC_FRAME
{
	u8 SMI[3];
	UINT	VendorType;
	u8 OpCode;
	u8 Flags;
}	WSC_FRAME, *PWSC_FRAME;

/* EAP frame format */
typedef	struct GNU_PACKED _EAP_FRAME	{
	u8 Code;						/* 1 = Request, 2 = Response */
	u8 Id;
	unsigned short Length;
	u8 Type;						/* 1 = Identity, 0xfe = reserved, used by WSC */
}	EAP_FRAME, *PEAP_FRAME;

static inline bool WscCheckWSCHeader(u8 *pData)
{
	WSC_FRAME *pWsc = (WSC_FRAME *)pData;

    /* Verify SMI first */
	if (((pWsc->SMI[0] * 256 + pWsc->SMI[1]) * 256 + pWsc->SMI[2]) != WSC_SMI)
		return  false; /* Wrong WSC SMI Vendor ID, Update WSC status */

    /* Verify Vendor Type */
	if (cpu2be32(get_unaligned32(&pWsc->VendorType)) != WSC_VENDOR_TYPE)
		return  false; /* Wrong WSC Vendor Type, Update WSC status */

    return true;
}


#endif	/* __WSC_H__ */


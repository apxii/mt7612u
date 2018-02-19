/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2009, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

 	Module Name:
	rtusb_io.h

	Abstract:

	Revision History:
	Who			When	    What
	--------	----------  ----------------------------------------------
	Name		Date	    Modification logs
*/

#ifndef __RTUSB_IO_H__
#define __RTUSB_IO_H__

#include "wpa_cmm.h"
#include "rtmp_type.h"

/* First RTUSB IO command number */
#define CMDTHREAD_FIRST_CMD_ID						0x0D730101

#define CMDTHREAD_RESET_BULK_OUT						0x0D730101
#define CMDTHREAD_RESET_BULK_IN						0x0D730102
#define CMDTHREAD_CHECK_GPIO							0x0D730103
#define CMDTHREAD_SET_ASIC_WCID						0x0D730104
#define CMDTHREAD_DEL_ASIC_WCID						0x0D730105
#define CMDTHREAD_SET_CLIENT_MAC_ENTRY				0x0D730106

#ifdef CONFIG_STA_SUPPORT
#define CMDTHREAD_SET_PSM_BIT							0x0D730107
#define CMDTHREAD_FORCE_WAKE_UP						0x0D730108
#define CMDTHREAD_FORCE_SLEEP_AUTO_WAKEUP			0x0D730109
#define CMDTHREAD_QKERIODIC_EXECUT					0x0D73010A
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
#define CMDTHREAD_AP_UPDATE_CAPABILITY_AND_ERPIE	0x0D73010B
#define CMDTHREAD_AP_ENABLE_TX_BURST				0x0D73010C
#define CMDTHREAD_AP_DISABLE_TX_BURST				0x0D73010D
#define CMDTHREAD_AP_ADJUST_EXP_ACK_TIME			0x0D73010E
#define CMDTHREAD_AP_RECOVER_EXP_ACK_TIME			0x0D73010F
#define CMDTHREAD_CHAN_RESCAN						0x0D730110
#endif /* CONFIG_AP_SUPPORT */

#define CMDTHREAD_SET_LED_STATUS				0x0D730111	/* Set WPS LED status (LED_WPS_XXX). */

/* Security related */
#define CMDTHREAD_SET_WCID_SEC_INFO					0x0D730113
#define CMDTHREAD_SET_ASIC_WCID_IVEIV				0x0D730114
#define CMDTHREAD_SET_ASIC_WCID_ATTR				0x0D730115
#define CMDTHREAD_SET_ASIC_SHARED_KEY				0x0D730116
#define CMDTHREAD_SET_ASIC_PAIRWISE_KEY				0x0D730117
#define CMDTHREAD_REMOVE_PAIRWISE_KEY				0x0D730118
#ifdef CONFIG_STA_SUPPORT
#define CMDTHREAD_SET_PORT_SECURED					0x0D730119
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
#define CMDTHREAD_802_11_COUNTER_MEASURE			0x0D73011A
#endif /* CONFIG_AP_SUPPORT */

/* add by johnli, fix "in_interrupt" error when call "MacTableDeleteEntry" in Rx tasklet */
#define CMDTHREAD_UPDATE_PROTECT					0x0D73011B
/* end johnli */

#ifdef RT_CFG80211_SUPPORT
#define CMDTHREAD_REG_HINT							0x0D73011C
#define CMDTHREAD_REG_HINT_11D						0x0D73011D
#define CMDTHREAD_SCAN_END							0x0D73011E
#define CMDTHREAD_CONNECT_RESULT_INFORM				0x0D73011F
#endif /* RT_CFG80211_SUPPORT */

typedef struct _CMDHandler_TLV {
	unsigned short Offset;
	unsigned short Length;
	u8 DataFirst;
} CMDHandler_TLV, *PCMDHandler_TLV;


typedef struct _RT_SET_ASIC_WCID {
	ULONG WCID;		/* mechanism for rekeying: 0:disable, 1: time-based, 2: packet-based */
	ULONG SetTid;		/* time-based: seconds, packet-based: kilo-packets */
	ULONG DeleteTid;	/* time-based: seconds, packet-based: kilo-packets */
	u8 Addr[MAC_ADDR_LEN];	/* avoid in interrupt when write key */
} RT_SET_ASIC_WCID, *PRT_SET_ASIC_WCID;

typedef struct _RT_ASIC_WCID_SEC_INFO {
	u8 BssIdx;
	u8 KeyIdx;
	u8 CipherAlg;
	UINT8 Wcid;
	UINT8 KeyTabFlag;
} RT_ASIC_WCID_SEC_INFO, *PRT_ASIC_WCID_SEC_INFO;

typedef struct _RT_ASIC_WCID_IVEIV_ENTRY {
	UINT8 Wcid;
	uint32_t Iv;
	uint32_t Eiv;
} RT_ASIC_WCID_IVEIV_ENTRY, *PRT_ASIC_WCID_IVEIV_ENTRY;

typedef struct _RT_ASIC_WCID_ATTR_ENTRY {
	u8 BssIdx;
	u8 KeyIdx;
	u8 CipherAlg;
	UINT8 Wcid;
	UINT8 KeyTabFlag;
} RT_ASIC_WCID_ATTR_ENTRY, *PRT_ASIC_WCID_ATTR_ENTRY;

typedef struct _RT_ASIC_PAIRWISE_KEY {
	UINT8 WCID;
	CIPHER_KEY CipherKey;
} RT_ASIC_PAIRWISE_KEY, *PRT_ASIC_PAIRWISE_KEY;

typedef struct _RT_ASIC_SHARED_KEY {
	u8 BssIndex;
	u8 KeyIdx;
	CIPHER_KEY CipherKey;
} RT_ASIC_SHARED_KEY, *PRT_ASIC_SHARED_KEY;

typedef struct _RT_ASIC_PROTECT_INFO {
	unsigned short OperationMode;
	u8 SetMask;
	bool bDisableBGProtect;
	bool bNonGFExist;
} RT_ASIC_PROTECT_INFO, *PRT_ASIC_PROTECT_INFO;

/******************************************************************************

  	USB Cmd to ASIC Related MACRO

******************************************************************************/
/* reset MAC of a station entry to 0xFFFFFFFFFFFF */
#define RTMP_STA_ENTRY_MAC_RESET(pAd, Wcid)					\
	{	RT_SET_ASIC_WCID	SetAsicWcid;						\
		SetAsicWcid.WCID = Wcid;								\
		RTEnqueueInternalCmd(pAd, CMDTHREAD_DEL_ASIC_WCID, 	\
				&SetAsicWcid, sizeof(RT_SET_ASIC_WCID));	}

/* Set MAC register value according operation mode */
#ifdef CONFIG_AP_SUPPORT
#define RTMP_AP_UPDATE_CAPABILITY_AND_ERPIE(pAd)	\
 	RTEnqueueInternalCmd(pAd, CMDTHREAD_AP_UPDATE_CAPABILITY_AND_ERPIE, NULL, 0);
#endif /* CONFIG_AP_SUPPORT */

/* Insert the BA bitmap to ASIC for the Wcid entry */
#define RTMP_ADD_BA_SESSION_TO_ASIC(_pAd, _Aid, _TID)					\
		do{																\
			RT_SET_ASIC_WCID	SetAsicWcid;							\
			SetAsicWcid.WCID = (_Aid);									\
			SetAsicWcid.SetTid = (0x10000<<(_TID));						\
			SetAsicWcid.DeleteTid = 0xffffffff;							\
			RTEnqueueInternalCmd((_pAd), CMDTHREAD_SET_ASIC_WCID, &SetAsicWcid, sizeof(RT_SET_ASIC_WCID));	\
		}while(0)

/* Remove the BA bitmap from ASIC for the Wcid entry */
#define RTMP_DEL_BA_SESSION_FROM_ASIC(_pAd, _Wcid, _TID)				\
		do{																\
			RT_SET_ASIC_WCID	SetAsicWcid;							\
			SetAsicWcid.WCID = (_Wcid);									\
			SetAsicWcid.SetTid = (0xffffffff);							\
			SetAsicWcid.DeleteTid = (0x10000<<(_TID) );					\
			RTEnqueueInternalCmd((_pAd), CMDTHREAD_SET_ASIC_WCID, &SetAsicWcid, sizeof(RT_SET_ASIC_WCID));	\
		}while(0)

#define RTMP_UPDATE_PROTECT(_pAd, _OperationMode, _SetMask, _bDisableBGProtect, _bNonGFExist)	\
		do {\
			RT_ASIC_PROTECT_INFO AsicProtectInfo;\
			AsicProtectInfo.OperationMode = (_OperationMode);\
			AsicProtectInfo.SetMask = (_SetMask);\
			AsicProtectInfo.bDisableBGProtect = (_bDisableBGProtect);\
			AsicProtectInfo.bNonGFExist = (_bNonGFExist);\
			RTEnqueueInternalCmd((_pAd), CMDTHREAD_UPDATE_PROTECT, &AsicProtectInfo, sizeof(RT_ASIC_PROTECT_INFO));\
		} while(0)

void mt76x2_init_rf_cr(struct rtmp_adapter *ad);
void mt76x2_init_mac_cr(struct rtmp_adapter *ad);
int mt76u_sys_write(struct rtmp_adapter *ad, uint16_t offset, uint32_t val);
u32 mt76u_sys_read(struct rtmp_adapter *ad, uint16_t offset);

enum mt_vendor_req {
/* Known USB Vendor Commands */
	MT_VEND_DEVICE_MODE = 0x01,
	MT_VEND_SINGLE_WRITE = 0x02,	/* WLAN */
	MT_VEND_MULTI_WRITE = 0x06,	/* WLAN 4 byte */
	MT_VEND_MULTI_READ = 0x07,	/* WLAN 4 byte */
	MT_VEND_WRITE_EEPROM = 0x08,	/* Not used */
	MT_VEND_READ_EEPROM = 0x09,
	MT_VEND_WRITE_FCE = 0x42,	/* FCE-WRITE */
	MT_VEND_SYS_READ = 0x47,	/* SYS-READ neded for 7612u series */
	MT_VEND_SYS_WRITE = 0x46,	/* SYS_WRITE neded for 7612u series */
};

#endif /* __RTUSB_IO_H__ */

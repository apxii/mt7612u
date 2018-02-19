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
    dot11n_ht.h

    Abstract:
	Defined IE/frame structures of 802.11n

    Revision History:
    Who        When          What
    ---------  ----------    ----------------------------------------------
*/



#ifndef _DOT11N_HT_H_
#define _DOT11N_HT_H_

#include "rtmp_type.h"
#include "dot11_base.h"

/*  HT Capability INFO field in HT Cap IE . */
typedef struct GNU_PACKED _HT_CAP_INFO{
#ifdef __BIG_ENDIAN
	uint16_t LSIGTxopProSup:1;
	uint16_t Forty_Mhz_Intolerant:1;
	uint16_t PSMP:1;
	uint16_t CCKmodein40:1;
	uint16_t AMsduSize:1;
	uint16_t DelayedBA:1;
	uint16_t RxSTBC:2;
	uint16_t TxSTBC:1;
	uint16_t ShortGIfor40:1;
	uint16_t ShortGIfor20:1;
	uint16_t GF:1;
	uint16_t MimoPs:2;
	uint16_t ChannelWidth:1;
	uint16_t ht_rx_ldpc:1;
#else
	uint16_t ht_rx_ldpc:1;
	uint16_t ChannelWidth:1;
	uint16_t MimoPs:2;		/* mimo power safe */
	uint16_t GF:1;			/* green field */
	uint16_t ShortGIfor20:1;
	uint16_t ShortGIfor40:1;	/* for40MHz */
	uint16_t TxSTBC:1;		/* 0:not supported,  1:if supported */
	uint16_t RxSTBC:2;
	uint16_t DelayedBA:1;
	uint16_t AMsduSize:1;	/* only support as zero */
	uint16_t CCKmodein40:1;
	uint16_t PSMP:1;
	uint16_t Forty_Mhz_Intolerant:1;
	uint16_t LSIGTxopProSup:1;
#endif /* __BIG_ENDIAN */
} HT_CAP_INFO;


/*  HT Capability INFO field in HT Cap IE . */
typedef struct GNU_PACKED _HT_CAP_PARM{
#ifdef __BIG_ENDIAN
	UINT8	rsv:3;/*momi power safe */
	UINT8	MpduDensity:3;
	UINT8	MaxRAmpduFactor:2;
#else
	UINT8	MaxRAmpduFactor:2;
	UINT8	MpduDensity:3;
	UINT8	rsv:3;/*momi power safe */
#endif /* __BIG_ENDIAN */
} HT_CAP_PARM, *PHT_CAP_PARM;


typedef struct GNU_PACKED _HT_MCS_SET_TX_SUBFIELD{
#ifdef __BIG_ENDIAN
	UINT8	TxMCSSetDefined:1;
	UINT8	TxRxNotEqual:1;
	UINT8	TxMaxStream:2;
	UINT8	TxUnqualModulation:1;
	UINT8	rsv:3;
#else
	UINT8	rsv:3;
	UINT8	TxUnqualModulation:1;
	UINT8	TxMaxStream:2;
	UINT8	TxRxNotEqual:1;
	UINT8	TxMCSSetDefined:1;
#endif /* __BIG_ENDIAN */
}HT_MCS_SET_TX_SUBFIELD, *PHT_MCS_SET_TX_SUBFIELD;


/*  HT Capability INFO field in HT Cap IE . */
typedef struct GNU_PACKED _HT_MCS_SET{
	UINT8	MCSSet[10];
	UINT8	SupRate[2];  /* unit : 1Mbps */
#ifdef __BIG_ENDIAN
	UINT8	rsv:3;
	UINT8	MpduDensity:1;
	UINT8	TxStream:2;
	UINT8	TxRxNotEqual:1;
	UINT8	TxMCSSetDefined:1;
#else
	UINT8	TxMCSSetDefined:1;
	UINT8	TxRxNotEqual:1;
	UINT8	TxStream:2;
	UINT8	MpduDensity:1;
	UINT8	rsv:3;
#endif /* __BIG_ENDIAN */
	UINT8	rsv3[3];
} HT_MCS_SET, *PHT_MCS_SET;

/*  HT Capability INFO field in HT Cap IE . */
typedef struct GNU_PACKED _EXT_HT_CAP_INFO{
#ifdef __BIG_ENDIAN
	uint16_t rsv2:4;
	uint16_t RDGSupport:1;	/*reverse Direction Grant  support */
	uint16_t PlusHTC:1;	/*+HTC control field support */
	uint16_t MCSFeedback:2;	/*0:no MCS feedback, 2:unsolicited MCS feedback, 3:Full MCS feedback,  1:rsv. */
	uint16_t rsv:5;/*momi power safe */
	uint16_t TranTime:2;
	uint16_t Pco:1;
#else
	uint16_t Pco:1;
	uint16_t TranTime:2;
	uint16_t rsv:5;/*momi power safe */
	uint16_t MCSFeedback:2;	/*0:no MCS feedback, 2:unsolicited MCS feedback, 3:Full MCS feedback,  1:rsv. */
	uint16_t PlusHTC:1;	/*+HTC control field support */
	uint16_t RDGSupport:1;	/*reverse Direction Grant  support */
	uint16_t rsv2:4;
#endif /* __BIG_ENDIAN */
} EXT_HT_CAP_INFO, *PEXT_HT_CAP_INFO;


/* HT Explicit Beamforming Feedback Capable */
#define HT_ExBF_FB_CAP_NONE			0
#define HT_ExBF_FB_CAP_DELAYED		1
#define HT_ExBF_FB_CAP_IMMEDIATE		2
#define HT_ExBF_FB_CAP_BOTH			3

/* HT Beamforming field in HT Cap IE */
typedef struct GNU_PACKED _HT_BF_CAP{
#ifdef __BIG_ENDIAN
	uint32_t rsv:3;
	uint32_t ChanEstimation:2;
	uint32_t CSIRowBFSup:2;
	uint32_t ComSteerBFAntSup:2;
	uint32_t NoComSteerBFAntSup:2;
	uint32_t CSIBFAntSup:2;
	uint32_t MinGrouping:2;
	uint32_t ExpComBF:2;
	uint32_t ExpNoComBF:2;
	uint32_t ExpCSIFbk:2;
	uint32_t ExpComSteerCapable:1;
	uint32_t ExpNoComSteerCapable:1;
	uint32_t ExpCSICapable:1;
	uint32_t Calibration:2;
	uint32_t ImpTxBFCapable:1;
	uint32_t TxNDPCapable:1;
	uint32_t RxNDPCapable:1;
	uint32_t TxSoundCapable:1;
	uint32_t RxSoundCapable:1;
	uint32_t TxBFRecCapable:1;
#else
	uint32_t TxBFRecCapable:1;
	uint32_t RxSoundCapable:1;
	uint32_t TxSoundCapable:1;
	uint32_t RxNDPCapable:1;
	uint32_t TxNDPCapable:1;
	uint32_t ImpTxBFCapable:1;
	uint32_t Calibration:2;
	uint32_t ExpCSICapable:1;
	uint32_t ExpNoComSteerCapable:1;
	uint32_t ExpComSteerCapable:1;
	uint32_t ExpCSIFbk:2;
	uint32_t ExpNoComBF:2;
	uint32_t ExpComBF:2;
	uint32_t MinGrouping:2;
	uint32_t CSIBFAntSup:2;
	uint32_t NoComSteerBFAntSup:2;
	uint32_t ComSteerBFAntSup:2;
	uint32_t CSIRowBFSup:2;
	uint32_t ChanEstimation:2;
	uint32_t rsv:3;
#endif /* __BIG_ENDIAN */
} HT_BF_CAP, *PHT_BF_CAP;

/*  HT antenna selection field in HT Cap IE . */
typedef struct GNU_PACKED _HT_AS_CAP{
#ifdef __BIG_ENDIAN
	UINT8	rsv:1;
	UINT8	TxSoundPPDU:1;
	UINT8	RxASel:1;
	UINT8	AntIndFbk:1;
	UINT8	ExpCSIFbk:1;
	UINT8	AntIndFbkTxASEL:1;
	UINT8	ExpCSIFbkTxASEL:1;
	UINT8	AntSelect:1;
#else
	UINT8	AntSelect:1;
	UINT8	ExpCSIFbkTxASEL:1;
	UINT8	AntIndFbkTxASEL:1;
	UINT8	ExpCSIFbk:1;
	UINT8	AntIndFbk:1;
	UINT8	RxASel:1;
	UINT8	TxSoundPPDU:1;
	UINT8	rsv:1;
#endif /* __BIG_ENDIAN */
} HT_AS_CAP, *PHT_AS_CAP;


/* Draft 1.0 set IE length 26, but is extensible.. */
#define SIZE_HT_CAP_IE		26
/* The structure for HT Capability IE. */
typedef struct GNU_PACKED _HT_CAPABILITY_IE{
	HT_CAP_INFO		HtCapInfo;
	HT_CAP_PARM		HtCapParm;
/*	HT_MCS_SET		HtMCSSet; */
	u8 		MCSSet[16];
	EXT_HT_CAP_INFO	ExtHtCapInfo;
	HT_BF_CAP		TxBFCap;	/* beamforming cap. rt2860c not support beamforming. */
	HT_AS_CAP		ASCap;	/*antenna selection. */
} HT_CAPABILITY_IE, *PHT_CAPABILITY_IE;


/*   field in Addtional HT Information IE . */
typedef struct GNU_PACKED _ADD_HTINFO{
#ifdef __BIG_ENDIAN
	u8 SerInterGranu:3;
	u8 S_PSMPSup:1;
	u8 RifsMode:1;
	u8 RecomWidth:1;
	u8 ExtChanOffset:2;
#else
	u8 ExtChanOffset:2;
	u8 RecomWidth:1;
	u8 RifsMode:1;
	u8 S_PSMPSup:1;	 /*Indicate support for scheduled PSMP */
	u8 SerInterGranu:3;	 /*service interval granularity */
#endif
} ADD_HTINFO, *PADD_HTINFO;


typedef struct GNU_PACKED _ADD_HTINFO2{
#ifdef __BIG_ENDIAN
	unsigned short rsv2:11;
	unsigned short OBSS_NonHTExist:1;
	unsigned short rsv:1;
	unsigned short NonGfPresent:1;
	unsigned short OperaionMode:2;
#else
	unsigned short OperaionMode:2;
	unsigned short NonGfPresent:1;
	unsigned short rsv:1;
	unsigned short OBSS_NonHTExist:1;
	unsigned short rsv2:11;
#endif
} ADD_HTINFO2, *PADD_HTINFO2;


/* TODO: Need sync with spec about the definition of StbcMcs. In Draft 3.03, it's reserved. */
typedef struct GNU_PACKED _ADD_HTINFO3{
#ifdef __BIG_ENDIAN
	unsigned short rsv:4;
	unsigned short PcoPhase:1;
	unsigned short PcoActive:1;
	unsigned short LsigTxopProt:1;
	unsigned short STBCBeacon:1;
	unsigned short DualCTSProtect:1;
	unsigned short DualBeacon:1;
	unsigned short StbcMcs:6;
#else
	unsigned short StbcMcs:6;
	unsigned short DualBeacon:1;
	unsigned short DualCTSProtect:1;
	unsigned short STBCBeacon:1;
	unsigned short LsigTxopProt:1;	/* L-SIG TXOP protection full support */
	unsigned short PcoActive:1;
	unsigned short PcoPhase:1;
	unsigned short rsv:4;
#endif /* __BIG_ENDIAN */
} ADD_HTINFO3, *PADD_HTINFO3;

#define SIZE_ADD_HT_INFO_IE		22
typedef struct  GNU_PACKED _ADD_HT_INFO_IE{
	u8 			ControlChan;
	ADD_HTINFO			AddHtInfo;
	ADD_HTINFO2			AddHtInfo2;
	ADD_HTINFO3			AddHtInfo3;
	u8 			MCSSet[16];		/* Basic MCS set */
} ADD_HT_INFO_IE, *PADD_HT_INFO_IE;


/* 802.11n draft3 related structure definitions. */
/* 7.3.2.60 */
#define dot11OBSSScanPassiveDwell							20	/* in TU. min amount of time that the STA continously scans each channel when performing an active OBSS scan. */
#define dot11OBSSScanActiveDwell							10	/* in TU.min amount of time that the STA continously scans each channel when performing an passive OBSS scan. */
#define dot11BSSWidthTriggerScanInterval					300  /* in sec. max interval between scan operations to be performed to detect BSS channel width trigger events. */
#define dot11OBSSScanPassiveTotalPerChannel					200	/* in TU. min total amount of time that the STA scans each channel when performing a passive OBSS scan. */
#define dot11OBSSScanActiveTotalPerChannel					20	/*in TU. min total amount of time that the STA scans each channel when performing a active OBSS scan */
#define dot11BSSWidthChannelTransactionDelayFactor			5	/* min ratio between the delay time in performing a switch from 20MHz BSS to 20/40 BSS operation and the maximum */
																/*	interval between overlapping BSS scan operations. */
#define dot11BSSScanActivityThreshold						25	/* in %%, max total time that a STA may be active on the medium during a period of */
																/*	(dot11BSSWidthChannelTransactionDelayFactor * dot11BSSWidthTriggerScanInterval) seconds without */
																/*	being obligated to perform OBSS Scan operations. default is 25(== 0.25%) */

typedef struct GNU_PACKED _OVERLAP_BSS_SCAN_IE{
	unsigned short 	ScanPassiveDwell;
	unsigned short 	ScanActiveDwell;
	unsigned short 	TriggerScanInt;				/* Trigger scan interval */
	unsigned short 	PassiveTalPerChannel;		/* passive total per channel */
	unsigned short 	ActiveTalPerChannel;		/* active total per channel */
	unsigned short 	DelayFactor;				/* BSS width channel transition delay factor */
	unsigned short 	ScanActThre;				/* Scan Activity threshold */
}OVERLAP_BSS_SCAN_IE, *POVERLAP_BSS_SCAN_IE;


/*  7.3.2.56. 20/40 Coexistence element used in  Element ID = 72 = IE_2040_BSS_COEXIST */
typedef union GNU_PACKED _BSS_2040_COEXIST_IE{
 struct GNU_PACKED {
 #ifdef __BIG_ENDIAN
	u8 rsv:3;
 	u8 ObssScanExempGrant:1;
	u8 ObssScanExempReq:1;
	u8 BSS20WidthReq:1;
	u8 Intolerant40:1;
	u8 InfoReq:1;
 #else
	u8 InfoReq:1;
	u8 Intolerant40:1;			/* Inter-BSS. set 1 when prohibits a receiving BSS from operating as a 20/40 Mhz BSS. */
	u8 BSS20WidthReq:1;		/* Intra-BSS set 1 when prohibits a receiving AP from operating its BSS as a 20/40MHz BSS. */
	u8 ObssScanExempReq:1;
	u8 ObssScanExempGrant:1;
	u8 rsv:3;
#endif /* __BIG_ENDIAN */
    } field;
 u8   word;
} BSS_2040_COEXIST_IE, *PBSS_2040_COEXIST_IE;


#endif /* _DOT11N_HT_H_ */


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
	sanity.c

	Abstract:

	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
	John Chang  2004-09-01      add WMM support
*/
#include "rt_config.h"

extern u8 CISCO_OUI[];

extern u8 WPA_OUI[];
extern u8 RSN_OUI[];
extern u8 WME_INFO_ELEM[];
extern u8 WME_PARM_ELEM[];
extern u8 RALINK_OUI[];
extern u8 BROADCOM_OUI[];
extern u8    WPS_OUI[];



typedef struct wsc_ie_probreq_data
{
	u8 ssid[32];
	u8 macAddr[6];
	u8 data[2];
} WSC_IE_PROBREQ_DATA;

/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise

	IRQL = DISPATCH_LEVEL

    ==========================================================================
 */
bool MlmeAddBAReqSanity(
    IN struct rtmp_adapter *pAd,
    IN VOID *Msg,
    IN ULONG MsgLen,
    OUT u8 *pAddr2)
{
    PMLME_ADDBA_REQ_STRUCT   pInfo;

    pInfo = (MLME_ADDBA_REQ_STRUCT *)Msg;

    if ((MsgLen != sizeof(MLME_ADDBA_REQ_STRUCT)))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("MlmeAddBAReqSanity fail - message lenght not correct.\n"));
        return false;
    }

    if ((pInfo->Wcid >= MAX_LEN_OF_MAC_TABLE))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("MlmeAddBAReqSanity fail - The peer Mac is not associated yet.\n"));
        return false;
    }

	/*
    if ((pInfo->BaBufSize > MAX_RX_REORDERBUF) || (pInfo->BaBufSize < 2))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("MlmeAddBAReqSanity fail - Rx Reordering buffer too big or too small\n"));
        return false;
    }
	*/

    if ((pInfo->pAddr[0]&0x01) == 0x01)
    {
        DBGPRINT(RT_DEBUG_TRACE, ("MlmeAddBAReqSanity fail - broadcast address not support BA\n"));
        return false;
    }

    return true;
}

/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise

	IRQL = DISPATCH_LEVEL

    ==========================================================================
 */
bool MlmeDelBAReqSanity(
    IN struct rtmp_adapter *pAd,
    IN VOID *Msg,
    IN ULONG MsgLen)
{
	MLME_DELBA_REQ_STRUCT *pInfo;
	pInfo = (MLME_DELBA_REQ_STRUCT *)Msg;

    if ((MsgLen != sizeof(MLME_DELBA_REQ_STRUCT)))
    {
        DBGPRINT(RT_DEBUG_ERROR, ("MlmeDelBAReqSanity fail - message lenght not correct.\n"));
        return false;
    }

    if ((pInfo->Wcid >= MAX_LEN_OF_MAC_TABLE))
    {
        DBGPRINT(RT_DEBUG_ERROR, ("MlmeDelBAReqSanity fail - The peer Mac is not associated yet.\n"));
        return false;
    }

    if ((pInfo->TID & 0xf0))
    {
        DBGPRINT(RT_DEBUG_ERROR, ("MlmeDelBAReqSanity fail - The peer TID is incorrect.\n"));
        return false;
    }

	if (!memcmp(pAd->MacTab.Content[pInfo->Wcid].Addr, pInfo->Addr, MAC_ADDR_LEN) == 0)
    {
        DBGPRINT(RT_DEBUG_ERROR, ("MlmeDelBAReqSanity fail - the peer addr dosen't exist.\n"));
        return false;
    }

    return true;
}


bool PeerAddBAReqActionSanity(
    IN struct rtmp_adapter *pAd,
    IN VOID *pMsg,
    IN ULONG MsgLen,
	OUT u8 *pAddr2)
{
	PFRAME_802_11 pFrame = (PFRAME_802_11)pMsg;
	PFRAME_ADDBA_REQ pAddFrame;
	pAddFrame = (PFRAME_ADDBA_REQ)(pMsg);
	if (MsgLen < (sizeof(FRAME_ADDBA_REQ)))
	{
		DBGPRINT(RT_DEBUG_ERROR,("PeerAddBAReqActionSanity: ADDBA Request frame length size = %ld incorrect\n", MsgLen));
		return false;
	}
	/* we support immediate BA.*/
#ifdef UNALIGNMENT_SUPPORT
	{
		BA_PARM		tmpBaParm;

		memmove(&tmpBaParm, &pAddFrame->BaParm, sizeof(BA_PARM));
		*(unsigned short *)(&tmpBaParm) = cpu2le16(*(unsigned short *)(&tmpBaParm));
		memmove(&pAddFrame->BaParm, &tmpBaParm, sizeof(BA_PARM));
	}
#else
	*(unsigned short *)(&pAddFrame->BaParm) = cpu2le16(*(unsigned short *)(&pAddFrame->BaParm));
#endif
	pAddFrame->TimeOutValue = cpu2le16(pAddFrame->TimeOutValue);
	pAddFrame->BaStartSeq.word = cpu2le16(pAddFrame->BaStartSeq.word);

	COPY_MAC_ADDR(pAddr2, pFrame->Hdr.Addr2);

	if (pAddFrame->BaParm.BAPolicy != IMMED_BA)
	{
		DBGPRINT(RT_DEBUG_ERROR,("PeerAddBAReqActionSanity: ADDBA Request Ba Policy[%d] not support\n", pAddFrame->BaParm.BAPolicy));
		DBGPRINT(RT_DEBUG_ERROR,("ADDBA Request. tid=%x, Bufsize=%x, AMSDUSupported=%x \n", pAddFrame->BaParm.TID, pAddFrame->BaParm.BufSize, pAddFrame->BaParm.AMSDUSupported));
		return false;
	}

	return true;
}

bool PeerAddBARspActionSanity(
    IN struct rtmp_adapter *pAd,
    IN VOID *pMsg,
    IN ULONG MsgLen)
{
	PFRAME_ADDBA_RSP pAddFrame;

	pAddFrame = (PFRAME_ADDBA_RSP)(pMsg);
	if (MsgLen < (sizeof(FRAME_ADDBA_RSP)))
	{
		DBGPRINT(RT_DEBUG_ERROR,("%s(): ADDBA Resp frame length incorrect(len=%ld)\n", __FUNCTION__, MsgLen));
		return false;
	}
	/* we support immediate BA.*/
#ifdef UNALIGNMENT_SUPPORT
	{
		BA_PARM		tmpBaParm;

		memmove(&tmpBaParm, &pAddFrame->BaParm, sizeof(BA_PARM));
		*(unsigned short *)(&tmpBaParm) = cpu2le16(*(unsigned short *)(&tmpBaParm));
		memmove(&pAddFrame->BaParm, &tmpBaParm, sizeof(BA_PARM));
	}
#else
	*(unsigned short *)(&pAddFrame->BaParm) = cpu2le16(*(unsigned short *)(&pAddFrame->BaParm));
#endif
	pAddFrame->StatusCode = cpu2le16(pAddFrame->StatusCode);
	pAddFrame->TimeOutValue = cpu2le16(pAddFrame->TimeOutValue);

	if (pAddFrame->BaParm.BAPolicy != IMMED_BA)
	{
		DBGPRINT(RT_DEBUG_ERROR,("%s(): ADDBA Resp Ba Policy[%d] not support\n", __FUNCTION__, pAddFrame->BaParm.BAPolicy));
		return false;
	}

	return true;

}

bool PeerDelBAActionSanity(
    IN struct rtmp_adapter *pAd,
    IN u8 Wcid,
    IN VOID *pMsg,
    IN ULONG MsgLen )
{
	PFRAME_DELBA_REQ  pDelFrame;

	if (MsgLen != (sizeof(FRAME_DELBA_REQ)))
		return false;

	if (Wcid >= MAX_LEN_OF_MAC_TABLE)
		return false;

	pDelFrame = (PFRAME_DELBA_REQ)(pMsg);

	*(unsigned short *)(&pDelFrame->DelbaParm) = cpu2le16(*(unsigned short *)(&pDelFrame->DelbaParm));
	pDelFrame->ReasonCode = cpu2le16(pDelFrame->ReasonCode);

	return true;
}


bool PeerBeaconAndProbeRspSanity_Old(
    IN struct rtmp_adapter *pAd,
    IN VOID *Msg,
    IN ULONG MsgLen,
    IN u8  MsgChannel,
    OUT u8 *pAddr2,
    OUT u8 *pBssid,
    OUT CHAR Ssid[],
    OUT u8 *pSsidLen,
    OUT u8 *pBssType,
    OUT unsigned short *pBeaconPeriod,
    OUT u8 *pChannel,
    OUT u8 *pNewChannel,
    OUT LARGE_INTEGER *pTimestamp,
    OUT CF_PARM *pCfParm,
    OUT unsigned short *pAtimWin,
    OUT unsigned short *pCapabilityInfo,
    OUT u8 *pErp,
    OUT u8 *pDtimCount,
    OUT u8 *pDtimPeriod,
    OUT u8 *pBcastFlag,
    OUT u8 *pMessageToMe,
    OUT u8 SupRate[],
    OUT u8 *pSupRateLen,
    OUT u8 ExtRate[],
    OUT u8 *pExtRateLen,
    OUT u8 *pCkipFlag,
    OUT u8 *pAironetCellPowerLimit,
    OUT PEDCA_PARM pEdcaParm,
    OUT PQBSS_LOAD_PARM pQbssLoad,
    OUT PQOS_CAPABILITY_PARM pQosCapability,
    OUT ULONG *pRalinkIe,
    OUT u8 *pHtCapabilityLen,
#ifdef CONFIG_STA_SUPPORT
    OUT u8 *pPreNHtCapabilityLen,
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
    OUT u8 *pSelReg,
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* CONFIG_STA_SUPPORT */
    OUT HT_CAPABILITY_IE *pHtCapability,
    OUT EXT_CAP_INFO_ELEMENT	*pExtCapInfo,
    OUT u8 *AddHtInfoLen,
    OUT ADD_HT_INFO_IE *AddHtInfo,
    OUT u8 *NewExtChannelOffset,		/* Ht extension channel offset(above or below)*/
    OUT unsigned short *LengthVIE,
    OUT PNDIS_802_11_VARIABLE_IEs pVIE)
{
    u8 			*Ptr;
#ifdef CONFIG_STA_SUPPORT
	u8 				TimLen;
#endif /* CONFIG_STA_SUPPORT */
    PFRAME_802_11		pFrame;
    PEID_STRUCT         pEid;
    u8 			SubType;
    u8 			Sanity;
    /*u8 			ECWMin, ECWMax;*/
    /*MAC_CSR9_STRUC		Csr9;*/
    ULONG				Length = 0;
	u8 			*pPeerWscIe = NULL;
	INT					PeerWscIeLen = 0;
	bool				bWscCheck = true;
    u8 			LatchRfChannel = 0;


	/*
		For some 11a AP which didn't have DS_IE, we use two conditions to decide the channel
		1. If the AP is 11n enabled, then check the control channel.
		2. If the AP didn't have any info about channel, use the channel we received this
			frame as the channel. (May inaccuracy!!)
	*/
	u8 		CtrlChannel = 0;


	pPeerWscIe = kmalloc(512, GFP_ATOMIC);
    /* Add for 3 necessary EID field check*/
    Sanity = 0;

    *pAtimWin = 0;
    *pErp = 0;
    *pDtimCount = 0;
    *pDtimPeriod = 0;
    *pBcastFlag = 0;
    *pMessageToMe = 0;
    *pExtRateLen = 0;
    *pCkipFlag = 0;			        /* Default of CkipFlag is 0*/
    *pAironetCellPowerLimit = 0xFF;  /* Default of AironetCellPowerLimit is 0xFF*/
    *LengthVIE = 0;					/* Set the length of VIE to init value 0*/
    *pHtCapabilityLen = 0;					/* Set the length of VIE to init value 0*/
#ifdef CONFIG_STA_SUPPORT
	if (pAd->OpMode == OPMODE_STA)
		*pPreNHtCapabilityLen = 0;					/* Set the length of VIE to init value 0*/
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
    *pSelReg = 0;
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* CONFIG_STA_SUPPORT */
    *AddHtInfoLen = 0;					/* Set the length of VIE to init value 0*/
    memset(pExtCapInfo, 0, sizeof(EXT_CAP_INFO_ELEMENT));
    *pRalinkIe = 0;
    *pNewChannel = 0;
    *NewExtChannelOffset = 0xff;	/*Default 0xff means no such IE*/
    pCfParm->bValid = false;        /* default: no IE_CF found*/
    pQbssLoad->bValid = false;      /* default: no IE_QBSS_LOAD found*/
    pEdcaParm->bValid = false;      /* default: no IE_EDCA_PARAMETER found*/
    pQosCapability->bValid = false; /* default: no IE_QOS_CAPABILITY found*/

    pFrame = (PFRAME_802_11)Msg;

    /* get subtype from header*/
    SubType = (u8)pFrame->Hdr.FC.SubType;

    /* get Addr2 and BSSID from header*/
    COPY_MAC_ADDR(pAddr2, pFrame->Hdr.Addr2);
    COPY_MAC_ADDR(pBssid, pFrame->Hdr.Addr3);

/*	hex_dump("Beacon", Msg, MsgLen);*/

    Ptr = pFrame->Octet;
    Length += LENGTH_802_11;

    /* get timestamp from payload and advance the pointer*/
    memmove(pTimestamp, Ptr, TIMESTAMP_LEN);

	pTimestamp->u.LowPart = cpu2le32(pTimestamp->u.LowPart);
	pTimestamp->u.HighPart = cpu2le32(pTimestamp->u.HighPart);

    Ptr += TIMESTAMP_LEN;
    Length += TIMESTAMP_LEN;

    /* get beacon interval from payload and advance the pointer*/
    memmove(pBeaconPeriod, Ptr, 2);
    Ptr += 2;
    Length += 2;

    /* get capability info from payload and advance the pointer*/
    memmove(pCapabilityInfo, Ptr, 2);
    Ptr += 2;
    Length += 2;

    if (CAP_IS_ESS_ON(*pCapabilityInfo))
        *pBssType = BSS_INFRA;
    else
        *pBssType = BSS_ADHOC;

    pEid = (PEID_STRUCT) Ptr;

    /* get variable fields from payload and advance the pointer*/
    while ((Length + 2 + pEid->Len) <= MsgLen)
    {

        /* Secure copy VIE to VarIE[MAX_VIE_LEN] didn't overflow.*/
        if ((*LengthVIE + pEid->Len + 2) >= MAX_VIE_LEN)
        {
            DBGPRINT(RT_DEBUG_WARN, ("%s() - Variable IEs out of resource [len(=%d) > MAX_VIE_LEN(=%d)]\n",
                    __FUNCTION__, (*LengthVIE + pEid->Len + 2), MAX_VIE_LEN));
            break;
        }

        switch(pEid->Eid)
        {
            case IE_SSID:
                /* Already has one SSID EID in this beacon, ignore the second one*/
                if (Sanity & 0x1)
                    break;
                if(pEid->Len <= MAX_LEN_OF_SSID)
                {
                    memmove(Ssid, pEid->Octet, pEid->Len);
                    *pSsidLen = pEid->Len;
                    Sanity |= 0x1;
                }
                else
                {
                    DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_SSID (len=%d)\n", __FUNCTION__, pEid->Len));
                    goto SanityCheck;
                }
                break;

            case IE_SUPP_RATES:
                if(pEid->Len <= MAX_LEN_OF_SUPPORTED_RATES)
                {
                    Sanity |= 0x2;
                    memmove(SupRate, pEid->Octet, pEid->Len);
                    *pSupRateLen = pEid->Len;

                    /*
						TODO: 2004-09-14 not a good design here, cause it exclude extra
							rates from ScanTab. We should report as is. And filter out
							unsupported rates in MlmeAux
					*/
                    /* Check against the supported rates*/
                    /* RTMPCheckRates(pAd, SupRate, pSupRateLen);*/
                }
                else
                {
                    DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_SUPP_RATES (len=%d)\n",__FUNCTION__, pEid->Len));
                    goto SanityCheck;
                }
                break;

            case IE_HT_CAP:
			if (pEid->Len >= SIZE_HT_CAP_IE)  /*Note: allow extension.!!*/
			{
				memmove(pHtCapability, pEid->Octet, sizeof(HT_CAPABILITY_IE));
				*pHtCapabilityLen = SIZE_HT_CAP_IE;	/* Nnow we only support 26 bytes.*/

				*(unsigned short *)(&pHtCapability->HtCapInfo) = cpu2le16(*(unsigned short *)(&pHtCapability->HtCapInfo));
#ifdef UNALIGNMENT_SUPPORT
				{
					EXT_HT_CAP_INFO extHtCapInfo;
					memmove(&extHtCapInfo, &pHtCapability->ExtHtCapInfo, sizeof(EXT_HT_CAP_INFO));
					*(unsigned short *)(&extHtCapInfo) = cpu2le16(*(unsigned short *)(&extHtCapInfo));
					memmove(&pHtCapability->ExtHtCapInfo, &extHtCapInfo, sizeof(EXT_HT_CAP_INFO));
				}
#else
				*(unsigned short *)(&pHtCapability->ExtHtCapInfo) = cpu2le16(*(unsigned short *)(&pHtCapability->ExtHtCapInfo));
#endif /* UNALIGNMENT_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
				IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
				{
					*pPreNHtCapabilityLen = 0;	/* Now we only support 26 bytes.*/

					Ptr = (u8 *) pVIE;
					memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
					*LengthVIE += (pEid->Len + 2);
				}
#endif /* CONFIG_STA_SUPPORT */
			}
			else
			{
				DBGPRINT(RT_DEBUG_WARN, ("%s() - wrong IE_HT_CAP. pEid->Len = %d\n", __FUNCTION__, pEid->Len));
			}

		break;
            case IE_ADD_HT:
			if (pEid->Len >= sizeof(ADD_HT_INFO_IE))
			{
				/*
					This IE allows extension, but we can ignore extra bytes beyond our
					knowledge , so only copy first sizeof(ADD_HT_INFO_IE)
				*/
				memmove(AddHtInfo, pEid->Octet, sizeof(ADD_HT_INFO_IE));
				*AddHtInfoLen = SIZE_ADD_HT_INFO_IE;

				CtrlChannel = AddHtInfo->ControlChan;

				*(unsigned short *)(&AddHtInfo->AddHtInfo2) = cpu2le16(*(unsigned short *)(&AddHtInfo->AddHtInfo2));
				*(unsigned short *)(&AddHtInfo->AddHtInfo3) = cpu2le16(*(unsigned short *)(&AddHtInfo->AddHtInfo3));

#ifdef CONFIG_STA_SUPPORT
				IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
				{
			                Ptr = (u8 *) pVIE;
			                memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
			                *LengthVIE += (pEid->Len + 2);
				}
#endif /* CONFIG_STA_SUPPORT */
			}
			else
			{
				DBGPRINT(RT_DEBUG_WARN, ("%s() - wrong IE_ADD_HT. \n", __FUNCTION__));
			}

		break;
            case IE_SECONDARY_CH_OFFSET:
			if (pEid->Len == 1)
			{
				*NewExtChannelOffset = pEid->Octet[0];
			}
			else
			{
				DBGPRINT(RT_DEBUG_WARN, ("%s() - wrong IE_SECONDARY_CH_OFFSET. \n", __FUNCTION__));
			}

		break;
            case IE_FH_PARM:
                DBGPRINT(RT_DEBUG_TRACE, ("%s(IE_FH_PARM) \n", __FUNCTION__));
                break;

            case IE_DS_PARM:
                if(pEid->Len == 1)
                {
                    *pChannel = *pEid->Octet;
#ifdef CONFIG_STA_SUPPORT
					IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
					{
						if (ChannelSanity(pAd, *pChannel) == 0)
						{

							goto SanityCheck;
						}
					}
#endif /* CONFIG_STA_SUPPORT */
                    Sanity |= 0x4;
                }
                else
                {
                    DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_DS_PARM (len=%d)\n",__FUNCTION__,pEid->Len));
                    goto SanityCheck;
                }
                break;

            case IE_CF_PARM:
                if(pEid->Len == 6)
                {
                    pCfParm->bValid = true;
                    pCfParm->CfpCount = pEid->Octet[0];
                    pCfParm->CfpPeriod = pEid->Octet[1];
                    pCfParm->CfpMaxDuration = pEid->Octet[2] + 256 * pEid->Octet[3];
                    pCfParm->CfpDurRemaining = pEid->Octet[4] + 256 * pEid->Octet[5];
                }
                else
                {
                    DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_CF_PARM\n", __FUNCTION__));
					if (pPeerWscIe)
						kfree(pPeerWscIe);
                    return false;
                }
                break;

            case IE_IBSS_PARM:
                if(pEid->Len == 2)
                {
                    memmove(pAtimWin, pEid->Octet, pEid->Len);
                }
                else
                {
                    DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_IBSS_PARM\n", __FUNCTION__));
					if (pPeerWscIe)
						kfree(pPeerWscIe);
                    return false;
                }
                break;

#ifdef CONFIG_STA_SUPPORT
            case IE_TIM:
                if(SubType == SUBTYPE_BEACON)
                {

					if (INFRA_ON(pAd) && !memcmp(pBssid, pAd->CommonCfg.Bssid, MAC_ADDR_LEN))
                    {
                        GetTimBit((PCHAR)pEid, pAd->StaActive.Aid, &TimLen, pBcastFlag, pDtimCount, pDtimPeriod, pMessageToMe);
                    }
                }
                break;
#endif /* CONFIG_STA_SUPPORT */
            case IE_CHANNEL_SWITCH_ANNOUNCEMENT:
                if(pEid->Len == 3)
                {
                	*pNewChannel = pEid->Octet[1];	/*extract new channel number*/
                }
                break;

            /*
				New for WPA
				CCX v2 has the same IE, we need to parse that too
				Wifi WMM use the same IE vale, need to parse that too
			*/
            /* case IE_WPA:*/
            case IE_VENDOR_SPECIFIC:
                /* Check the OUI version, filter out non-standard usage*/
                if (!memcmp(pEid->Octet, RALINK_OUI, 3) && (pEid->Len == 7))
                {
			if (pEid->Octet[3] != 0)
        				*pRalinkIe = pEid->Octet[3];
        			else
        				*pRalinkIe = 0xf0000000; /* Set to non-zero value (can't set bit0-2) to represent this is Ralink Chip. So at linkup, we will set ralinkchip flag.*/
                }
#ifdef CONFIG_STA_SUPPORT
		/* This HT IE is before IEEE draft set HT IE value.2006-09-28 by Jan.*/

                /* Other vendors had production before IE_HT_CAP value is assigned. To backward support those old-firmware AP,*/
                /* Check broadcom-defiend pre-802.11nD1.0 OUI for HT related IE, including HT Capatilities IE and HT Information IE*/
                else if ((*pHtCapabilityLen == 0) && !memcmp(pEid->Octet, BROADCOM_OUI, 3) && (pEid->Len >= 4) && (pAd->OpMode == OPMODE_STA))
                {
                    if ((pEid->Octet[3] == OUI_BROADCOM_HT) && (pEid->Len >= 30) && (*pHtCapabilityLen == 0))
                    {
                        memmove(pHtCapability, &pEid->Octet[4], sizeof(HT_CAPABILITY_IE));
                        *pPreNHtCapabilityLen = SIZE_HT_CAP_IE;
                    }

                    if ((pEid->Octet[3] == OUI_PREN_ADD_HT) && (pEid->Len >= 26))
                    {
                        memmove(AddHtInfo, &pEid->Octet[4], sizeof(ADD_HT_INFO_IE));
                        *AddHtInfoLen = SIZE_ADD_HT_INFO_IE;
                    }
                }
#endif /* CONFIG_STA_SUPPORT */
                else if (!memcmp(pEid->Octet, WPA_OUI, 4))
                {
                    /* Copy to pVIE which will report to bssid list.*/
                    Ptr = (u8 *) pVIE;
                    memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
                    *LengthVIE += (pEid->Len + 2);
                }
                else if (!memcmp(pEid->Octet, WME_PARM_ELEM, 6) && (pEid->Len == 24))
                {
                    u8 *ptr;
                    int i;

                    /* parsing EDCA parameters*/
                    pEdcaParm->bValid          = true;
                    pEdcaParm->bQAck           = false; /* pEid->Octet[0] & 0x10;*/
                    pEdcaParm->bQueueRequest   = false; /* pEid->Octet[0] & 0x20;*/
                    pEdcaParm->bTxopRequest    = false; /* pEid->Octet[0] & 0x40;*/
                    pEdcaParm->EdcaUpdateCount = pEid->Octet[6] & 0x0f;
                    pEdcaParm->bAPSDCapable    = (pEid->Octet[6] & 0x80) ? 1 : 0;
                    ptr = &pEid->Octet[8];
                    for (i=0; i<4; i++)
                    {
                        u8 aci = (*ptr & 0x60) >> 5; /* b5~6 is AC INDEX*/
                        pEdcaParm->bACM[aci]  = (((*ptr) & 0x10) == 0x10);   /* b5 is ACM*/
                        pEdcaParm->Aifsn[aci] = (*ptr) & 0x0f;               /* b0~3 is AIFSN*/
                        pEdcaParm->Cwmin[aci] = *(ptr+1) & 0x0f;             /* b0~4 is Cwmin*/
                        pEdcaParm->Cwmax[aci] = *(ptr+1) >> 4;               /* b5~8 is Cwmax*/
                        pEdcaParm->Txop[aci]  = *(ptr+2) + 256 * (*(ptr+3)); /* in unit of 32-us*/
                        ptr += 4; /* point to next AC*/
                    }
                }
                else if (!memcmp(pEid->Octet, WME_INFO_ELEM, 6) && (pEid->Len == 7))
                {
                    /* parsing EDCA parameters*/
                    pEdcaParm->bValid          = true;
                    pEdcaParm->bQAck           = false; /* pEid->Octet[0] & 0x10;*/
                    pEdcaParm->bQueueRequest   = false; /* pEid->Octet[0] & 0x20;*/
                    pEdcaParm->bTxopRequest    = false; /* pEid->Octet[0] & 0x40;*/
                    pEdcaParm->EdcaUpdateCount = pEid->Octet[6] & 0x0f;
                    pEdcaParm->bAPSDCapable    = (pEid->Octet[6] & 0x80) ? 1 : 0;

                    /* use default EDCA parameter*/
                    pEdcaParm->bACM[QID_AC_BE]  = 0;
                    pEdcaParm->Aifsn[QID_AC_BE] = 3;
                    pEdcaParm->Cwmin[QID_AC_BE] = pAd->wmm_cw_min;
                    pEdcaParm->Cwmax[QID_AC_BE] = pAd->wmm_cw_max;
                    pEdcaParm->Txop[QID_AC_BE]  = 0;

                    pEdcaParm->bACM[QID_AC_BK]  = 0;
                    pEdcaParm->Aifsn[QID_AC_BK] = 7;
                    pEdcaParm->Cwmin[QID_AC_BK] = pAd->wmm_cw_min;
                    pEdcaParm->Cwmax[QID_AC_BK] = pAd->wmm_cw_max;
                    pEdcaParm->Txop[QID_AC_BK]  = 0;

                    pEdcaParm->bACM[QID_AC_VI]  = 0;
                    pEdcaParm->Aifsn[QID_AC_VI] = 2;
                    pEdcaParm->Cwmin[QID_AC_VI] = pAd->wmm_cw_min - 1;
                    pEdcaParm->Cwmax[QID_AC_VI] = pAd->wmm_cw_max;
                    pEdcaParm->Txop[QID_AC_VI]  = 96;   /* AC_VI: 96*32us ~= 3ms*/

                    pEdcaParm->bACM[QID_AC_VO]  = 0;
                    pEdcaParm->Aifsn[QID_AC_VO] = 2;
                    pEdcaParm->Cwmin[QID_AC_VO] = pAd->wmm_cw_min - 2;
                    pEdcaParm->Cwmax[QID_AC_VO] = pAd->wmm_cw_max - 1;
                    pEdcaParm->Txop[QID_AC_VO]  = 48;   /* AC_VO: 48*32us ~= 1.5ms*/
                }
				else if (!memcmp(pEid->Octet, WPS_OUI, 4)
 				)
                {
					if (pPeerWscIe)
					{
						/* Ignore old WPS IE fragments, if we get the version 0x10 */
						if (pEid->Octet[4] == 0x10) //First WPS IE will have version 0x10
						{
							memmove(pPeerWscIe, pEid->Octet+4, pEid->Len - 4);
							PeerWscIeLen = (pEid->Len - 4);
						}
						else // reassembly remanning, other IE fragmentations will not have version 0x10
						{
							if ((PeerWscIeLen +(pEid->Len - 4)) <= 512)
							{
								memmove(pPeerWscIe+PeerWscIeLen, pEid->Octet+4, pEid->Len - 4);
								PeerWscIeLen += (pEid->Len - 4);
							}
							else /* ((PeerWscIeLen +(pEid->Len - 4)) > 512) */
							{
								bWscCheck = false;
								DBGPRINT(RT_DEBUG_ERROR, ("%s: Error!!! Sum of All PeerWscIeLen = %d (> 512)\n", __FUNCTION__, (PeerWscIeLen +(pEid->Len - 4))));
							}
						}
					}
					else
					{
						bWscCheck = false;
						DBGPRINT(RT_DEBUG_ERROR, ("%s: Error!!! pPeerWscIe is empty!\n", __FUNCTION__));
					}


#ifdef CONFIG_STA_SUPPORT
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
			if (SubType == SUBTYPE_BEACON)
			{
				u8 *	pData;
				short	Len = 0;
				unsigned short 	DataLen = 0;
				PWSC_IE		pWscIE;

				pData = (u8 *) pEid->Octet + 4;
				Len = (short)(pEid->Len - 4);

				while (Len > 0)
				{
					WSC_IE	WscIE;
					memmove(&WscIE, pData, sizeof(WSC_IE));
					// Check for WSC IEs
					pWscIE = &WscIE;

					if (be2cpu16(pWscIE->Type) == 0x1041 /*WSC_ID_SEL_REGISTRAR*/ )
					{
						DataLen = be2cpu16(pWscIE->Length);
						memmove(pSelReg, pData + 4, DataLen);
						break;
					}

					// Set the offset and look for next WSC Tag information
					// Since Type and Length are both short type, we need to offset 4, not 2
					pData += (be2cpu16(pWscIE->Length) + 4);
					Len   -= (be2cpu16(pWscIE->Length) + 4);
				}


				//WscGetDataFromPeerByTag(pAd, pPeerWscIe, PeerWscIeLen, WSC_ID_SEL_REGISTRAR, &bSelReg, NULL);
			}
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* CONFIG_STA_SUPPORT */


                }


                break;

            case IE_EXT_SUPP_RATES:
                if (pEid->Len <= MAX_LEN_OF_SUPPORTED_RATES)
                {
                    memmove(ExtRate, pEid->Octet, pEid->Len);
                    *pExtRateLen = pEid->Len;

                    /*
						TODO: 2004-09-14 not a good design here, cause it exclude extra rates
								from ScanTab. We should report as is. And filter out unsupported
								rates in MlmeAux
					*/
                    /* Check against the supported rates*/
                    /* RTMPCheckRates(pAd, ExtRate, pExtRateLen);*/
                }
                break;

            case IE_ERP:
                if (pEid->Len == 1)
                {
                    *pErp = (u8)pEid->Octet[0];
                }
                break;

            case IE_AIRONET_CKIP:
                /*
					0. Check Aironet IE length, it must be larger or equal to 28
						Cisco AP350 used length as 28
						Cisco AP12XX used length as 30
				*/
                if (pEid->Len < (CKIP_NEGOTIATION_LENGTH - 2))
                    break;

                /* 1. Copy CKIP flag byte to buffer for process*/
                *pCkipFlag = *(pEid->Octet + 8);
                break;

            case IE_AP_TX_POWER:
                /* AP Control of Client Transmit Power*/
                /*0. Check Aironet IE length, it must be 6*/
                if (pEid->Len != 0x06)
                    break;

                /* Get cell power limit in dBm*/
                if (!memcmp(pEid->Octet, CISCO_OUI, 3))
                    *pAironetCellPowerLimit = *(pEid->Octet + 4);
                break;

            /* WPA2 & 802.11i RSN*/
            case IE_RSN:
                /* There is no OUI for version anymore, check the group cipher OUI before copying*/
                if (!memcmp(pEid->Octet + 2, RSN_OUI, 3))
                {
                    /* Copy to pVIE which will report to microsoft bssid list.*/
                    Ptr = (u8 *) pVIE;
                    memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
                    *LengthVIE += (pEid->Len + 2);
                }
                break;

#ifdef CONFIG_STA_SUPPORT
			case IE_COUNTRY:
				Ptr = (u8 *) pVIE;
                memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
                *LengthVIE += (pEid->Len + 2);
				break;
#endif /* CONFIG_STA_SUPPORT */

            case IE_QBSS_LOAD:
                if (pEid->Len == 5)
                {
                    pQbssLoad->bValid = true;
                    pQbssLoad->StaNum = pEid->Octet[0] + pEid->Octet[1] * 256;
                    pQbssLoad->ChannelUtilization = pEid->Octet[2];
                    pQbssLoad->RemainingAdmissionControl = pEid->Octet[3] + pEid->Octet[4] * 256;

					/* Copy to pVIE*/
                    Ptr = (u8 *) pVIE;
                    memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
                    *LengthVIE += (pEid->Len + 2);
                }
                break;



			case IE_EXT_CAPABILITY:
				if (pEid->Len >= 1)
				{
					u8 MaxSize;
					u8 MySize = sizeof(EXT_CAP_INFO_ELEMENT);

					MaxSize = min(pEid->Len, MySize);

					memmove(pExtCapInfo,&pEid->Octet[0], MaxSize);
				}
				break;
            default:
                break;
        }

        Length = Length + 2 + pEid->Len;  /* Eid[1] + Len[1]+ content[Len]*/
        pEid = (PEID_STRUCT)((u8 *)pEid + 2 + pEid->Len);
    }

	LatchRfChannel = MsgChannel;

		if ((pAd->LatchRfRegs.Channel > 14) && ((Sanity & 0x4) == 0))
		{
			if (CtrlChannel != 0)
				*pChannel = CtrlChannel;
			else
				*pChannel = LatchRfChannel;
			Sanity |= 0x4;
		}

		if (pPeerWscIe && (PeerWscIeLen > 0) && (PeerWscIeLen <= 512) && ( bWscCheck == true))
		{
			u8 WscIe[] = {0xdd, 0x00, 0x00, 0x50, 0xF2, 0x04};
			Ptr = (u8 *) pVIE;
			WscIe[1] = PeerWscIeLen + 4;
			memmove(Ptr + *LengthVIE, WscIe, 6);
			memmove(Ptr + *LengthVIE + 6, pPeerWscIe, PeerWscIeLen);
			*LengthVIE += (PeerWscIeLen + 6);
		}


SanityCheck:
	if (pPeerWscIe)
		kfree(pPeerWscIe);

	if ((Sanity != 0x7) || ( bWscCheck == false))
	{
		DBGPRINT(RT_DEBUG_LOUD, ("%s() - missing field, Sanity=0x%02x\n", __FUNCTION__, Sanity));
		return false;
	}
	else
	{
		return true;
	}

}


/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise

	IRQL = DISPATCH_LEVEL

    ==========================================================================
 */
bool PeerBeaconAndProbeRspSanity(
	IN struct rtmp_adapter *pAd,
	IN VOID *Msg,
	IN ULONG MsgLen,
	IN u8  MsgChannel,
	OUT BCN_IE_LIST *ie_list,
	OUT unsigned short *LengthVIE,
	OUT PNDIS_802_11_VARIABLE_IEs pVIE)
{
	u8 *Ptr;
#ifdef CONFIG_STA_SUPPORT
	u8 TimLen;
#endif /* CONFIG_STA_SUPPORT */
	PFRAME_802_11 pFrame;
	PEID_STRUCT pEid;
	u8 SubType;
	u8 Sanity;
	ULONG Length = 0;
	u8 *pPeerWscIe = NULL;
	INT PeerWscIeLen = 0;
	bool bWscCheck = true;
	u8 LatchRfChannel = 0;


	/*
		For some 11a AP which didn't have DS_IE, we use two conditions to decide the channel
		1. If the AP is 11n enabled, then check the control channel.
		2. If the AP didn't have any info about channel, use the channel we received this
			frame as the channel. (May inaccuracy!!)
	*/
	u8 CtrlChannel = 0;


	pPeerWscIe = kmalloc(512, GFP_ATOMIC);
	Sanity = 0;		/* Add for 3 necessary EID field check*/

	ie_list->AironetCellPowerLimit = 0xFF;  /* Default of AironetCellPowerLimit is 0xFF*/
	ie_list->NewExtChannelOffset = 0xff;	/*Default 0xff means no such IE*/
	*LengthVIE = 0; /* Set the length of VIE to init value 0*/

	pFrame = (PFRAME_802_11)Msg;

	/* get subtype from header*/
	SubType = (u8)pFrame->Hdr.FC.SubType;

    /* get Addr2 and BSSID from header*/
    COPY_MAC_ADDR(&ie_list->Addr2[0], pFrame->Hdr.Addr2);
    COPY_MAC_ADDR(&ie_list->Bssid[0], pFrame->Hdr.Addr3);

    Ptr = pFrame->Octet;
    Length += LENGTH_802_11;

    /* get timestamp from payload and advance the pointer*/
    memmove(&ie_list->TimeStamp, Ptr, TIMESTAMP_LEN);

	ie_list->TimeStamp.u.LowPart = cpu2le32(ie_list->TimeStamp.u.LowPart);
	ie_list->TimeStamp.u.HighPart = cpu2le32(ie_list->TimeStamp.u.HighPart);

    Ptr += TIMESTAMP_LEN;
    Length += TIMESTAMP_LEN;

    /* get beacon interval from payload and advance the pointer*/
    memmove(&ie_list->BeaconPeriod, Ptr, 2);
    Ptr += 2;
    Length += 2;

    /* get capability info from payload and advance the pointer*/
    memmove(&ie_list->CapabilityInfo, Ptr, 2);
    Ptr += 2;
    Length += 2;

    if (CAP_IS_ESS_ON(ie_list->CapabilityInfo))
        ie_list->BssType = BSS_INFRA;
    else
        ie_list->BssType = BSS_ADHOC;

    pEid = (PEID_STRUCT) Ptr;

    /* get variable fields from payload and advance the pointer*/
    while ((Length + 2 + pEid->Len) <= MsgLen)
    {

        /* Secure copy VIE to VarIE[MAX_VIE_LEN] didn't overflow.*/
        if ((*LengthVIE + pEid->Len + 2) >= MAX_VIE_LEN)
        {
            DBGPRINT(RT_DEBUG_WARN, ("%s() - Variable IEs out of resource [len(=%d) > MAX_VIE_LEN(=%d)]\n",
                    __FUNCTION__, (*LengthVIE + pEid->Len + 2), MAX_VIE_LEN));
            break;
        }

        switch(pEid->Eid)
	{
		case IE_SSID:
			/* Already has one SSID EID in this beacon, ignore the second one*/
			if (Sanity & 0x1)
				break;
			if(pEid->Len <= MAX_LEN_OF_SSID)
			{
				memmove(&ie_list->Ssid[0], pEid->Octet, pEid->Len);
				ie_list->SsidLen = pEid->Len;
				Sanity |= 0x1;
			}
			else
			{
				DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_SSID (len=%d)\n",__FUNCTION__,pEid->Len));
				goto SanityCheck;
			}
			break;

		case IE_SUPP_RATES:
			if(pEid->Len <= MAX_LEN_OF_SUPPORTED_RATES)
			{
				Sanity |= 0x2;
				memmove(&ie_list->SupRate[0], pEid->Octet, pEid->Len);
				ie_list->SupRateLen = pEid->Len;

				/*
				TODO: 2004-09-14 not a good design here, cause it exclude extra
				rates from ScanTab. We should report as is. And filter out
				unsupported rates in MlmeAux
				*/
				/* Check against the supported rates*/
				/* RTMPCheckRates(pAd, SupRate, pSupRateLen);*/
			}
			else
			{
				DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_SUPP_RATES (len=%d)\n",__FUNCTION__,pEid->Len));
				goto SanityCheck;
			}
			break;

		case IE_HT_CAP:
			if (pEid->Len >= SIZE_HT_CAP_IE)  /*Note: allow extension.!!*/
			{
				memmove(&ie_list->HtCapability, pEid->Octet, sizeof(HT_CAPABILITY_IE));
				ie_list->HtCapabilityLen = SIZE_HT_CAP_IE;	/* Nnow we only support 26 bytes.*/

				*(unsigned short *)(&ie_list->HtCapability.HtCapInfo) = cpu2le16(*(unsigned short *)(&ie_list->HtCapability.HtCapInfo));
#ifdef UNALIGNMENT_SUPPORT
				{
					EXT_HT_CAP_INFO extHtCapInfo;
					memmove(&extHtCapInfo, &ie_list->HtCapability.ExtHtCapInfo, sizeof(EXT_HT_CAP_INFO));
					*(unsigned short *)(&extHtCapInfo) = cpu2le16(*(unsigned short *)(&extHtCapInfo));
					memmove(&ie_list->HtCapability.ExtHtCapInfo, &extHtCapInfo, sizeof(EXT_HT_CAP_INFO));
				}
#else
				*(unsigned short *)(&ie_list->HtCapability.ExtHtCapInfo) = cpu2le16(*(unsigned short *)(&ie_list->HtCapability.ExtHtCapInfo));
#endif /* UNALIGNMENT_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
				IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
				{
					ie_list->PreNHtCapabilityLen = 0;	/* Now we only support 26 bytes.*/

					Ptr = (u8 *) pVIE;
					memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
					*LengthVIE += (pEid->Len + 2);
				}
#endif /* CONFIG_STA_SUPPORT */
			}
			else
			{
				DBGPRINT(RT_DEBUG_WARN, ("%s() - wrong IE_HT_CAP. pEid->Len = %d\n", __FUNCTION__, pEid->Len));
			}

			break;
		case IE_ADD_HT:
			if (pEid->Len >= sizeof(ADD_HT_INFO_IE))
			{
				/*
				This IE allows extension, but we can ignore extra bytes beyond our
				knowledge , so only copy first sizeof(ADD_HT_INFO_IE)
				*/
				memmove(&ie_list->AddHtInfo, pEid->Octet, sizeof(ADD_HT_INFO_IE));
				ie_list->AddHtInfoLen = SIZE_ADD_HT_INFO_IE;

				CtrlChannel = ie_list->AddHtInfo.ControlChan;

				*(unsigned short *)(&ie_list->AddHtInfo.AddHtInfo2) = cpu2le16(*(unsigned short *)(&ie_list->AddHtInfo.AddHtInfo2));
				*(unsigned short *)(&ie_list->AddHtInfo.AddHtInfo3) = cpu2le16(*(unsigned short *)(&ie_list->AddHtInfo.AddHtInfo3));

#ifdef CONFIG_STA_SUPPORT
				IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
				{
					Ptr = (u8 *) pVIE;
					memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
					*LengthVIE += (pEid->Len + 2);
				}
#endif /* CONFIG_STA_SUPPORT */
			}
			else
			{
				DBGPRINT(RT_DEBUG_WARN, ("%s() - wrong IE_ADD_HT. \n", __FUNCTION__));
			}

			break;
		case IE_SECONDARY_CH_OFFSET:
			if (pEid->Len == 1)
				ie_list->NewExtChannelOffset = pEid->Octet[0];
			else
			{
				DBGPRINT(RT_DEBUG_WARN, ("%s() - wrong IE_SECONDARY_CH_OFFSET. \n", __FUNCTION__));
			}
			break;

		case IE_FH_PARM:
			DBGPRINT(RT_DEBUG_TRACE, ("%s(IE_FH_PARM) \n", __FUNCTION__));
			break;

		case IE_DS_PARM:
			if(pEid->Len == 1)
			{
				ie_list->Channel = *pEid->Octet;
#ifdef CONFIG_STA_SUPPORT
				IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
				{
					if (ChannelSanity(pAd, ie_list->Channel) == 0)
					{
						goto SanityCheck;
					}
				}
#endif /* CONFIG_STA_SUPPORT */
				Sanity |= 0x4;
			}
			else
			{
				DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_DS_PARM (len=%d)\n",__FUNCTION__,pEid->Len));
				goto SanityCheck;
			}
			break;

		case IE_CF_PARM:
			if(pEid->Len == 6)
			{
				ie_list->CfParm.bValid = true;
				ie_list->CfParm.CfpCount = pEid->Octet[0];
				ie_list->CfParm.CfpPeriod = pEid->Octet[1];
				ie_list->CfParm.CfpMaxDuration = pEid->Octet[2] + 256 * pEid->Octet[3];
				ie_list->CfParm.CfpDurRemaining = pEid->Octet[4] + 256 * pEid->Octet[5];
			}
			else
			{
				DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_CF_PARM\n", __FUNCTION__));
				if (pPeerWscIe)
					kfree(pPeerWscIe);
				return false;
			}
			break;

		case IE_IBSS_PARM:
			if(pEid->Len == 2)
			{
				memmove(&ie_list->AtimWin, pEid->Octet, pEid->Len);
			}
			else
			{
				DBGPRINT(RT_DEBUG_TRACE, ("%s() - wrong IE_IBSS_PARM\n", __FUNCTION__));
				if (pPeerWscIe)
					kfree(pPeerWscIe);
				return false;
			}
			break;

#ifdef CONFIG_STA_SUPPORT
		case IE_TIM:
			if(SubType == SUBTYPE_BEACON)
			{

				if (INFRA_ON(pAd) && !memcmp(&ie_list->Bssid[0], pAd->CommonCfg.Bssid, MAC_ADDR_LEN))
				{
					GetTimBit((PCHAR)pEid, pAd->StaActive.Aid, &TimLen, &ie_list->BcastFlag,
					&ie_list->DtimCount, &ie_list->DtimPeriod, &ie_list->MessageToMe);
				}
			}
			break;
#endif /* CONFIG_STA_SUPPORT */
		case IE_CHANNEL_SWITCH_ANNOUNCEMENT:
			if(pEid->Len == 3)
				ie_list->NewChannel = pEid->Octet[1];	/*extract new channel number*/
			break;

			/*
			New for WPA
			CCX v2 has the same IE, we need to parse that too
			Wifi WMM use the same IE vale, need to parse that too
			*/
		/* case IE_WPA:*/
		case IE_VENDOR_SPECIFIC:
			/* Check the OUI version, filter out non-standard usage*/
			if (!memcmp(pEid->Octet, RALINK_OUI, 3) && (pEid->Len == 7))
			{
				if (pEid->Octet[3] != 0)
					ie_list->RalinkIe = pEid->Octet[3];
				else
					ie_list->RalinkIe = 0xf0000000; /* Set to non-zero value (can't set bit0-2) to represent this is Ralink Chip. So at linkup, we will set ralinkchip flag.*/
			}
#ifdef CONFIG_STA_SUPPORT
			/* This HT IE is before IEEE draft set HT IE value.2006-09-28 by Jan.*/

			/* Other vendors had production before IE_HT_CAP value is assigned. To backward support those old-firmware AP,*/
			/* Check broadcom-defiend pre-802.11nD1.0 OUI for HT related IE, including HT Capatilities IE and HT Information IE*/
			else if ((ie_list->HtCapabilityLen == 0) && !memcmp(pEid->Octet, BROADCOM_OUI, 3) && (pEid->Len >= 4) && (pAd->OpMode == OPMODE_STA))
			{
				if ((pEid->Octet[3] == OUI_BROADCOM_HT) && (pEid->Len >= 30) && (ie_list->HtCapabilityLen == 0))
				{
					memmove(&ie_list->HtCapability, &pEid->Octet[4], sizeof(HT_CAPABILITY_IE));
					ie_list->PreNHtCapabilityLen = SIZE_HT_CAP_IE;
				}

				if ((pEid->Octet[3] == OUI_PREN_ADD_HT) && (pEid->Len >= 26))
				{
					memmove(&ie_list->AddHtInfo, &pEid->Octet[4], sizeof(ADD_HT_INFO_IE));
					ie_list->AddHtInfoLen = SIZE_ADD_HT_INFO_IE;
				}
			}
#endif /* CONFIG_STA_SUPPORT */
			else if (!memcmp(pEid->Octet, WPA_OUI, 4))
			{
				/* Copy to pVIE which will report to bssid list.*/
				Ptr = (u8 *) pVIE;
				memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
				*LengthVIE += (pEid->Len + 2);
			}
			else if (!memcmp(pEid->Octet, WME_PARM_ELEM, 6) && (pEid->Len == 24))
			{
				u8 *ptr;
				int i;

				/* parsing EDCA parameters*/
				ie_list->EdcaParm.bValid          = true;
				ie_list->EdcaParm.bQAck           = false; /* pEid->Octet[0] & 0x10;*/
				ie_list->EdcaParm.bQueueRequest   = false; /* pEid->Octet[0] & 0x20;*/
				ie_list->EdcaParm.bTxopRequest    = false; /* pEid->Octet[0] & 0x40;*/
				ie_list->EdcaParm.EdcaUpdateCount = pEid->Octet[6] & 0x0f;
				ie_list->EdcaParm.bAPSDCapable    = (pEid->Octet[6] & 0x80) ? 1 : 0;
				ptr = &pEid->Octet[8];
				for (i=0; i<4; i++)
				{
					u8 aci = (*ptr & 0x60) >> 5; /* b5~6 is AC INDEX*/
					ie_list->EdcaParm.bACM[aci]  = (((*ptr) & 0x10) == 0x10);   /* b5 is ACM*/
					ie_list->EdcaParm.Aifsn[aci] = (*ptr) & 0x0f;               /* b0~3 is AIFSN*/
					ie_list->EdcaParm.Cwmin[aci] = *(ptr+1) & 0x0f;             /* b0~4 is Cwmin*/
					ie_list->EdcaParm.Cwmax[aci] = *(ptr+1) >> 4;               /* b5~8 is Cwmax*/
					ie_list->EdcaParm.Txop[aci]  = *(ptr+2) + 256 * (*(ptr+3)); /* in unit of 32-us*/
					ptr += 4; /* point to next AC*/
				}
			}
			else if (!memcmp(pEid->Octet, WME_INFO_ELEM, 6) && (pEid->Len == 7))
			{
				/* parsing EDCA parameters*/
				ie_list->EdcaParm.bValid          = true;
				ie_list->EdcaParm.bQAck           = false; /* pEid->Octet[0] & 0x10;*/
				ie_list->EdcaParm.bQueueRequest   = false; /* pEid->Octet[0] & 0x20;*/
				ie_list->EdcaParm.bTxopRequest    = false; /* pEid->Octet[0] & 0x40;*/
				ie_list->EdcaParm.EdcaUpdateCount = pEid->Octet[6] & 0x0f;
				ie_list->EdcaParm.bAPSDCapable    = (pEid->Octet[6] & 0x80) ? 1 : 0;

				/* use default EDCA parameter*/
				ie_list->EdcaParm.bACM[QID_AC_BE]  = 0;
				ie_list->EdcaParm.Aifsn[QID_AC_BE] = 3;
				ie_list->EdcaParm.Cwmin[QID_AC_BE] = pAd->wmm_cw_min;
				ie_list->EdcaParm.Cwmax[QID_AC_BE] = pAd->wmm_cw_max;
				ie_list->EdcaParm.Txop[QID_AC_BE]  = 0;

				ie_list->EdcaParm.bACM[QID_AC_BK]  = 0;
				ie_list->EdcaParm.Aifsn[QID_AC_BK] = 7;
				ie_list->EdcaParm.Cwmin[QID_AC_BK] = pAd->wmm_cw_min;
				ie_list->EdcaParm.Cwmax[QID_AC_BK] = pAd->wmm_cw_max;
				ie_list->EdcaParm.Txop[QID_AC_BK]  = 0;

				ie_list->EdcaParm.bACM[QID_AC_VI]  = 0;
				ie_list->EdcaParm.Aifsn[QID_AC_VI] = 2;
				ie_list->EdcaParm.Cwmin[QID_AC_VI] = pAd->wmm_cw_min - 1;
				ie_list->EdcaParm.Cwmax[QID_AC_VI] = pAd->wmm_cw_max;
				ie_list->EdcaParm.Txop[QID_AC_VI]  = 96;   /* AC_VI: 96*32us ~= 3ms*/

				ie_list->EdcaParm.bACM[QID_AC_VO]  = 0;
				ie_list->EdcaParm.Aifsn[QID_AC_VO] = 2;
				ie_list->EdcaParm.Cwmin[QID_AC_VO] = pAd->wmm_cw_min - 2;
				ie_list->EdcaParm.Cwmax[QID_AC_VO] = pAd->wmm_cw_max - 1;
				ie_list->EdcaParm.Txop[QID_AC_VO]  = 48;   /* AC_VO: 48*32us ~= 1.5ms*/
			}
			else if (!memcmp(pEid->Octet, WPS_OUI, 4)
			)
			{
				if (pPeerWscIe)
				{
					/* Ignore old WPS IE fragments, if we get the version 0x10 */
					if (pEid->Octet[4] == 0x10) //First WPS IE will have version 0x10
					{
						memmove(pPeerWscIe, pEid->Octet+4, pEid->Len - 4);
						PeerWscIeLen = (pEid->Len - 4);
					}
					else // reassembly remanning, other IE fragmentations will not have version 0x10
					{
						if ((PeerWscIeLen +(pEid->Len - 4)) <= 512)
						{
							memmove(pPeerWscIe+PeerWscIeLen, pEid->Octet+4, pEid->Len - 4);
							PeerWscIeLen += (pEid->Len - 4);
						}
						else /* ((PeerWscIeLen +(pEid->Len - 4)) > 512) */
						{
							bWscCheck = false;
							DBGPRINT(RT_DEBUG_ERROR, ("%s: Error!!! Sum of All PeerWscIeLen = %d (> 512)\n", __FUNCTION__, (PeerWscIeLen +(pEid->Len - 4))));
						}
					}
				}
				else
				{
					bWscCheck = false;
					DBGPRINT(RT_DEBUG_ERROR, ("%s: Error!!! pPeerWscIe is empty!\n", __FUNCTION__));
				}


#ifdef CONFIG_STA_SUPPORT
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
			if ( SubType == SUBTYPE_BEACON )
			{
				u8 *	pData;
				short	Len = 0;
				unsigned short 	DataLen = 0;
				PWSC_IE		pWscIE;

				pData = (u8 *) pEid->Octet + 4;
				Len = (short)(pEid->Len - 4);

				while (Len > 0)
				{
					WSC_IE	WscIE;
					memmove(&WscIE, pData, sizeof(WSC_IE));
					// Check for WSC IEs
					pWscIE = &WscIE;

					if (be2cpu16(pWscIE->Type) == 0x1041 /*WSC_ID_SEL_REGISTRAR*/ )
					{
						DataLen = be2cpu16(pWscIE->Length);
						memmove(&ie_list->selReg, pData + 4, sizeof(ie_list->selReg));
						break;
					}

					// Set the offset and look for next WSC Tag information
					// Since Type and Length are both short type, we need to offset 4, not 2
					pData += (be2cpu16(pWscIE->Length) + 4);
					Len   -= (be2cpu16(pWscIE->Length) + 4);
				}


				//WscGetDataFromPeerByTag(pAd, pPeerWscIe, PeerWscIeLen, WSC_ID_SEL_REGISTRAR, &bSelReg, NULL);
			}
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* CONFIG_STA_SUPPORT */

			}


			break;

		case IE_EXT_SUPP_RATES:
			if (pEid->Len <= MAX_LEN_OF_SUPPORTED_RATES)
			{
				memmove(&ie_list->ExtRate[0], pEid->Octet, pEid->Len);
				ie_list->ExtRateLen = pEid->Len;

				/*
				TODO: 2004-09-14 not a good design here, cause it exclude extra rates
				from ScanTab. We should report as is. And filter out unsupported
				rates in MlmeAux
				*/
				/* Check against the supported rates*/
				/* RTMPCheckRates(pAd, ExtRate, pExtRateLen);*/
			}
			break;

		case IE_ERP:
			if (pEid->Len == 1)
				ie_list->Erp = (u8)pEid->Octet[0];
			break;

		case IE_AIRONET_CKIP:
			/*
			0. Check Aironet IE length, it must be larger or equal to 28
			Cisco AP350 used length as 28
			Cisco AP12XX used length as 30
			*/
			if (pEid->Len < (CKIP_NEGOTIATION_LENGTH - 2))
				break;

			/* 1. Copy CKIP flag byte to buffer for process*/
			ie_list->CkipFlag = *(pEid->Octet + 8);
			break;

		case IE_AP_TX_POWER:
			/* AP Control of Client Transmit Power*/
			/*0. Check Aironet IE length, it must be 6*/
			if (pEid->Len != 0x06)
				break;

			/* Get cell power limit in dBm*/
			if (!memcmp(pEid->Octet, CISCO_OUI, 3))
				ie_list->AironetCellPowerLimit = *(pEid->Octet + 4);
			break;

		/* WPA2 & 802.11i RSN*/
		case IE_RSN:
			/* There is no OUI for version anymore, check the group cipher OUI before copying*/
			if (!memcmp(pEid->Octet + 2, RSN_OUI, 3))
			{
				/* Copy to pVIE which will report to microsoft bssid list.*/
				Ptr = (u8 *) pVIE;
				memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
				*LengthVIE += (pEid->Len + 2);
			}
			break;


#ifdef CONFIG_STA_SUPPORT
		case IE_COUNTRY:
			Ptr = (u8 *) pVIE;
			memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
			*LengthVIE += (pEid->Len + 2);
			break;
#endif /* CONFIG_STA_SUPPORT */

		case IE_QBSS_LOAD:
			if (pEid->Len == 5)
			{
				ie_list->QbssLoad.bValid = true;
				ie_list->QbssLoad.StaNum = pEid->Octet[0] + pEid->Octet[1] * 256;
				ie_list->QbssLoad.ChannelUtilization = pEid->Octet[2];
				ie_list->QbssLoad.RemainingAdmissionControl = pEid->Octet[3] + pEid->Octet[4] * 256;

				/* Copy to pVIE*/
				Ptr = (u8 *) pVIE;
				memmove(Ptr + *LengthVIE, &pEid->Eid, pEid->Len + 2);
				*LengthVIE += (pEid->Len + 2);
			}
			break;



		case IE_EXT_CAPABILITY:
			if (pEid->Len >= 1)
			{
				u8 cp_len, buf_space = sizeof(EXT_CAP_INFO_ELEMENT);

				cp_len = min(pEid->Len, buf_space);
				memmove(&ie_list->ExtCapInfo,&pEid->Octet[0], cp_len);
			}
			break;

		case IE_VHT_CAP:
			if (pEid->Len == sizeof(VHT_CAP_IE)) {
				memmove(&ie_list->vht_cap_ie, &pEid->Octet[0], sizeof(VHT_CAP_IE));
				ie_list->vht_cap_len = pEid->Len;
			}
			break;
		case IE_VHT_OP:
			if (pEid->Len == sizeof(VHT_OP_IE)) {
				memmove(&ie_list->vht_op_ie, &pEid->Octet[0], sizeof(VHT_OP_IE));
				ie_list->vht_op_len = pEid->Len;
			}
			break;
		case IE_OPERATING_MODE_NOTIFY:
			if (pEid->Len == sizeof(OPERATING_MODE)) {
#ifdef CONFIG_STA_SUPPORT
				MAC_TABLE_ENTRY *pEntry = &pAd->MacTab.Content[BSSID_WCID];
				OPERATING_MODE op_mode;

				if (!INFRA_ON(pAd))
					break;

				memmove(&op_mode, &pEid->Octet[0], sizeof(OPERATING_MODE));

				if (op_mode.rx_nss_type == 0) {
					pEntry->force_op_mode = true;
					memmove(&pEntry->operating_mode, &op_mode, sizeof(OPERATING_MODE));
				}
#endif /* CONFIG_STA_SUPPORT */
				memmove(&ie_list->operating_mode, &pEid->Octet[0], sizeof(OPERATING_MODE));

				DBGPRINT(RT_DEBUG_INFO, ("%s() - IE_OPERATING_MODE_NOTIFY(=%d)\n", __FUNCTION__, pEid->Eid));
			}
			break;

		default:
			break;
		}

		Length = Length + 2 + pEid->Len;  /* Eid[1] + Len[1]+ content[Len]*/
		pEid = (PEID_STRUCT)((u8 *)pEid + 2 + pEid->Len);
    }

	LatchRfChannel = MsgChannel;

	if ((pAd->LatchRfRegs.Channel > 14) && ((Sanity & 0x4) == 0))
	{
		if (CtrlChannel != 0)
			ie_list->Channel = CtrlChannel;
		else {
			if ((pAd->CommonCfg.RegTransmitSetting.field.BW == BW_40)
				|| (pAd->CommonCfg.RegTransmitSetting.field.BW == BW_80)
			) {
				if (pAd->MlmeAux.Channel)
					ie_list->Channel = pAd->MlmeAux.Channel;
				else
					ie_list->Channel = pAd->CommonCfg.Channel;
			}
			else
				ie_list->Channel = LatchRfChannel;
		}
		Sanity |= 0x4;
	}

	if (pPeerWscIe && (PeerWscIeLen > 0) && (PeerWscIeLen <= 512) && ( bWscCheck == true))
	{
		u8 WscIe[] = {0xdd, 0x00, 0x00, 0x50, 0xF2, 0x04};
		Ptr = (u8 *) pVIE;
		WscIe[1] = PeerWscIeLen + 4;
		memmove(Ptr + *LengthVIE, WscIe, 6);
		memmove(Ptr + *LengthVIE + 6, pPeerWscIe, PeerWscIeLen);
		*LengthVIE += (PeerWscIeLen + 6);
	}


SanityCheck:
	if (pPeerWscIe)
		kfree(pPeerWscIe);

	if ((Sanity != 0x7) || ( bWscCheck == false))
	{
		DBGPRINT(RT_DEBUG_LOUD, ("%s() - missing field, Sanity=0x%02x\n", __FUNCTION__, Sanity));
		return false;
	}
	else
	{
		return true;
	}
}


/*
	==========================================================================
	Description:
		MLME message sanity check for some IE addressed  in 802.11n d3.03.
	Return:
		true if all parameters are OK, false otherwise

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
bool PeerBeaconAndProbeRspSanity2(
	IN struct rtmp_adapter *pAd,
	IN VOID *Msg,
	IN ULONG MsgLen,
	IN OVERLAP_BSS_SCAN_IE *BssScan,
	OUT u8 	*RegClass)
{
	CHAR				*Ptr;
	PFRAME_802_11		pFrame;
	PEID_STRUCT			pEid;
	ULONG				Length = 0;
	bool				brc;

	pFrame = (PFRAME_802_11)Msg;

	*RegClass = 0;
	Ptr = pFrame->Octet;
	Length += LENGTH_802_11;

	/* get timestamp from payload and advance the pointer*/
	Ptr += TIMESTAMP_LEN;
	Length += TIMESTAMP_LEN;

	/* get beacon interval from payload and advance the pointer*/
	Ptr += 2;
	Length += 2;

	/* get capability info from payload and advance the pointer*/
	Ptr += 2;
	Length += 2;

	pEid = (PEID_STRUCT) Ptr;
	brc = false;

	memset(BssScan, 0, sizeof(OVERLAP_BSS_SCAN_IE));
	/* get variable fields from payload and advance the pointer*/
	while ((Length + 2 + pEid->Len) <= MsgLen)
	{
		switch(pEid->Eid)
		{
			case IE_SUPP_REG_CLASS:
				if(pEid->Len > 0)
				{
					*RegClass = *pEid->Octet;
				}
				else
				{
					DBGPRINT(RT_DEBUG_TRACE, ("PeerBeaconAndProbeRspSanity - wrong IE_SUPP_REG_CLASS (len=%d)\n",pEid->Len));
				}
				break;
			case IE_OVERLAPBSS_SCAN_PARM:
				if (pEid->Len == sizeof(OVERLAP_BSS_SCAN_IE))
				{
					brc = true;
					memmove(BssScan, pEid->Octet, sizeof(OVERLAP_BSS_SCAN_IE));
				}
				else
				{
					DBGPRINT(RT_DEBUG_TRACE, ("PeerBeaconAndProbeRspSanity - wrong IE_OVERLAPBSS_SCAN_PARM (len=%d)\n",pEid->Len));
				}
				break;

			case IE_EXT_CHANNEL_SWITCH_ANNOUNCEMENT:
				DBGPRINT(RT_DEBUG_TRACE, ("PeerBeaconAndProbeRspSanity - IE_EXT_CHANNEL_SWITCH_ANNOUNCEMENT\n"));
				break;

		}

		Length = Length + 2 + pEid->Len;  /* Eid[1] + Len[1]+ content[Len]	*/
		pEid = (PEID_STRUCT)((u8 *)pEid + 2 + pEid->Len);
	}

	return brc;

}

#if defined(AP_SCAN_SUPPORT) || defined(CONFIG_STA_SUPPORT)
/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise
    ==========================================================================
 */
bool MlmeScanReqSanity(
	IN struct rtmp_adapter *pAd,
	IN VOID *Msg,
	IN ULONG MsgLen,
	OUT u8 *pBssType,
	OUT CHAR Ssid[],
	OUT u8 *pSsidLen,
	OUT u8 *pScanType)
{
	MLME_SCAN_REQ_STRUCT *Info;

	Info = (MLME_SCAN_REQ_STRUCT *)(Msg);
	*pBssType = Info->BssType;
	*pSsidLen = Info->SsidLen;
	memmove(Ssid, Info->Ssid, *pSsidLen);
	*pScanType = Info->ScanType;

	if ((*pBssType == BSS_INFRA || *pBssType == BSS_ADHOC || *pBssType == BSS_ANY)
		&& (SCAN_MODE_VALID(*pScanType))
	)
	{
		return true;
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("MlmeScanReqSanity fail - wrong BssType or ScanType\n"));
		return false;
	}
}
#endif

/* IRQL = DISPATCH_LEVEL*/
u8 ChannelSanity(
    IN struct rtmp_adapter *pAd,
    IN u8 channel)
{
    int i;

    for (i = 0; i < pAd->ChannelListNum; i ++)
    {
        if (channel == pAd->ChannelList[i].Channel)
            return 1;
    }
    return 0;
}

/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise

	IRQL = DISPATCH_LEVEL

    ==========================================================================
 */
bool PeerDeauthSanity(
    IN struct rtmp_adapter *pAd,
    IN VOID *Msg,
    IN ULONG MsgLen,
    OUT u8 *pAddr1,
    OUT u8 *pAddr2,
    OUT u8 *pAddr3,
    OUT unsigned short *pReason)
{
    PFRAME_802_11 pFrame = (PFRAME_802_11)Msg;

	COPY_MAC_ADDR(pAddr1, pFrame->Hdr.Addr1);
    COPY_MAC_ADDR(pAddr2, pFrame->Hdr.Addr2);
	COPY_MAC_ADDR(pAddr3, pFrame->Hdr.Addr3);
    memmove(pReason, &pFrame->Octet[0], 2);

    return true;
}

/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise

	IRQL = DISPATCH_LEVEL

    ==========================================================================
 */
bool PeerAuthSanity(
    IN struct rtmp_adapter *pAd,
    IN VOID *Msg,
    IN ULONG MsgLen,
    OUT u8 *pAddr,
    OUT unsigned short *pAlg,
    OUT unsigned short *pSeq,
    OUT unsigned short *pStatus,
    CHAR *pChlgText)
{
    PFRAME_802_11 pFrame = (PFRAME_802_11)Msg;

    COPY_MAC_ADDR(pAddr,   pFrame->Hdr.Addr2);
    memmove(pAlg,    &pFrame->Octet[0], 2);
    memmove(pSeq,    &pFrame->Octet[2], 2);
    memmove(pStatus, &pFrame->Octet[4], 2);

    if (*pAlg == AUTH_MODE_OPEN)
    {
        if (*pSeq == 1 || *pSeq == 2)
        {
            return true;
        }
        else
        {
            DBGPRINT(RT_DEBUG_TRACE, ("PeerAuthSanity fail - wrong Seg#\n"));
            return false;
        }
    }
    else if (*pAlg == AUTH_MODE_KEY)
    {
        if (*pSeq == 1 || *pSeq == 4)
        {
            return true;
        }
        else if (*pSeq == 2 || *pSeq == 3)
        {
            memmove(pChlgText, &pFrame->Octet[8], CIPHER_TEXT_LEN);
            return true;
        }
        else
        {
            DBGPRINT(RT_DEBUG_TRACE, ("PeerAuthSanity fail - wrong Seg#\n"));
            return false;
        }
    }
    else
    {
        DBGPRINT(RT_DEBUG_TRACE, ("PeerAuthSanity fail - wrong algorithm\n"));
        return false;
    }
}

/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise
    ==========================================================================
 */
bool MlmeAuthReqSanity(
    IN struct rtmp_adapter *pAd,
    IN VOID *Msg,
    IN ULONG MsgLen,
    OUT u8 *pAddr,
    OUT ULONG *pTimeout,
    OUT unsigned short *pAlg)
{
    MLME_AUTH_REQ_STRUCT *pInfo;

    pInfo  = (MLME_AUTH_REQ_STRUCT *)Msg;
    COPY_MAC_ADDR(pAddr, pInfo->Addr);
    *pTimeout = pInfo->Timeout;
    *pAlg = pInfo->Alg;

    if (((*pAlg == AUTH_MODE_KEY) ||(*pAlg == AUTH_MODE_OPEN)
     	) &&
        ((*pAddr & 0x01) == 0))
    {
#ifdef CONFIG_STA_SUPPORT
#endif /* CONFIG_STA_SUPPORT */
        return true;
    }
    else
    {
        DBGPRINT(RT_DEBUG_TRACE, ("MlmeAuthReqSanity fail - wrong algorithm\n"));
        return false;
    }
}

/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise

	IRQL = DISPATCH_LEVEL

    ==========================================================================
 */
bool MlmeAssocReqSanity(
    IN struct rtmp_adapter *pAd,
    IN VOID *Msg,
    IN ULONG MsgLen,
    OUT u8 *pApAddr,
    OUT unsigned short *pCapabilityInfo,
    OUT ULONG *pTimeout,
    OUT unsigned short *pListenIntv)
{
    MLME_ASSOC_REQ_STRUCT *pInfo;

    pInfo = (MLME_ASSOC_REQ_STRUCT *)Msg;
    *pTimeout = pInfo->Timeout;                             /* timeout*/
    COPY_MAC_ADDR(pApAddr, pInfo->Addr);                   /* AP address*/
    *pCapabilityInfo = pInfo->CapabilityInfo;               /* capability info*/
    *pListenIntv = pInfo->ListenIntv;

    return true;
}

/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise

	IRQL = DISPATCH_LEVEL

    ==========================================================================
 */
bool PeerDisassocSanity(
    IN struct rtmp_adapter *pAd,
    IN VOID *Msg,
    IN ULONG MsgLen,
    OUT u8 *pAddr2,
    OUT unsigned short *pReason)
{
    PFRAME_802_11 pFrame = (PFRAME_802_11)Msg;

    COPY_MAC_ADDR(pAddr2, pFrame->Hdr.Addr2);
    memmove(pReason, &pFrame->Octet[0], 2);

    return true;
}

/*
	========================================================================
	Routine Description:
		Sanity check NetworkType (11b, 11g or 11a)

	Arguments:
		pBss - Pointer to BSS table.

	Return Value:
        Ndis802_11DS .......(11b)
        Ndis802_11OFDM24....(11g)
        Ndis802_11OFDM5.....(11a)

	IRQL = DISPATCH_LEVEL

	========================================================================
*/
NDIS_802_11_NETWORK_TYPE NetworkTypeInUseSanity(BSS_ENTRY *pBss)
{
	NDIS_802_11_NETWORK_TYPE	NetWorkType;
	u8 					rate, i;

	NetWorkType = Ndis802_11DS;

	if (pBss->Channel <= 14)
	{

		/* First check support Rate.*/
		for (i = 0; i < pBss->SupRateLen; i++)
		{
			rate = pBss->SupRate[i] & 0x7f; /* Mask out basic rate set bit*/
			if ((rate == 2) || (rate == 4) || (rate == 11) || (rate == 22))
			{
				continue;
			}
			else
			{

				/* Otherwise (even rate > 108) means Ndis802_11OFDM24*/
				NetWorkType = Ndis802_11OFDM24;
				break;
			}
		}


		/* Second check Extend Rate.*/
		if (NetWorkType != Ndis802_11OFDM24)
		{
			for (i = 0; i < pBss->ExtRateLen; i++)
			{
				rate = pBss->SupRate[i] & 0x7f; /* Mask out basic rate set bit*/
				if ((rate == 2) || (rate == 4) || (rate == 11) || (rate == 22))
				{
					continue;
				}
				else
				{

					/* Otherwise (even rate > 108) means Ndis802_11OFDM24*/
					NetWorkType = Ndis802_11OFDM24;
					break;
				}
			}
		}
	}
	else
	{
		NetWorkType = Ndis802_11OFDM5;
	}

	if (pBss->HtCapabilityLen != 0)
	{
		if (NetWorkType == Ndis802_11OFDM5) {
			if (pBss->vht_cap_len != 0)
				NetWorkType = Ndis802_11OFDM5_AC;
			else
				NetWorkType = Ndis802_11OFDM5_N;
		} else
			NetWorkType = Ndis802_11OFDM24_N;
	}

	return NetWorkType;
}

#ifdef CONFIG_STA_SUPPORT
#endif /* CONFIG_STA_SUPPORT */



/*
    ==========================================================================
    Description:
        MLME message sanity check
    Return:
        true if all parameters are OK, false otherwise
    ==========================================================================
 */
bool PeerProbeReqSanity(
	IN struct rtmp_adapter *pAd,
	IN VOID *Msg,
	IN ULONG MsgLen,
	OUT PEER_PROBE_REQ_PARAM *ProbeReqParam)
{
    PFRAME_802_11 Fr = (PFRAME_802_11)Msg;
    u8 	*Ptr;
    u8 	eid =0, eid_len = 0, *eid_data;
#ifdef CONFIG_AP_SUPPORT
    u8       apidx = MAIN_MBSSID;
	u8       Addr1[MAC_ADDR_LEN];
#endif /* CONFIG_AP_SUPPORT */
	UINT		total_ie_len = 0;

	memset(ProbeReqParam, 0, sizeof(*ProbeReqParam));

    /* to prevent caller from using garbage output value*/
#ifdef CONFIG_AP_SUPPORT
	apidx = apidx; /* avoid compile warning */
#endif /* CONFIG_AP_SUPPORT */

    COPY_MAC_ADDR(ProbeReqParam->Addr2, &Fr->Hdr.Addr2);

    if (Fr->Octet[0] != IE_SSID || Fr->Octet[1] > MAX_LEN_OF_SSID)
    {
        DBGPRINT(RT_DEBUG_TRACE, ("%s(): sanity fail - wrong SSID IE\n", __FUNCTION__));
        return false;
    }

    ProbeReqParam->SsidLen = Fr->Octet[1];
    memmove(ProbeReqParam->Ssid, &Fr->Octet[2], ProbeReqParam->SsidLen);

#ifdef CONFIG_AP_SUPPORT
	COPY_MAC_ADDR(Addr1, &Fr->Hdr.Addr1);
#endif /* CONFIG_AP_SUPPORT */

    Ptr = Fr->Octet;
    eid = Ptr[0];
    eid_len = Ptr[1];
	total_ie_len = eid_len + 2;
	eid_data = Ptr+2;

    /* get variable fields from payload and advance the pointer*/
	while((eid_data + eid_len) <= ((u8 *)Fr + MsgLen))
    {
        switch(eid)
        {
	        case IE_VENDOR_SPECIFIC:
				if (eid_len <= 4)
					break;
#ifdef RSSI_FEEDBACK
                if (ProbeReqParam->bRssiRequested &&
					 !memcmp(eid_data, RALINK_OUI, 3) && (eid_len == 7))
                {
					if (*(eid_data + 3/* skip RALINK_OUI */) & 0x8)
                    	ProbeReqParam->bRssiRequested = true;
                    break;
                }
#endif /* RSSI_FEEDBACK */

                if (!memcmp(eid_data, WPS_OUI, 4)
 					)
                {
#ifdef CONFIG_AP_SUPPORT
#endif /* CONFIG_AP_SUPPORT */

                }
                    break;
			case IE_EXT_CAPABILITY:
				break;
            default:
                break;
        }
		eid = Ptr[total_ie_len];
    	eid_len = Ptr[total_ie_len + 1];
		eid_data = Ptr + total_ie_len + 2;
		total_ie_len += (eid_len + 2);
	}

#ifdef CONFIG_AP_SUPPORT
#endif /* CONFIG_AP_SUPPORT */

    return true;
}




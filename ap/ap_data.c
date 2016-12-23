/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	ap_data.c

	Abstract:
	Data path subroutines

	Revision History:
	Who 		When			What
	--------	----------		----------------------------------------------
*/
#include "rt_config.h"

#define IS_MULTICAST_MAC_ADDR(Addr)			((((Addr[0]) & 0x01) == 0x01) && ((Addr[0]) != 0xff))
#define IS_BROADCAST_MAC_ADDR(Addr)			((((Addr[0]) & 0xff) == 0xff))



INT ApAllowToSendPacket(
	IN struct rtmp_adapter *pAd,
	IN struct rtmp_wifi_dev *wdev,
	IN struct sk_buff *pPacket,
	IN UCHAR *pWcid)
{
	PACKET_INFO PacketInfo;
	UCHAR *pSrcBufVA;
	UINT SrcBufLen;
	MAC_TABLE_ENTRY *pEntry = NULL;
#ifdef MBSS_SUPPORT
	UCHAR IdBss;
#endif /* MBSS_SUPPORT */

	RTMP_QueryPacketInfo(pPacket, &PacketInfo, &pSrcBufVA, &SrcBufLen);

#ifdef MBSS_SUPPORT
	/* 0 is main BSS, FIRST_MBSSID = 1 */
	// TODO: shiang-6590, for old coding, will RTMP_SET_PACKET_NET_DEVICE_MBSSID(), how about now??
	for(IdBss= MAIN_MBSSID; IdBss < pAd->ApCfg.BssidNum; IdBss++)
	{
		if (wdev == &pAd->ApCfg.MBSSID[IdBss].wdev)
		{
			RTMP_SET_PACKET_NET_DEVICE_MBSSID(pPacket, IdBss);
			break;
		}
	}

	ASSERT(IdBss < pAd->ApCfg.BssidNum);
#endif /* MBSS_SUPPORT */

#ifdef RT_CFG80211_SUPPORT
	//CFG_TODO: POS NO GOOD
	if (pAd->cfg80211_ctrl.isCfgInApMode == RT_CMD_80211_IFTYPE_AP)
	{
		RTMP_SET_PACKET_OPMODE(pPacket, OPMODE_AP);
	}
#endif /* RT_CFG80211_SUPPORT */

	if (MAC_ADDR_IS_GROUP(pSrcBufVA)) /* mcast & broadcast address */
	{
		*pWcid = MCAST_WCID;
		return TRUE;
	}
	else /* unicast address */
	{
		pEntry = MacTableLookup(pAd, pSrcBufVA);
		if (pEntry && (pEntry->Sst == SST_ASSOC))
		{
			*pWcid = (UCHAR)pEntry->wcid;
			return TRUE;
		}

	}

	return FALSE;
}


enum pkt_tx_status{
	PKT_SUCCESS = 0,
	INVALID_PKT_LEN = 1,
	INVALID_TR_WCID = 2,
	INVALID_TR_ENTRY = 3,
	INVALID_WDEV = 4,
	INVALID_ETH_TYPE = 5,
	DROP_PORT_SECURE = 6,
	DROP_PSQ_FULL = 7,
	DROP_TXQ_FULL = 8,
	DROP_TX_JAM = 9,
	DROP_TXQ_ENQ_FAIL = 10,
};

struct reason_id_str{
	INT id;
	char *code_str;
};

static struct reason_id_str pkt_drop_code[]={
		{PKT_SUCCESS, "TxSuccess"},
		{INVALID_PKT_LEN, "pkt error"},
		{INVALID_TR_WCID, "invalid TR wcid"},
		{INVALID_TR_ENTRY, "wrong TR entry type"},
		{INVALID_WDEV, "Invalid wdev"},
		{INVALID_ETH_TYPE, "ether type check fail"},
		{DROP_PORT_SECURE, "port not secure"},
		{DROP_PSQ_FULL, "PsQ full"},
		{DROP_TXQ_FULL, "TxQ full"},
		{DROP_TX_JAM, "Tx jam"},
		{DROP_TXQ_ENQ_FAIL, "TxQ EnQ fail"},
};


/*
	========================================================================
	Routine Description:
		This routine is used to do packet parsing and classification for Tx packet
		to AP device, and it will en-queue packets to our TxSwQ depends on AC
		class.

	Arguments:
		pAd    Pointer to our adapter
		pPacket 	Pointer to send packet

	Return Value:
		NDIS_STATUS_SUCCESS		If succes to queue the packet into TxSwQ.
		NDIS_STATUS_FAILURE			If failed to do en-queue.

	pre: Before calling this routine, caller should have filled the following fields

		pPacket->MiniportReserved[6] - contains packet source
		pPacket->MiniportReserved[5] - contains RA's WDS index (if RA on WDS link) or AID
										(if RA directly associated to this AP)
	post:This routine should decide the remaining pPacket->MiniportReserved[] fields
		before calling APHardTransmit(), such as:

		pPacket->MiniportReserved[4] - Fragment # and User PRiority
		pPacket->MiniportReserved[7] - RTS/CTS-to-self protection method and TX rate

	Note:
		You only can put OS-indepened & AP related code in here.
========================================================================
*/
INT APSendPacket(struct rtmp_adapter *pAd, struct sk_buff *pPacket)
{
	PACKET_INFO PacketInfo;
	UCHAR *pSrcBufVA;
	UINT SrcBufLen, AllowFragSize;
	UCHAR NumberOfFrag;
	UCHAR QueIdx;
	UCHAR UserPriority, PsMode = PWR_ACTIVE;
	UCHAR Wcid;
	unsigned long IrqFlags;
	MAC_TABLE_ENTRY *pMacEntry = NULL;
	struct rtmp_wifi_dev *wdev;

#ifdef APCLI_SUPPORT
	PAPCLI_STRUCT pApCliEntry = NULL;
	pApCliEntry = &pAd->ApCfg.ApCliTab[0];
#endif


	RTMP_QueryPacketInfo(pPacket, &PacketInfo, &pSrcBufVA, &SrcBufLen);
	if ((pSrcBufVA == NULL) || (SrcBufLen <= 14))
	{
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
		DBGPRINT(RT_DEBUG_ERROR, ("%s():pkt error(%p, %d)\n",
					__FUNCTION__, pSrcBufVA, SrcBufLen));
		return NDIS_STATUS_FAILURE;
	}

	Wcid = RTMP_GET_PACKET_WCID(pPacket);
	pMacEntry = &pAd->MacTab.Content[Wcid];

        if (Wcid != MCAST_WCID)
        {
                wdev = pMacEntry->wdev;
        }
        else // don't pass pMacEntry->wdev to RTMPCheckEtherType(), when Wcid is MCAST_WCID
        {
                UCHAR IfIdx = 0;
                IfIdx = RTMP_GET_PACKET_WDEV(pPacket);
                if ((IfIdx < WDEV_NUM_MAX) && (pAd->wdev_list[IfIdx] != NULL)) {
                        wdev = pAd->wdev_list[IfIdx];
                }
                else
                {
                        RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
                        return NDIS_STATUS_FAILURE;
                }
        }

	/*
		Check the Ethernet Frame type, and set RTMP_SET_PACKET_SPECIFIC flags
		Here we set the PACKET_SPECIFIC flags(LLC, VLAN, DHCP/ARP, EAPOL).
	*/
	UserPriority = 0;
	QueIdx = QID_AC_BE;
	if (RTMPCheckEtherType(pAd, pPacket, pMacEntry, wdev, &UserPriority, &QueIdx) == FALSE)
	{
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
		return NDIS_STATUS_FAILURE;
	}


	if (IS_VALID_ENTRY(pMacEntry) || (Wcid == MCAST_WCID))
	{
		PsMode = pMacEntry->PsMode;

		// TODO: shiang-6590, how to disinguish the MCAST/BCAST from different BSS if we don't have RTMP_GET_PACKET_NET_DEVICE_MBSSID??
		// TODO: shiang-6590, use wdev->allow_data_tx replace (pAd->ApCfg.EntryClientCount == 0)!
		if (0 /*Wcid == MCAST_WCID*/)
		{
			if (pAd->ApCfg.EntryClientCount == 0)
			{
				RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
				return NDIS_STATUS_FAILURE;
			}
		}


		/* AP does not send packets before port secured. */
		if (((wdev->AuthMode >= Ndis802_11AuthModeWPA)
#ifdef DOT1X_SUPPORT
			|| (wdev->IEEE8021X == TRUE)
#endif /* DOT1X_SUPPORT */
			) &&
			(RTMP_GET_PACKET_EAPOL(pPacket) == FALSE)
		)
		{
			/* Process for multicast or broadcast frame */
			if (Wcid == MCAST_WCID)
			{
				if (wdev->PortSecured == WPA_802_1X_PORT_NOT_SECURED) {
					RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
					return NDIS_STATUS_FAILURE;
				}
			}
			else
			{	/* Process for unicast frame */
				if (pMacEntry->PortSecured == WPA_802_1X_PORT_NOT_SECURED) {
					RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
					return NDIS_STATUS_FAILURE;
				}
			}
		}

	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, ("%s(%s):Drop unknow packet\n",
					__FUNCTION__, RtmpOsGetNetDevName(wdev->if_dev)));
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
		return NDIS_STATUS_FAILURE;
	}



	/*
		STEP 1. Decide number of fragments required to deliver this MSDU.
			The estimation here is not very accurate because difficult to
			take encryption overhead into consideration here. The result
			"NumberOfFrag" is then just used to pre-check if enough free
			TXD are available to hold this MSDU.
	*/
	if ((*pSrcBufVA & 0x01)	/* fragmentation not allowed on multicast & broadcast */
	)
		NumberOfFrag = 1;
	else if (pMacEntry && IS_ENTRY_CLIENT(pMacEntry)
			&& CLIENT_STATUS_TEST_FLAG(pMacEntry, fCLIENT_STATUS_AGGREGATION_CAPABLE))
	{
		NumberOfFrag = 1;	/* Aggregation overwhelms fragmentation */
	}
	else
	{
		/*
			The calculated "NumberOfFrag" is a rough estimation because of various
			encryption/encapsulation overhead not taken into consideration. This number is just
			used to make sure enough free TXD are available before fragmentation takes place.
			In case the actual required number of fragments of an NDIS packet
			excceeds "NumberOfFrag"caculated here and not enough free TXD available, the
			last fragment (i.e. last MPDU) will be dropped in RTMPHardTransmit() due to out of
			resource, and the NDIS packet will be indicated NDIS_STATUS_FAILURE. This should
			rarely happen and the penalty is just like a TX RETRY fail. Affordable.
		*/
		uint32_t Size;

		AllowFragSize = (pAd->CommonCfg.FragmentThreshold) - LENGTH_802_11 - LENGTH_CRC;
		Size = PacketInfo.TotalPacketLength - LENGTH_802_3 + LENGTH_802_1_H;
		if (Size >= AllowFragSize)
			NumberOfFrag = (Size / AllowFragSize) + 1;
		else
			NumberOfFrag = 1;
	}

	/* Save fragment number to Ndis packet reserved field */
	RTMP_SET_PACKET_FRAGMENTS(pPacket, NumberOfFrag);


	/* detect AC Category of tx packets to tune AC0(BE) TX_OP (MAC reg 0x1300) */
	// TODO: shiang-usw, check this for REG access
#ifdef APCLI_CERT_SUPPORT
	if (pApCliEntry->wdev.bWmmCapable == FALSE)
#endif /* APCLI_CERT_SUPPORT */
	detect_wmm_traffic(pAd, UserPriority, 1);

	RTMP_SET_PACKET_UP(pPacket, UserPriority);
	RTMP_SET_PACKET_MGMT_PKT(pPacket, 0x00); /* mark as non-management frame */

	/*
		4. put to corrsponding TxSwQueue or Power-saving queue

		WDS/ApClient/Mesh link should never go into power-save mode; just send out the frame
	*/
	if (pMacEntry && (IS_ENTRY_WDS(pMacEntry) || IS_ENTRY_APCLI(pMacEntry) || IS_ENTRY_MESH(pMacEntry)))
	{

		if (pAd->TxSwQueue[QueIdx].Number >= pAd->TxSwQMaxLen)
		{
#ifdef BLOCK_NET_IF
			StopNetIfQueue(pAd, QueIdx, pPacket);
#endif /* BLOCK_NET_IF */
			RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
			return NDIS_STATUS_FAILURE;
		}
		else
		{
			RTMP_IRQ_LOCK(&pAd->irq_lock, IrqFlags);
			InsertTailQueueAc(pAd, pMacEntry, &pAd->TxSwQueue[QueIdx], PACKET_TO_QUEUE_ENTRY(pPacket));
			RTMP_IRQ_UNLOCK(&pAd->irq_lock, IrqFlags);
		}
	}
	/* M/BCAST frames are put to PSQ as long as there's any associated STA in power-save mode */
	else if ((*pSrcBufVA & 0x01) && pAd->MacTab.fAnyStationInPsm
	)
	{
		/*
			we don't want too many MCAST/BCAST backlog frames to eat up all buffers.
			So in case number of backlog MCAST/BCAST frames exceeds a pre-defined
			watermark within a DTIM period, simply drop coming new MCAST/BCAST frames.
			This design is similiar to "BROADCAST throttling in most manageable
			Ethernet Switch chip.
		*/
		if (pAd->MacTab.McastPsQueue.Number >= MAX_PACKETS_IN_MCAST_PS_QUEUE)
		{
			RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
			DBGPRINT(RT_DEBUG_TRACE, ("M/BCAST PSQ(=%ld) full, drop it!\n", pAd->MacTab.McastPsQueue.Number));
			return NDIS_STATUS_FAILURE;
		}
		else
		{
			// TODO: shiang-6590, remove the apidx!!!!
			UCHAR apidx;
			if (Wcid == MCAST_WCID)
				apidx = RTMP_GET_PACKET_NET_DEVICE_MBSSID(pPacket);
			else
				apidx = pMacEntry->apidx;

			RTMP_IRQ_LOCK(&pAd->irq_lock, IrqFlags);
			InsertHeadQueue(&pAd->MacTab.McastPsQueue, PACKET_TO_QUEUE_ENTRY(pPacket));
			RTMP_IRQ_UNLOCK(&pAd->irq_lock, IrqFlags);

			WLAN_MR_TIM_BCMC_SET(apidx); /* mark MCAST/BCAST TIM bit */

		}
	}
	/* else if the associted STA in power-save mode, frame also goes to PSQ */
	else if ((PsMode == PWR_SAVE) && pMacEntry &&
			IS_ENTRY_CLIENT(pMacEntry) && (pMacEntry->Sst == SST_ASSOC))
	{
		if (APInsertPsQueue(pAd, pPacket, pMacEntry, QueIdx) != NDIS_STATUS_SUCCESS)
			return NDIS_STATUS_FAILURE;
	}
	/* 3. otherwise, transmit the frame */
	else /* (PsMode == PWR_ACTIVE) || (PsMode == PWR_UNKNOWN) */
	{
		{
			if (pAd->TxSwQueue[QueIdx].Number >= pAd->TxSwQMaxLen)
			{
				{
#ifdef BLOCK_NET_IF
					StopNetIfQueue(pAd, QueIdx, pPacket);
#endif /* BLOCK_NET_IF */
					RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);

					return NDIS_STATUS_FAILURE;
				}
			}
			else
			{
				RTMP_IRQ_LOCK(&pAd->irq_lock, IrqFlags);
				InsertTailQueueAc(pAd, pMacEntry, &pAd->TxSwQueue[QueIdx], PACKET_TO_QUEUE_ENTRY(pPacket));
				RTMP_IRQ_UNLOCK(&pAd->irq_lock, IrqFlags);
			}
		}
	}

#ifdef DOT11_N_SUPPORT
	RTMP_BASetup(pAd, pMacEntry, UserPriority);
#endif /* DOT11_N_SUPPORT */
#ifdef APCLI_CERT_SUPPORT
	pAd->RalinkCounters.OneSecOsTxCount[QueIdx]++;
#endif /* APCLI_CERT_SUPPORT */
	return NDIS_STATUS_SUCCESS;
}


/*
	--------------------------------------------------------
	FIND ENCRYPT KEY AND DECIDE CIPHER ALGORITHM
		Find the WPA key, either Group or Pairwise Key
		LEAP + TKIP also use WPA key.
	--------------------------------------------------------
	Decide WEP bit and cipher suite to be used.
	Same cipher suite should be used for whole fragment burst
	In Cisco CCX 2.0 Leap Authentication
		WepStatus is Ndis802_11WEPEnabled but the key will use PairwiseKey
		Instead of the SharedKey, SharedKey Length may be Zero.
*/
static inline VOID APFindCipherAlgorithm(struct rtmp_adapter *pAd, TX_BLK *pTxBlk)
{
	CIPHER_KEY *pKey = NULL;
	UCHAR KeyIdx = 0, CipherAlg = CIPHER_NONE;
	UCHAR apidx = pTxBlk->apidx;
	UCHAR RAWcid = pTxBlk->Wcid;
	MAC_TABLE_ENTRY *pMacEntry = pTxBlk->pMacEntry;
	MULTISSID_STRUCT *pMbss;
	struct rtmp_wifi_dev *wdev;

	pMbss = &pAd->ApCfg.MBSSID[apidx];
	wdev = &pMbss->wdev;

#ifdef APCLI_SUPPORT
	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bApCliPacket))
	{
		APCLI_STRUCT *pApCliEntry = pTxBlk->pApCliEntry;
		wdev = &pApCliEntry->wdev;

		if (RTMP_GET_PACKET_EAPOL(pTxBlk->pPacket))
		{
			/* These EAPoL frames must be clear before 4-way handshaking is completed. */
			if ((!(TX_BLK_TEST_FLAG(pTxBlk, fTX_bClearEAPFrame))) &&
				(pMacEntry->PairwiseKey.CipherAlg) &&
				(pMacEntry->PairwiseKey.KeyLen))
			{
				CipherAlg  = pMacEntry->PairwiseKey.CipherAlg;
				if (CipherAlg)
					pKey = &pMacEntry->PairwiseKey;
			}
			else
			{
				CipherAlg = CIPHER_NONE;
				pKey = NULL;
			}
		}
#ifdef WPA_SUPPLICANT_SUPPORT
		else if (pApCliEntry->wpa_supplicant_info.WpaSupplicantUP &&
			(pMacEntry->WepStatus  == Ndis802_11WEPEnabled) &&
			(pApCliEntry->wdev.IEEE8021X == TRUE) &&
			(pMacEntry->PortSecured == WPA_802_1X_PORT_NOT_SECURED))
		{
			CipherAlg = CIPHER_NONE;
		}
#endif /* WPA_SUPPLICANT_SUPPORT */
		else if (pMacEntry->WepStatus == Ndis802_11WEPEnabled)
		{
			CipherAlg  = pApCliEntry->SharedKey[wdev->DefaultKeyId].CipherAlg;
			if (CipherAlg)
				pKey = &pApCliEntry->SharedKey[wdev->DefaultKeyId];
		}
		else if (pMacEntry->WepStatus == Ndis802_11TKIPEnable ||
	 			 pMacEntry->WepStatus == Ndis802_11AESEnable)
		{
			CipherAlg  = pMacEntry->PairwiseKey.CipherAlg;
			if (CipherAlg)
				pKey = &pMacEntry->PairwiseKey;
		}
		else
		{
			CipherAlg = CIPHER_NONE;
			pKey = NULL;
		}
	}
	else
#endif /* APCLI_SUPPORT */
	if ((RTMP_GET_PACKET_EAPOL(pTxBlk->pPacket)) ||
#ifdef DOT1X_SUPPORT
		((wdev->WepStatus == Ndis802_11WEPEnabled) && (wdev->IEEE8021X == TRUE)) ||
#endif /* DOT1X_SUPPORT */
		(wdev->WepStatus == Ndis802_11TKIPEnable)	||
		(wdev->WepStatus == Ndis802_11AESEnable)	||
		(wdev->WepStatus == Ndis802_11TKIPAESMix))
	{
		if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bClearEAPFrame))
		{
			DBGPRINT(RT_DEBUG_TRACE,("APHardTransmit --> clear eap frame !!!\n"));
			CipherAlg = CIPHER_NONE;
			pKey = NULL;
		}
		else if (!pMacEntry) /* M/BCAST to local BSS, use default key in shared key table */
		{
			KeyIdx = wdev->DefaultKeyId;
			CipherAlg  = pAd->SharedKey[apidx][KeyIdx].CipherAlg;
			if (CipherAlg)
				pKey = &pAd->SharedKey[apidx][KeyIdx];
		}
		else /* unicast to local BSS */
		{
			CipherAlg = pAd->MacTab.Content[RAWcid].PairwiseKey.CipherAlg;
			if (CipherAlg)
				pKey = &pAd->MacTab.Content[RAWcid].PairwiseKey;

#ifdef SOFT_ENCRYPT
			if (CLIENT_STATUS_TEST_FLAG(pMacEntry, fCLIENT_STATUS_SOFTWARE_ENCRYPT))
			{
				TX_BLK_SET_FLAG(pTxBlk, fTX_bSwEncrypt);

				/* TSC increment pre encryption transmittion */
				if (pKey == NULL)
					DBGPRINT(RT_DEBUG_ERROR, ("%s pKey == NULL!\n", __FUNCTION__));
				else
				{
					INC_TX_TSC(pKey->TxTsc, LEN_WPA_TSC);
				}
			}
#endif /* SOFT_ENCRYPT */
		}
	}
	else if (wdev->WepStatus == Ndis802_11WEPEnabled) /* WEP always uses shared key table */
	{
		KeyIdx = wdev->DefaultKeyId;
		CipherAlg  = pAd->SharedKey[apidx][KeyIdx].CipherAlg;
		if (CipherAlg)
			pKey = &pAd->SharedKey[apidx][KeyIdx];
	}
	else
	{
		CipherAlg = CIPHER_NONE;
		pKey = NULL;
	}

	pTxBlk->CipherAlg = CipherAlg;
	pTxBlk->pKey = pKey;
	pTxBlk->KeyIdx = KeyIdx;
}


#ifdef DOT11_N_SUPPORT
static inline VOID APBuildCache802_11Header(
	IN struct rtmp_adapter *pAd,
	IN TX_BLK *pTxBlk,
	IN UCHAR *pHeader)
{
	MAC_TABLE_ENTRY *pMacEntry;
	PHEADER_802_11 pHeader80211;

	pHeader80211 = (PHEADER_802_11)pHeader;
	pMacEntry = pTxBlk->pMacEntry;

	/*
		Update the cached 802.11 HEADER
	*/

	/* normal wlan header size : 24 octets */
	pTxBlk->MpduHeaderLen = sizeof(HEADER_802_11);

	/* More Bit */
	pHeader80211->FC.MoreData = TX_BLK_TEST_FLAG(pTxBlk, fTX_bMoreData);

	/* Sequence */
	pHeader80211->Sequence = pMacEntry->TxSeq[pTxBlk->UserPriority];
	pMacEntry->TxSeq[pTxBlk->UserPriority] = (pMacEntry->TxSeq[pTxBlk->UserPriority]+1) & MAXSEQ;

	/* SA */
#ifdef APCLI_SUPPORT
	if(IS_ENTRY_APCLI(pMacEntry))
	{	/* The addr3 of Ap-client packet is Destination Mac address. */
		COPY_MAC_ADDR(pHeader80211->Addr3, pTxBlk->pSrcBufHeader);
	}
	else
#endif /* APCLI_SUPPORT */
	{	/* The addr3 of normal packet send from DS is Src Mac address. */
		COPY_MAC_ADDR(pHeader80211->Addr3, pTxBlk->pSrcBufHeader + MAC_ADDR_LEN);
	}
}


#ifdef HDR_TRANS_TX_SUPPORT
static inline VOID APBuildCacheWifiInfo(
	IN struct rtmp_adapter 	*pAd,
	IN TX_BLK			*pTxBlk,
	IN UCHAR			*pWiInfo)
{
	MAC_TABLE_ENTRY	*pMacEntry;

	PWIFI_INFO_STRUC pWI;

	pWI = (PWIFI_INFO_STRUC)pWiInfo;
	pMacEntry = pTxBlk->pMacEntry;

	/* WIFI INFO size : 4 octets */
	pTxBlk->MpduHeaderLen = WIFI_INFO_SIZE;

	/* More Bit */
	pWI->field.More_Data = TX_BLK_TEST_FLAG(pTxBlk, fTX_bMoreData);

	/* Sequence */
	pWI->field.Seq_Num = pMacEntry->TxSeq[pTxBlk->UserPriority];
	pMacEntry->TxSeq[pTxBlk->UserPriority] = (pMacEntry->TxSeq[pTxBlk->UserPriority]+1) & MAXSEQ;
}
#endif /* HDR_TRANS_TX_SUPPORT */
#endif /* DOT11_N_SUPPORT */


#ifdef HDR_TRANS_TX_SUPPORT
static inline VOID APBuildWifiInfo(
	IN  struct rtmp_adapter *  pAd,
	IN  TX_BLK          *pTxBlk)
{
	UINT8 TXWISize = pAd->chipCap.TXWISize;
	PWIFI_INFO_STRUC pWI;


	/* WIFI INFO size : 4 octets */
	pTxBlk->MpduHeaderLen = WIFI_INFO_SIZE;

	pWI =
	    (WIFI_INFO_STRUC *) & pTxBlk->HeaderBuf[TXINFO_SIZE + TXWISize];

	memset(pWI, 0, WIFI_INFO_SIZE);

#ifdef APCLI_SUPPORT
	if (IS_ENTRY_APCLI(pTxBlk->pMacEntry))
		pWI->field.Mode = 2;	/* STA */
	else
#endif /* APCLI_SUPPORT */
	pWI->field.Mode = 1;	/* AP */

	pWI->field.QoS = (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM)) ? 1 : 0;

	if (pTxBlk->pMacEntry)
	{
		if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bForceNonQoS))
		{
			pWI->field.Seq_Num = pTxBlk->pMacEntry->NonQosDataSeq;
			pTxBlk->pMacEntry->NonQosDataSeq = (pTxBlk->pMacEntry->NonQosDataSeq+1) & MAXSEQ;
		}
		else
		{
    	    pWI->field.Seq_Num = pTxBlk->pMacEntry->TxSeq[pTxBlk->UserPriority];
    	    pTxBlk->pMacEntry->TxSeq[pTxBlk->UserPriority] = (pTxBlk->pMacEntry->TxSeq[pTxBlk->UserPriority]+1) & MAXSEQ;
    	}
		pWI->field.BssIdx = pTxBlk->pMacEntry->apidx;
	}
	else
	{
		pWI->field.Seq_Num = pAd->Sequence;
		pAd->Sequence = (pAd->Sequence+1) & MAXSEQ; /* next sequence */
	}

	pWI->field.More_Data = TX_BLK_TEST_FLAG(pTxBlk, fTX_bMoreData);

	if (pTxBlk->CipherAlg != CIPHER_NONE)
		pWI->field.WEP = 1;
}
#endif /* HDR_TRANS_TX_SUPPORT */


static inline VOID APBuildCommon802_11Header(struct rtmp_adapter *pAd, TX_BLK *pTxBlk)
{
	HEADER_802_11 *wifi_hdr;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

	/*
		MAKE A COMMON 802.11 HEADER
	*/

	/* normal wlan header size : 24 octets */
	pTxBlk->MpduHeaderLen = sizeof(HEADER_802_11);
	wifi_hdr = (HEADER_802_11 *) &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWISize + TSO_SIZE];
	memset(wifi_hdr, 0, sizeof(HEADER_802_11));

	wifi_hdr->FC.FrDs = 1;
	wifi_hdr->FC.Type = FC_TYPE_DATA;
	wifi_hdr->FC.SubType = ((TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM)) ? SUBTYPE_QDATA : SUBTYPE_DATA);

	if (pTxBlk->pMacEntry)
	{
		if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bForceNonQoS))
		{
			wifi_hdr->Sequence = pTxBlk->pMacEntry->NonQosDataSeq;
			pTxBlk->pMacEntry->NonQosDataSeq = (pTxBlk->pMacEntry->NonQosDataSeq+1) & MAXSEQ;
		}
		else
		{
			wifi_hdr->Sequence = pTxBlk->pMacEntry->TxSeq[pTxBlk->UserPriority];
			pTxBlk->pMacEntry->TxSeq[pTxBlk->UserPriority] = (pTxBlk->pMacEntry->TxSeq[pTxBlk->UserPriority]+1) & MAXSEQ;
    		}
	}
	else
	{
		wifi_hdr->Sequence = pAd->Sequence;
		pAd->Sequence = (pAd->Sequence+1) & MAXSEQ; /* next sequence */
	}

	wifi_hdr->Frag = 0;
	wifi_hdr->FC.MoreData = TX_BLK_TEST_FLAG(pTxBlk, fTX_bMoreData);

#ifdef APCLI_SUPPORT
	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bApCliPacket))
	{
		wifi_hdr->FC.ToDs = 1;
		wifi_hdr->FC.FrDs = 0;
		COPY_MAC_ADDR(wifi_hdr->Addr1, APCLI_ROOT_BSSID_GET(pAd, pTxBlk->Wcid));	/* to AP2 */
		COPY_MAC_ADDR(wifi_hdr->Addr2, pTxBlk->pApCliEntry->wdev.if_addr);		/* from AP1 */
		COPY_MAC_ADDR(wifi_hdr->Addr3, pTxBlk->pSrcBufHeader);					/* DA */
	}
	else
#endif /* APCLI_SUPPORT */
	{
		/* TODO: how about "MoreData" bit? AP need to set this bit especially for PS-POLL response */
		{
		   	COPY_MAC_ADDR(wifi_hdr->Addr1, pTxBlk->pSrcBufHeader);					/* DA */
		}
		COPY_MAC_ADDR(wifi_hdr->Addr2, pAd->ApCfg.MBSSID[pTxBlk->apidx].wdev.bssid);		/* BSSID */
		COPY_MAC_ADDR(wifi_hdr->Addr3, pTxBlk->pSrcBufHeader + MAC_ADDR_LEN);			/* SA */
	}


#ifdef RT_CFG80211_P2P_SUPPORT
            /* To not disturb the Opps test, set psm bit if I use power save mode.  */
            /* P2P Test case 7.1.3 */
            if (CFG_P2PCLI_ON(pAd) && pAd->cfg80211_ctrl.bP2pCliPmEnable &&
                CFG80211_P2P_TEST_BIT(pAd->cfg80211_ctrl.CTWindows, P2P_OPPS_BIT))
            {
                wifi_hdr->FC.PwrMgmt = PWR_SAVE;
            }
#endif /*RT_CFG80211_P2P_SUPPORT*/

	if (pTxBlk->CipherAlg != CIPHER_NONE)
		wifi_hdr->FC.Wep = 1;
}


static inline PUCHAR AP_Build_ARalink_Frame_Header(struct rtmp_adapter *pAd, TX_BLK *pTxBlk)
{
	PUCHAR			pHeaderBufPtr;/*, pSaveBufPtr; */
	HEADER_802_11	*pHeader_802_11;
	struct sk_buff *pNextPacket;
	uint32_t 		nextBufLen;
	PQUEUE_ENTRY	pQEntry;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

	APFindCipherAlgorithm(pAd, pTxBlk);
	APBuildCommon802_11Header(pAd, pTxBlk);


	pHeaderBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWISize];
	pHeader_802_11 = (HEADER_802_11 *) pHeaderBufPtr;

	/* steal "order" bit to mark "aggregation" */
	pHeader_802_11->FC.Order = 1;

	/* skip common header */
	pHeaderBufPtr += pTxBlk->MpduHeaderLen;

	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM))
	{
		/*
			build QOS Control bytes
		*/
		*pHeaderBufPtr = (pTxBlk->UserPriority & 0x0F);
#ifdef UAPSD_SUPPORT
		if (CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_APSD_CAPABLE)
		)
		{
			/*
			 * we can not use bMoreData bit to get EOSP bit because
			 * maybe bMoreData = 1 & EOSP = 1 when Max SP Length != 0
			 */
			 if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM_UAPSD_EOSP))
				*pHeaderBufPtr |= (1 << 4);
		}
#endif /* UAPSD_SUPPORT */

		*(pHeaderBufPtr+1) = 0;
		pHeaderBufPtr +=2;
		pTxBlk->MpduHeaderLen += 2;
	}

	/* padding at front of LLC header. LLC header should at 4-bytes aligment. */
	pTxBlk->HdrPadLen = (ULONG)pHeaderBufPtr;
	pHeaderBufPtr = (PUCHAR)ROUND_UP(pHeaderBufPtr, 4);
	pTxBlk->HdrPadLen = (ULONG)(pHeaderBufPtr - pTxBlk->HdrPadLen);


	/*
		For RA Aggregation, put the 2nd MSDU length(extra 2-byte field) after
		QOS_CONTROL in little endian format
	*/
	pQEntry = pTxBlk->TxPacketList.Head;
	pNextPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
	nextBufLen = GET_OS_PKT_LEN(pNextPacket);
	if (RTMP_GET_PACKET_VLAN(pNextPacket))
		nextBufLen -= LENGTH_802_1Q;

	*pHeaderBufPtr = (UCHAR)nextBufLen & 0xff;
	*(pHeaderBufPtr+1) = (UCHAR)(nextBufLen >> 8);

	pHeaderBufPtr += 2;
	pTxBlk->MpduHeaderLen += 2;

	return pHeaderBufPtr;

}


#ifdef DOT11_N_SUPPORT
static inline BOOLEAN BuildHtcField(
	IN struct rtmp_adapter *pAd,
	IN TX_BLK *pTxBlk,
	IN  MAC_TABLE_ENTRY *pMacEntry,
	IN PUCHAR pHeaderBufPtr)
{
	BOOLEAN bHTCPlus = FALSE;


	return bHTCPlus;
}


static inline PUCHAR AP_Build_AMSDU_Frame_Header(
	IN struct rtmp_adapter *pAd,
	IN TX_BLK *pTxBlk)
{
	UCHAR *pHeaderBufPtr;
	HEADER_802_11 *pHeader_802_11;
	UINT8 TXWISize = pAd->chipCap.TXWISize;


	APFindCipherAlgorithm(pAd, pTxBlk);
	APBuildCommon802_11Header(pAd, pTxBlk);

	pHeaderBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWISize];
	pHeader_802_11 = (HEADER_802_11 *) pHeaderBufPtr;

	/* skip common header */
	pHeaderBufPtr += pTxBlk->MpduHeaderLen;

	/* build QOS Control bytes */
	*pHeaderBufPtr = (pTxBlk->UserPriority & 0x0F);
#ifdef UAPSD_SUPPORT
	if (CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_APSD_CAPABLE)
	)
	{
		/*
		 * we can not use bMoreData bit to get EOSP bit because
		 * maybe bMoreData = 1 & EOSP = 1 when Max SP Length != 0
		 */
		if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM_UAPSD_EOSP))
			*pHeaderBufPtr |= (1 << 4);
	}
#endif /* UAPSD_SUPPORT */


	/* A-MSDU packet */
	*pHeaderBufPtr |= 0x80;

	*(pHeaderBufPtr+1) = 0;
	pHeaderBufPtr +=2;
	pTxBlk->MpduHeaderLen += 2;

#ifdef TXBF_SUPPORT
	if (pTxBlk->pMacEntry && pAd->chipCap.FlgHwTxBfCap)
	{
		MAC_TABLE_ENTRY *pMacEntry = pTxBlk->pMacEntry;
		BOOLEAN bHTCPlus = FALSE;

		pTxBlk->TxSndgPkt = SNDG_TYPE_DISABLE;

		NdisAcquireSpinLock(&pMacEntry->TxSndgLock);
		if (pMacEntry->TxSndgType >= SNDG_TYPE_SOUNDING)
		{
			memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));

			if (pMacEntry->TxSndgType == SNDG_TYPE_SOUNDING)
			{
				/* Select compress if supported. Otherwise select noncompress */
				if (pAd->CommonCfg.ETxBfNoncompress==0 &&
					(pMacEntry->HTCapability.TxBFCap.ExpComBF>0) )
					((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 3;
				else
					((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 2;

				/* Clear NDP Announcement */
				((PHT_CONTROL)pHeaderBufPtr)->NDPAnnounce = 0;

			}
			else if (pMacEntry->TxSndgType == SNDG_TYPE_NDP)
			{
				/* Select compress if supported. Otherwise select noncompress */
				if (pAd->CommonCfg.ETxBfNoncompress==0 &&
					(pMacEntry->HTCapability.TxBFCap.ExpComBF>0) &&
						(pMacEntry->HTCapability.TxBFCap.ComSteerBFAntSup >= (pMacEntry->sndgMcs/8))
					)
					((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 3;
					else
					((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 2;

				/* Set NDP Announcement */
				((PHT_CONTROL)pHeaderBufPtr)->NDPAnnounce = 1;

				pTxBlk->TxNDPSndgBW = pMacEntry->sndgBW;
				pTxBlk->TxNDPSndgMcs = pMacEntry->sndgMcs;
			}

			pTxBlk->TxSndgPkt = pMacEntry->TxSndgType;
			/* arvin add for julian request send NDP */
			pMacEntry->TxSndgType = SNDG_TYPE_DISABLE;
			bHTCPlus = TRUE;
		}
		NdisReleaseSpinLock(&pMacEntry->TxSndgLock);

#ifdef MFB_SUPPORT
#if defined(MRQ_FORCE_TX) /* have to replace this by the correct condition!!! */
		pMacEntry->HTCapability.ExtHtCapInfo.MCSFeedback = MCSFBK_MRQ;
#endif

		/*
			Ignore sounding frame because the signal format of sounding frmae may
			be different from normal data frame, which may result in different MFB
		*/
		if ((pMacEntry->HTCapability.ExtHtCapInfo.MCSFeedback >=MCSFBK_MRQ) &&
			(pTxBlk->TxSndgPkt == SNDG_TYPE_DISABLE))
		{
			if (bHTCPlus == FALSE)
			{
				bHTCPlus = TRUE;
				memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));
			}

			MFB_PerPareMRQ(pAd, pHeaderBufPtr, pMacEntry);
		}

		if (pAd->CommonCfg.HtCapability.ExtHtCapInfo.MCSFeedback >=MCSFBK_MRQ && pMacEntry->toTxMfb == 1)
		{
			if (bHTCPlus == FALSE)
			{
				memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));
				bHTCPlus = TRUE;
			}

			MFB_PerPareMFB(pAd, pHeaderBufPtr, pMacEntry); /* not complete yet!!! */
			pMacEntry->toTxMfb = 0;
		}
#endif /* MFB_SUPPORT */

		if (bHTCPlus == TRUE)
		{
			pHeader_802_11->FC.Order = 1;
			pHeaderBufPtr += 4;
			pTxBlk->MpduHeaderLen += 4;
		}
	}
#endif /* TXBF_SUPPORT */

	/*
		padding at front of LLC header
		LLC header should locate at 4-octets aligment
		@@@ MpduHeaderLen excluding padding @@@
	*/
	pTxBlk->HdrPadLen = (ULONG)pHeaderBufPtr;
	pHeaderBufPtr = (PUCHAR) ROUND_UP(pHeaderBufPtr, 4);
	pTxBlk->HdrPadLen = (ULONG)(pHeaderBufPtr - pTxBlk->HdrPadLen);

	return pHeaderBufPtr;

}


VOID AP_AMPDU_Frame_Tx(struct rtmp_adapter *pAd, TX_BLK *pTxBlk)
{
	HEADER_802_11 *pHeader_802_11;
	UCHAR *pHeaderBufPtr;
	USHORT freeCnt = 1;
	MAC_TABLE_ENTRY *pMacEntry;
	PQUEUE_ENTRY pQEntry;
	BOOLEAN	 bHTCPlus = FALSE;
	UINT hdr_offset;
	UINT8 TXWISize = pAd->chipCap.TXWISize;


	ASSERT(pTxBlk);

	pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
	pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
	if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE)
	{
#ifdef STATS_COUNT_SUPPORT
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;

		if (pMbss != NULL)
			pMbss->TxDropCount ++;
#endif /* STATS_COUNT_SUPPORT */

		RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
		return;
	}

	hdr_offset = TXINFO_SIZE + TXWISize + TSO_SIZE;
	pMacEntry = pTxBlk->pMacEntry;
	if ((pMacEntry->isCached)
#ifdef TXBF_SUPPORT
		&& (pMacEntry->TxSndgType == SNDG_TYPE_DISABLE)
#endif /* TXBF_SUPPORT */
	)
	{
#ifndef VENDOR_FEATURE1_SUPPORT
		memmove(&pTxBlk->HeaderBuf[TXINFO_SIZE], &pMacEntry->CachedBuf[0], TXWISize + sizeof(HEADER_802_11));
#else
		pTxBlk->HeaderBuf = (UCHAR *)(pMacEntry->HeaderBuf);
#endif /* VENDOR_FEATURE1_SUPPORT */
		pHeaderBufPtr = (PUCHAR)(&pTxBlk->HeaderBuf[hdr_offset]);
		APBuildCache802_11Header(pAd, pTxBlk, pHeaderBufPtr);

#ifdef SOFT_ENCRYPT
		RTMPUpdateSwCacheCipherInfo(pAd, pTxBlk, pHeaderBufPtr);
#endif /* SOFT_ENCRYPT */
	}
	else
	{
		APFindCipherAlgorithm(pAd, pTxBlk);
		APBuildCommon802_11Header(pAd, pTxBlk);

		pHeaderBufPtr = &pTxBlk->HeaderBuf[hdr_offset];
	}

#ifdef SOFT_ENCRYPT
	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt))
	{
		if (RTMPExpandPacketForSwEncrypt(pAd, pTxBlk) == FALSE)
		{
			RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
			return;
		}
	}
#endif /* SOFT_ENCRYPT */

#ifdef VENDOR_FEATURE1_SUPPORT
	if(pMacEntry->isCached
		&& (pMacEntry->Protocol == (RTMP_GET_PACKET_PROTOCOL(pTxBlk->pPacket)))
#ifdef SOFT_ENCRYPT
		&& !TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt)
#endif /* SOFT_ENCRYPT */
#ifdef TXBF_SUPPORT
		&& (pMacEntry->TxSndgType == SNDG_TYPE_DISABLE)
#endif /* TXBF_SUPPORT */
	)
	{
		pHeader_802_11 = (HEADER_802_11 *) pHeaderBufPtr;

		/* skip common header */
		pHeaderBufPtr += pTxBlk->MpduHeaderLen;

		/* build QOS Control bytes */
		*pHeaderBufPtr = (pTxBlk->UserPriority & 0x0F);
#ifdef UAPSD_SUPPORT
		if (CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_APSD_CAPABLE)
			)
		{
			/*
			 * we can not use bMoreData bit to get EOSP bit because
			 * maybe bMoreData = 1 & EOSP = 1 when Max SP Length != 0
			 */
			if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM_UAPSD_EOSP))
				*pHeaderBufPtr |= (1 << 4);
		}
#endif /* UAPSD_SUPPORT */
		pTxBlk->MpduHeaderLen = pMacEntry->MpduHeaderLen;
		pHeaderBufPtr = ((PUCHAR)pHeader_802_11) + pTxBlk->MpduHeaderLen;

		pTxBlk->HdrPadLen = pMacEntry->HdrPadLen;

		/* skip 802.3 header */
		pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
		pTxBlk->SrcBufLen  -= LENGTH_802_3;

		/* skip vlan tag */
		if (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket))
		{
			pTxBlk->pSrcBufData += LENGTH_802_1Q;
			pTxBlk->SrcBufLen -= LENGTH_802_1Q;
		}
	}
	else
#endif /* VENDOR_FEATURE1_SUPPORT */
	{
		pHeader_802_11 = (HEADER_802_11 *) pHeaderBufPtr;

		/* skip common header */
		pHeaderBufPtr += pTxBlk->MpduHeaderLen;

		/* build QOS Control bytes */
		*pHeaderBufPtr = (pTxBlk->UserPriority & 0x0F);
#ifdef UAPSD_SUPPORT
		if (CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_APSD_CAPABLE)
			)
		{
			/*
			 * we can not use bMoreData bit to get EOSP bit because
			 * maybe bMoreData = 1 & EOSP = 1 when Max SP Length != 0
			 */
			if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM_UAPSD_EOSP))
				*pHeaderBufPtr |= (1 << 4);
		}
#endif /* UAPSD_SUPPORT */

		*(pHeaderBufPtr+1) = 0;
		pHeaderBufPtr +=2;
		pTxBlk->MpduHeaderLen += 2;

		/* build HTC control filed after QoS field */
		if ((pAd->CommonCfg.bRdg == TRUE)
			&& (CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_RDG_CAPABLE))
#ifdef TXBF_SUPPORT
			&& (pMacEntry->TxSndgType != SNDG_TYPE_NDP)
#endif /* TXBF_SUPPORT */
		)
		{
			memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));
			((PHT_CONTROL)pHeaderBufPtr)->RDG = 1;
			bHTCPlus = TRUE;
		}

#ifdef TXBF_SUPPORT
		if (pAd->chipCap.FlgHwTxBfCap)
		{
			pTxBlk->TxSndgPkt = SNDG_TYPE_DISABLE;

			NdisAcquireSpinLock(&pMacEntry->TxSndgLock);
			if (pMacEntry->TxSndgType >= SNDG_TYPE_SOUNDING)
			{
				if (bHTCPlus == FALSE)
				{
				memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));
					bHTCPlus = TRUE;
				}

				if (pMacEntry->TxSndgType == SNDG_TYPE_SOUNDING)
				{
					/* Select compress if supported. Otherwise select noncompress */
					if (pAd->CommonCfg.ETxBfNoncompress==0 &&
						(pMacEntry->HTCapability.TxBFCap.ExpComBF>0) )
							((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 3;
					else
							((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 2;

					/* Clear NDP Announcement */
					((PHT_CONTROL)pHeaderBufPtr)->NDPAnnounce = 0;

				}
				else if (pMacEntry->TxSndgType == SNDG_TYPE_NDP)
				{
					/* Select compress if supported. Otherwise select noncompress */
					if ((pAd->CommonCfg.ETxBfNoncompress==0) &&
						(pMacEntry->HTCapability.TxBFCap.ExpComBF>0) &&
						(pMacEntry->HTCapability.TxBFCap.ComSteerBFAntSup >= (pMacEntry->sndgMcs/8))
					)
							((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 3;
					else
							((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 2;

					/* Set NDP Announcement */
					((PHT_CONTROL)pHeaderBufPtr)->NDPAnnounce = 1;

					pTxBlk->TxNDPSndgBW = pMacEntry->sndgBW;
					pTxBlk->TxNDPSndgMcs = pMacEntry->sndgMcs;
				}

				pTxBlk->TxSndgPkt = pMacEntry->TxSndgType;
				pMacEntry->TxSndgType = SNDG_TYPE_DISABLE;
			}

			NdisReleaseSpinLock(&pMacEntry->TxSndgLock);

#ifdef MFB_SUPPORT
#if defined(MRQ_FORCE_TX)
			/* have to replace this by the correct condition!!! */
			pMacEntry->HTCapability.ExtHtCapInfo.MCSFeedback = MCSFBK_MRQ;
#endif

			/*
				Ignore sounding frame because the signal format of sounding frmae may
				be different from normal data frame, which may result in different MFB
			*/
			if ((pMacEntry->HTCapability.ExtHtCapInfo.MCSFeedback >=MCSFBK_MRQ) &&
				(pTxBlk->TxSndgPkt == SNDG_TYPE_DISABLE))
			{
				if (bHTCPlus == FALSE)
				{
				memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));
					bHTCPlus = TRUE;
				}
				MFB_PerPareMRQ(pAd, pHeaderBufPtr, pMacEntry);
			}

			if (pAd->CommonCfg.HtCapability.ExtHtCapInfo.MCSFeedback >=MCSFBK_MRQ &&
				pMacEntry->toTxMfb == 1)
			{
				if (bHTCPlus == FALSE)
				{
				memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));
					bHTCPlus = TRUE;
				}
				MFB_PerPareMFB(pAd, pHeaderBufPtr, pMacEntry);/* not complete yet!!! */
				pMacEntry->toTxMfb = 0;
			}
#endif /* MFB_SUPPORT */
		}
#endif /* TXBF_SUPPORT */

		if (bHTCPlus == TRUE)
		{
			/* mark HTC bit */
			pHeader_802_11->FC.Order = 1;
			pHeaderBufPtr += 4;
			pTxBlk->MpduHeaderLen += 4;
		}

		/*pTxBlk->MpduHeaderLen = pHeaderBufPtr - pTxBlk->HeaderBuf - TXWI_SIZE - TXINFO_SIZE; */
		ASSERT(pTxBlk->MpduHeaderLen >= 24);

		/* skip 802.3 header */
		pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
		pTxBlk->SrcBufLen  -= LENGTH_802_3;

		/* skip vlan tag */
		if (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket))
		{
			pTxBlk->pSrcBufData += LENGTH_802_1Q;
			pTxBlk->SrcBufLen -= LENGTH_802_1Q;
		}

		/* The remaining content of MPDU header should locate at 4-octets aligment */
		pTxBlk->HdrPadLen = (ULONG)pHeaderBufPtr;
		pHeaderBufPtr = (PUCHAR) ROUND_UP(pHeaderBufPtr, 4);
		pTxBlk->HdrPadLen = (ULONG)(pHeaderBufPtr - pTxBlk->HdrPadLen);

#ifdef VENDOR_FEATURE1_SUPPORT
		pMacEntry->HdrPadLen = pTxBlk->HdrPadLen;
#endif /* VENDOR_FEATURE1_SUPPORT */

#ifdef SOFT_ENCRYPT
		if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt))
		{
			UCHAR iv_offset = 0, ext_offset = 0;

			/*
				If original Ethernet frame contains no LLC/SNAP,
				then an extra LLC/SNAP encap is required
			*/
			EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(pTxBlk->pSrcBufData - 2, pTxBlk->pExtraLlcSnapEncap);

			/* Insert LLC-SNAP encapsulation (8 octets) to MPDU data buffer */
			if (pTxBlk->pExtraLlcSnapEncap)
			{
				/* Reserve the front 8 bytes of data for LLC header */
				pTxBlk->pSrcBufData -= LENGTH_802_1_H;
				pTxBlk->SrcBufLen += LENGTH_802_1_H;

				memmove(pTxBlk->pSrcBufData, pTxBlk->pExtraLlcSnapEncap, 6);
			}

			/* Construct and insert specific IV header to MPDU header */
			RTMPSoftConstructIVHdr(pTxBlk->CipherAlg,
								   pTxBlk->KeyIdx,
								   pTxBlk->pKey->TxTsc,
								   pHeaderBufPtr,
								   &iv_offset);
			pHeaderBufPtr += iv_offset;
			pTxBlk->MpduHeaderLen += iv_offset;

			/* Encrypt the MPDU data by software */
			RTMPSoftEncryptionAction(pAd,
									 pTxBlk->CipherAlg,
									 (PUCHAR)pHeader_802_11,
									pTxBlk->pSrcBufData,
									pTxBlk->SrcBufLen,
									pTxBlk->KeyIdx,
									   pTxBlk->pKey,
									 &ext_offset);
			pTxBlk->SrcBufLen += ext_offset;
			pTxBlk->TotalFrameLen += ext_offset;

		}
		else
#endif /* SOFT_ENCRYPT */
		{


			/* Insert LLC-SNAP encapsulation - 8 octets */
			EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(pTxBlk->pSrcBufData-2, pTxBlk->pExtraLlcSnapEncap);
			if (pTxBlk->pExtraLlcSnapEncap)
			{
				memmove(pHeaderBufPtr, pTxBlk->pExtraLlcSnapEncap, 6);

				pHeaderBufPtr += 6;
				/* get 2 octets (TypeofLen) */
				memmove(pHeaderBufPtr, pTxBlk->pSrcBufData-2, 2);

				pHeaderBufPtr += 2;
				pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
			}
		}

#ifdef VENDOR_FEATURE1_SUPPORT
		pMacEntry->Protocol = RTMP_GET_PACKET_PROTOCOL(pTxBlk->pPacket);
		pMacEntry->MpduHeaderLen = pTxBlk->MpduHeaderLen;
#endif /* VENDOR_FEATURE1_SUPPORT */
	}

	if ((pMacEntry->isCached)
#ifdef TXBF_SUPPORT
		&& (pTxBlk->TxSndgPkt == SNDG_TYPE_DISABLE)
#endif /* TXBF_SUPPORT */
	)
	{
		RTMPWriteTxWI_Cache(pAd, (TXWI_STRUC *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), pTxBlk);
	}
	else
	{
		RTMPWriteTxWI_Data(pAd, (TXWI_STRUC *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), pTxBlk);



		memset((PUCHAR)(&pMacEntry->CachedBuf[0]), 0, sizeof(pMacEntry->CachedBuf));
		memmove(&pMacEntry->CachedBuf[0], &pTxBlk->HeaderBuf[TXINFO_SIZE],
						(pHeaderBufPtr - (PUCHAR)(&pTxBlk->HeaderBuf[TXINFO_SIZE])));

#ifdef VENDOR_FEATURE1_SUPPORT
		/* use space to get performance enhancement */
		memset((PUCHAR)(&pMacEntry->HeaderBuf[0]), 0, sizeof(pMacEntry->HeaderBuf));
		memmove(&pMacEntry->HeaderBuf[0], &pTxBlk->HeaderBuf[0],
						(pHeaderBufPtr - (PUCHAR)(&pTxBlk->HeaderBuf[0])));
#endif /* VENDOR_FEATURE1_SUPPORT */

		pMacEntry->isCached = TRUE;

		if (RTMP_GET_PACKET_LOWRATE(pTxBlk->pPacket))
			pMacEntry->isCached = FALSE;
	}

#ifdef TXBF_SUPPORT
	if (pTxBlk->TxSndgPkt != SNDG_TYPE_DISABLE)
		pMacEntry->isCached = FALSE;
#endif /* TXBF_SUPPORT */

#ifdef STATS_COUNT_SUPPORT
	/* calculate Transmitted AMPDU count and ByteCount */
	{
		pAd->RalinkCounters.TransmittedMPDUsInAMPDUCount.u.LowPart ++;
		pAd->RalinkCounters.TransmittedOctetsInAMPDUCount.QuadPart += pTxBlk->SrcBufLen;
	}

	/* calculate Tx count and ByteCount per BSS */
	{
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;


		if (pMbss != NULL)
		{
			pMbss->TransmittedByteCount += pTxBlk->SrcBufLen;
			pMbss->TxCount ++;

#ifdef STATS_COUNT_SUPPORT
			if(IS_MULTICAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->mcPktsTx++;
			else if(IS_BROADCAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->bcPktsTx++;
			else
				pMbss->ucPktsTx++;
#endif /* STATS_COUNT_SUPPORT */
		}

		if(pMacEntry->Sst == SST_ASSOC)
		{
			INC_COUNTER64(pMacEntry->TxPackets);
			pMacEntry->TxBytes+=pTxBlk->SrcBufLen;
		}
	}

#endif /* STATS_COUNT_SUPPORT */

	HAL_WriteTxResource(pAd, pTxBlk, TRUE, &freeCnt);


#ifdef DBG_CTRL_SUPPORT
#ifdef INCLUDE_DEBUG_QUEUE
	if (pAd->CommonCfg.DebugFlags & DBF_DBQ_TXFRAME)
		dbQueueEnqueueTxFrame((UCHAR *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), (UCHAR *)pHeader_802_11);
#endif /* INCLUDE_DEBUG_QUEUE */
#endif /* DBG_CTRL_SUPPORT */

	/*
		Kick out Tx
	*/
	HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);

	pAd->RalinkCounters.KickTxCount++;
	pAd->RalinkCounters.OneSecTxDoneCount++;

}


#ifdef HDR_TRANS_TX_SUPPORT
VOID AP_AMPDU_Frame_Tx_Hdr_Trns(
	IN	struct rtmp_adapter *pAd,
	IN	TX_BLK			*pTxBlk)
{
	PUCHAR			pWiBufPtr;
/*	UCHAR			QueIdx = pTxBlk->QueIdx; */
	USHORT			FreeNumber = 1; /* no use */
	MAC_TABLE_ENTRY	*pMacEntry;
	PQUEUE_ENTRY	pQEntry;
	UINT8 TXWISize = pAd->chipCap.TXWISize;
	PWIFI_INFO_STRUC pWI;

	ASSERT(pTxBlk);

	pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
	pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
	if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE)
	{
#ifdef STATS_COUNT_SUPPORT
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;

		if (pMbss != NULL)
			pMbss->TxDropCount ++;
#endif /* STATS_COUNT_SUPPORT */

		RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
		return;
	}

/*
	if ( pAd->debug_on )
	{
		UCHAR index = RTMP_GET_DEBUG_INDEX(pTxBlk->pPacket);
		do_gettimeofday(&(pAd->debug_time_2[index]));
	}
*/

	pMacEntry = pTxBlk->pMacEntry;
	if ((pMacEntry->isCached)
	)
	{
		/* It should be cleared!!! */
		/*memset((PUCHAR)(&pTxBlk->HeaderBuf[0]), 0, sizeof(pTxBlk->HeaderBuf)); */
		memmove(&pTxBlk->HeaderBuf[TXINFO_SIZE], &pMacEntry->CachedBuf[0],TXWISize + WIFI_INFO_SIZE);

		pWiBufPtr = (PUCHAR)(&pTxBlk->HeaderBuf[TXINFO_SIZE + TXWISize]);
		APBuildCacheWifiInfo(pAd, pTxBlk, pWiBufPtr);
	}
	else
	{
		APFindCipherAlgorithm(pAd, pTxBlk);
		APBuildWifiInfo(pAd, pTxBlk);

		pWiBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWISize];
	}

	pWI = (PWIFI_INFO_STRUC)pWiBufPtr;

	pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader;

	if (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket))
		pWI->field.VLAN = TRUE;

	pWI->field.TID = (pTxBlk->UserPriority & 0x0F);
#ifdef UAPSD_SUPPORT
	if (CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_APSD_CAPABLE)
		&& TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM_UAPSD_EOSP))
		pWI->field.EOSP = TRUE;
#endif /* UAPSD_SUPPORT */

	{

		/*
			build HTC+
			HTC control filed following QoS field
		*/
		if ((pAd->CommonCfg.bRdg == TRUE)
			&& (CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_RDG_CAPABLE))
		)
		{
			pWI->field.RDG = 1;
		}

	}

/*
	if ( pAd->debug_on )
	{
		UCHAR index = RTMP_GET_DEBUG_INDEX(pTxBlk->pPacket);
		do_gettimeofday(&(pAd->debug_time_3[index]));
	}
*/

	if ((pMacEntry->isCached)
	)
	{
		RTMPWriteTxWI_Cache(pAd, (TXWI_STRUC *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), pTxBlk);
	}
	else
	{
		RTMPWriteTxWI_Data(pAd, (TXWI_STRUC *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), pTxBlk);

		memset((PUCHAR)(&pMacEntry->CachedBuf[0]), 0, sizeof(pMacEntry->CachedBuf));
		memmove(&pMacEntry->CachedBuf[0], pTxBlk->HeaderBuf[TXINFO_SIZE],
						TXWISize + WIFI_INFO_SIZE);


		pMacEntry->isCached = TRUE;

	}


#ifdef STATS_COUNT_SUPPORT
	/* calculate Transmitted AMPDU count and ByteCount */
	{
		pAd->RalinkCounters.TransmittedMPDUsInAMPDUCount.u.LowPart ++;
		pAd->RalinkCounters.TransmittedOctetsInAMPDUCount.QuadPart += pTxBlk->SrcBufLen;
	}

	/* calculate Tx count and ByteCount per BSS */
	{
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;


		if (pMbss != NULL)
		{
			pMbss->TransmittedByteCount += pTxBlk->SrcBufLen;
			pMbss->TxCount ++;

#ifdef STATS_COUNT_SUPPORT
			if(IS_MULTICAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->mcPktsTx++;
			else if(IS_BROADCAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->bcPktsTx++;
			else
				pMbss->ucPktsTx++;
#endif /* STATS_COUNT_SUPPORT */
		}

		if(pMacEntry->Sst == SST_ASSOC)
		{
			INC_COUNTER64(pMacEntry->TxPackets);
			pMacEntry->TxBytes+=pTxBlk->SrcBufLen;
		}
	}

#endif /* STATS_COUNT_SUPPORT */

	/*FreeNumber = GET_TXRING_FREENO(pAd, QueIdx); */

	HAL_WriteTxResource(pAd, pTxBlk, TRUE, &FreeNumber);


	/*
	if ( pAd->debug_on )
	{
		UCHAR index = RTMP_GET_DEBUG_INDEX(pTxBlk->pPacket);
		do_gettimeofday(&(pAd->debug_time_4[index]));
	}
*/

	/*
		Kick out Tx
	*/
	HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);

	pAd->RalinkCounters.KickTxCount++;
	pAd->RalinkCounters.OneSecTxDoneCount++;

/*
	if ( pAd->debug_on )
	{
		UCHAR index = RTMP_GET_DEBUG_INDEX(pTxBlk->pPacket);
		do_gettimeofday(&(pAd->debug_time_5[index]));
		if ( pAd->debug_index < 201 )
		{
			pAd->debug_index ++;
		} else {
			pAd->debug_on = 0;
		}
	}
*/

}
#endif /* HDR_TRANS_TX_SUPPORT */

VOID AP_AMSDU_Frame_Tx(struct rtmp_adapter *pAd, TX_BLK *pTxBlk)
{
	UCHAR *pHeaderBufPtr, *subFrameHeader;
	USHORT freeCnt = 1; /* no use */
	USHORT subFramePayloadLen = 0;	/* AMSDU Subframe length without AMSDU-Header / Padding. */
	USHORT totalMPDUSize=0;
	UCHAR padding = 0;
	USHORT FirstTx = 0, LastTxIdx = 0;
	int frameNum = 0;
	PQUEUE_ENTRY pQEntry;

#ifdef CONFIG_AP_SUPPORT
#ifdef APCLI_SUPPORT
	PAPCLI_STRUCT   pApCliEntry = NULL;
#endif /* APCLI_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */

	MAC_TABLE_ENTRY *pMacEntry = pTxBlk->pMacEntry;

	ASSERT((pTxBlk->TxPacketList.Number > 1));

	while(pTxBlk->TxPacketList.Head)
	{
		pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
		pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);

		if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE)
		{
#ifdef STATS_COUNT_SUPPORT
			MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;

			if (pMbss != NULL)
				pMbss->TxDropCount++;
#endif /* STATS_COUNT_SUPPORT */
			RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
			continue;
		}

		/* skip 802.3 header */
		pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
		pTxBlk->SrcBufLen -= LENGTH_802_3;

		/* skip vlan tag */
		if (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket))
		{
			pTxBlk->pSrcBufData	+= LENGTH_802_1Q;
			pTxBlk->SrcBufLen -= LENGTH_802_1Q;
		}

		if (frameNum == 0)
		{
			pHeaderBufPtr = AP_Build_AMSDU_Frame_Header(pAd, pTxBlk);

			/* NOTE: TxWI->TxWIMPDUByteCnt will be updated after final frame was handled. */
#ifdef WFA_VHT_PF
			if (pAd->force_amsdu)
			{
				UCHAR RABAOriIdx;

				if (pMacEntry) {
					 RABAOriIdx = pMacEntry->BAOriWcidArray[pTxBlk->UserPriority];
					if (((pMacEntry->TXBAbitmap & (1<<pTxBlk->UserPriority)) != 0) &&
						(pAd->BATable.BAOriEntry[RABAOriIdx].amsdu_cap == TRUE))
						TX_BLK_SET_FLAG (pTxBlk, fTX_AmsduInAmpdu);
				}
			}
#endif /* WFA_VHT_PF */
			RTMPWriteTxWI_Data(pAd, (TXWI_STRUC *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), pTxBlk);

			if (RTMP_GET_PACKET_LOWRATE(pTxBlk->pPacket))
				if (pMacEntry)
					pMacEntry->isCached = FALSE;
		}
		else
		{
			pHeaderBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE];
			padding = ROUND_UP(AMSDU_SUBHEAD_LEN + subFramePayloadLen, 4) - (AMSDU_SUBHEAD_LEN + subFramePayloadLen);
			memset(pHeaderBufPtr, 0, padding + AMSDU_SUBHEAD_LEN);
			pHeaderBufPtr += padding;
			pTxBlk->MpduHeaderLen = padding;
			pTxBlk->HdrPadLen += padding;
		}

		/*
			A-MSDU subframe
				DA(6)+SA(6)+Length(2) + LLC/SNAP Encap
		*/
		subFrameHeader = pHeaderBufPtr;
		subFramePayloadLen = pTxBlk->SrcBufLen;

		memmove(subFrameHeader, pTxBlk->pSrcBufHeader, 12);

#ifdef APCLI_SUPPORT
		if(TX_BLK_TEST_FLAG(pTxBlk, fTX_bApCliPacket))
		{
			{
				pApCliEntry = &pAd->ApCfg.ApCliTab[pTxBlk->pMacEntry->wdev_idx];
				if (pApCliEntry->Valid)
					memmove(&subFrameHeader[6] , pApCliEntry->wdev.if_addr, 6);
			}
		}
#endif /* APCLI_SUPPORT */


		pHeaderBufPtr += AMSDU_SUBHEAD_LEN;
		pTxBlk->MpduHeaderLen += AMSDU_SUBHEAD_LEN;



		/* Insert LLC-SNAP encapsulation - 8 octets */
		EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(pTxBlk->pSrcBufData-2, pTxBlk->pExtraLlcSnapEncap);

		subFramePayloadLen = pTxBlk->SrcBufLen;

		if (pTxBlk->pExtraLlcSnapEncap)
		{
			memmove(pHeaderBufPtr, pTxBlk->pExtraLlcSnapEncap, 6);
			pHeaderBufPtr += 6;
			/* get 2 octets (TypeofLen) */
			memmove(pHeaderBufPtr, pTxBlk->pSrcBufData-2, 2);
			pHeaderBufPtr += 2;
			pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
			subFramePayloadLen += LENGTH_802_1_H;
		}

		/* update subFrame Length field */
		subFrameHeader[12] = (subFramePayloadLen & 0xFF00) >> 8;
		subFrameHeader[13] = subFramePayloadLen & 0xFF;

		totalMPDUSize += pTxBlk->MpduHeaderLen + pTxBlk->SrcBufLen;

		if (frameNum ==0)
			FirstTx = HAL_WriteMultiTxResource(pAd, pTxBlk, frameNum, &freeCnt);
		else
			LastTxIdx = HAL_WriteMultiTxResource(pAd, pTxBlk, frameNum, &freeCnt);

#ifdef DBG_CTRL_SUPPORT
#ifdef INCLUDE_DEBUG_QUEUE
		if (pAd->CommonCfg.DebugFlags & DBF_DBQ_TXFRAME)
			dbQueueEnqueueTxFrame((UCHAR *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), NULL);
#endif /* INCLUDE_DEBUG_QUEUE */
#endif /* DBG_CTRL_SUPPORT */

		frameNum++;


		pAd->RalinkCounters.KickTxCount++;
		pAd->RalinkCounters.OneSecTxDoneCount++;

#ifdef STATS_COUNT_SUPPORT
		{
			/* calculate Transmitted AMSDU Count and ByteCount */
			pAd->RalinkCounters.TransmittedAMSDUCount.u.LowPart ++;
			pAd->RalinkCounters.TransmittedOctetsInAMSDU.QuadPart += totalMPDUSize;
		}

		/* calculate Tx count and ByteCount per BSS */
		{
			MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;
			MAC_TABLE_ENTRY *pMacEntry = pTxBlk->pMacEntry;


			if (pMbss != NULL)
			{
				pMbss->TransmittedByteCount += totalMPDUSize;
				pMbss->TxCount ++;

#ifdef STATS_COUNT_SUPPORT
				if(IS_MULTICAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
					pMbss->mcPktsTx++;
				else if(IS_BROADCAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
					pMbss->bcPktsTx++;
				else
					pMbss->ucPktsTx++;
#endif /* STATS_COUNT_SUPPORT */
			}

			if(pMacEntry->Sst == SST_ASSOC)
			{
				INC_COUNTER64(pMacEntry->TxPackets);
				pMacEntry->TxBytes+=pTxBlk->SrcBufLen;
			}
		}

#endif /* STATS_COUNT_SUPPORT */
	}

	HAL_FinalWriteTxResource(pAd, pTxBlk, totalMPDUSize, FirstTx);
	HAL_LastTxIdx(pAd, pTxBlk->QueIdx, LastTxIdx);

	/*
		Kick out Tx
	*/
	HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);
}
#endif /* DOT11_N_SUPPORT */


VOID AP_Legacy_Frame_Tx(struct rtmp_adapter *pAd, TX_BLK *pTxBlk)
{
	HEADER_802_11 *wifi_hdr;
	UCHAR *pHeaderBufPtr;
	USHORT freeCnt = 1;
	BOOLEAN bVLANPkt;
	QUEUE_ENTRY *pQEntry;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

	ASSERT(pTxBlk);

	pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
	pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);

	if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE)
	{
#ifdef STATS_COUNT_SUPPORT
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;

		if (pMbss != NULL)
			pMbss->TxDropCount++;
#endif /* STATS_COUNT_SUPPORT */
		RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
		return;
	}

#ifdef STATS_COUNT_SUPPORT
	if (pTxBlk->TxFrameType == TX_MCAST_FRAME)
	{
		INC_COUNTER64(pAd->WlanCounters.MulticastTransmittedFrameCount);
	}
#endif /* STATS_COUNT_SUPPORT */


	APFindCipherAlgorithm(pAd, pTxBlk);
	APBuildCommon802_11Header(pAd, pTxBlk);

#ifdef SOFT_ENCRYPT
	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt))
	{
		if (RTMPExpandPacketForSwEncrypt(pAd, pTxBlk) == FALSE)
		{
			RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
			return;
		}
	}
#endif /* SOFT_ENCRYPT */

	/* skip 802.3 header */
	pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
	pTxBlk->SrcBufLen -= LENGTH_802_3;

	/* skip vlan tag */
	bVLANPkt = (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket) ? TRUE : FALSE);
	if (bVLANPkt)
	{
		pTxBlk->pSrcBufData += LENGTH_802_1Q;
		pTxBlk->SrcBufLen -= LENGTH_802_1Q;
	}

	/* record these MCAST_TX frames for group key rekey */
	if (pTxBlk->TxFrameType == TX_MCAST_FRAME)
	{
		INT	idx;

		for (idx = 0; idx < pAd->ApCfg.BssidNum; idx++)
		{
			if (pAd->ApCfg.MBSSID[idx].REKEYTimerRunning &&
				pAd->ApCfg.MBSSID[idx].WPAREKEY.ReKeyMethod == PKT_REKEY)
			{
				pAd->ApCfg.MBSSID[idx].REKEYCOUNTER += (pTxBlk->SrcBufLen);
			}
		}
	}

	pHeaderBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWISize + TSO_SIZE];
	wifi_hdr = (HEADER_802_11 *)pHeaderBufPtr;

	/* skip common header */
	pHeaderBufPtr += pTxBlk->MpduHeaderLen;

	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM))
	{
		/* build QOS Control bytes */
		*pHeaderBufPtr = ((pTxBlk->UserPriority & 0x0F) | (pAd->CommonCfg.AckPolicy[pTxBlk->QueIdx]<<5));
#ifdef WFA_VHT_PF
		if (pAd->force_noack)
			*pHeaderBufPtr |= (1 << 5);
#endif /* WFA_VHT_PF */

#ifdef UAPSD_SUPPORT
		if (CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_APSD_CAPABLE)
		)
		{
			/*
			 * we can not use bMoreData bit to get EOSP bit because
			 * maybe bMoreData = 1 & EOSP = 1 when Max SP Length != 0
			 */
			if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM_UAPSD_EOSP))
				*pHeaderBufPtr |= (1 << 4);
		}
#endif /* UAPSD_SUPPORT */

		*(pHeaderBufPtr+1) = 0;
		pHeaderBufPtr +=2;
		pTxBlk->MpduHeaderLen += 2;

#ifdef TXBF_SUPPORT
		if (pAd->chipCap.FlgHwTxBfCap &&
			(pTxBlk->pMacEntry) &&
			(pTxBlk->pTransmit->field.MODE >= MODE_HTMIX))
		{
			MAC_TABLE_ENTRY *pMacEntry = pTxBlk->pMacEntry;
			BOOLEAN bHTCPlus = FALSE;

			pTxBlk->TxSndgPkt = SNDG_TYPE_DISABLE;

			NdisAcquireSpinLock(&pMacEntry->TxSndgLock);
			if (pMacEntry->TxSndgType >= SNDG_TYPE_SOUNDING)
			{
				memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));

				if (pMacEntry->TxSndgType == SNDG_TYPE_SOUNDING)
				{
					/* Select compress if supported. Otherwise select noncompress */
					if ((pAd->CommonCfg.ETxBfNoncompress==0) &&
						(pMacEntry->HTCapability.TxBFCap.ExpComBF>0))
						((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 3;
					else
						((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 2;

					/* Clear NDP Announcement */
					((PHT_CONTROL)pHeaderBufPtr)->NDPAnnounce = 0;

				}
				else if (pMacEntry->TxSndgType == SNDG_TYPE_NDP)
				{
					/* Select compress if supported. Otherwise select noncompress */
					if ((pAd->CommonCfg.ETxBfNoncompress == 0) &&
						(pMacEntry->HTCapability.TxBFCap.ExpComBF>0) &&
						(pMacEntry->HTCapability.TxBFCap.ComSteerBFAntSup >= (pMacEntry->sndgMcs/8))
					)
						((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 3;
					else
						((PHT_CONTROL)pHeaderBufPtr)->CSISTEERING = 2;

					/* Set NDP Announcement */
					((PHT_CONTROL)pHeaderBufPtr)->NDPAnnounce = 1;

					pTxBlk->TxNDPSndgBW = pMacEntry->sndgBW;
					pTxBlk->TxNDPSndgMcs = pMacEntry->sndgMcs;
				}

				pTxBlk->TxSndgPkt = pMacEntry->TxSndgType;
				pMacEntry->TxSndgType = SNDG_TYPE_DISABLE;
				bHTCPlus = TRUE;
			}
			NdisReleaseSpinLock(&pMacEntry->TxSndgLock);

#ifdef MFB_SUPPORT
#if defined(MRQ_FORCE_TX)
			/* have to replace this by the correct condition!!! */
			pMacEntry->HTCapability.ExtHtCapInfo.MCSFeedback = MCSFBK_MRQ;
#endif

			/*
				Ignore sounding frame because the signal format of sounding frmae may
				be different from normal data frame, which may result in different MFB
			*/
			if ((pMacEntry->HTCapability.ExtHtCapInfo.MCSFeedback >=MCSFBK_MRQ) &&
				(pTxBlk->TxSndgPkt == SNDG_TYPE_DISABLE))
			{
				if (bHTCPlus == FALSE)
				{
					memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));
					bHTCPlus = TRUE;
				}
				MFB_PerPareMRQ(pAd, pHeaderBufPtr, pMacEntry);
			}

			if (pAd->CommonCfg.HtCapability.ExtHtCapInfo.MCSFeedback >=MCSFBK_MRQ &&
				pMacEntry->toTxMfb == 1)
			{
				if (bHTCPlus == FALSE)
				{
					memset(pHeaderBufPtr, 0, sizeof(HT_CONTROL));
					bHTCPlus = TRUE;
				}

				MFB_PerPareMFB(pAd, pHeaderBufPtr, pMacEntry);/*  not complete yet!!!*/
				pMacEntry->toTxMfb = 0;
			}
#endif /* MFB_SUPPORT */

			if (bHTCPlus == TRUE)
			{
				/* mark HTC bit */
				wifi_hdr->FC.Order = 1;
				pHeaderBufPtr += 4;
				pTxBlk->MpduHeaderLen += 4;
			}
		}
#endif /* TXBF_SUPPORT */
	}

	/* The remaining content of MPDU header should locate at 4-octets aligment	*/
	pTxBlk->HdrPadLen = (ULONG)pHeaderBufPtr;
	pHeaderBufPtr = (PUCHAR) ROUND_UP(pHeaderBufPtr, 4);
	pTxBlk->HdrPadLen = (ULONG)(pHeaderBufPtr - pTxBlk->HdrPadLen);

#ifdef SOFT_ENCRYPT
	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt))
	{
		UCHAR	iv_offset = 0, ext_offset = 0;

		/*
			If original Ethernet frame contains no LLC/SNAP,
			then an extra LLC/SNAP encap is required
		*/
		EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(pTxBlk->pSrcBufData - 2, pTxBlk->pExtraLlcSnapEncap);

		/* Insert LLC-SNAP encapsulation (8 octets) to MPDU data buffer */
		if (pTxBlk->pExtraLlcSnapEncap)
		{
			/* Reserve the front 8 bytes of data for LLC header */
			pTxBlk->pSrcBufData -= LENGTH_802_1_H;
			pTxBlk->SrcBufLen  += LENGTH_802_1_H;

			memmove(pTxBlk->pSrcBufData, pTxBlk->pExtraLlcSnapEncap, 6);
		}

		/* Construct and insert specific IV header to MPDU header */
		RTMPSoftConstructIVHdr(pTxBlk->CipherAlg,
							   pTxBlk->KeyIdx,
							   pTxBlk->pKey->TxTsc,
							   pHeaderBufPtr,
							   &iv_offset);
		pHeaderBufPtr += iv_offset;
		pTxBlk->MpduHeaderLen += iv_offset;

		/* Encrypt the MPDU data by software */
		RTMPSoftEncryptionAction(pAd,
								 pTxBlk->CipherAlg,
								 (PUCHAR)wifi_hdr,
								pTxBlk->pSrcBufData,
								pTxBlk->SrcBufLen,
								pTxBlk->KeyIdx,
								   pTxBlk->pKey,
								 &ext_offset);
		pTxBlk->SrcBufLen += ext_offset;
		pTxBlk->TotalFrameLen += ext_offset;

	}
	else
#endif /* SOFT_ENCRYPT */
	{

		/*
			Insert LLC-SNAP encapsulation - 8 octets
			if original Ethernet frame contains no LLC/SNAP,
			then an extra LLC/SNAP encap is required
		*/
		EXTRA_LLCSNAP_ENCAP_FROM_PKT_START(pTxBlk->pSrcBufHeader, pTxBlk->pExtraLlcSnapEncap);
		if (pTxBlk->pExtraLlcSnapEncap)
		{
			UCHAR vlan_size;

			memmove(pHeaderBufPtr, pTxBlk->pExtraLlcSnapEncap, 6);
			pHeaderBufPtr += 6;
			/* skip vlan tag */
			vlan_size =  (bVLANPkt) ? LENGTH_802_1Q : 0;
			/* get 2 octets (TypeofLen) */
			memmove(pHeaderBufPtr, pTxBlk->pSrcBufHeader+12+vlan_size, 2);
			pHeaderBufPtr += 2;
			pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
		}
	}

#ifdef STATS_COUNT_SUPPORT
	/* calculate Tx count and ByteCount per BSS */
	{
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;
		MAC_TABLE_ENTRY *pMacEntry=pTxBlk->pMacEntry;


		if (pMbss != NULL)
		{
			pMbss->TransmittedByteCount += pTxBlk->SrcBufLen;
			pMbss->TxCount ++;

#ifdef STATS_COUNT_SUPPORT
			if(IS_MULTICAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->mcPktsTx++;
			else if(IS_BROADCAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->bcPktsTx++;
			else
				pMbss->ucPktsTx++;
#endif /* STATS_COUNT_SUPPORT */
		}

		if(pMacEntry && pMacEntry->Sst == SST_ASSOC)
		{
			INC_COUNTER64(pMacEntry->TxPackets);
			pMacEntry->TxBytes+=pTxBlk->SrcBufLen;
		}
	}

#endif /* STATS_COUNT_SUPPORT */

	/*
		prepare for TXWI
	*/

	/* update Hardware Group Key Index */
	if (!pTxBlk->pMacEntry)
	{
		/* use Wcid as Hardware Key Index */
		GET_GroupKey_WCID(pAd, pTxBlk->Wcid, pTxBlk->apidx);
	}

	RTMPWriteTxWI_Data(pAd, (TXWI_STRUC *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), pTxBlk);
	if (RTMP_GET_PACKET_LOWRATE(pTxBlk->pPacket))
		if (pTxBlk->pMacEntry)
			pTxBlk->pMacEntry->isCached = FALSE;

	HAL_WriteTxResource(pAd, pTxBlk, TRUE, &freeCnt);


#ifdef DBG_CTRL_SUPPORT
#ifdef INCLUDE_DEBUG_QUEUE
	if (pAd->CommonCfg.DebugFlags & DBF_DBQ_TXFRAME)
		dbQueueEnqueueTxFrame((UCHAR *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), (UCHAR *)wifi_hdr);
#endif /* INCLUDE_DEBUG_QUEUE */
#endif /* DBG_CTRL_SUPPORT */

	pAd->RalinkCounters.KickTxCount++;
	pAd->RalinkCounters.OneSecTxDoneCount++;

	/*
		Kick out Tx
	*/
	HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);
}


#ifdef HDR_TRANS_TX_SUPPORT
VOID AP_Legacy_Frame_Tx_Hdr_Trns(
	IN	struct rtmp_adapter *pAd,
	IN	TX_BLK			*pTxBlk)
{
/*	UCHAR			QueIdx = pTxBlk->QueIdx; */
	USHORT			FreeNumber = 1; /* no use */
	BOOLEAN			bVLANPkt;
	PQUEUE_ENTRY	pQEntry;
	UINT8 TXWISize = pAd->chipCap.TXWISize;
	PWIFI_INFO_STRUC pWI;

	ASSERT(pTxBlk);


	pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
	pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);

	if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE)
	{
#ifdef STATS_COUNT_SUPPORT
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;

		if (pMbss != NULL)
			pMbss->TxDropCount++;
#endif /* STATS_COUNT_SUPPORT */
		RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
		return;
	}

#ifdef STATS_COUNT_SUPPORT
	if (pTxBlk->TxFrameType == TX_MCAST_FRAME)
	{
		INC_COUNTER64(pAd->WlanCounters.MulticastTransmittedFrameCount);
	}
#endif /* STATS_COUNT_SUPPORT */

	bVLANPkt = (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket) ? TRUE : FALSE);

	APFindCipherAlgorithm(pAd, pTxBlk);

/*
	if ( pAd->debug_on )
	{
		UCHAR index = RTMP_GET_DEBUG_INDEX(pTxBlk->pPacket);
		do_gettimeofday(&(pAd->debug_time_2[index]));
	}
*/

	APBuildWifiInfo(pAd, pTxBlk);

	pWI = (PWIFI_INFO_STRUC)&pTxBlk->HeaderBuf[TXINFO_SIZE + TXWISize];
	pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader;
	if (bVLANPkt)
		pWI->field.VLAN = TRUE;

	pWI->field.TID = (pTxBlk->UserPriority & 0x0F);
#ifdef UAPSD_SUPPORT
	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM)
		&& CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_APSD_CAPABLE)
		&& TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM_UAPSD_EOSP))
		pWI->field.EOSP = TRUE;
#endif /* UAPSD_SUPPORT */

#ifdef STATS_COUNT_SUPPORT
	/* calculate Tx count and ByteCount per BSS */
	{
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;
		MAC_TABLE_ENTRY *pMacEntry=pTxBlk->pMacEntry;


		if (pMbss != NULL)
		{
			pMbss->TransmittedByteCount += pTxBlk->SrcBufLen;
			pMbss->TxCount ++;

#ifdef STATS_COUNT_SUPPORT
			if(IS_MULTICAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->mcPktsTx++;
			else if(IS_BROADCAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->bcPktsTx++;
			else
				pMbss->ucPktsTx++;
#endif /* STATS_COUNT_SUPPORT */
		}

		if(pMacEntry && pMacEntry->Sst == SST_ASSOC)
		{
			INC_COUNTER64(pMacEntry->TxPackets);
			pMacEntry->TxBytes+=pTxBlk->SrcBufLen;
		}
	}

#endif /* STATS_COUNT_SUPPORT */

	/*
	if ( pAd->debug_on )
	{
		UCHAR index = RTMP_GET_DEBUG_INDEX(pTxBlk->pPacket);
		do_gettimeofday(&(pAd->debug_time_3[index]));
	}
*/

	/*
		prepare for TXWI
	*/

	/* update Hardware Group Key Index */
	if (!pTxBlk->pMacEntry)
	{
		/* use Wcid as Hardware Key Index */
		GET_GroupKey_WCID(pAd, pTxBlk->Wcid, pTxBlk->apidx);
	}

	RTMPWriteTxWI_Data(pAd, (TXWI_STRUC *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), pTxBlk);

	/*FreeNumber = GET_TXRING_FREENO(pAd, QueIdx); */

	HAL_WriteTxResource(pAd, pTxBlk, TRUE, &FreeNumber);


#ifdef DBG_CTRL_SUPPORT
#ifdef INCLUDE_DEBUG_QUEUE
	if (pAd->CommonCfg.DebugFlags & DBF_DBQ_TXFRAME)
		dbQueueEnqueueTxFrame((UCHAR *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), (UCHAR *)pHeader_802_11);
#endif /* INCLUDE_DEBUG_QUEUE */
#endif /* DBG_CTRL_SUPPORT */

	pAd->RalinkCounters.KickTxCount++;
	pAd->RalinkCounters.OneSecTxDoneCount++;

	/*
	if ( pAd->debug_on )
	{
		UCHAR index = RTMP_GET_DEBUG_INDEX(pTxBlk->pPacket);
		do_gettimeofday(&(pAd->debug_time_4[index]));
	}
*/

	/*
		Kick out Tx
	*/
	HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);

/*
	if ( pAd->debug_on )
	{
		UCHAR index = RTMP_GET_DEBUG_INDEX(pTxBlk->pPacket);
		do_gettimeofday(&(pAd->debug_time_5[index]));
		if ( pAd->debug_index < 201 )
		{
			pAd->debug_index ++;
		} else {
			pAd->debug_on = 0;
		}

	}
*/

}
#endif /* HDR_TRANS_TX_SUPPORT */


VOID AP_Fragment_Frame_Tx(struct rtmp_adapter *pAd, TX_BLK *pTxBlk)
{
	HEADER_802_11 *pHeader_802_11;
	UCHAR *pHeaderBufPtr;
	USHORT freeCnt = 1; /* no use */
	UCHAR fragNum = 0;
	USHORT EncryptionOverhead = 0;
	uint32_t FreeMpduSize, SrcRemainingBytes;
	USHORT AckDuration;
	UINT NextMpduSize;
	BOOLEAN bVLANPkt;
	PQUEUE_ENTRY pQEntry;
	PACKET_INFO PacketInfo;
#ifdef SOFT_ENCRYPT
	UCHAR *tmp_ptr = NULL;
	uint32_t buf_offset = 0;
#endif /* SOFT_ENCRYPT */
	HTTRANSMIT_SETTING	*pTransmit;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

	ASSERT(pTxBlk);

	pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
	pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);

	if(RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE)
	{
#ifdef STATS_COUNT_SUPPORT
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;

		if (pMbss != NULL)
			pMbss->TxDropCount++;
#endif /* STATS_COUNT_SUPPORT */
		RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
		return;
	}

	ASSERT(TX_BLK_TEST_FLAG(pTxBlk, fTX_bAllowFrag));

	bVLANPkt = (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket) ? TRUE : FALSE);

	APFindCipherAlgorithm(pAd, pTxBlk);
	APBuildCommon802_11Header(pAd, pTxBlk);

#ifdef SOFT_ENCRYPT
	/*
		Check if the original data has enough buffer
		to insert or append extended field.
	*/
	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt))
	{
		if (RTMPExpandPacketForSwEncrypt(pAd, pTxBlk) == FALSE)
		{
			RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
			return;
		}
	}
#endif /* SOFT_ENCRYPT */

	if (pTxBlk->CipherAlg == CIPHER_TKIP)
	{
		pTxBlk->pPacket = duplicate_pkt_with_TKIP_MIC(pAd, pTxBlk->pPacket);
		if (pTxBlk->pPacket == NULL)
			return;
		RTMP_QueryPacketInfo(pTxBlk->pPacket, &PacketInfo, &pTxBlk->pSrcBufHeader, &pTxBlk->SrcBufLen);
	}

	/* skip 802.3 header */
	pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
	pTxBlk->SrcBufLen  -= LENGTH_802_3;

	/* skip vlan tag */
	if (bVLANPkt)
	{
		pTxBlk->pSrcBufData	+= LENGTH_802_1Q;
		pTxBlk->SrcBufLen	-= LENGTH_802_1Q;
	}

	pHeaderBufPtr = &pTxBlk->HeaderBuf[TXINFO_SIZE + TXWISize];
	pHeader_802_11 = (HEADER_802_11 *)pHeaderBufPtr;

	/* skip common header */
	pHeaderBufPtr += pTxBlk->MpduHeaderLen;

	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM))
	{
		/* build QOS Control bytes */
		*pHeaderBufPtr = (pTxBlk->UserPriority & 0x0F);
#ifdef UAPSD_SUPPORT
		if (pTxBlk->pMacEntry &&
			CLIENT_STATUS_TEST_FLAG(pTxBlk->pMacEntry, fCLIENT_STATUS_APSD_CAPABLE)
		)
		{
			/*
			 * we can not use bMoreData bit to get EOSP bit because
			 * maybe bMoreData = 1 & EOSP = 1 when Max SP Length != 0
			 */
			if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bWMM_UAPSD_EOSP))
				*pHeaderBufPtr |= (1 << 4);
		}
#endif /* UAPSD_SUPPORT */

		*(pHeaderBufPtr+1) = 0;
		pHeaderBufPtr +=2;
		pTxBlk->MpduHeaderLen += 2;
	}

	/* The remaining content of MPDU header should locate at 4-octets aligment */
	pTxBlk->HdrPadLen = (ULONG)pHeaderBufPtr;
	pHeaderBufPtr = (PUCHAR) ROUND_UP(pHeaderBufPtr, 4);
	pTxBlk->HdrPadLen = (ULONG)(pHeaderBufPtr - pTxBlk->HdrPadLen);

#ifdef SOFT_ENCRYPT
	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt))
	{
		UCHAR	iv_offset = 0;

		/*
			If original Ethernet frame contains no LLC/SNAP,
			then an extra LLC/SNAP encap is required
		*/
		EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(pTxBlk->pSrcBufData - 2, pTxBlk->pExtraLlcSnapEncap);

		/* Insert LLC-SNAP encapsulation (8 octets) to MPDU data buffer */
		if (pTxBlk->pExtraLlcSnapEncap)
		{
			/* Reserve the front 8 bytes of data for LLC header */
			pTxBlk->pSrcBufData -= LENGTH_802_1_H;
			pTxBlk->SrcBufLen  += LENGTH_802_1_H;

			memmove(pTxBlk->pSrcBufData, pTxBlk->pExtraLlcSnapEncap, 6);
		}

		/* Construct and insert specific IV header to MPDU header */
		RTMPSoftConstructIVHdr(pTxBlk->CipherAlg,
							   pTxBlk->KeyIdx,
							   pTxBlk->pKey->TxTsc,
							   pHeaderBufPtr,
							   &iv_offset);
		pHeaderBufPtr += iv_offset;
		pTxBlk->MpduHeaderLen += iv_offset;

	}
	else
#endif /* SOFT_ENCRYPT */
	{

		/*
			Insert LLC-SNAP encapsulation - 8 octets
			If original Ethernet frame contains no LLC/SNAP,
			then an extra LLC/SNAP encap is required
		*/
		EXTRA_LLCSNAP_ENCAP_FROM_PKT_START(pTxBlk->pSrcBufHeader, pTxBlk->pExtraLlcSnapEncap);
		if (pTxBlk->pExtraLlcSnapEncap)
		{
			UCHAR vlan_size;

			memmove(pHeaderBufPtr, pTxBlk->pExtraLlcSnapEncap, 6);
			pHeaderBufPtr += 6;
			/* skip vlan tag */
			vlan_size =  (bVLANPkt) ? LENGTH_802_1Q : 0;
			/* get 2 octets (TypeofLen) */
			memmove(pHeaderBufPtr, pTxBlk->pSrcBufHeader+12+vlan_size, 2);
			pHeaderBufPtr += 2;
			pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
		}
	}

	/*  1. If TKIP is used and fragmentation is required. Driver has to
		   append TKIP MIC at tail of the scatter buffer
		2. When TXWI->FRAG is set as 1 in TKIP mode,
		   MAC ASIC will only perform IV/EIV/ICV insertion but no TKIP MIC */
	/*  TKIP appends the computed MIC to the MSDU data prior to fragmentation into MPDUs. */
	if (pTxBlk->CipherAlg == CIPHER_TKIP)
	{
		RTMPCalculateMICValue(pAd, pTxBlk->pPacket, pTxBlk->pExtraLlcSnapEncap, pTxBlk->pKey, pTxBlk->apidx);

		/*
			NOTE: DON'T refer the skb->len directly after following copy. Becasue the length is not adjust
				to correct lenght, refer to pTxBlk->SrcBufLen for the packet length in following progress.
		*/
		memmove(pTxBlk->pSrcBufData + pTxBlk->SrcBufLen, &pAd->PrivateInfo.Tx.MIC[0], 8);
		pTxBlk->SrcBufLen += 8;
		pTxBlk->TotalFrameLen += 8;
	}

#ifdef STATS_COUNT_SUPPORT
	/* calculate Tx count and ByteCount per BSS */
	{
		MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;
		MAC_TABLE_ENTRY	*pMacEntry=pTxBlk->pMacEntry;


		if (pMbss != NULL)
		{
			pMbss->TransmittedByteCount += pTxBlk->SrcBufLen;
			pMbss->TxCount ++;

#ifdef STATS_COUNT_SUPPORT
			if(IS_MULTICAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->mcPktsTx++;
			else if(IS_BROADCAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
				pMbss->bcPktsTx++;
			else
				pMbss->ucPktsTx++;
#endif /* STATS_COUNT_SUPPORT */
		}

		if(pMacEntry && pMacEntry->Sst == SST_ASSOC)
		{
			INC_COUNTER64(pMacEntry->TxPackets);
			pMacEntry->TxBytes+=pTxBlk->SrcBufLen;
		}
	}

#endif /* STATS_COUNT_SUPPORT */

	/*
		calcuate the overhead bytes that encryption algorithm may add. This
		affects the calculate of "duration" field
	*/
	if ((pTxBlk->CipherAlg == CIPHER_WEP64) || (pTxBlk->CipherAlg == CIPHER_WEP128))
		EncryptionOverhead = 8; /*WEP: IV[4] + ICV[4]; */
	else if (pTxBlk->CipherAlg == CIPHER_TKIP)
		EncryptionOverhead = 12;/*TKIP: IV[4] + EIV[4] + ICV[4], MIC will be added to TotalPacketLength */
	else if (pTxBlk->CipherAlg == CIPHER_AES)
		EncryptionOverhead = 16;	/* AES: IV[4] + EIV[4] + MIC[8] */
	else
		EncryptionOverhead = 0;

	pTransmit = pTxBlk->pTransmit;
	/* Decide the TX rate */
	if (pTransmit->field.MODE == MODE_CCK)
		pTxBlk->TxRate = pTransmit->field.MCS;
	else if (pTransmit->field.MODE == MODE_OFDM)
		pTxBlk->TxRate = pTransmit->field.MCS + RATE_FIRST_OFDM_RATE;
	else
		pTxBlk->TxRate = RATE_6_5;

	/* decide how much time an ACK/CTS frame will consume in the air */
	if (pTxBlk->TxRate <= RATE_LAST_OFDM_RATE)
		AckDuration = RTMPCalcDuration(pAd, pAd->CommonCfg.ExpectedACKRate[pTxBlk->TxRate], 14);
	else
		AckDuration = RTMPCalcDuration(pAd, RATE_6_5, 14);
	/*DBGPRINT(RT_DEBUG_INFO, ("!!!Fragment AckDuration(%d), TxRate(%d)!!!\n", AckDuration, pTxBlk->TxRate)); */

	/* Init the total payload length of this frame. */
	SrcRemainingBytes = pTxBlk->SrcBufLen;

	pTxBlk->TotalFragNum = 0xff;

#ifdef SOFT_ENCRYPT
	if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt))
	{
		/* store the outgoing frame for calculating MIC per fragmented frame */
		tmp_ptr = kmalloc(pTxBlk->SrcBufLen, GFP_ATOMIC);
		if (tmp_ptr == NULL)
		{
			DBGPRINT(RT_DEBUG_ERROR, ("!!!%s : no memory for SW MIC calculation !!!\n",
										__FUNCTION__));
			RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
			return;
		}
		memmove(tmp_ptr, pTxBlk->pSrcBufData, pTxBlk->SrcBufLen);
	}
#endif /* SOFT_ENCRYPT */

	do {

		FreeMpduSize = pAd->CommonCfg.FragmentThreshold - LENGTH_CRC;

		FreeMpduSize -= pTxBlk->MpduHeaderLen;

		if (SrcRemainingBytes <= FreeMpduSize)
		{
			/* This is the last or only fragment */
			pTxBlk->SrcBufLen = SrcRemainingBytes;

			pHeader_802_11->FC.MoreFrag = 0;
			pHeader_802_11->Duration = pAd->CommonCfg.Dsifs + AckDuration;

			/* Indicate the lower layer that this's the last fragment. */
			pTxBlk->TotalFragNum = fragNum;
		}
		else
		{	/* more fragment is required */
			pTxBlk->SrcBufLen = FreeMpduSize;

			NextMpduSize = min(((UINT)SrcRemainingBytes - pTxBlk->SrcBufLen), ((UINT)pAd->CommonCfg.FragmentThreshold));
			pHeader_802_11->FC.MoreFrag = 1;
			pHeader_802_11->Duration = (3 * pAd->CommonCfg.Dsifs) + (2 * AckDuration) + RTMPCalcDuration(pAd, pTxBlk->TxRate, NextMpduSize + EncryptionOverhead);
		}

		SrcRemainingBytes -= pTxBlk->SrcBufLen;

		if (fragNum == 0)
			pTxBlk->FrameGap = IFS_HTTXOP;
		else
			pTxBlk->FrameGap = IFS_SIFS;

#ifdef SOFT_ENCRYPT
		if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt))
		{
			UCHAR	ext_offset = 0;

			memmove(pTxBlk->pSrcBufData, tmp_ptr + buf_offset, pTxBlk->SrcBufLen);
			buf_offset += pTxBlk->SrcBufLen;

			/* Encrypt the MPDU data by software */
			RTMPSoftEncryptionAction(pAd,
									 pTxBlk->CipherAlg,
									 (PUCHAR)pHeader_802_11,
									pTxBlk->pSrcBufData,
									pTxBlk->SrcBufLen,
									pTxBlk->KeyIdx,
									   pTxBlk->pKey,
									 &ext_offset);
			pTxBlk->SrcBufLen += ext_offset;
			pTxBlk->TotalFrameLen += ext_offset;
		}
#endif /* SOFT_ENCRYPT */

		RTMPWriteTxWI_Data(pAd, (TXWI_STRUC *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), pTxBlk);

		HAL_WriteFragTxResource(pAd, pTxBlk, fragNum, &freeCnt);


#ifdef DBG_CTRL_SUPPORT
#ifdef INCLUDE_DEBUG_QUEUE
		if (pAd->CommonCfg.DebugFlags & DBF_DBQ_TXFRAME)
			dbQueueEnqueueTxFrame((UCHAR *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), (UCHAR *)pHeader_802_11);
#endif /* INCLUDE_DEBUG_QUEUE */
#endif /* DBG_CTRL_SUPPORT */

		pAd->RalinkCounters.KickTxCount++;
		pAd->RalinkCounters.OneSecTxDoneCount++;

#ifdef SOFT_ENCRYPT
		if (TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt))
		{
			if ((pTxBlk->CipherAlg == CIPHER_WEP64) || (pTxBlk->CipherAlg == CIPHER_WEP128))
			{
				inc_iv_byte(pTxBlk->pKey->TxTsc, LEN_WEP_TSC, 1);
				/* Construct and insert 4-bytes WEP IV header to MPDU header */
				RTMPConstructWEPIVHdr(pTxBlk->KeyIdx, pTxBlk->pKey->TxTsc,
										pHeaderBufPtr - (LEN_WEP_IV_HDR));
			}
			else if (pTxBlk->CipherAlg == CIPHER_TKIP)
				;
			else if (pTxBlk->CipherAlg == CIPHER_AES)
			{
				inc_iv_byte(pTxBlk->pKey->TxTsc, LEN_WPA_TSC, 1);
				/* Construct and insert 8-bytes CCMP header to MPDU header */
				RTMPConstructCCMPHdr(pTxBlk->KeyIdx, pTxBlk->pKey->TxTsc,
										pHeaderBufPtr - (LEN_CCMP_HDR));
			}
		}
		else
#endif /* SOFT_ENCRYPT */
		{
			/* Update the frame number, remaining size of the NDIS packet payload. */
			if (fragNum == 0 && pTxBlk->pExtraLlcSnapEncap)
				pTxBlk->MpduHeaderLen -= LENGTH_802_1_H;	/* space for 802.11 header. */
		}

		fragNum++;
		/*SrcRemainingBytes -= pTxBlk->SrcBufLen; */
		pTxBlk->pSrcBufData += pTxBlk->SrcBufLen;

		pHeader_802_11->Frag++;	 /* increase Frag # */

	}while(SrcRemainingBytes > 0);

#ifdef SOFT_ENCRYPT
	if (tmp_ptr != NULL)
		kfree(tmp_ptr);
#endif /* SOFT_ENCRYPT */

	/*
		Kick out Tx
	*/
	HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);

}


VOID AP_ARalink_Frame_Tx(struct rtmp_adapter *pAd, TX_BLK *pTxBlk)
{
	UCHAR *pHeaderBufPtr;
	USHORT freeCnt = 1; /* no use */
	USHORT totalMPDUSize=0;
	USHORT FirstTx, LastTxIdx;
	int frameNum = 0;
	BOOLEAN bVLANPkt;
	PQUEUE_ENTRY pQEntry;


	ASSERT(pTxBlk);
	ASSERT((pTxBlk->TxPacketList.Number== 2));

	FirstTx = LastTxIdx = 0;  /* Is it ok init they as 0? */
	while(pTxBlk->TxPacketList.Head)
	{
		pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
		pTxBlk->pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
		if (RTMP_FillTxBlkInfo(pAd, pTxBlk) != TRUE)
		{
#ifdef STATS_COUNT_SUPPORT
			MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;

			if (pMbss != NULL)
				pMbss->TxDropCount++;
#endif /* STATS_COUNT_SUPPORT */

			RELEASE_NDIS_PACKET(pAd, pTxBlk->pPacket, NDIS_STATUS_FAILURE);
			continue;
		}

		/* skip 802.3 header */
		pTxBlk->pSrcBufData = pTxBlk->pSrcBufHeader + LENGTH_802_3;
		pTxBlk->SrcBufLen  -= LENGTH_802_3;

		/* skip vlan tag */
		bVLANPkt = (RTMP_GET_PACKET_VLAN(pTxBlk->pPacket) ? TRUE : FALSE);
		if (bVLANPkt)
		{
			pTxBlk->pSrcBufData	+= LENGTH_802_1Q;
			pTxBlk->SrcBufLen	-= LENGTH_802_1Q;
		}

		if (frameNum == 0)
		{	/* For first frame, we need to create the 802.11 header + padding(optional) + RA-AGG-LEN + SNAP Header */

			pHeaderBufPtr = AP_Build_ARalink_Frame_Header(pAd, pTxBlk);

			/*
				It's ok write the TxWI here, because the TxWI->TxWIMPDUByteCnt
				will be updated after final frame was handled.
			*/
			RTMPWriteTxWI_Data(pAd, (TXWI_STRUC *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), pTxBlk);


			/* Insert LLC-SNAP encapsulation - 8 octets */
			EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(pTxBlk->pSrcBufData-2, pTxBlk->pExtraLlcSnapEncap);

			if (pTxBlk->pExtraLlcSnapEncap)
			{
				memmove(pHeaderBufPtr, pTxBlk->pExtraLlcSnapEncap, 6);
				pHeaderBufPtr += 6;
				/* get 2 octets (TypeofLen) */
				memmove(pHeaderBufPtr, pTxBlk->pSrcBufData-2, 2);
				pHeaderBufPtr += 2;
				pTxBlk->MpduHeaderLen += LENGTH_802_1_H;
			}
		}
		else
		{
			/*
				For second aggregated frame, we need create the 802.3 header to
				headerBuf, because PCI will copy it to SDPtr0.
			*/
			pHeaderBufPtr = &pTxBlk->HeaderBuf[0];
			pTxBlk->MpduHeaderLen = 0;

			/*
				A-Ralink sub-sequent frame header is the same as 802.3 header.
					DA(6)+SA(6)+FrameType(2)
			*/
			memmove(pHeaderBufPtr, pTxBlk->pSrcBufHeader, 12);
			pHeaderBufPtr += 12;
			/* get 2 octets (TypeofLen) */
			memmove(pHeaderBufPtr, pTxBlk->pSrcBufData-2, 2);
			pHeaderBufPtr += 2;
			pTxBlk->MpduHeaderLen = ARALINK_SUBHEAD_LEN;
		}

		totalMPDUSize += pTxBlk->MpduHeaderLen + pTxBlk->SrcBufLen;

		if (frameNum ==0)
			FirstTx = HAL_WriteMultiTxResource(pAd, pTxBlk, frameNum, &freeCnt);
		else
			LastTxIdx = HAL_WriteMultiTxResource(pAd, pTxBlk, frameNum, &freeCnt);


#ifdef DBG_CTRL_SUPPORT
#ifdef INCLUDE_DEBUG_QUEUE
		if (pAd->CommonCfg.DebugFlags & DBF_DBQ_TXFRAME)
			dbQueueEnqueueTxFrame((UCHAR *)(&pTxBlk->HeaderBuf[TXINFO_SIZE]), NULL);
#endif /* INCLUDE_DEBUG_QUEUE */
#endif /* DBG_CTRL_SUPPORT */

		frameNum++;

		pAd->RalinkCounters.OneSecTxAggregationCount++;
		pAd->RalinkCounters.KickTxCount++;
		pAd->RalinkCounters.OneSecTxDoneCount++;

#ifdef STATS_COUNT_SUPPORT
		/* calculate Tx count and ByteCount per BSS */
		{
			MULTISSID_STRUCT *pMbss = pTxBlk->pMbss;
			MAC_TABLE_ENTRY *pMacEntry=pTxBlk->pMacEntry;


			if (pMbss != NULL)
			{
				pMbss->TransmittedByteCount += totalMPDUSize;
				pMbss->TxCount ++;

#ifdef STATS_COUNT_SUPPORT
				if(IS_MULTICAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
					pMbss->mcPktsTx++;
				else if(IS_BROADCAST_MAC_ADDR(pTxBlk->pSrcBufHeader))
					pMbss->bcPktsTx++;
				else
					pMbss->ucPktsTx++;
#endif /* STATS_COUNT_SUPPORT */
			}

			if(pMacEntry && pMacEntry->Sst == SST_ASSOC)
			{
				INC_COUNTER64(pMacEntry->TxPackets);
				pMacEntry->TxBytes+=pTxBlk->SrcBufLen;
			}

		}


#endif /* STATS_COUNT_SUPPORT */
	}


	HAL_FinalWriteTxResource(pAd, pTxBlk, totalMPDUSize, FirstTx);
	HAL_LastTxIdx(pAd, pTxBlk->QueIdx, LastTxIdx);

	/*
		Kick out Tx
	*/
	HAL_KickOutTx(pAd, pTxBlk, pTxBlk->QueIdx);

}


#ifdef VHT_TXBF_SUPPORT
VOID AP_NDPA_Frame_Tx(struct rtmp_adapter *pAd, TX_BLK *pTxBlk)
{
	UCHAR *buf;
	VHT_NDPA_FRAME *vht_ndpa;
	struct rtmp_wifi_dev *wdev;
	UINT frm_len, sta_cnt;
	SNDING_STA_INFO *sta_info;
	MAC_TABLE_ENTRY *pMacEntry;

	pTxBlk->Wcid = RTMP_GET_PACKET_WCID(pTxBlk->pPacket);
	pTxBlk->pMacEntry = &pAd->MacTab.Content[pTxBlk->Wcid];
	pMacEntry = pTxBlk->pMacEntry;

	if (pMacEntry)
	{
		wdev = pMacEntry->wdev;

		buf = kmalloc(MGMT_DMA_BUFFER_SIZE, GFP_ATOMIC);
		if (buf == NULL)
			return;

		memset(buf, 0, MGMT_DMA_BUFFER_SIZE);

		vht_ndpa = (VHT_NDPA_FRAME *)buf;
		frm_len = sizeof(VHT_NDPA_FRAME);
		vht_ndpa->fc.Type = FC_TYPE_CNTL;
		vht_ndpa->fc.SubType = SUBTYPE_VHT_NDPA;
		COPY_MAC_ADDR(vht_ndpa->ra, pMacEntry->Addr);
		COPY_MAC_ADDR(vht_ndpa->ta, wdev->if_addr);

		/* Currnetly we only support 1 STA for a VHT DNPA */
		sta_info = vht_ndpa->sta_info;
		for (sta_cnt = 0; sta_cnt < 1; sta_cnt++) {
			sta_info->aid12 = pMacEntry->Aid;
			sta_info->fb_type = SNDING_FB_SU;
			sta_info->nc_idx = 0;
			vht_ndpa->token.token_num = pMacEntry->snd_dialog_token;
			frm_len += sizeof(SNDING_STA_INFO);
			sta_info++;
			if (frm_len >= (MGMT_DMA_BUFFER_SIZE - sizeof(SNDING_STA_INFO))) {
				DBGPRINT(RT_DEBUG_ERROR, ("%s(): len(%d) too large!cnt=%d\n",
							__FUNCTION__, frm_len, sta_cnt));
				break;
			}
		}
		if (pMacEntry->snd_dialog_token & 0xc0)
			pMacEntry->snd_dialog_token = 0;
		else
			pMacEntry->snd_dialog_token++;

		vht_ndpa->duration = 100;

		//DBGPRINT(RT_DEBUG_OFF, ("Send VHT NDPA Frame to STA(%02x:%02x:%02x:%02x:%02x:%02x)\n",
		//						PRINT_MAC(pMacEntry->Addr)));
		//hex_dump("VHT NDPA Frame", buf, frm_len);

		// NDPA's BW needs to sync with Tx BW
		pAd->CommonCfg.MlmeTransmit.field.BW = pMacEntry->HTPhyMode.field.BW;

		pTxBlk->Flags = FALSE; // No Acq Request

		MiniportMMRequest(pAd, 0, buf, frm_len);
		kfree(buf);
	}

	pMacEntry->TxSndgType = SNDG_TYPE_DISABLE;
}
#endif /* VHT_TXBF_SUPPORT */


/*
	========================================================================
	Routine Description:
		Copy frame from waiting queue into relative ring buffer and set
	appropriate ASIC register to kick hardware encryption before really
	sent out to air.

	Arguments:
		pAd 	   		Pointer to our adapter
		pTxBlk			Pointer to outgoing TxBlk structure.
		QueIdx			Queue index for processing

	Return Value:
		None
	========================================================================
*/
int APHardTransmit(struct rtmp_adapter *pAd, TX_BLK *pTxBlk, UCHAR QueIdx)
{
	PQUEUE_ENTRY pQEntry;
	struct sk_buff *pPacket;

	if ((pAd->Dot11_H.RDMode != RD_NORMAL_MODE)
		)
	{
		while(pTxBlk->TxPacketList.Head)
		{
			pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
			pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
			if (pPacket)
				RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
		}
		return NDIS_STATUS_FAILURE;
	}

	if (pTxBlk->wdev->bVLAN_Tag == TRUE)
	{
		RTMP_SET_PACKET_VLAN(pTxBlk->pPacket, FALSE);
	}


#ifdef HDR_TRANS_TX_SUPPORT
#ifdef SOFT_ENCRYPT
	if ( TX_BLK_TEST_FLAG(pTxBlk, fTX_bSwEncrypt)) /* need LLC, not yet generated */
		pTxBlk->NeedTrans = FALSE;
	else
#endif /* SOFT_ENCRYPT */
	{
		pTxBlk->NeedTrans = TRUE;
#ifdef TXBF_SUPPORT
		pTxBlk->NeedTrans = FALSE;
#endif // TXBF_SUPPORT //
	}
#endif /* HDR_TRANS_TX_SUPPORT */

#ifdef VHT_TXBF_SUPPORT
	if ((pTxBlk->TxFrameType & TX_NDPA_FRAME) > 0)
	{
		UCHAR mlmeMCS, mlmeBW, mlmeMode;

		mlmeMCS  = pAd->CommonCfg.MlmeTransmit.field.MCS;
		mlmeBW   = pAd->CommonCfg.MlmeTransmit.field.BW;
		mlmeMode = pAd->CommonCfg.MlmeTransmit.field.MODE;

		pAd->NDPA_Request = TRUE;

		AP_NDPA_Frame_Tx(pAd, pTxBlk);

		pAd->NDPA_Request = FALSE;
		pTxBlk->TxFrameType &= ~TX_NDPA_FRAME;

		// Finish NDPA and then recover to mlme's own setting
		pAd->CommonCfg.MlmeTransmit.field.MCS  = mlmeMCS;
		pAd->CommonCfg.MlmeTransmit.field.BW   = mlmeBW;
		pAd->CommonCfg.MlmeTransmit.field.MODE = mlmeMode;
	}
#endif

	switch (pTxBlk->TxFrameType)
	{
#ifdef DOT11_N_SUPPORT
		case TX_AMPDU_FRAME:
#ifdef HDR_TRANS_TX_SUPPORT
			if (pTxBlk->NeedTrans)
				AP_AMPDU_Frame_Tx_Hdr_Trns(pAd, pTxBlk);
			else
#endif /* HDR_TRANS_TX_SUPPORT */
				AP_AMPDU_Frame_Tx(pAd, pTxBlk);
			break;
#endif /* DOT11_N_SUPPORT */
		case TX_LEGACY_FRAME:
#ifdef HDR_TRANS_TX_SUPPORT
			if (pTxBlk->NeedTrans)
				AP_Legacy_Frame_Tx_Hdr_Trns(pAd, pTxBlk);
			else
#endif /* HDR_TRANS_TX_SUPPORT */
				AP_Legacy_Frame_Tx(pAd, pTxBlk);
			break;
		case TX_MCAST_FRAME:
#ifdef HDR_TRANS_TX_SUPPORT
			pTxBlk->NeedTrans = FALSE;
#endif /* HDR_TRANS_TX_SUPPORT */
			AP_Legacy_Frame_Tx(pAd, pTxBlk);
			break;
#ifdef DOT11_N_SUPPORT
		case TX_AMSDU_FRAME:
			AP_AMSDU_Frame_Tx(pAd, pTxBlk);
			break;
#endif /* DOT11_N_SUPPORT */
		case TX_RALINK_FRAME:
			AP_ARalink_Frame_Tx(pAd, pTxBlk);
			break;
		case TX_FRAG_FRAME:
			AP_Fragment_Frame_Tx(pAd, pTxBlk);
			break;
		default:
			{
				/* It should not happened! */
				DBGPRINT(RT_DEBUG_ERROR, ("Send a pacekt was not classified!! It should not happen!\n"));
				while(pTxBlk->TxPacketList.Head)
				{
					pQEntry = RemoveHeadQueue(&pTxBlk->TxPacketList);
					pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
					if (pPacket)
						RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
				}
			}
			break;
	}

	return (NDIS_STATUS_SUCCESS);

}


/*
	========================================================================
	Routine Description:
		Check Rx descriptor, return NDIS_STATUS_FAILURE if any error found
	========================================================================
*/
INT APCheckRxError(struct rtmp_adapter *pAd, RXINFO_STRUC *pRxInfo, RX_BLK *pRxBlk)
{
	if (pRxInfo->Crc || pRxInfo->CipherErr)
	{
#ifdef DBG_DIAGNOSE
		if (pRxInfo->Crc)
		{
			if (pAd->DiagStruct.inited) {
				struct dbg_diag_info *diag_info;
				diag_info = &pAd->DiagStruct.diag_info[pAd->DiagStruct.ArrayCurIdx];
#ifdef DBG_RX_MCS
				if (pRxBlk->rx_rate.field.MODE == MODE_HTMIX ||
					pRxBlk->rx_rate.field.MODE == MODE_HTGREENFIELD) {
					if (pRxBlk->rx_rate.field.MCS < MAX_MCS_SET)
						diag_info->RxCrcErrCnt_HT[pRxBlk->rx_rate.field.MCS]++;
				}
#ifdef DOT11_VHT_AC
				if (pRxBlk->rx_rate.field.MODE == MODE_VHT) {
					INT mcs_idx = ((pRxBlk->rx_rate.field.MCS >> 4) * 10) +
									(pRxBlk->rx_rate.field.MCS & 0xf);
					if (mcs_idx < MAX_VHT_MCS_SET)
						diag_info->RxCrcErrCnt_VHT[mcs_idx]++;
				}
#endif /* DOT11_VHT_AC */
#endif /* DBG_RX_MCS */
			}
		}
#endif /* DBG_DIAGNOSE */

		/*
			WCID equal to 255 mean MAC couldn't find any matched entry in Asic-MAC table.
			The incoming packet mays come from WDS or AP-Client link.
			We need them for further process. Can't drop the packet here.
		*/
		if ((pRxInfo->U2M)
			&& (pRxInfo->CipherErr)
			&& (pRxBlk->wcid == 255)
		)
		{
			/* pass those packet for further process. */
			return NDIS_STATUS_SUCCESS;
		}
		else
		{
			DBGPRINT(RT_DEBUG_INFO, ("%s(): pRxInfo:Crc=%d, CipherErr=%d, U2M=%d, Wcid=%d\n",
						__FUNCTION__, pRxInfo->Crc, pRxInfo->CipherErr, pRxInfo->U2M, pRxBlk->wcid));
			return NDIS_STATUS_FAILURE;
		}
	}

	return NDIS_STATUS_SUCCESS;
}


/*
  ========================================================================
  Description:
	This routine checks if a received frame causes class 2 or class 3
	error, and perform error action (DEAUTH or DISASSOC) accordingly
  ========================================================================
*/
BOOLEAN APChkCls2Cls3Err(struct rtmp_adapter *pAd, UCHAR wcid, HEADER_802_11 *hdr)
{
	/* software MAC table might be smaller than ASIC on-chip total size. */
	/* If no mathed wcid index in ASIC on chip, do we need more check???  need to check again. 06-06-2006 */
	if (wcid >= MAX_LEN_OF_MAC_TABLE)
	{
		MAC_TABLE_ENTRY *pEntry;

		DBGPRINT(RT_DEBUG_WARN, ("%s():Rx a frame from %02x:%02x:%02x:%02x:%02x:%02x with WCID(%ld) > %d\n",
					__FUNCTION__, PRINT_MAC(hdr->Addr2),
					wcid, MAX_LEN_OF_MAC_TABLE));
//+++Add by shiang for debug
		pEntry = MacTableLookup(pAd, hdr->Addr2);
		if (pEntry)
		{
			if ((pEntry->Sst == SST_ASSOC) && IS_ENTRY_CLIENT(pEntry))
			{
			}
			return FALSE;
		}
//---Add by shiang for debug

		APCls2errAction(pAd, MAX_LEN_OF_MAC_TABLE, hdr);
		return TRUE;
	}

	if (pAd->MacTab.Content[wcid].Sst == SST_ASSOC)
		; /* okay to receive this DATA frame */
	else if (pAd->MacTab.Content[wcid].Sst == SST_AUTH)
	{
		APCls3errAction(pAd, wcid, hdr);
		return TRUE;
	}
	else
	{
		APCls2errAction(pAd, wcid, hdr);
		return TRUE;
	}
	return FALSE;
}


/*
	detect AC Category of trasmitting packets
	to turn AC0(BE) TX_OP (MAC reg 0x1300)
*/
/*static UCHAR is_on; */
VOID detect_wmm_traffic(
	IN struct rtmp_adapter *pAd,
	IN UCHAR UserPriority,
	IN UCHAR FlgIsOutput)
{
	if (pAd == NULL)
		return;

	/* For BE & BK case and TxBurst function is disabled */
	if ((pAd->CommonCfg.bEnableTxBurst == FALSE)
#ifdef DOT11_N_SUPPORT
		&& (pAd->CommonCfg.bRdg == FALSE)
		&& (pAd->CommonCfg.bRalinkBurstMode == FALSE)
#endif /* DOT11_N_SUPPORT */
		&& (FlgIsOutput == 1)
	)
	{
		if (WMM_UP2AC_MAP[UserPriority] == QID_AC_BK)
		{
			/* has any BK traffic */
			if (pAd->flg_be_adjust == 0)
			{
				/* yet adjust */
#ifdef RTMP_MAC_USB
				RTEnqueueInternalCmd(pAd, CMDTHREAD_AP_ENABLE_TX_BURST, NULL, 0);
#endif /* RTMP_MAC_USB */
				pAd->flg_be_adjust = 1;
				NdisGetSystemUpTime(&pAd->be_adjust_last_time);

				DBGPRINT(RT_DEBUG_TRACE, ("wmm> adjust be!\n"));
			}
		}
		else
		{
			if (pAd->flg_be_adjust != 0)
			{
				PQUEUE_HEADER pQueue;

				/* has adjusted */
				pQueue = &pAd->TxSwQueue[QID_AC_BK];

				if ((pQueue == NULL) ||
					((pQueue != NULL) && (pQueue->Head == NULL)))
				{
					ULONG	now;
					NdisGetSystemUpTime(&now);
					if ((now - pAd->be_adjust_last_time) > TIME_ONE_SECOND)
					{
						/* no any BK traffic */
#ifdef RTMP_MAC_USB
						RTEnqueueInternalCmd(pAd, CMDTHREAD_AP_DISABLE_TX_BURST, NULL, 0);
#endif /* RTMP_MAC_USB */
						pAd->flg_be_adjust = 0;

						DBGPRINT(RT_DEBUG_TRACE, ("wmm> recover be!\n"));
					}
				}
				else
					NdisGetSystemUpTime(&pAd->be_adjust_last_time);
			}
		}
	}

	/* count packets which priority is more than BE */
	if (UserPriority > 3)
	{
		pAd->OneSecondnonBEpackets++;

		if (pAd->OneSecondnonBEpackets > 100
#ifdef DOT11_N_SUPPORT
			&& pAd->MacTab.fAnyStationMIMOPSDynamic
#endif /* DOT11_N_SUPPORT */
		)
		{
			if (!pAd->is_on)
			{
#ifdef RTMP_MAC_USB
				RTEnqueueInternalCmd(pAd, CMDTHREAD_AP_ADJUST_EXP_ACK_TIME, NULL, 0);
#endif /* RTMP_MAC_USB */
				pAd->is_on = 1;
			}
		}
		else
		{
			if (pAd->is_on)
			{
#ifdef RTMP_MAC_USB
				RTEnqueueInternalCmd(pAd, CMDTHREAD_AP_RECOVER_EXP_ACK_TIME, NULL, 0);
#endif /* RTMP_MAC_USB */
				pAd->is_on = 0;
			}
		}
	}
}

/*
	Wirte non-zero value to AC0 TXOP to boost performace
	To pass WMM, AC0 TXOP must be zero.
	It is necessary to turn AC0 TX_OP dynamically.
*/

VOID dynamic_tune_be_tx_op(struct rtmp_adapter *pAd, ULONG nonBEpackets)
{
	uint32_t RegValue;
	AC_TXOP_CSR0_STRUC csr0;

	if (pAd->CommonCfg.bEnableTxBurst
#ifdef DOT11_N_SUPPORT
		|| pAd->CommonCfg.bRdg
		|| pAd->CommonCfg.bRalinkBurstMode
#endif /* DOT11_N_SUPPORT */
	)
	{

		if (
#ifdef DOT11_N_SUPPORT
			(pAd->WIFItestbed.bGreenField && pAd->MacTab.fAnyStationNonGF == TRUE) ||
			((pAd->OneSecondnonBEpackets > nonBEpackets) || pAd->MacTab.fAnyStationMIMOPSDynamic) ||
#endif /* DOT11_N_SUPPORT */
			(pAd->MacTab.fAnyTxOPForceDisable))
		{
			if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_DYNAMIC_BE_TXOP_ACTIVE))
			{
				RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &RegValue);

				if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RALINK_BURST_MODE))
				{
					RegValue = pAd->CommonCfg.RestoreBurstMode;
					RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RALINK_BURST_MODE);
				}

				if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RDG_ACTIVE))
				{
					TX_LINK_CFG_STRUC   TxLinkCfg;

					RTMP_IO_READ32(pAd, TX_LINK_CFG, &TxLinkCfg.word);
					TxLinkCfg.field.TxRDGEn = 0;
					RTMP_IO_WRITE32(pAd, TX_LINK_CFG, TxLinkCfg.word);

					RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RDG_ACTIVE);
				}
				/* disable AC0(BE) TX_OP */
				RegValue  &= 0xFFFFFF00; /* for WMM test */
				/*if ((RegValue & 0x0000FF00) == 0x00004300) */
				/*	RegValue += 0x00001100; */
				RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, RegValue);
				if (pAd->CommonCfg.APEdcaParm.Txop[QID_AC_VO] != 102)
				{
					csr0.field.Ac0Txop = 0;		/* QID_AC_BE */
				}
				else
				{
					/* for legacy b mode STA */
					csr0.field.Ac0Txop = 10;		/* QID_AC_BE */
				}
				csr0.field.Ac1Txop = 0;		/* QID_AC_BK */
				RTMP_IO_WRITE32(pAd, WMM_TXOP0_CFG, csr0.word);
				RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_DYNAMIC_BE_TXOP_ACTIVE);
			}
		}
		else
		{
			if ((RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_DYNAMIC_BE_TXOP_ACTIVE)==0) ||
				(pAd->ApCfg.ChangeTxOpClient != pAd->MacTab.Size))
			{
				/* enable AC0(BE) TX_OP */
				UCHAR	txop_value_burst = 0x20;	/* default txop for Tx-Burst */
				UCHAR   txop_value = 0;

				pAd->ApCfg.ChangeTxOpClient = pAd->MacTab.Size;
#ifdef LINUX
#endif /* LINUX */

				RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &RegValue);

				if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RALINK_BURST_MODE))
					txop_value = 0x80;
				else if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RDG_ACTIVE))
					txop_value = 0x80;
#ifdef DOT11_VHT_AC
				else if (pAd->MacTab.Size == 1) {
					MAC_TABLE_ENTRY *pEntry = NULL;
					uint32_t i = 0;

		                    for (i = 1; i< MAX_LEN_OF_MAC_TABLE; i++) {
						pEntry = &pAd->MacTab.Content[i];

						if (IS_ENTRY_CLIENT(pEntry) && (pEntry->Sst == SST_ASSOC))
							break;
		                    }

					if (pEntry && i < MAX_LEN_OF_MAC_TABLE) {
						if (((pEntry->HTPhyMode.field.MODE == MODE_HTMIX || pEntry->HTPhyMode.field.MODE == MODE_HTGREENFIELD) &&
							(((pAd->CommonCfg.TxStream == 2) && (pEntry->HTPhyMode.field.MCS >= MCS_14)) ||
							((pAd->CommonCfg.TxStream == 1) && (pEntry->HTPhyMode.field.MCS >= MCS_6))))
							|| ((pEntry->HTPhyMode.field.MODE == MODE_VHT) &&
							(((pAd->CommonCfg.TxStream == 2) && (pEntry->HTPhyMode.field.MCS >= 23)) ||
							((pAd->CommonCfg.TxStream == 1) && (pEntry->HTPhyMode.field.MCS >= 7))))) {
							txop_value = 0x60;
							DBGPRINT(RT_DEBUG_INFO, ("%s::enable Tx burst to 0x60 under HT/VHT mode\n", __FUNCTION__));
						}
					}
				}
#endif /* DOT11_VHT_AC */
				else if (pAd->CommonCfg.bEnableTxBurst)
					txop_value = txop_value_burst;
				else
					txop_value = 0;

#ifdef MULTI_CLIENT_SUPPORT
				if(pAd->ApCfg.EntryClientCount > 2) /* for Multi-Clients */
					txop_value = 0;
#endif /* MULTI_CLIENT_SUPPORT */

				RegValue  &= 0xFFFFFF00;
				/*if ((RegValue & 0x0000FF00) == 0x00005400)
					RegValue -= 0x00001100; */
				/*txop_value = 0; */
				RegValue  |= txop_value;  /* for performance, set the TXOP to non-zero */
				RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, RegValue);
				csr0.field.Ac0Txop = txop_value;	/* QID_AC_BE */
				csr0.field.Ac1Txop = 0;				/* QID_AC_BK */
				RTMP_IO_WRITE32(pAd, WMM_TXOP0_CFG, csr0.word);
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_DYNAMIC_BE_TXOP_ACTIVE);
			}
		}
	}
	pAd->OneSecondnonBEpackets = 0;
}


VOID APRxErrorHandle(struct rtmp_adapter *pAd, RX_BLK *pRxBlk)
{
	MAC_TABLE_ENTRY *pEntry = NULL;
	RXINFO_STRUC *pRxInfo = pRxBlk->pRxInfo;
	PCIPHER_KEY pWpaKey;
	UCHAR							FromWhichBSSID = BSS0;
	UCHAR			Wcid;
	PHEADER_802_11	pHeader = pRxBlk->pHeader;

	if (pRxInfo->CipherErr)
		INC_COUNTER64(pAd->WlanCounters.WEPUndecryptableCount);

	if (pRxInfo->CipherErr)
	{
		if (pRxBlk->wcid < MAX_LEN_OF_MAC_TABLE)
		{
#ifdef APCLI_SUPPORT

			Wcid = pRxBlk->wcid;
			if (VALID_WCID(Wcid))
				pEntry = ApCliTableLookUpByWcid(pAd, Wcid, pHeader->Addr2);
			else
				pEntry = MacTableLookup(pAd, pHeader->Addr2);

			if (pEntry && IS_ENTRY_APCLI(pEntry))
			{
				FromWhichBSSID = pEntry->wdev_idx + MIN_NET_DEVICE_FOR_APCLI;
				if (pRxInfo->CipherErr == 2)
				{
					pWpaKey = &pEntry->PairwiseKey;
#ifdef WPA_SUPPLICANT_SUPPORT
					if (pAd->ApCfg.ApCliTab[pEntry->wdev_idx].wpa_supplicant_info.WpaSupplicantUP)
					{
						WpaSendMicFailureToWpaSupplicant(pAd->net_dev, pHeader->Addr2,
											 (pWpaKey->Type == PAIRWISEKEY) ? TRUE : FALSE,
											 (INT)pRxBlk->key_idx, NULL);
					}
					if (((pRxInfo->CipherErr & 2) == 2) && INFRA_ON(pAd))
						RTMPSendWirelessEvent(pAd, IW_MIC_ERROR_EVENT_FLAG, pEntry->Addr, FromWhichBSSID, 0);
#else
					ApCliRTMPReportMicError(pAd, pWpaKey, BSS0);
#endif /* APCLI_WPA_SUPPLICANT_SUPPORT */
					DBGPRINT_RAW(RT_DEBUG_ERROR,("Rx MIC Value error\n"));
				}
			}
			else
#endif /* APCLI_SUPPORT */
			if (pRxInfo->U2M)
			{
				pEntry = &pAd->MacTab.Content[pRxBlk->wcid];

				/*
					MIC error
					Before verifying the MIC, the receiver shall check FCS, ICV and TSC.
					This avoids unnecessary MIC failure events.
				*/
				if ((pEntry->WepStatus == Ndis802_11TKIPEnable)
					&& (pRxInfo->CipherErr == 2))
				{
#ifdef HOSTAPD_SUPPORT
					if(pAd->ApCfg.MBSSID[pEntry->apidx].Hostapd == Hostapd_EXT)
						ieee80211_notify_michael_failure(pAd, pRxBlk->pHeader, (uint32_t)pRxBlk->key_idx, 0);
			      		else
#endif/*HOSTAPD_SUPPORT*/
		      			{
		      				RTMP_HANDLE_COUNTER_MEASURE(pAd, pEntry);
		      			}
				}

				/* send wireless event - for icv error */
				if ((pRxInfo->CipherErr & 1) == 1)
					RTMPSendWirelessEvent(pAd, IW_ICV_ERROR_EVENT_FLAG, pEntry->Addr, 0, 0);
			}
		}

		DBGPRINT(RT_DEBUG_TRACE, ("Rx u2me Cipher Err(MPDUsize=%d, WCID=%d, CipherErr=%d)\n",
					pRxBlk->MPDUtotalByteCnt, pRxBlk->wcid, pRxInfo->CipherErr));

	}

	pAd->Counters8023.RxErrors++;
}


static int dump_next_valid = 0;
BOOLEAN APCheckVaildDataFrame(struct rtmp_adapter *pAd, RX_BLK *pRxBlk)
{
	HEADER_802_11 *pHeader = pRxBlk->pHeader;
	BOOLEAN isVaild = FALSE;

	do
	{
#ifndef APCLI_SUPPORT
		/* should not drop Ap-Client packet. */
		if (pHeader->FC.ToDs == 0)
			break;
#endif /* APCLI_SUPPORT */


		/* check if Class2 or 3 error */
		if ((pHeader->FC.FrDs == 0) && (APChkCls2Cls3Err(pAd, pRxBlk->wcid, pHeader)))
			break;

//+++Add by shiang for debug
#ifdef RLT_MAC_DBG
		if (pAd->chipCap.hif_type == HIF_RLT) {
			if (pRxBlk->wcid >= MAX_LEN_OF_MAC_TABLE) {
				MAC_TABLE_ENTRY *pEntry = NULL;

				DBGPRINT(RT_DEBUG_WARN, ("ErrWcidPkt: seq=%d, ts=0x%02x%02x%02x%02x\n",
									pHeader->Sequence,
									pRxBlk->pRxWI->RXWI_N.rssi[0],
									pRxBlk->pRxWI->RXWI_N.rssi[1],
									pRxBlk->pRxWI->RXWI_N.rssi[2],
									pRxBlk->pRxWI->RXWI_N.rssi[3]));
				pEntry = MacTableLookup(pAd, pHeader->Addr2);
				if (pEntry && (pEntry->Sst == SST_ASSOC) && IS_ENTRY_CLIENT(pEntry))
					pRxBlk->wcid = pEntry->wcid;

				dump_next_valid = 1;
			}
			else if (dump_next_valid)
			{
				DBGPRINT(RT_DEBUG_WARN, ("NextValidWcidPkt: seq=%d, ts=0x%02x%02x%02x%02x\n",
									pHeader->Sequence,
									pRxBlk->pRxWI->RXWI_N.rssi[0],
									pRxBlk->pRxWI->RXWI_N.rssi[1],
									pRxBlk->pRxWI->RXWI_N.rssi[2],
									pRxBlk->pRxWI->RXWI_N.rssi[3]));
				dump_next_valid = 0;
			}
		}
#endif /* RLT_MAC_DBG */
//---Add by shiang for debug

		if(pAd->ApCfg.BANClass3Data == TRUE)
			break;

		isVaild = TRUE;
	} while (0);

	return isVaild;
}

/* For TKIP frame, calculate the MIC value */
BOOLEAN APCheckTkipMICValue(
	IN	struct rtmp_adapter *pAd,
	IN	MAC_TABLE_ENTRY	*pEntry,
	IN	RX_BLK			*pRxBlk)
{
	PHEADER_802_11	pHeader = pRxBlk->pHeader;
	UCHAR			*pData = pRxBlk->pData;
	USHORT			DataSize = pRxBlk->DataSize;
	UCHAR			UserPriority = pRxBlk->UserPriority;
	PCIPHER_KEY		pWpaKey;
	UCHAR			*pDA, *pSA;

	pWpaKey = &pEntry->PairwiseKey;

	if (RX_BLK_TEST_FLAG(pRxBlk, fRX_WDS))
	{
		pDA = pHeader->Addr3;
		pSA = (PUCHAR)pHeader + sizeof(HEADER_802_11);
	}
	else if (RX_BLK_TEST_FLAG(pRxBlk, fRX_APCLI))
	{
		pDA = pHeader->Addr1;
		pSA = pHeader->Addr3;
	}
	else
	{
		pDA = pHeader->Addr3;
		pSA = pHeader->Addr2;
	}

	if (RTMPTkipCompareMICValue(pAd,
								pData,
								pDA,
								pSA,
								pWpaKey->RxMic,
								UserPriority,
								DataSize) == FALSE)
	{
		DBGPRINT_RAW(RT_DEBUG_ERROR,("Rx MIC Value error 2\n"));

#ifdef APCLI_SUPPORT
#ifdef WPA_SUPPLICANT_SUPPORT
		if (IS_ENTRY_APCLI(pEntry) && pAd->ApCfg.ApCliTab[pEntry->wdev_idx].wpa_supplicant_info.WpaSupplicantUP)
		{
			WpaSendMicFailureToWpaSupplicant(pAd->net_dev, pHeader->Addr2,
							 (pWpaKey->Type == PAIRWISEKEY) ? TRUE : FALSE,
							 (INT)pRxBlk->key_idx, NULL);
		}
		else
#endif /* WPA_SUPPLICANT_SUPPORT */
#endif /* APCLI_SUPPORT */
		{
			RTMP_HANDLE_COUNTER_MEASURE(pAd, pEntry);
		}

		/* release packet */
		RELEASE_NDIS_PACKET(pAd, pRxBlk->pRxPacket, NDIS_STATUS_FAILURE);
		return FALSE;
	}

	return TRUE;
}


VOID APRxEAPOLFrameIndicate(
	IN	struct rtmp_adapter *pAd,
	IN	MAC_TABLE_ENTRY	*pEntry,
	IN	RX_BLK			*pRxBlk,
	IN	UCHAR			FromWhichBSSID)
{
	BOOLEAN 		CheckPktSanity = TRUE;
	UCHAR			*pTmpBuf;
#ifdef APCLI_SUPPORT
#ifdef WPA_SUPPLICANT_SUPPORT
	INT eapcode;
#endif /* WPA_SUPPLICANT_SUPPORT */
#endif /* APCLI_SUPPORT */
	do
	{
	} while (FALSE);

	/* Sanity Check */
	if(pRxBlk->DataSize < (LENGTH_802_1_H + LENGTH_EAPOL_H))
	{
		CheckPktSanity = FALSE;
		DBGPRINT(RT_DEBUG_ERROR, ("Total pkts size is too small.\n"));
	}
	else if (!RTMPEqualMemory(SNAP_802_1H, pRxBlk->pData, 6))
	{
		CheckPktSanity = FALSE;
		DBGPRINT(RT_DEBUG_ERROR, ("Can't find SNAP_802_1H parameter.\n"));
	}
	else if (!RTMPEqualMemory(EAPOL, pRxBlk->pData+6, 2))
	{
		CheckPktSanity = FALSE;
		DBGPRINT(RT_DEBUG_ERROR, ("Can't find EAPOL parameter.\n"));
	}
	else if(*(pRxBlk->pData+9) > EAPOLASFAlert)
	{
		CheckPktSanity = FALSE;
		DBGPRINT(RT_DEBUG_ERROR, ("Unknown EAP type(%d).\n", *(pRxBlk->pData+9)));
	}

	if(CheckPktSanity == FALSE)
	{
		goto done;
	}



#ifdef HOSTAPD_SUPPORT
	if ((pEntry) && pAd->ApCfg.MBSSID[pEntry->apidx].Hostapd == Hostapd_EXT)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("Indicate_Legacy_Packet\n"));
		Indicate_Legacy_Packet(pAd, pRxBlk, FromWhichBSSID);
		return;
	}
#endif/*HOSTAPD_SUPPORT*/
#ifdef RT_CFG80211_SUPPORT
	if (pEntry)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_AP EAPOL Indicate_Legacy_Packet\n"));
		Indicate_Legacy_Packet(pAd, pRxBlk, FromWhichBSSID);
        return;
	}
#endif/*RT_CFG80211_SUPPORT*/

#ifdef APCLI_SUPPORT
#ifdef WPA_SUPPLICANT_SUPPORT
	if (IS_ENTRY_APCLI(pEntry))
	{
		APCLI_STRUCT *apcli_entry = &pAd->ApCfg.ApCliTab[pEntry->wdev_idx];

		eapcode=WpaCheckEapCode(pAd, pRxBlk->pData,
									pRxBlk->DataSize,
									LENGTH_802_1_H);

		DBGPRINT(RT_DEBUG_TRACE, ("eapcode=%d\n",eapcode));
		if (apcli_entry->wpa_supplicant_info.WpaSupplicantUP &&
			apcli_entry->wdev.IEEE8021X == TRUE && (EAP_CODE_SUCCESS == eapcode))
		{
			PUCHAR	Key;
			UCHAR 	CipherAlg;
			int     idx = 0;
			int BssIdx = pAd->ApCfg.BssidNum + MAX_MESH_NUM + pEntry->wdev_idx;
			WPA_SUPPLICANT_INFO *sup_info = &apcli_entry->wpa_supplicant_info;

			DBGPRINT_RAW(RT_DEBUG_TRACE, ("Receive EAP-SUCCESS Packet\n"));
			/* pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; */
			/* STA_PORT_SECURED(pAd); */
			pEntry->PortSecured=WPA_802_1X_PORT_SECURED;
			pEntry->PrivacyFilter=Ndis802_11PrivFilterAcceptAll;
			if (sup_info->IEEE8021x_required_keys == FALSE)
			{
				idx = sup_info->DesireSharedKeyId;
				CipherAlg = sup_info->DesireSharedKey[idx].CipherAlg;
				Key = sup_info->DesireSharedKey[idx].Key;

				if (sup_info->DesireSharedKey[idx].KeyLen > 0)
    				{
					/* Set key material and cipherAlg to Asic */
					RTMP_ASIC_SHARED_KEY_TABLE(pAd,BssIdx, idx,
									&sup_info->DesireSharedKey[idx]);

					/* STA doesn't need to set WCID attribute for group key */
					/* Assign pairwise key info */
					RTMP_SET_WCID_SEC_INFO(pAd, BssIdx, idx, CipherAlg, pEntry->wcid, SHAREDKEYTABLE);

					/* RTMP_IndicateMediaState(pAd, NdisMediaStateConnected); */
                       		/* pAd->ExtraInfo = GENERAL_LINK_UP; */

					/*  For Preventing ShardKey Table is cleared by remove key procedure. */
    				apcli_entry->SharedKey[idx].CipherAlg = CipherAlg;
					apcli_entry->SharedKey[idx].KeyLen = sup_info->DesireSharedKey[idx].KeyLen;
					memmove(apcli_entry->SharedKey[idx].Key,
									   sup_info->DesireSharedKey[idx].Key,
									   sup_info->DesireSharedKey[idx].KeyLen);
    				}
				}
			}

		if (apcli_entry->wpa_supplicant_info.WpaSupplicantUP &&
			((pEntry->AuthMode == Ndis802_11AuthModeWPA) ||
			(pEntry->AuthMode == Ndis802_11AuthModeWPA2) ||
			(apcli_entry->wdev.IEEE8021X == TRUE))
		)
		{
			DBGPRINT(RT_DEBUG_TRACE, ("Indicate_Legacy_Packet\n"));
			Indicate_Legacy_Packet(pAd, pRxBlk, FromWhichBSSID);
			return;
		}
	}
#endif/* WPA_SUPPLICANT_SUPPORT */
#endif/* APCLI_SUPPORT */

#ifdef DOT1X_SUPPORT
	/* sent this frame to upper layer TCPIP */
	if ((pEntry) && (pEntry->WpaState < AS_INITPMK) &&
		((pEntry->AuthMode == Ndis802_11AuthModeWPA) ||
		((pEntry->AuthMode == Ndis802_11AuthModeWPA2) && (pEntry->PMKID_CacheIdx == ENTRY_NOT_FOUND)) ||
		pAd->ApCfg.MBSSID[pEntry->apidx].wdev.IEEE8021X == TRUE))
	{


		Indicate_Legacy_Packet(pAd, pRxBlk, FromWhichBSSID);
		return;
	}
	else	/* sent this frame to WPA state machine */
#endif /* DOT1X_SUPPORT */
	{
		pTmpBuf = pRxBlk->pData - LENGTH_802_11;
		memmove(pTmpBuf, pRxBlk->pHeader, LENGTH_802_11);
		REPORT_MGMT_FRAME_TO_MLME(pAd, pRxBlk->wcid, pTmpBuf,
							pRxBlk->DataSize + LENGTH_802_11,
							pRxBlk->rssi[0], pRxBlk->rssi[1], pRxBlk->rssi[2],
							0, OPMODE_AP);
	}

done:
	RELEASE_NDIS_PACKET(pAd, pRxBlk->pRxPacket, NDIS_STATUS_FAILURE);
	return;

}

VOID Announce_or_Forward_802_3_Packet(
	IN	struct rtmp_adapter *pAd,
	IN	struct sk_buff *pPacket,
	IN	UCHAR			FromWhichBSSID)
{
	if (APFowardWirelessStaToWirelessSta(pAd, pPacket, FromWhichBSSID))
		announce_802_3_packet(pAd, pPacket,OPMODE_AP);
	else
	{
		/* release packet */
		RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
	}
}


VOID APRxDataFrameAnnounce(
	IN struct rtmp_adapter *pAd,
	IN MAC_TABLE_ENTRY *pEntry,
	IN RX_BLK *pRxBlk,
	IN UCHAR FromWhichBSSID)
{

	/* non-EAP frame */
	if (!RTMPCheckWPAframe(pAd, pEntry, pRxBlk->pData, pRxBlk->DataSize, FromWhichBSSID))
	{

		/*
			drop all non-EAP DATA frame before
			this client's Port-Access-Control is secured
		 */
		if (pEntry->PrivacyFilter == Ndis802_11PrivFilter8021xWEP)
		{
			/*
				If	1) no any EAP frame is received within 5 sec and
					2) an encrypted non-EAP frame from peer associated STA is received,
				AP would send de-authentication to this STA.
			 */
			if (IS_ENTRY_CLIENT(pEntry) && pRxBlk->pHeader->FC.Wep &&
				pEntry->StaConnectTime > 5 && pEntry->WpaState < AS_AUTHENTICATION2)
			{
				DBGPRINT(RT_DEBUG_WARN, ("==> De-Auth this STA(%02x:%02x:%02x:%02x:%02x:%02x)\n", PRINT_MAC(pEntry->Addr)));
				MlmeDeAuthAction(pAd, pEntry, REASON_NO_LONGER_VALID, FALSE);
			}

			/* release packet */
			RELEASE_NDIS_PACKET(pAd, pRxBlk->pRxPacket, NDIS_STATUS_FAILURE);
			return;
		}




#ifdef STATS_COUNT_SUPPORT
		if (pEntry
			&& (IS_ENTRY_CLIENT(pEntry))
			&& (pEntry->pMbss))
		{
			MULTISSID_STRUCT *pMbss = pEntry->pMbss;
			if(IS_MULTICAST_MAC_ADDR(pRxBlk->pHeader->Addr3) ||
				IS_MULTICAST_MAC_ADDR(pRxBlk->pHeader->Addr1))
				pMbss->mcPktsRx++;
			else if(IS_BROADCAST_MAC_ADDR(pRxBlk->pHeader->Addr3) ||
				IS_BROADCAST_MAC_ADDR(pRxBlk->pHeader->Addr1))
				pMbss->bcPktsRx++;
			else
				pMbss->ucPktsRx++;
		}
#endif /* STATS_COUNT_SUPPORT */
		RX_BLK_CLEAR_FLAG(pRxBlk, fRX_EAP);
		if (!RX_BLK_TEST_FLAG(pRxBlk, fRX_ARALINK))
		{
			/* Normal legacy, AMPDU or AMSDU */
			CmmRxnonRalinkFrameIndicate(pAd, pRxBlk, FromWhichBSSID);
		}
		else
		{
			/* ARALINK */
			CmmRxRalinkFrameIndicate(pAd, pEntry, pRxBlk, FromWhichBSSID);
		}
	}
	else
	{
		RX_BLK_SET_FLAG(pRxBlk, fRX_EAP);

		/* Update the WPA STATE to indicate the EAP handshaking is started */
		if (pEntry->WpaState == AS_AUTHENTICATION)
			pEntry->WpaState = AS_AUTHENTICATION2;

#ifdef DOT11_N_SUPPORT
		if (RX_BLK_TEST_FLAG(pRxBlk, fRX_AMPDU) && (pAd->CommonCfg.bDisableReordering == 0))
		{
			Indicate_AMPDU_Packet(pAd, pRxBlk, FromWhichBSSID);
		}
		else
#endif /* DOT11_N_SUPPORT */
		{
			/* Determin the destination of the EAP frame */
			/*  to WPA state machine or upper layer */
			APRxEAPOLFrameIndicate(pAd, pEntry, pRxBlk, FromWhichBSSID);
		}
	}
}


#ifdef HDR_TRANS_SUPPORT
VOID APRxDataFrameAnnounce_Hdr_Trns(
	IN struct rtmp_adapter *pAd,
	IN MAC_TABLE_ENTRY *pEntry,
	IN RX_BLK *pRxBlk,
	IN UCHAR FromWhichBSSID)
{

	/* non-EAP frame */
	if (!RTMPCheckWPAframe_Hdr_Trns(pAd, pEntry, pRxBlk->pTransData, pRxBlk->TransDataSize, FromWhichBSSID))
	{

		/*
			drop all non-EAP DATA frame before
			this client's Port-Access-Control is secured
		 */
		if (pEntry->PrivacyFilter == Ndis802_11PrivFilter8021xWEP)
		{
			/*
				If	1) no any EAP frame is received within 5 sec and
					2) an encrypted non-EAP frame from peer associated STA is received,
				AP would send de-authentication to this STA.
			 */
			if (IS_ENTRY_CLIENT(pEntry) && pRxBlk->pHeader->FC.Wep &&
				pEntry->StaConnectTime > 5 && pEntry->WpaState < AS_AUTHENTICATION2)
			{
				DBGPRINT(RT_DEBUG_WARN, ("==> De-Auth this STA(%02x:%02x:%02x:%02x:%02x:%02x)\n", PRINT_MAC(pEntry->Addr)));
				MlmeDeAuthAction(pAd, pEntry, REASON_NO_LONGER_VALID, FALSE);
			}

			/* release packet */
			RELEASE_NDIS_PACKET(pAd, pRxBlk->pRxPacket, NDIS_STATUS_FAILURE);
			return;
		}



#ifdef STATS_COUNT_SUPPORT
		if (pEntry
			&& (IS_ENTRY_CLIENT(pEntry))
			&& (pEntry->pMbss))
		{
			MULTISSID_STRUCT *pMbss = pEntry->pMbss;
			if(IS_MULTICAST_MAC_ADDR(pRxBlk->pHeader->Addr3) || IS_MULTICAST_MAC_ADDR(pRxBlk->pHeader->Addr1))
			{
					pMbss->mcPktsRx++;
			}
			else if(IS_BROADCAST_MAC_ADDR(pRxBlk->pHeader->Addr3) || IS_BROADCAST_MAC_ADDR(pRxBlk->pHeader->Addr1))
			{
					pMbss->bcPktsRx++;
			}
			else
			{
					pMbss->ucPktsRx++;
			}
		}
#endif /* STATS_COUNT_SUPPORT */
		RX_BLK_CLEAR_FLAG(pRxBlk, fRX_EAP);
		if (!RX_BLK_TEST_FLAG(pRxBlk, fRX_ARALINK))
		{
			/* Normal legacy, AMPDU or AMSDU */
			CmmRxnonRalinkFrameIndicate_Hdr_Trns(pAd, pRxBlk, FromWhichBSSID);
		}
		else
		{
			/* ARALINK */
			CmmRxRalinkFrameIndicate(pAd, pEntry, pRxBlk, FromWhichBSSID);
		}
	}
	else
	{
		RX_BLK_SET_FLAG(pRxBlk, fRX_EAP);

		/* Update the WPA STATE to indicate the EAP handshaking is started */
		if (pEntry->WpaState == AS_AUTHENTICATION)
			pEntry->WpaState = AS_AUTHENTICATION2;

#ifdef DOT11_N_SUPPORT
		if (RX_BLK_TEST_FLAG(pRxBlk, fRX_AMPDU) && (pAd->CommonCfg.bDisableReordering == 0))
		{
			Indicate_AMPDU_Packet_Hdr_Trns(pAd, pRxBlk, FromWhichBSSID);
		}
		else
#endif /* DOT11_N_SUPPORT */
		{
			/* Determin the destination of the EAP frame */
			/*  to WPA state machine or upper layer */
			APRxEAPOLFrameIndicate(pAd, pEntry, pRxBlk, FromWhichBSSID);
		}
	}
}
#endif /* HDR_TRANS_SUPPORT */


/*
	All Rx routines use RX_BLK structure to hande rx events
	It is very important to build pRxBlk attributes
		1. pHeader pointer to 802.11 Header
		2. pData pointer to payload including LLC (just skip Header)
		3. set payload size including LLC to DataSize
		4. set some flags with RX_BLK_SET_FLAG()
*/
VOID APHandleRxDataFrame(struct rtmp_adapter *pAd, RX_BLK *pRxBlk)
{
	RXINFO_STRUC *pRxInfo = pRxBlk->pRxInfo;
	RXWI_STRUC *pRxWI = pRxBlk->pRxWI;
	HEADER_802_11 *pHeader = pRxBlk->pHeader;
	struct sk_buff *pRxPacket = pRxBlk->pRxPacket;
	BOOLEAN bFragment = FALSE;
	MAC_TABLE_ENTRY *pEntry = NULL;
	UCHAR FromWhichBSSID = BSS0;
	UCHAR OldPwrMgmt = PWR_ACTIVE;	/* UAPSD AP SUPPORT */
	UCHAR UserPriority = 0;
	INT hdr_len = LENGTH_802_11;
	FRAME_CONTROL *pFmeCtrl = &pHeader->FC;
	COUNTER_RALINK *pCounter = &pAd->RalinkCounters;
#ifdef APCLI_SUPPORT
	PAPCLI_STRUCT pApCliEntry = NULL;
	pApCliEntry = &pAd->ApCfg.ApCliTab[0];
#endif

//+++Add by shiang for debug
//---Add by shiangf for debug

	if (APCheckVaildDataFrame(pAd, pRxBlk) != TRUE)
		goto err;


	if (pRxInfo->U2M)
	{
		Update_Rssi_Sample(pAd, &pAd->ApCfg.RssiSample, pRxWI);
		pAd->ApCfg.NumOfAvgRssiSample ++;

#ifdef DBG_DIAGNOSE
		if (pAd->DiagStruct.inited) {
			struct dbg_diag_info *diag_info;
			diag_info = &pAd->DiagStruct.diag_info[pAd->DiagStruct.ArrayCurIdx];
			diag_info->RxDataCnt++;
#ifdef DBG_RX_MCS
			if (pRxBlk->rx_rate.field.MODE == MODE_HTMIX ||
				pRxBlk->rx_rate.field.MODE == MODE_HTGREENFIELD) {
				if (pRxBlk->rx_rate.field.MCS < MAX_MCS_SET)
					diag_info->RxMcsCnt_HT[pRxBlk->rx_rate.field.MCS]++;
			}
#ifdef DOT11_VHT_AC
			if (pRxBlk->rx_rate.field.MODE == MODE_VHT) {
				INT mcs_idx = ((pRxBlk->rx_rate.field.MCS >> 4) * 10) +
								(pRxBlk->rx_rate.field.MCS & 0xf);
				if (mcs_idx < MAX_VHT_MCS_SET)
					diag_info->RxMcsCnt_VHT[mcs_idx]++;
			}
#endif /* DOT11_VHT_AC */
#endif /* DBG_RX_MCS */
		}
#endif /* DBG_DIAGNOSE */
	}

	/* handle WDS */
	if ((pFmeCtrl->FrDs == 1) && (pFmeCtrl->ToDs == 1))
	{
		do
		{


		} while(FALSE);

		/* have no WDS or MESH support, drop it */
		if (pEntry == NULL)
			goto err;
	}
	else if ((pFmeCtrl->FrDs == 1) && (pFmeCtrl->ToDs == 0))
	{
#ifdef APCLI_SUPPORT
		/* handle APCLI. */
		if (VALID_WCID(pRxBlk->wcid))
			pEntry = ApCliTableLookUpByWcid(pAd, pRxBlk->wcid, pHeader->Addr2);
		else
			pEntry = MacTableLookup(pAd, pHeader->Addr2);

		if (pEntry && IS_ENTRY_APCLI(pEntry))
		{
			ULONG Now32;

			if (!(pEntry && APCLI_IF_UP_CHECK(pAd, pEntry->wdev_idx)))
				goto err;

			pApCliEntry = &pAd->ApCfg.ApCliTab[pEntry->wdev_idx];

			if (pApCliEntry)
			{
				NdisGetSystemUpTime(&Now32);
				pApCliEntry->ApCliRcvBeaconTime = Now32;
			}

			FromWhichBSSID = pEntry->wdev_idx + MIN_NET_DEVICE_FOR_APCLI;
			RX_BLK_SET_FLAG(pRxBlk, fRX_APCLI);

			/* Process broadcast packets */
			if (pRxInfo->Mcast || pRxInfo->Bcast)
			{
				/* Process the received broadcast frame for AP-Client. */
				if (!ApCliHandleRxBroadcastFrame(pAd, pRxBlk, pEntry, FromWhichBSSID))
				{
					RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);
				}
				return;
			}
		}
		else
#endif /* APCLI_SUPPORT */
		{
			goto err;
		}
	}
	else
	{
		pEntry = PACInquiry(pAd, pRxBlk->wcid);

		/*	can't find associated STA entry then filter invlid data frame */
		if (!pEntry)
			goto err;

		FromWhichBSSID = pEntry->apidx;

#ifdef STATS_COUNT_SUPPORT
		/* Increase received byte counter per BSS */
		if (pHeader->FC.FrDs == 0 &&
			pRxInfo->U2M &&
			FromWhichBSSID < pAd->ApCfg.BssidNum)
		{
			MULTISSID_STRUCT *pMbss = pEntry->pMbss;
			if (pMbss != NULL)
			{
				pMbss->ReceivedByteCount += pRxBlk->MPDUtotalByteCnt;
				pMbss->RxCount ++;
			}
		}

		/* update multicast counter */
                if (IS_MULTICAST_MAC_ADDR(pHeader->Addr3))
                        INC_COUNTER64(pAd->WlanCounters.MulticastReceivedFrameCount);
#endif /* STATS_COUNT_SUPPORT */
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
		FromWhichBSSID += MIN_NET_DEVICE_FOR_CFG80211_VIF_P2P_GO;
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
	}

	ASSERT(pEntry->Aid == pRxBlk->wcid);


#ifdef DOT11_N_SUPPORT
#ifndef DOT11_VHT_AC
#ifndef WFA_VHT_PF
// TODO: shiang@PF#2, is this atheros protection still necessary here????
	/* check Atheros Client */
	if (!pEntry->bIAmBadAtheros && (pFmeCtrl->Retry) &&
		(pRxBlk->rx_rate.field.MODE < MODE_VHT) &&
		(pRxInfo->AMPDU == 1) && (pAd->CommonCfg.bHTProtect == TRUE)
	)
	{
		if (pAd->CommonCfg.IOTestParm.bRTSLongProtOn == FALSE)
			RTMP_UPDATE_PROTECT(pAd, 8 , ALLN_SETPROTECT, FALSE, FALSE);
		pEntry->bIAmBadAtheros = TRUE;

		if (pEntry->WepStatus != Ndis802_11WEPDisabled)
			pEntry->MpduDensity = 6;
	}
#endif /* WFA_VHT_PF */
#endif /* DOT11_VHT_AC */
#endif /* DOT11_N_SUPPORT */

   	/* update rssi sample */
   	Update_Rssi_Sample(pAd, &pEntry->RssiSample, pRxWI);

	if (pAd->ApCfg.MBSSID[pEntry->apidx].RssiLowForStaKickOut != 0)
	{
		pEntry->curLastDataRssiIndex = pEntry->curLastDataRssiIndex % MAX_LAST_DATA_RSSI_LEN;
		pEntry->LastDataRssi[pEntry->curLastDataRssiIndex] = RTMPMaxRssi(pAd, pEntry->RssiSample.LastRssi0,
					pEntry->RssiSample.LastRssi1, pEntry->RssiSample.LastRssi2);
		//DBGPRINT(RT_DEBUG_TRACE, ("Recored ==> %d:[%d].\n",pEntry->curLastDataRssiIndex,
		//						   pEntry->LastDataRssi[pEntry->curLastDataRssiIndex]));
		pEntry->curLastDataRssiIndex++;
	}


	if (pRxInfo->U2M)
	{
		pEntry->LastRxRate = (ULONG)(pRxBlk->rx_rate.word);

#ifdef TXBF_SUPPORT
		if (pRxBlk->rx_rate.field.ShortGI)
			pEntry->OneSecRxSGICount++;
		else
			pEntry->OneSecRxLGICount++;
#endif // TXBF_SUPPORT //
	}

	pAd->ApCfg.LastSNR0 = (UCHAR)(pRxBlk->snr[0]);
	pAd->ApCfg.LastSNR1 = (UCHAR)(pRxBlk->snr[1]);
#ifdef DOT11N_SS3_SUPPORT
	pAd->ApCfg.LastSNR2 = (UCHAR)(pRxBlk->snr[2]);
#endif /* DOT11N_SS3_SUPPORT */
	pEntry->freqOffset = (CHAR)(pRxBlk->freq_offset);
	pEntry->freqOffsetValid = TRUE;


   	/* Gather PowerSave information from all valid DATA frames. IEEE 802.11/1999 p.461 */
   	/* must be here, before no DATA check */
	pRxBlk->pData = (UCHAR *)pHeader;

   	/* 1: PWR_SAVE, 0: PWR_ACTIVE */
   	OldPwrMgmt = RtmpPsIndicate(pAd, pHeader->Addr2, pEntry->wcid, pFmeCtrl->PwrMgmt);
#ifdef UAPSD_SUPPORT
	if (pFmeCtrl->PwrMgmt)
	{
	   	if ((CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_APSD_CAPABLE)) &&
			(pFmeCtrl->SubType & 0x08))
	   	{
			/*
				In IEEE802.11e, 11.2.1.4 Power management with APSD,
				If there is no unscheduled SP in progress, the unscheduled SP begins
				when the QAP receives a trigger frame from a non-AP QSTA, which is a
				QoS data or QoS Null frame associated with an AC the STA has
				configured to be trigger-enabled.

				In WMM v1.1, A QoS Data or QoS Null frame that indicates transition
				to/from Power Save Mode is not considered to be a Trigger Frame and
				the AP shall not respond with a QoS Null frame.
			*/
			/* Trigger frame must be QoS data or QoS Null frame */
	   		UCHAR  OldUP;

			OldUP = (*(pRxBlk->pData+LENGTH_802_11) & 0x07);
			if (OldPwrMgmt == PWR_SAVE)
			{
#ifdef DROP_MASK_SUPPORT
				/* Disable Drop Mask */
				set_drop_mask_per_client(pAd, pEntry, 2, 0);
#endif /* DROP_MASK_SUPPORT */

				UAPSD_TriggerFrameHandle(pAd, pEntry, OldUP);
			}
		}
	}
#endif /* UAPSD_SUPPORT */

	/* Drop NULL, CF-ACK(no data), CF-POLL(no data), and CF-ACK+CF-POLL(no data) data frame */
	if ((pFmeCtrl->SubType & 0x04) && (pFmeCtrl->Order == 0)) /* bit 2 : no DATA */
	{
		/* Increase received drop packet counter per BSS */
		if (pFmeCtrl->FrDs == 0 &&
			pRxInfo->U2M &&
			pRxBlk->bss_idx < pAd->ApCfg.BssidNum)
		{
			pAd->ApCfg.MBSSID[pRxBlk->bss_idx].RxDropCount ++;
		}

		RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);
		return;
	}

	/*
		update RxBlk->pData, DataSize, 802.11 Header, QOS, HTC, Hw Padding
	*/

	/* 1. skip 802.11 HEADER */
	pRxBlk->pData += hdr_len;
	pRxBlk->DataSize -= hdr_len;

	/* 2. QOS */
	if (pFmeCtrl->SubType & 0x08)
	{
		RX_BLK_SET_FLAG(pRxBlk, fRX_QOS);
		UserPriority = *(pRxBlk->pData) & 0x0f;


		/* count packets priroity more than BE */
#ifdef APCLI_CERT_SUPPORT
		//if (pAd->bApCliCertTest == FALSE)
		if (pApCliEntry->wdev.bWmmCapable == FALSE)
#endif /* APCLI_CERT_SUPPORT */
		detect_wmm_traffic(pAd, UserPriority, 0);
		/* bit 7 in QoS Control field signals the HT A-MSDU format */
		if ((*pRxBlk->pData) & 0x80)
		{
			RX_BLK_SET_FLAG(pRxBlk, fRX_AMSDU);

			/* calculate received AMSDU count and ByteCount */
			pCounter->ReceivedAMSDUCount.u.LowPart ++;
			pCounter->ReceivedOctesInAMSDUCount.QuadPart += (pRxBlk->DataSize + hdr_len);
		}

		/* skip QOS contorl field */
		pRxBlk->pData += 2;
		pRxBlk->DataSize -=2;
	}
	pRxBlk->UserPriority = UserPriority;

#ifdef TXBF_SUPPORT
	if (pAd->chipCap.FlgHwTxBfCap &&
		(pHeader->FC.SubType & 0x08) && pHeader->FC.Order)
	{
		handleHtcField(pAd, pRxBlk);
	}
#endif /* TXBF_SUPPORT */

	/* 3. Order bit: A-Ralink or HTC+ */
	if (pFmeCtrl->Order)
	{
#ifdef AGGREGATION_SUPPORT
		if (
#ifdef DOT11_N_SUPPORT
			(pRxBlk->rx_rate.field.MODE < MODE_HTMIX) &&
#endif /* DOT11_N_SUPPORT */
			(CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_AGGREGATION_CAPABLE))
		)
		{
			RX_BLK_SET_FLAG(pRxBlk, fRX_ARALINK);
		}
		else
#endif
		{
			RX_BLK_SET_FLAG(pRxBlk, fRX_HTC);
			/* skip HTC control field */
			pRxBlk->pData += 4;
			pRxBlk->DataSize -= 4;
		}
	}

	/* 4. skip HW padding */
	if (pRxInfo->L2PAD)
	{
		/* just move pData pointer because DataSize excluding HW padding */
		RX_BLK_SET_FLAG(pRxBlk, fRX_PAD);
		pRxBlk->pData += 2;
	}

	if (pRxInfo->BA)
	{
		RX_BLK_SET_FLAG(pRxBlk, fRX_AMPDU);

		/* incremented by the number of MPDUs */
		/* received in the A-MPDU when an A-MPDU is received. */
		pCounter->MPDUInReceivedAMPDUCount.u.LowPart ++;
	}

#ifdef SOFT_ENCRYPT
	/* Use software to decrypt the encrypted frame if necessary.
	   If a received "encrypted" unicast packet(its WEP bit as 1)
	   and it's passed to driver with "Decrypted" marked as 0 in RxD. */
	if ((pHeader->FC.Wep == 1) && (pRxInfo->Decrypted == 0))
	{
		if (RTMPSoftDecryptionAction(pAd,
								 	(PUCHAR)pHeader,
									 UserPriority,
									 &pEntry->PairwiseKey,
								 	 pRxBlk->pData,
									 &(pRxBlk->DataSize)) != NDIS_STATUS_SUCCESS)
		{
			RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);
			return;
		}
		/* Record the Decrypted bit as 1 */
		pRxInfo->Decrypted = 1;
	}
#endif /* SOFT_ENCRYPT */

	if (!((pHeader->Frag == 0) && (pFmeCtrl->MoreFrag == 0)))
	{
		/*
			re-assemble the fragmented packets, return complete
			frame (pRxPacket) or NULL
		*/
		bFragment = TRUE;
		pRxPacket = RTMPDeFragmentDataFrame(pAd, pRxBlk);
	}

	if (pRxPacket)
	{
		/* process complete frame */
		if (bFragment && (pFmeCtrl->Wep) && (pEntry->WepStatus == Ndis802_11TKIPEnable))
		{
			pRxBlk->DataSize -= 8; /* Minus MIC length */

			/* For TKIP frame, calculate the MIC value */
			if (APCheckTkipMICValue(pAd, pEntry, pRxBlk) == FALSE)
				return;
		}

		if (pEntry)
		{
			pEntry->RxBytes += pRxBlk->MPDUtotalByteCnt;
			INC_COUNTER64(pEntry->RxPackets);
		}

		APRxDataFrameAnnounce(pAd, pEntry, pRxBlk, FromWhichBSSID);
	}
	else
	{
		/*
			just return because RTMPDeFragmentDataFrame() will release rx
			packet, if packet is fragmented
		*/
		return;
	}
	return;

err:
	/* Increase received error packet counter per BSS */
	if (pFmeCtrl->FrDs == 0 &&
		pRxInfo->U2M &&
		pRxBlk->bss_idx < pAd->ApCfg.BssidNum)
	{
		pAd->ApCfg.MBSSID[pRxBlk->bss_idx].RxDropCount ++;
		pAd->ApCfg.MBSSID[pRxBlk->bss_idx].RxErrorCount ++;
	}

	RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);

	return;
}


#ifdef HDR_TRANS_SUPPORT
VOID APHandleRxDataFrame_Hdr_Trns(
	IN	struct rtmp_adapter *pAd,
	IN	RX_BLK			*pRxBlk)
{
	RXINFO_STRUC *pRxInfo = pRxBlk->pRxInfo;
	RXWI_STRUC *pRxWI = pRxBlk->pRxWI;
	HEADER_802_11 *pHeader = pRxBlk->pHeader;
	struct sk_buff *pRxPacket = pRxBlk->pRxPacket;
	BOOLEAN bFragment = FALSE;
	MAC_TABLE_ENTRY *pEntry = NULL;
	UCHAR FromWhichBSSID = BSS0;
	UCHAR OldPwrMgmt = PWR_ACTIVE;	/* UAPSD AP SUPPORT */
	UCHAR UserPriority = 0;
	FRAME_CONTROL *pFmeCtrl = &pHeader->FC;
	COUNTER_RALINK *pCounter = &pAd->RalinkCounters;
	UCHAR *pData;

//+++Add by shiang for debug
if (0 /*!(pRxInfo->Mcast || pRxInfo->Bcast)*/){
	DBGPRINT(RT_DEBUG_OFF, ("-->%s(%d): Dump Related Info!\n", __FUNCTION__, __LINE__));
	hex_dump("DataFrameHeader", pHeader, 36);
	hex_dump("DataFramePayload", pRxBlk->pTransData , pRxBlk->TransDataSize);
}
//---Add by shiangf for debug

	if (APCheckVaildDataFrame(pAd, pRxBlk) != TRUE)
	{
		goto err;
	}


	/* handle WDS */
	if ((pFmeCtrl->FrDs == 1) && (pFmeCtrl->ToDs == 1))
	{
		do
		{


		} while(FALSE);

		if (pEntry == NULL)
		{
			/* have no WDS or MESH support */
			/* drop the packet */
			goto err;
		}
	}
	/* handle APCLI. */
	else if ((pFmeCtrl->FrDs == 1) && (pFmeCtrl->ToDs == 0))
	{
#ifdef APCLI_SUPPORT
		if (VALID_WCID(pRxBlk->wcid))
			pEntry = &pAd->MacTab.Content[pRxBlk->wcid];
		else
			pEntry = MacTableLookup(pAd, pHeader->Addr2);

		if (pEntry)
		{
			// TODO: shiang, add MAC_REPEATER_SUPPORT code here!!
			if (!(pEntry && APCLI_IF_UP_CHECK(pAd, pEntry->wdev_idx)))
			{
				goto err;
			}
			FromWhichBSSID = pEntry->wdev_idx + MIN_NET_DEVICE_FOR_APCLI;
			RX_BLK_SET_FLAG(pRxBlk, fRX_APCLI);

			/* Process broadcast packets */
			if (pRxInfo->Mcast || pRxInfo->Bcast)
			{
				/* Process the received broadcast frame for AP-Client. */
				if (!ApCliHandleRxBroadcastFrame(pAd, pRxBlk, pEntry, FromWhichBSSID))
				{
					/* release packet */
					RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);
				}
				return;
			}
		}
		else
#endif /* APCLI_SUPPORT */
		{
			/* no APCLI support */
			/* release packet */
			goto err;
		}
	}
	else
	{
		pEntry = PACInquiry(pAd, pRxBlk->wcid);

		/*	can't find associated STA entry then filter invlid data frame */
		if (!pEntry)
		{
			goto err;
		}

		FromWhichBSSID = pEntry->apidx;

#ifdef STATS_COUNT_SUPPORT
		/* Increase received byte counter per BSS */
		if (pHeader->FC.FrDs == 0 &&
			pRxInfo->U2M &&
			FromWhichBSSID < pAd->ApCfg.BssidNum)
		{
			MULTISSID_STRUCT *pMbss = pEntry->pMbss;
			if (pMbss != NULL)
			{
				pMbss->ReceivedByteCount += pRxBlk->MPDUtotalByteCnt;
				pMbss->RxCount ++;
			}
		}

		/* update multicast counter */
                if (IS_MULTICAST_MAC_ADDR(pHeader->Addr3))
                        INC_COUNTER64(pAd->WlanCounters.MulticastReceivedFrameCount);
#endif /* STATS_COUNT_SUPPORT */
	}

	ASSERT(pEntry->Aid == pRxBlk->wcid);



#ifdef DOT11_N_SUPPORT
	/* check Atheros Client */
	// TODO: shiang@PF#2, is this atheros protection still necessary here????
	if (!pEntry->bIAmBadAtheros && (pFmeCtrl->Retry) &&
		(pRxBlk->rx_rate.field.MODE < MODE_VHT) &&
		(pRxInfo->AMPDU == 1) && (pAd->CommonCfg.bHTProtect == TRUE)
	)
	{
		if (pAd->CommonCfg.IOTestParm.bRTSLongProtOn == FALSE)
			RTMP_UPDATE_PROTECT(pAd, 8 , ALLN_SETPROTECT, FALSE, FALSE);
		pEntry->bIAmBadAtheros = TRUE;

	}
#endif /* DOT11_N_SUPPORT */

   	/* update rssi sample */
   	Update_Rssi_Sample(pAd, &pEntry->RssiSample, pRxWI);

	if (pRxInfo->U2M)
	{
		pEntry->LastRxRate = pRxBlk->rx_rate.word;

#ifdef TXBF_SUPPORT
		if (pRxBlk->rx_rate.field.ShortGI)
			pEntry->OneSecRxSGICount++;
		else
			pEntry->OneSecRxLGICount++;
#endif // TXBF_SUPPORT //
	}

	pAd->ApCfg.LastSNR0 = (UCHAR)(pRxBlk->snr[0]);
	pAd->ApCfg.LastSNR1 = (UCHAR)(pRxBlk->snr[1]);
#ifdef DOT11N_SS3_SUPPORT
	pAd->ApCfg.LastSNR2 = (UCHAR)(pRxBlk->snr[2]);
#endif /* DOT11N_SS3_SUPPORT */
	pEntry->freqOffset = (CHAR)(pRxBlk->freq_offset);
	pEntry->freqOffsetValid = TRUE;


   	/* Gather PowerSave information from all valid DATA frames. IEEE 802.11/1999 p.461 */
   	/* must be here, before no DATA check */


	pData = (UCHAR *)pHeader;


   	/* 1: PWR_SAVE, 0: PWR_ACTIVE */
   	OldPwrMgmt = RtmpPsIndicate(pAd, pHeader->Addr2, pEntry->Aid, pFmeCtrl->PwrMgmt);
#ifdef UAPSD_SUPPORT
	if (pFmeCtrl->PwrMgmt)
	{
	   	if ((CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_APSD_CAPABLE)) &&
			(pFmeCtrl->SubType & 0x08))
	   	{
			/*
				In IEEE802.11e, 11.2.1.4 Power management with APSD,
				If there is no unscheduled SP in progress, the unscheduled SP begins
				when the QAP receives a trigger frame from a non-AP QSTA, which is a
				QoS data or QoS Null frame associated with an AC the STA has
				configured to be trigger-enabled.
			*/
			/*
				In WMM v1.1, A QoS Data or QoS Null frame that indicates transition
				to/from Power Save Mode is not considered to be a Trigger Frame and
				the AP shall not respond with a QoS Null frame.
			*/
			/* Trigger frame must be QoS data or QoS Null frame */
	   		UCHAR  OldUP;

			OldUP = (*(pData+LENGTH_802_11) & 0x07);
	    	if (OldPwrMgmt == PWR_SAVE)
	    		UAPSD_TriggerFrameHandle(pAd, pEntry, OldUP);
	    	/* End of if */
		}
    } /* End of if */
#endif /* UAPSD_SUPPORT */

	/* Drop NULL, CF-ACK(no data), CF-POLL(no data), and CF-ACK+CF-POLL(no data) data frame */
	if ((pFmeCtrl->SubType & 0x04) && (pFmeCtrl->Order == 0)) /* bit 2 : no DATA */
	{
		/* Increase received drop packet counter per BSS */
		if (pFmeCtrl->FrDs == 0 &&
			pRxInfo->U2M &&
			pRxBlk->bss_idx < pAd->ApCfg.BssidNum)
		{
			pAd->ApCfg.MBSSID[pRxBlk->bss_idx].RxDropCount++;
		}

		/* release packet */
		RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);
		return;
	}

	/*
		update RxBlk->pData, DataSize
		802.11 Header, QOS, HTC, Hw Padding
	*/

	/* 1. skip 802.11 HEADER */
	{
		pData += LENGTH_802_11;
	}

	/* 2. QOS */
	if (pFmeCtrl->SubType & 0x08)
	{
		RX_BLK_SET_FLAG(pRxBlk, fRX_QOS);
		UserPriority = *(pData) & 0x0f;


		/* count packets priroity more than BE */
		detect_wmm_traffic(pAd, UserPriority, 0);
		/* bit 7 in QoS Control field signals the HT A-MSDU format */
		if ((*pData) & 0x80)
		{
			RX_BLK_SET_FLAG(pRxBlk, fRX_AMSDU);

		}

		/* skip QOS contorl field */
		pData += 2;
	}
	pRxBlk->UserPriority = UserPriority;

#ifdef TXBF_SUPPORT
	if (pAd->chipCap.FlgHwTxBfCap &&
		(pHeader->FC.SubType & 0x08) && pHeader->FC.Order)
	{
		handleHtcField(pAd, pRxBlk);
	}
#endif /* TXBF_SUPPORT */

	/* 3. Order bit: A-Ralink or HTC+ */
	if (pFmeCtrl->Order)
	{
#ifdef AGGREGATION_SUPPORT
		if (
#ifdef DOT11_N_SUPPORT
			(pRxBlk->rx_rate.field.MODE < MODE_HTMIX) &&
#endif /* DOT11_N_SUPPORT */
			(CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_AGGREGATION_CAPABLE))
		)
		{
			RX_BLK_SET_FLAG(pRxBlk, fRX_ARALINK);
		}
		else
#endif
		{
			RX_BLK_SET_FLAG(pRxBlk, fRX_HTC);
			/* skip HTC control field */
			pData += 4;
		}
	}

	/* 4. skip HW padding */
	if (pRxInfo->L2PAD)
	{
		/* just move pData pointer */
		/* because DataSize excluding HW padding */
		RX_BLK_SET_FLAG(pRxBlk, fRX_PAD);
		pData += 2;
	}

	if (pRxInfo->BA)
	{
		RX_BLK_SET_FLAG(pRxBlk, fRX_AMPDU);

		/* incremented by the number of MPDUs */
		/* received in the A-MPDU when an A-MPDU is received. */
		pCounter->MPDUInReceivedAMPDUCount.u.LowPart ++;
	}

#ifdef SOFT_ENCRYPT
	/* Use software to decrypt the encrypted frame if necessary.
	   If a received "encrypted" unicast packet(its WEP bit as 1)
	   and it's passed to driver with "Decrypted" marked as 0 in RxD. */
	if ((pHeader->FC.Wep == 1) && (pRxInfo->Decrypted == 0))
	{
		if (RTMPSoftDecryptionAction(pAd,
								 	(PUCHAR)pHeader,
									 UserPriority,
									 &pEntry->PairwiseKey,
								 	 pRxBlk->pTransData + 14,
									 &(pRxBlk->TransDataSize)) != NDIS_STATUS_SUCCESS)
		{
			/* release packet */
			RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);
			return;
		}
		/* Record the Decrypted bit as 1 */
		pRxInfo->Decrypted = 1;
	}
#endif /* SOFT_ENCRYPT */

	if (!((pHeader->Frag == 0) && (pFmeCtrl->MoreFrag == 0)))
	{
		/* re-assemble the fragmented packets */
		/* return complete frame (pRxPacket) or NULL */
		bFragment = TRUE;
		pRxPacket = RTMPDeFragmentDataFrame(pAd, pRxBlk);
	}

	if (pRxPacket)
	{
		/* process complete frame */
		if (bFragment && (pFmeCtrl->Wep) && (pEntry->WepStatus == Ndis802_11TKIPEnable))
		{
			/* Minus MIC length */
			pRxBlk->DataSize -= 8;

			/* For TKIP frame, calculate the MIC value */
			if (APCheckTkipMICValue(pAd, pEntry, pRxBlk) == FALSE)
			{
				return;
			}
		}

		if (pEntry)
		{
			pEntry->RxBytes+=pRxBlk->MPDUtotalByteCnt;
			INC_COUNTER64(pEntry->RxPackets);
		}
		APRxDataFrameAnnounce_Hdr_Trns(pAd, pEntry, pRxBlk, FromWhichBSSID);
	}
	else
	{
		/* just return */
		/* because RTMPDeFragmentDataFrame() will release rx packet, */
		/* if packet is fragmented */
		return;
	}
	return;

err:
	/* Increase received error packet counter per BSS */
	if (pFmeCtrl->FrDs == 0 &&
		pRxInfo->U2M &&
		pRxBlk->bss_idx < pAd->ApCfg.BssidNum)
	{
		pAd->ApCfg.MBSSID[pRxBlk->bss_idx].RxDropCount ++;
		pAd->ApCfg.MBSSID[pRxBlk->bss_idx].RxErrorCount ++;
	}

	/* release packet */
	RELEASE_NDIS_PACKET(pAd, pRxPacket, NDIS_STATUS_FAILURE);
	return;

}
#endif /* HDR_TRANS_SUPPORT */


BOOLEAN APFowardWirelessStaToWirelessSta(
	IN	struct rtmp_adapter *pAd,
	IN	struct sk_buff *pPacket,
	IN	ULONG FromWhichBSSID)
{
	MAC_TABLE_ENTRY *pEntry = NULL;
	BOOLEAN bAnnounce, bDirectForward;
	UCHAR *pHeader802_3;
	struct sk_buff *pForwardPacket;


#ifdef APCLI_SUPPORT
	/* have no need to forwad the packet to WM */
	if (FromWhichBSSID >= MIN_NET_DEVICE_FOR_APCLI)
		/* need annouce to upper layer */
		return TRUE;
	else
#endif /* APCLI_SUPPORT */

	pEntry = NULL;
	bAnnounce = TRUE;
	bDirectForward = FALSE;

	pHeader802_3 = GET_OS_PKT_DATAPTR(pPacket);

	if (pHeader802_3[0] & 0x01)
	{
		/*
		 * In the case, the BSS have only one STA behind.
		 * AP have no necessary to forward the M/Bcase packet back to STA again.
		*/
		if (
			((FromWhichBSSID < MAX_MBSSID_NUM(pAd)) &&
			(FromWhichBSSID < HW_BEACON_MAX_NUM) &&
			(pAd->ApCfg.MBSSID[FromWhichBSSID].StaCount > 1)))
		{
			if (pAd->ApCfg.MBSSID[FromWhichBSSID].IsolateInterStaMBCast == 0)
			{
				bDirectForward  = TRUE;
			}
		}

		/* tell caller to deliver the packet to upper layer */
		bAnnounce = TRUE;
	}
	else
	{
		/* if destinated STA is a associated wireless STA */
		pEntry = MacTableLookup(pAd, pHeader802_3);
		if (pEntry && (pEntry->Sst == SST_ASSOC) && IS_ENTRY_CLIENT(pEntry))
		{
			bDirectForward = TRUE;
			bAnnounce = FALSE;

			if (FromWhichBSSID == pEntry->apidx)
			{/* STAs in same SSID */
				if ((pAd->ApCfg.MBSSID[pEntry->apidx].IsolateInterStaTraffic == 1))
				{
					/* release the packet */
					bDirectForward = FALSE;
					bAnnounce = FALSE;
				}
			}
			else
			{/* STAs in different SSID */
				if (pAd->ApCfg.IsolateInterStaTrafficBTNBSSID == 1 ||
					((FromWhichBSSID < MAX_MBSSID_NUM(pAd)) &&
					(FromWhichBSSID < HW_BEACON_MAX_NUM) &&
					(pAd->ApCfg.MBSSID[pEntry->apidx].wdev.VLAN_VID != pAd->ApCfg.MBSSID[FromWhichBSSID].wdev.VLAN_VID)))
					/* destination VLAN ID != source VLAN ID */
				{
					/*
						Do not need to care WDS mode because packets from a
						WDS interface will be passed to upper layer for bridging.
					*/
					bDirectForward = FALSE;
					bAnnounce = FALSE;
				}
			}
		}
		else
		{
			/* announce this packet to upper layer (bridge) */
			bDirectForward = FALSE;
			bAnnounce = TRUE;
		}
	}

	if (bDirectForward)
	{
		/* build an NDIS packet */
		pForwardPacket = RTMP_DUPLICATE_PACKET(pAd, pPacket, FromWhichBSSID);

		if (pForwardPacket == NULL)
		{
			return bAnnounce;
		}

		{
			/* 1.1 apidx != 0, then we need set packet mbssid attribute. */
			RTMP_SET_PACKET_NET_DEVICE_MBSSID(pForwardPacket, MAIN_MBSSID);	/* set a default value */
			if (pEntry && (pEntry->apidx != 0))
			{
				RTMP_SET_PACKET_NET_DEVICE_MBSSID(pForwardPacket, pEntry->apidx);
                            if ( pEntry && pEntry->wcid != MCAST_WCID)
                            {
                                    RTMP_SET_PACKET_WDEV(pPacket, pEntry->wdev->wdev_idx);
                            }
			}

			/* send bc/mc frame back to the same bss */
			if (!pEntry)
			{
				RTMP_SET_PACKET_NET_DEVICE_MBSSID(pForwardPacket, FromWhichBSSID);


                            //also need to send back to same bss
                            if ((FromWhichBSSID >= 0) &&
                                    (FromWhichBSSID < pAd->ApCfg.BssidNum) &&
                                            (FromWhichBSSID < MAX_MBSSID_NUM(pAd)) &&
                                            (FromWhichBSSID < HW_BEACON_MAX_NUM))
                            {
                                    RTMP_SET_PACKET_WDEV(pForwardPacket, pAd->ApCfg.MBSSID[FromWhichBSSID].wdev.wdev_idx);
                            }
			}

			RTMP_SET_PACKET_WCID(pForwardPacket, pEntry ? pEntry->wcid : MCAST_WCID);
			RTMP_SET_PACKET_MOREDATA(pForwardPacket, FALSE);

			APSendPacket(pAd, pForwardPacket);
		}

		/* Dequeue outgoing frames from TxSwQueue0..3 queue and process it */
		RTMPDeQueuePacket(pAd, FALSE, NUM_OF_TX_RING, MAX_TX_PROCESS);
	}

	return bAnnounce;
}

/*
	========================================================================
	Routine Description:
		This routine is used to do insert packet into power-saveing queue.

	Arguments:
		pAd: Pointer to our adapter
		pPacket: Pointer to send packet
		pMacEntry: portint to entry of MacTab. the pMacEntry store attribute of client (STA).
		QueIdx: Priority queue idex.

	Return Value:
		NDIS_STATUS_SUCCESS:If succes to queue the packet into TxSwQueue.
		NDIS_STATUS_FAILURE: If failed to do en-queue.
========================================================================
*/
int APInsertPsQueue(
	IN struct rtmp_adapter *pAd,
	IN struct sk_buff *pPacket,
	IN MAC_TABLE_ENTRY *pMacEntry,
	IN UCHAR QueIdx)
{
	ULONG IrqFlags;
#ifdef UAPSD_SUPPORT
	/* put the U-APSD packet to its U-APSD queue by AC ID */
	uint32_t ac_id = QueIdx - QID_AC_BE; /* should be >= 0 */


	if (UAPSD_MR_IS_UAPSD_AC(pMacEntry, ac_id))
		UAPSD_PacketEnqueue(pAd, pMacEntry, pPacket, ac_id);
	else
#endif /* UAPSD_SUPPORT */
	{
		if (pMacEntry->PsQueue.Number >= MAX_PACKETS_IN_PS_QUEUE)
		{
			RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_FAILURE);
			return NDIS_STATUS_FAILURE;
		}
		else
		{
			RTMP_IRQ_LOCK(&pAd->irq_lock, IrqFlags);
			InsertTailQueue(&pMacEntry->PsQueue, PACKET_TO_QUEUE_ENTRY(pPacket));
			RTMP_IRQ_UNLOCK(&pAd->irq_lock, IrqFlags);
		}
	}

	/* mark corresponding TIM bit in outgoing BEACON frame */
#ifdef UAPSD_SUPPORT
	if (UAPSD_MR_IS_NOT_TIM_BIT_NEEDED_HANDLED(pMacEntry, QueIdx))
	{
		/*
			1. the station is UAPSD station;
			2. one of AC is non-UAPSD (legacy) AC;
			3. the destinated AC of the packet is UAPSD AC.
			So we can not set TIM bit due to one of AC is legacy AC
		*/
	}
	else
#endif /* UAPSD_SUPPORT */
	{
		WLAN_MR_TIM_BIT_SET(pAd, pMacEntry->apidx, pMacEntry->Aid);
	}
	return NDIS_STATUS_SUCCESS;
}

#ifdef APCLI_SUPPORT
VOID ApCliRTMPSendNullFrame(
	IN	struct rtmp_adapter *pAd,
	IN	UCHAR			TxRate,
	IN	BOOLEAN 		bQosNull,
	IN 	PMAC_TABLE_ENTRY pMacEntry,
	IN 	USHORT		PwrMgmt)
{
	UCHAR	NullFrame[48];
	ULONG	Length;
	PHEADER_802_11	pHeader_802_11;
	PAPCLI_STRUCT pApCliEntry = NULL;
	struct rtmp_wifi_dev *wdev;

	pApCliEntry = &pAd->ApCfg.ApCliTab[pMacEntry->wdev_idx];
	wdev = &pApCliEntry->wdev;

    /* WPA 802.1x secured port control */
    if (((wdev->AuthMode == Ndis802_11AuthModeWPA) ||
         (wdev->AuthMode == Ndis802_11AuthModeWPAPSK) ||
         (wdev->AuthMode == Ndis802_11AuthModeWPA2) ||
         (wdev->AuthMode == Ndis802_11AuthModeWPA2PSK) ||
         (wdev->IEEE8021X == TRUE)
        ) &&
       (pMacEntry->PortSecured == WPA_802_1X_PORT_NOT_SECURED))
	{
		return;
	}

	memset(NullFrame, 0, 48);
	Length = sizeof(HEADER_802_11);

	pHeader_802_11 = (PHEADER_802_11) NullFrame;

	pHeader_802_11->FC.Type = FC_TYPE_DATA;
	pHeader_802_11->FC.SubType = SUBTYPE_DATA_NULL;
	pHeader_802_11->FC.ToDs = 1;

	COPY_MAC_ADDR(pHeader_802_11->Addr1, pMacEntry->Addr);
		COPY_MAC_ADDR(pHeader_802_11->Addr2, pApCliEntry->wdev.if_addr);
	COPY_MAC_ADDR(pHeader_802_11->Addr3, pMacEntry->Addr);

	if (pAd->CommonCfg.bAPSDForcePowerSave)
		pHeader_802_11->FC.PwrMgmt = PWR_SAVE;
	else
		pHeader_802_11->FC.PwrMgmt = PwrMgmt;

	pHeader_802_11->Duration = pAd->CommonCfg.Dsifs + RTMPCalcDuration(pAd, TxRate, 14);

	/* sequence is increased in MlmeHardTx */
	pHeader_802_11->Sequence = pAd->Sequence;
	pAd->Sequence = (pAd->Sequence+1) & MAXSEQ; /* next sequence  */

	/* Prepare QosNull function frame */
	if (bQosNull)
	{
		pHeader_802_11->FC.SubType = SUBTYPE_QOS_NULL;

		/* copy QOS control bytes */
		NullFrame[Length]	=  0;
		NullFrame[Length+1] =  0;
		Length += 2;/* if pad with 2 bytes for alignment, APSD will fail */
	}

	HAL_KickOutNullFrameTx(pAd, 0, NullFrame, Length);

}
#endif/*APCLI_SUPPORT*/


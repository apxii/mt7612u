/****************************************************************************
 * Ralink Tech Inc.
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2010, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************/

/****************************************************************************

    Abstract:

	All related POWER SAVE function body.

***************************************************************************/

#include "rt_config.h"


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
int RtmpInsertPsQueue(
	struct rtmp_adapter *pAd,
	struct sk_buff *pPacket,
	MAC_TABLE_ENTRY *pMacEntry,
	u8 QueIdx)
{
	{
		if (pMacEntry->PsQueue.Number >= MAX_PACKETS_IN_PS_QUEUE)
		{
			dev_kfree_skb_any(pPacket);
			return NDIS_STATUS_FAILURE;
		}
		else
		{
			DBGPRINT(RT_DEBUG_TRACE, ("legacy ps> queue a packet!\n"));
			spin_lock_bh(&pAd->irq_lock);
			InsertTailQueue(&pMacEntry->PsQueue, PACKET_TO_QUEUE_ENTRY(pPacket));
			spin_unlock_bh(&pAd->irq_lock);
		}
	}

#ifdef CONFIG_AP_SUPPORT
	/* mark corresponding TIM bit in outgoing BEACON frame */
	{
		WLAN_MR_TIM_BIT_SET(pAd, pMacEntry->apidx, pMacEntry->Aid);

	}
#endif /* CONFIG_AP_SUPPORT */

	return NDIS_STATUS_SUCCESS;
}


/*
	==========================================================================
	Description:
		This routine is used to clean up a specified power-saving queue. It's
		used whenever a wireless client is deleted.
	==========================================================================
 */
VOID RtmpCleanupPsQueue(struct rtmp_adapter *pAd, QUEUE_HEADER *pQueue)
{
	QUEUE_ENTRY *pQEntry;
	struct sk_buff *pPacket;

	DBGPRINT(RT_DEBUG_TRACE, ("RtmpCleanupPsQueue (0x%08lx)...\n", (ULONG)pQueue));

	while (pQueue->Head)
	{
		DBGPRINT(RT_DEBUG_TRACE,
					("RtmpCleanupPsQueue %d...\n",pQueue->Number));

		pQEntry = RemoveHeadQueue(pQueue);
		/*pPacket = CONTAINING_RECORD(pEntry, NDIS_PACKET, MiniportReservedEx); */
		pPacket = QUEUE_ENTRY_TO_PACKET(pQEntry);
		dev_kfree_skb_any(pPacket);

		DBGPRINT(RT_DEBUG_TRACE, ("RtmpCleanupPsQueue pkt = %lx...\n", (ULONG)pPacket));
	}
}


/*
  ========================================================================
  Description:
	This routine frees all packets in PSQ that's destined to a specific DA.
	BCAST/MCAST in DTIMCount=0 case is also handled here, just like a PS-POLL
	is received from a WSTA which has MAC address FF:FF:FF:FF:FF:FF
  ========================================================================
*/
VOID RtmpHandleRxPsPoll(struct rtmp_adapter *pAd, u8 *pAddr, unsigned short wcid, bool isActive)
{
	QUEUE_ENTRY *pQEntry;
	MAC_TABLE_ENTRY *pMacEntry;

	/*
	DBGPRINT(RT_DEBUG_TRACE, ("rcv PS-POLL (AID=%d) from %02x:%02x:%02x:%02x:%02x:%02x\n",
				Aid, PRINT_MAC(pAddr)));
	*/

	pMacEntry = &pAd->MacTab.Content[wcid];
	if (RTMPEqualMemory(pMacEntry->Addr, pAddr, MAC_ADDR_LEN))
	{
		/* Sta is change to Power Active stat. Reset ContinueTxFailCnt */
		pMacEntry->ContinueTxFailCnt = 0;


		spin_lock_bh(&pAd->irq_lock);
		if (isActive == false)
		{
			if (pMacEntry->PsQueue.Head)
			{
				struct sk_buff *skb;

				pQEntry = RemoveHeadQueue(&pMacEntry->PsQueue);
				skb = QUEUE_ENTRY_TO_PACKET(pQEntry);

				if ( pMacEntry->PsQueue.Number >=1 ) {
					RTMP_SET_PACKET_MOREDATA(skb, true);
				}
				InsertTailQueueAc(pAd, pMacEntry, &pAd->TxSwQueue[QID_AC_BE], pQEntry);

			}
			else
			{
				/*
					or transmit a (QoS) Null Frame;

					In addtion, in Station Keep Alive mechanism, we need to
					send a QoS Null frame to detect the station live status.
				*/
				bool bQosNull = false;

				if (CLIENT_STATUS_TEST_FLAG(pMacEntry, fCLIENT_STATUS_WMM_CAPABLE))
					bQosNull = true;

				RtmpEnqueueNullFrame(pAd, pMacEntry->Addr, pMacEntry->CurrTxRate,
										pMacEntry->Aid, pMacEntry->apidx,
										bQosNull, true, 0);
			}
		}
		else
		{

			while(pMacEntry->PsQueue.Head)
			{
				pQEntry = RemoveHeadQueue(&pMacEntry->PsQueue);
				InsertTailQueueAc(pAd, pMacEntry, &pAd->TxSwQueue[QID_AC_BE], pQEntry);
			}
		}

		if ((pMacEntry->Aid > 0) && (pMacEntry->Aid < MAX_LEN_OF_MAC_TABLE) &&
			(pMacEntry->PsQueue.Number == 0))
		{
			/* clear corresponding TIM bit because no any PS packet */
#ifdef CONFIG_AP_SUPPORT
			if(pMacEntry->wdev->wdev_type == WDEV_TYPE_AP)
			{
				WLAN_MR_TIM_BIT_CLEAR(pAd, pMacEntry->apidx, pMacEntry->Aid);
			}
#endif /* CONFIG_AP_SUPPORT */
			pMacEntry->PsQIdleCount = 0;
		}

		spin_unlock_bh(&pAd->irq_lock);

		/*
			Dequeue outgoing frames from TxSwQueue0..3 queue and process it
			TODO: 2004-12-27 it's not a good idea to handle "More Data" bit here.
				because the RTMPDeQueue process doesn't guarantee to de-queue the
				desired MSDU from the corresponding TxSwQueue/PsQueue when QOS
				in-used. We should consider "HardTransmt" this MPDU using MGMT
				queue or things like that.
		*/
		RTMPDeQueuePacket(pAd, false, NUM_OF_TX_RING, MAX_TX_PROCESS);
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR,("rcv PS-POLL (AID=%d not match) from %02x:%02x:%02x:%02x:%02x:%02x\n",
			  pMacEntry->Aid, PRINT_MAC(pAddr)));

	}
}


/*
	==========================================================================
	Description:
		Update the station current power save mode. Calling this routine also
		prove the specified client is still alive. Otherwise AP will age-out
		this client once IdleCount exceeds a threshold.
	==========================================================================
 */
bool RtmpPsIndicate(struct rtmp_adapter *pAd, u8 *pAddr, u8 wcid, u8 Psm)
{
	MAC_TABLE_ENTRY *pEntry;
	u8 old_psmode;

	if (wcid >= MAX_LEN_OF_MAC_TABLE)
		return PWR_ACTIVE;

	pEntry = &pAd->MacTab.Content[wcid];
	old_psmode = pEntry->PsMode;

		/*
			Change power save mode first because we will call
			RTMPDeQueuePacket() in RtmpHandleRxPsPoll().

			Or when Psm = PWR_ACTIVE, we will not do Aggregation in
			RTMPDeQueuePacket().
		*/
		pEntry->NoDataIdleCount = 0;
		pEntry->PsMode = Psm;

		if (old_psmode != Psm) {
			DBGPRINT(RT_DEBUG_INFO, ("%s():%02x:%02x:%02x:%02x:%02x:%02x %s!\n",
					__FUNCTION__, PRINT_MAC(pAddr),
					(Psm == PWR_SAVE ? "Sleep" : "wakes up, act like rx PS-POLL")));
		}

		if ((old_psmode == PWR_SAVE) && (Psm == PWR_ACTIVE))
		{

			/* sleep station awakes, move all pending frames from PSQ to TXQ if any */
			RtmpHandleRxPsPoll(pAd, pAddr, pEntry->wcid, true);
		}

	return old_psmode;
}


#ifdef CONFIG_STA_SUPPORT
/*
========================================================================
Routine Description:
    Check if PM of any packet is set.

Arguments:
	pAd		Pointer to our adapter

Return Value:
    true	can set
	false	can not set

Note:
========================================================================
*/
bool RtmpPktPmBitCheck(struct rtmp_adapter *pAd)
{
	bool FlgCanPmBitSet = true;


	if (FlgCanPmBitSet == true)
		return (pAd->StaCfg.Psm == PWR_SAVE);

	return false;
}


VOID RtmpPsActiveExtendCheck(struct rtmp_adapter *pAd)
{
	/* count down the TDLS active counter */
}


VOID RtmpPsModeChange(struct rtmp_adapter *pAd, uint32_t PsMode)
{
	if (pAd->StaCfg.BssType == BSS_INFRA)
	{
		/* reset ps mode */
		if (PsMode == Ndis802_11PowerModeMAX_PSP)
		{
			// do NOT turn on PSM bit here, wait until MlmeCheckForPsmChange()
			// to exclude certain situations.
			//	   MlmeSetPsm(pAd, PWR_SAVE);
			OPSTATUS_SET_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM);
			if (pAd->StaCfg.bWindowsACCAMEnable == false)
				pAd->StaCfg.WindowsPowerMode = Ndis802_11PowerModeMAX_PSP;
			pAd->StaCfg.WindowsBatteryPowerMode = Ndis802_11PowerModeMAX_PSP;
			pAd->StaCfg.DefaultListenCount = 5;
		}
		else if (PsMode == Ndis802_11PowerModeFast_PSP)
		{
			// do NOT turn on PSM bit here, wait until MlmeCheckForPsmChange()
			// to exclude certain situations.
			OPSTATUS_SET_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM);
			if (pAd->StaCfg.bWindowsACCAMEnable == false)
				pAd->StaCfg.WindowsPowerMode = Ndis802_11PowerModeFast_PSP;
			pAd->StaCfg.WindowsBatteryPowerMode = Ndis802_11PowerModeFast_PSP;
			pAd->StaCfg.DefaultListenCount = 3;
		}
		else if (PsMode == Ndis802_11PowerModeLegacy_PSP)
		{
			// do NOT turn on PSM bit here, wait until MlmeCheckForPsmChange()
			// to exclude certain situations.
			OPSTATUS_SET_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM);
			if (pAd->StaCfg.bWindowsACCAMEnable == false)
				pAd->StaCfg.WindowsPowerMode = Ndis802_11PowerModeLegacy_PSP;
			pAd->StaCfg.WindowsBatteryPowerMode = Ndis802_11PowerModeLegacy_PSP;
			pAd->StaCfg.DefaultListenCount = 3;
		}
		else
		{ //Default Ndis802_11PowerModeCAM
			// clear PSM bit immediately
			RTMP_SET_PSM_BIT(pAd, PWR_ACTIVE);
			OPSTATUS_SET_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM);
			if (pAd->StaCfg.bWindowsACCAMEnable == false)
				pAd->StaCfg.WindowsPowerMode = Ndis802_11PowerModeCAM;
			pAd->StaCfg.WindowsBatteryPowerMode = Ndis802_11PowerModeCAM;
		}

		/* change ps mode */
		RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, true, false);

		DBGPRINT(RT_DEBUG_TRACE, ("PSMode=%ld\n", pAd->StaCfg.WindowsPowerMode));
	}
}
#endif /* CONFIG_STA_SUPPORT */


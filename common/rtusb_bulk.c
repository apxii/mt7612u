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
	rtusb_bulk.c

	Abstract:

	Revision History:
	Who			When		What
	--------	----------	----------------------------------------------
	Name		Date		Modification logs
	Paul Lin	06-25-2004	created

*/



#include	"rt_config.h"

VOID	RTUSBInitTxDesc(
	IN	struct rtmp_adapter *pAd,
	IN	PTX_CONTEXT		pTxContext,
	IN	u8 		BulkOutPipeId,
	IN	usb_complete_t	Func)
{
	struct urb *urb;
	u8 *pSrc = NULL;
	struct usb_device *udev = mt7612u_to_usb_dev(pAd);
	struct rtmp_chip_cap *pChipCap = &pAd->chipCap;

	urb = pTxContext->pUrb;

	/* Store BulkOut PipeId*/
	pTxContext->BulkOutPipeId = BulkOutPipeId;

	pSrc = (u8 *) pTxContext->TransferBuffer->field.WirelessPacket;

	usb_fill_bulk_urb(urb, udev,
			  usb_sndbulkpipe(udev, pChipCap->WMM0ACBulkOutAddr[BulkOutPipeId]),
			  pSrc, pTxContext->BulkOutSize,
			  Func, pTxContext);

	urb->transfer_dma = pTxContext->data_dma;
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
}

VOID	RTUSBInitHTTxDesc(
	IN	struct rtmp_adapter *pAd,
	IN	PHT_TX_CONTEXT	pTxContext,
	IN	u8 		BulkOutPipeId,
	IN	ULONG			BulkOutSize,
	IN	usb_complete_t	Func)
{
	struct urb *urb;
	u8 *pSrc = NULL;
	struct usb_device *udev = mt7612u_to_usb_dev(pAd);
	struct rtmp_chip_cap *pChipCap = &pAd->chipCap;

	urb = pTxContext->pUrb;

	/* Store BulkOut PipeId*/
	pTxContext->BulkOutPipeId = BulkOutPipeId;

	pSrc = &pTxContext->TransferBuffer->field.WirelessPacket[pTxContext->NextBulkOutPosition];

	usb_fill_bulk_urb(urb, udev,
			  usb_sndbulkpipe(udev, pChipCap->WMM0ACBulkOutAddr[BulkOutPipeId]),
			  pSrc, BulkOutSize, Func, pTxContext);
	urb->transfer_dma = pTxContext->data_dma + pTxContext->NextBulkOutPosition;
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
}

VOID	RTUSBInitRxDesc(
	IN	struct rtmp_adapter *pAd,
	IN	PRX_CONTEXT		pRxContext)
{
	struct urb *urb;
	struct usb_device *udev = mt7612u_to_usb_dev(pAd);
	ULONG				RX_bulk_size;
	struct rtmp_chip_cap *pChipCap = &pAd->chipCap;

	urb = pRxContext->pUrb;

	if ( pAd->in_max_packet == 64)
		RX_bulk_size = 4096;
	else
		RX_bulk_size = MAX_RXBULK_SIZE;

	usb_fill_bulk_urb(urb, udev,
			  usb_rcvbulkpipe(udev, pChipCap->DataBulkInAddr),
			  &(pRxContext->TransferBuffer[pAd->NextRxBulkInPosition]), RX_bulk_size - (pAd->NextRxBulkInPosition),
			  RtmpUsbBulkRxComplete, pRxContext);
	urb->transfer_dma = pRxContext->data_dma + pAd->NextRxBulkInPosition;
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
}




/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/

VOID RTUSBBulkOutDataPacket(struct rtmp_adapter *pAd, u8 BulkOutPipeId, u8 Index)
{
	PHT_TX_CONTEXT pHTTXContext;
	struct urb *pUrb;
	int ret = 0;
	struct mt7612_txinfo_pkt *txinfo, *pLastTxInfo = NULL;
	struct mt7612u_txwi *txwi;
	unsigned short txwi_pkt_len = 0;
	u8 ampdu = 0, phy_mode = 0, pid;
	ULONG TmpBulkEndPos, ThisBulkSize;
	u8 *pWirelessPkt, *pAppendant;
	uint32_t aggregation_num = 0;
	bool	 bTxQLastRound = false;
	u8 allzero[4]= {0x0,0x0,0x0,0x0};

	spin_lock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
	if ((pAd->BulkOutPending[BulkOutPipeId] == true) || RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NEED_STOP_TX))
	{
		spin_unlock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
		return;
	}
	pAd->BulkOutPending[BulkOutPipeId] = true;

	if (((!OPSTATUS_TEST_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED)) &&
		( !OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED)))
		)
	{
		pAd->BulkOutPending[BulkOutPipeId] = false;
		spin_unlock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
		return;
	}
	spin_unlock_bh(&pAd->BulkOutLock[BulkOutPipeId]);


	pHTTXContext = &(pAd->TxContext[BulkOutPipeId]);

	spin_lock_bh(&pAd->TxContextQueueLock[BulkOutPipeId]);
	if ((pHTTXContext->ENextBulkOutPosition == pHTTXContext->CurWritePosition)
		|| ((pHTTXContext->ENextBulkOutPosition-8) == pHTTXContext->CurWritePosition)
		)  /* druing writing. */
	{
		spin_unlock_bh(&pAd->TxContextQueueLock[BulkOutPipeId]);

		spin_lock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
		pAd->BulkOutPending[BulkOutPipeId] = false;

		/* Clear Data flag*/
		RTUSB_CLEAR_BULK_FLAG(pAd, (fRTUSB_BULK_OUT_DATA_FRAG << BulkOutPipeId));
		RTUSB_CLEAR_BULK_FLAG(pAd, (fRTUSB_BULK_OUT_DATA_NORMAL << BulkOutPipeId));

		spin_unlock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
		return;
	}

	/* Clear Data flag*/
	RTUSB_CLEAR_BULK_FLAG(pAd, (fRTUSB_BULK_OUT_DATA_FRAG << BulkOutPipeId));
	RTUSB_CLEAR_BULK_FLAG(pAd, (fRTUSB_BULK_OUT_DATA_NORMAL << BulkOutPipeId));

	/*
		DBGPRINT(RT_DEBUG_TRACE,("BulkOut-B:I=0x%lx, CWPos=%ld, CWRPos=%ld, NBPos=%ld, ENBPos=%ld, bCopy=%d!\n",
					in_interrupt(),
					pHTTXContext->CurWritePosition, pHTTXContext->CurWriteRealPos,
					pHTTXContext->NextBulkOutPosition, pHTTXContext->ENextBulkOutPosition,
					pHTTXContext->bCopySavePad));
	*/
	pHTTXContext->NextBulkOutPosition = pHTTXContext->ENextBulkOutPosition;
	ThisBulkSize = 0;
	TmpBulkEndPos = pHTTXContext->NextBulkOutPosition;
	pWirelessPkt = &pHTTXContext->TransferBuffer->field.WirelessPacket[0];

	if ((pHTTXContext->bCopySavePad == true))
	{
		if (RTMPEqualMemory(pHTTXContext->SavedPad, allzero,4))
		{
			DBGPRINT_RAW(RT_DEBUG_ERROR,("e1, allzero : %x  %x  %x  %x  %x  %x  %x  %x \n",
				pHTTXContext->SavedPad[0], pHTTXContext->SavedPad[1], pHTTXContext->SavedPad[2],pHTTXContext->SavedPad[3]
				,pHTTXContext->SavedPad[4], pHTTXContext->SavedPad[5], pHTTXContext->SavedPad[6],pHTTXContext->SavedPad[7]));
		}
		memmove(&pWirelessPkt[TmpBulkEndPos], pHTTXContext->SavedPad, 8);
		pHTTXContext->bCopySavePad = false;
		if (pAd->bForcePrintTX == true)
			DBGPRINT(RT_DEBUG_TRACE,("RTUSBBulkOutDataPacket --> COPY PAD. CurWrite = %ld, NextBulk = %ld.   ENextBulk = %ld.\n",
						pHTTXContext->CurWritePosition, pHTTXContext->NextBulkOutPosition, pHTTXContext->ENextBulkOutPosition));
	}


	do
	{
		txinfo = (struct mt7612_txinfo_pkt *)&pWirelessPkt[TmpBulkEndPos];
		txwi = (struct mt7612u_txwi *)&pWirelessPkt[TmpBulkEndPos + MT_DMA_HDR_LEN];

		{
			ampdu = txwi->AMPDU;
			phy_mode = txwi->PHYMODE;
			pid = txwi->TxPktId;
			txwi_pkt_len = txwi->MPDUtotalByteCnt;
		}

		if (pAd->bForcePrintTX == true)
			DBGPRINT(RT_DEBUG_TRACE, ("RTUSBBulkOutDataPacket AMPDU = %d.\n",   ampdu));

		/* add by Iverson, limit BulkOut size to 4k to pass WMM b mode 2T1R test items*/
		/*if ((ThisBulkSize != 0)  && (txwi->AMPDU == 0))*/
		if ((ThisBulkSize != 0) && (phy_mode == MODE_CCK))
		{
			if (((ThisBulkSize&0xffff8000) != 0) || ((ThisBulkSize&0x1000) == 0x1000))
			{
				/* Limit BulkOut size to about 4k bytes.*/
				pHTTXContext->ENextBulkOutPosition = TmpBulkEndPos;
				break;
			}
			else if (((pAd->out_max_packet < 512) && ((ThisBulkSize&0xfffff800) != 0) ) /*|| ( (ThisBulkSize != 0)  && (txwi->AMPDU == 0))*/)
			{
				/* For USB 1.1 or peer which didn't support AMPDU, limit the BulkOut size. */
				/* For performence in b/g mode, now just check for USB 1.1 and didn't care about the APMDU or not! 2008/06/04.*/
				pHTTXContext->ENextBulkOutPosition = TmpBulkEndPos;
				break;
			}
		}
		/* end Iverson*/
		else
		{


		if (((ThisBulkSize&0xffffe000) != 0) || ((ThisBulkSize&0x6000) == 0x6000))
		{	/* Limit BulkOut size to about 24k bytes.*/
			pHTTXContext->ENextBulkOutPosition = TmpBulkEndPos;

			break;
		}
		else if (((pAd->out_max_packet < 512) && ((ThisBulkSize&0xfffff800) != 0) ) /*|| ( (ThisBulkSize != 0)  && (txwi->AMPDU == 0))*/)
		{	/* For USB 1.1 or peer which didn't support AMPDU, limit the BulkOut size. */
			/* For performence in b/g mode, now just check for USB 1.1 and didn't care about the APMDU or not! 2008/06/04.*/
			pHTTXContext->ENextBulkOutPosition = TmpBulkEndPos;
			break;
		}

		}

		if (TmpBulkEndPos == pHTTXContext->CurWritePosition)
		{
			pHTTXContext->ENextBulkOutPosition = TmpBulkEndPos;
			break;
		}

		if (txinfo->QSEL != MT_QSEL_EDCA)
		{
			DBGPRINT(RT_DEBUG_ERROR, ("%s(): ====> txinfo->QueueSel(%d)!= MT_QSEL_EDCA!!!!\n",
										__FUNCTION__, txinfo->QSEL));
			DBGPRINT(RT_DEBUG_ERROR, ("\tCWPos=%ld, NBPos=%ld, ENBPos=%ld, bCopy=%d!\n",
										pHTTXContext->CurWritePosition, pHTTXContext->NextBulkOutPosition,
										pHTTXContext->ENextBulkOutPosition, pHTTXContext->bCopySavePad));
			hex_dump("Wrong QSel Pkt:", (u8 *)&pWirelessPkt[TmpBulkEndPos], (pHTTXContext->CurWritePosition - pHTTXContext->NextBulkOutPosition));
		}

		if (txinfo->pkt_len <= 8)
		{
			spin_unlock_bh(&pAd->TxContextQueueLock[BulkOutPipeId]);
			DBGPRINT(RT_DEBUG_ERROR /*RT_DEBUG_TRACE*/,("e2, mt7612_txinfo_pkt.pkt_le==0, Size=%ld, bCSPad=%d, CWPos=%ld, NBPos=%ld, CWRPos=%ld!\n",
					pHTTXContext->BulkOutSize, pHTTXContext->bCopySavePad, pHTTXContext->CurWritePosition, pHTTXContext->NextBulkOutPosition, pHTTXContext->CurWriteRealPos));
			{
				DBGPRINT_RAW(RT_DEBUG_ERROR /*RT_DEBUG_TRACE*/,("%x  %x  %x  %x  %x  %x  %x  %x \n",
					pHTTXContext->SavedPad[0], pHTTXContext->SavedPad[1], pHTTXContext->SavedPad[2],pHTTXContext->SavedPad[3]
					,pHTTXContext->SavedPad[4], pHTTXContext->SavedPad[5], pHTTXContext->SavedPad[6],pHTTXContext->SavedPad[7]));
			}
			pAd->bForcePrintTX = true;
			spin_lock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
			pAd->BulkOutPending[BulkOutPipeId] = false;
			spin_unlock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
			/*DBGPRINT(RT_DEBUG_LOUD,("Out:txinfo->mt7612_txinfo_pkt.pkt_le=%d!\n", txinfo->mt7612_txinfo_pkt.pkt_le));*/
			return;
		}

		/* Increase Total transmit byte counter*/
		pAd->RalinkCounters.OneSecTransmittedByteCount +=  txwi_pkt_len;
		pAd->RalinkCounters.TransmittedByteCount +=  txwi_pkt_len;

		pLastTxInfo = txinfo;

		/* Make sure we use EDCA QUEUE.  */
		txinfo->QSEL = MT_QSEL_EDCA;
		ThisBulkSize += (txinfo->pkt_len+4);
		TmpBulkEndPos += (txinfo->pkt_len+4);

		if (TmpBulkEndPos != pHTTXContext->CurWritePosition)
			txinfo->next_vld = 1;

		if (txinfo->sw_lst_rnd == 1)
		{
			if (pHTTXContext->CurWritePosition == 8)
				txinfo->next_vld = 0;
			txinfo->sw_lst_rnd = 0;

			bTxQLastRound = true;
			pHTTXContext->ENextBulkOutPosition = 8;

	#ifdef __BIG_ENDIAN
			RTMPDescriptorEndianChange((u8 *)txinfo, TYPE_TXINFO);
			RTMPWIEndianChange(pAd, (u8 *)txwi, TYPE_TXWI);
	#endif /* __BIG_ENDIAN */

			break;
		}
#ifdef __BIG_ENDIAN
		RTMPDescriptorEndianChange((u8 *)txinfo, TYPE_TXINFO);
		RTMPWIEndianChange(pAd, (u8 *)txwi, TYPE_TXWI);
#endif /* __BIG_ENDIAN */

		aggregation_num++;

		if ((aggregation_num == 1) && (!pAd->usb_ctl.usb_aggregation)) {
			pHTTXContext->ENextBulkOutPosition = TmpBulkEndPos;
			break;
		}
	}while (true);

	/* adjust the txinfo->mt7612_txinfo_pkt.next_vld value of last txinfo.*/
	if (pLastTxInfo)
	{
#ifdef __BIG_ENDIAN
		RTMPDescriptorEndianChange((u8 *)pLastTxInfo, TYPE_TXINFO);
#endif /* __BIG_ENDIAN */
		pLastTxInfo->next_vld = 0;
#ifdef __BIG_ENDIAN
		RTMPDescriptorEndianChange((u8 *)pLastTxInfo, TYPE_TXINFO);
#endif /* __BIG_ENDIAN */
	}

	/*
		We need to copy SavedPad when following condition matched!
			1. Not the last round of the TxQueue and
			2. any match of following cases:
				(1). The End Position of this bulk out is reach to the Currenct Write position and
						the TxInfo and related header already write to the CurWritePosition.
			   		=>(ENextBulkOutPosition == CurWritePosition) && (CurWriteRealPos > CurWritePosition)

				(2). The EndPosition of the bulk out is not reach to the Current Write Position.
					=>(ENextBulkOutPosition != CurWritePosition)
	*/
	if ((bTxQLastRound == false) &&
		 (((pHTTXContext->ENextBulkOutPosition == pHTTXContext->CurWritePosition) && (pHTTXContext->CurWriteRealPos > pHTTXContext->CurWritePosition)) ||
		  (pHTTXContext->ENextBulkOutPosition != pHTTXContext->CurWritePosition))
		)
	{
		memmove(pHTTXContext->SavedPad, &pWirelessPkt[pHTTXContext->ENextBulkOutPosition], 8);
		pHTTXContext->bCopySavePad = true;
		if (RTMPEqualMemory(pHTTXContext->SavedPad, allzero,4))
		{
			u8 *pBuf = &pHTTXContext->SavedPad[0];
			DBGPRINT_RAW(RT_DEBUG_ERROR,("WARNING-Zero-3:%02x%02x%02x%02x%02x%02x%02x%02x,CWPos=%ld, CWRPos=%ld, bCW=%d, NBPos=%ld, TBPos=%ld, TBSize=%ld\n",
				pBuf[0], pBuf[1], pBuf[2],pBuf[3],pBuf[4], pBuf[5], pBuf[6],pBuf[7], pHTTXContext->CurWritePosition, pHTTXContext->CurWriteRealPos,
				pHTTXContext->bCurWriting, pHTTXContext->NextBulkOutPosition, TmpBulkEndPos, ThisBulkSize));

			pBuf = &pWirelessPkt[pHTTXContext->CurWritePosition];
			DBGPRINT_RAW(RT_DEBUG_ERROR,("\tCWPos=%02x%02x%02x%02x%02x%02x%02x%02x\n", pBuf[0], pBuf[1], pBuf[2],pBuf[3],pBuf[4], pBuf[5], pBuf[6],pBuf[7]));
		}
		/*DBGPRINT(RT_DEBUG_LOUD,("ENPos==CWPos=%ld, CWRPos=%ld, bCSPad=%d!\n", pHTTXContext->CurWritePosition, pHTTXContext->CurWriteRealPos, pHTTXContext->bCopySavePad));*/
	}

	if (pAd->bForcePrintTX == true)
		DBGPRINT(RT_DEBUG_TRACE,("BulkOut-A:Size=%ld, CWPos=%ld, NBPos=%ld, ENBPos=%ld, bCopy=%d!\n", ThisBulkSize, pHTTXContext->CurWritePosition, pHTTXContext->NextBulkOutPosition, pHTTXContext->ENextBulkOutPosition, pHTTXContext->bCopySavePad));
	/*DBGPRINT(RT_DEBUG_LOUD,("BulkOut-A:Size=%ld, CWPos=%ld, CWRPos=%ld, NBPos=%ld, ENBPos=%ld, bCopy=%d, bLRound=%d!\n", ThisBulkSize, pHTTXContext->CurWritePosition, pHTTXContext->CurWriteRealPos, pHTTXContext->NextBulkOutPosition, pHTTXContext->ENextBulkOutPosition, pHTTXContext->bCopySavePad, bTxQLastRound));*/

		/* USB DMA engine requires to pad extra 4 bytes. This pad doesn't count into real bulkoutsize.*/
	pAppendant = &pWirelessPkt[TmpBulkEndPos];
	memset(pAppendant, 0, 8);
		ThisBulkSize += 4;
		pHTTXContext->LastOne = true;

	pHTTXContext->BulkOutSize = ThisBulkSize;


	pAd->watchDogTxPendingCnt[BulkOutPipeId] = 1;
	spin_unlock_bh(&pAd->TxContextQueueLock[BulkOutPipeId]);

	/* Init Tx context descriptor*/
	RTUSBInitHTTxDesc(pAd, pHTTXContext, BulkOutPipeId, ThisBulkSize, (usb_complete_t)RtmpUsbBulkOutDataPacketComplete);

	pUrb = pHTTXContext->pUrb;
	if((ret = usb_submit_urb(pUrb, GFP_ATOMIC))!=0)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("RTUSBBulkOutDataPacket: Submit Tx URB failed %d\n", ret));

		spin_lock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
		pAd->BulkOutPending[BulkOutPipeId] = false;
		pAd->watchDogTxPendingCnt[BulkOutPipeId] = 0;
		spin_unlock_bh(&pAd->BulkOutLock[BulkOutPipeId]);

		return;
	}

	spin_lock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
	pHTTXContext->IRPPending = true;
	spin_unlock_bh(&pAd->BulkOutLock[BulkOutPipeId]);
	pAd->BulkOutReq++;

}


USBHST_STATUS RTUSBBulkOutDataPacketComplete(URBCompleteStatus Status, struct urb *pURB, pregs *pt_regs)
{
	PHT_TX_CONTEXT	pHTTXContext;
	struct rtmp_adapter *pAd;
	struct os_cookie *		pObj;
	u8 		BulkOutPipeId;


	pHTTXContext	= pURB->context;
	pAd 		= pHTTXContext->pAd;
	pObj 		= pAd->OS_Cookie;

	/* Store BulkOut PipeId*/
	BulkOutPipeId	= pHTTXContext->BulkOutPipeId;
	pAd->BulkOutDataOneSecCount++;

	switch (BulkOutPipeId)
	{
		case EDCA_AC0_PIPE:
					RTMP_NET_TASK_DATA_ASSIGN(&pObj->ac0_dma_done_task, (unsigned long)pURB);
					RTMP_OS_TASKLET_SCHE(&pObj->ac0_dma_done_task);
				break;
		case EDCA_AC1_PIPE:
				RTMP_NET_TASK_DATA_ASSIGN(&pObj->ac1_dma_done_task, (unsigned long)pURB);
				RTMP_OS_TASKLET_SCHE(&pObj->ac1_dma_done_task);
				break;
		case EDCA_AC2_PIPE:
				RTMP_NET_TASK_DATA_ASSIGN(&pObj->ac2_dma_done_task, (unsigned long)pURB);
				RTMP_OS_TASKLET_SCHE(&pObj->ac2_dma_done_task);
				break;
		case EDCA_AC3_PIPE:
				RTMP_NET_TASK_DATA_ASSIGN(&pObj->ac3_dma_done_task, (unsigned long)pURB);
				RTMP_OS_TASKLET_SCHE(&pObj->ac3_dma_done_task);
				break;
		case HCCA_PIPE:
				RTMP_NET_TASK_DATA_ASSIGN(&pObj->hcca_dma_done_task, (unsigned long)pURB);
				RTMP_OS_TASKLET_SCHE(&pObj->hcca_dma_done_task);
				break;
	}


}


/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note: NULL frame use BulkOutPipeId = 0

	========================================================================
*/
VOID	RTUSBBulkOutNullFrame(
	IN	struct rtmp_adapter *pAd)
{
	PTX_CONTEXT		pNullContext = &(pAd->NullContext);
	struct urb *		pUrb;
	int				ret = 0;

	spin_lock_bh(&pAd->BulkOutLock[0]);
	if ((pAd->BulkOutPending[0] == true) || RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NEED_STOP_TX))
	{
		spin_unlock_bh(&pAd->BulkOutLock[0]);
		return;
	}
	pAd->BulkOutPending[0] = true;
	pAd->watchDogTxPendingCnt[0] = 1;
	pNullContext->IRPPending = true;
	spin_unlock_bh(&pAd->BulkOutLock[0]);

	/* Increase Total transmit byte counter*/
	pAd->RalinkCounters.TransmittedByteCount +=  pNullContext->BulkOutSize;


	/* Clear Null frame bulk flag*/
	RTUSB_CLEAR_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NULL);

#ifdef __BIG_ENDIAN
	RTMPDescriptorEndianChange((u8 *)pNullContext->TransferBuffer, TYPE_TXINFO);
#endif /* __BIG_ENDIAN */

	/* Init Tx context descriptor*/
	RTUSBInitTxDesc(pAd, pNullContext, 0, (usb_complete_t)RtmpUsbBulkOutNullFrameComplete);

	pUrb = pNullContext->pUrb;
	if((ret = usb_submit_urb(pUrb, GFP_ATOMIC))!=0)
	{
		spin_lock_bh(&pAd->BulkOutLock[0]);
		pAd->BulkOutPending[0] = false;
		pAd->watchDogTxPendingCnt[0] = 0;
		pNullContext->IRPPending = false;
		spin_unlock_bh(&pAd->BulkOutLock[0]);

		DBGPRINT(RT_DEBUG_ERROR, ("RTUSBBulkOutNullFrame: Submit Tx URB failed %d\n", ret));
		return;
	}

}

/* NULL frame use BulkOutPipeId = 0*/
USBHST_STATUS RTUSBBulkOutNullFrameComplete(URBCompleteStatus Status, struct urb *pURB, pregs *pt_regs)
{
	struct rtmp_adapter *	pAd;
	PTX_CONTEXT			pNullContext;
	int 		Status;
	struct os_cookie *		pObj;


	pNullContext	= pURB->context;
	pAd 		= pNullContext->pAd;
	Status 		= pURB->status; /*->rtusb_urb_status;*/

	pObj = pAd->OS_Cookie;
	RTMP_NET_TASK_DATA_ASSIGN(&pObj->null_frame_complete_task, (unsigned long)pURB);
	RTMP_OS_TASKLET_SCHE(&pObj->null_frame_complete_task);

}


/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note: MLME use BulkOutPipeId = 0

	========================================================================
*/
VOID	RTUSBBulkOutMLMEPacket(
	IN	struct rtmp_adapter *pAd,
	IN	u8 		Index)
{
	PTX_CONTEXT		pMLMEContext;
	struct urb *		pUrb;
	int				ret = 0;

	pMLMEContext = (PTX_CONTEXT)pAd->MgmtRing.Cell[pAd->MgmtRing.TxDmaIdx].AllocVa;
	pUrb = pMLMEContext->pUrb;

	if ((pAd->MgmtRing.TxSwFreeIdx >= MGMT_RING_SIZE) ||
		(pMLMEContext->InUse == false) ||
		(pMLMEContext->bWaitingBulkOut == false))
	{


		/* Clear MLME bulk flag*/
		RTUSB_CLEAR_BULK_FLAG(pAd, fRTUSB_BULK_OUT_MLME);

		return;
	}


	spin_lock_bh(&pAd->BulkOutLock[MGMTPIPEIDX]);
	if ((pAd->BulkOutPending[MGMTPIPEIDX] == true) || RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NEED_STOP_TX))
	{
		spin_unlock_bh(&pAd->BulkOutLock[MGMTPIPEIDX]);
		return;
	}

	pAd->BulkOutPending[MGMTPIPEIDX] = true;
	pAd->watchDogTxPendingCnt[MGMTPIPEIDX] = 1;
	pMLMEContext->IRPPending = true;
	pMLMEContext->bWaitingBulkOut = false;
	spin_unlock_bh(&pAd->BulkOutLock[MGMTPIPEIDX]);

	/* Increase Total transmit byte counter*/
	pAd->RalinkCounters.TransmittedByteCount +=  pMLMEContext->BulkOutSize;

	/* Clear MLME bulk flag*/
	RTUSB_CLEAR_BULK_FLAG(pAd, fRTUSB_BULK_OUT_MLME);

#ifdef __BIG_ENDIAN
	RTMPDescriptorEndianChange((u8 *)pMLMEContext->TransferBuffer, TYPE_TXINFO);
#endif /* __BIG_ENDIAN */

	/* Init Tx context descriptor*/
	RTUSBInitTxDesc(pAd, pMLMEContext, MGMTPIPEIDX, (usb_complete_t)RtmpUsbBulkOutMLMEPacketComplete);

	pUrb->transfer_dma	= 0;	\
	pUrb->transfer_flags &= (~URB_NO_TRANSFER_DMA_MAP);	\

	pUrb = pMLMEContext->pUrb;
	if((ret = usb_submit_urb(pUrb, GFP_ATOMIC))!=0)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("RTUSBBulkOutMLMEPacket: Submit MLME URB failed %d\n", ret));
		spin_lock_bh(&pAd->BulkOutLock[MGMTPIPEIDX]);
		pAd->BulkOutPending[MGMTPIPEIDX] = false;
		pAd->watchDogTxPendingCnt[MGMTPIPEIDX] = 0;
		pMLMEContext->IRPPending = false;
		pMLMEContext->bWaitingBulkOut = true;
		spin_unlock_bh(&pAd->BulkOutLock[MGMTPIPEIDX]);

		return;
	}
}


USBHST_STATUS RTUSBBulkOutMLMEPacketComplete(URBCompleteStatus Status, struct urb *pURB, pregs *pt_regs)
{
	PTX_CONTEXT			pMLMEContext;
	struct rtmp_adapter *	pAd;
	int 		Status;
	struct os_cookie *			pObj;
	int					index;

	pMLMEContext	= pURB->context;
	pAd 		= pMLMEContext->pAd;
	pObj 		= pAd->OS_Cookie;
	Status		= pURB->status;
	index 		= pMLMEContext->SelfIdx;

	RTMP_NET_TASK_DATA_ASSIGN(&pObj->mgmt_dma_done_task, (unsigned long)pURB);
	RTMP_OS_TASKLET_SCHE(&pObj->mgmt_dma_done_task);
}


/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note: PsPoll use BulkOutPipeId = 0

	========================================================================
*/
VOID	RTUSBBulkOutPsPoll(
	IN	struct rtmp_adapter *pAd)
{
	PTX_CONTEXT		pPsPollContext = &(pAd->PsPollContext);
	struct urb *		pUrb;
	int				ret = 0;

	spin_lock_bh(&pAd->BulkOutLock[0]);
	if ((pAd->BulkOutPending[0] == true) || RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NEED_STOP_TX))
	{
		spin_unlock_bh(&pAd->BulkOutLock[0]);
		return;
	}
	pAd->BulkOutPending[0] = true;
	pAd->watchDogTxPendingCnt[0] = 1;
	pPsPollContext->IRPPending = true;
	spin_unlock_bh(&pAd->BulkOutLock[0]);


	/* Clear PS-Poll bulk flag*/
	RTUSB_CLEAR_BULK_FLAG(pAd, fRTUSB_BULK_OUT_PSPOLL);

#ifdef __BIG_ENDIAN
	RTMPDescriptorEndianChange((u8 *)pPsPollContext->TransferBuffer, TYPE_TXINFO);
#endif /* __BIG_ENDIAN */

	/* Init Tx context descriptor*/
	RTUSBInitTxDesc(pAd, pPsPollContext, MGMTPIPEIDX, (usb_complete_t)RtmpUsbBulkOutPsPollComplete);

	pUrb = pPsPollContext->pUrb;
	if((ret = usb_submit_urb(pUrb, GFP_ATOMIC))!=0)
	{
		spin_lock_bh(&pAd->BulkOutLock[0]);
		pAd->BulkOutPending[0] = false;
		pAd->watchDogTxPendingCnt[0] = 0;
		pPsPollContext->IRPPending = false;
		spin_unlock_bh(&pAd->BulkOutLock[0]);

		DBGPRINT(RT_DEBUG_ERROR, ("RTUSBBulkOutPsPoll: Submit Tx URB failed %d\n", ret));
		return;
	}

}

/* PS-Poll frame use BulkOutPipeId = 0*/
USBHST_STATUS RTUSBBulkOutPsPollComplete(URBCompleteStatus Status, struct urb *pURB, pregs *pt_regs)
{
	struct rtmp_adapter *	pAd;
	PTX_CONTEXT			pPsPollContext;
	int 		Status;
	struct os_cookie *		pObj;


	pPsPollContext= pURB->context;
	pAd = pPsPollContext->pAd;
	Status = pURB->status;

	pObj = pAd->OS_Cookie;
	RTMP_NET_TASK_DATA_ASSIGN(&pObj->pspoll_frame_complete_task, (unsigned long)pURB);
	RTMP_OS_TASKLET_SCHE(&pObj->pspoll_frame_complete_task);

}


VOID DoBulkIn(IN struct rtmp_adapter *pAd)
{
	PRX_CONTEXT		pRxContext;
	struct urb *		pUrb;
	int				ret = 0;

	spin_lock_bh(&pAd->BulkInLock);
	pRxContext = &(pAd->RxContext[pAd->NextRxBulkInIndex]);
	if ((pAd->PendingRx > 0) || (pRxContext->Readable == true) || (pRxContext->InUse == true))
	{
		spin_unlock_bh(&pAd->BulkInLock);
		return;
	}
	pRxContext->InUse = true;
	pRxContext->IRPPending = true;
	pAd->PendingRx++;
	pAd->BulkInReq++;
	spin_unlock_bh(&pAd->BulkInLock);

	/* Init Rx context descriptor*/
	memset(pRxContext->TransferBuffer, 0, pRxContext->BulkInOffset);
	RTUSBInitRxDesc(pAd, pRxContext);

	pUrb = pRxContext->pUrb;
	if ((ret = usb_submit_urb(pUrb, GFP_ATOMIC))!=0)
	{	/* fail*/

		spin_lock_bh(&pAd->BulkInLock);
		pRxContext->InUse = false;
		pRxContext->IRPPending = false;
		pAd->PendingRx--;
		pAd->BulkInReq--;
		spin_unlock_bh(&pAd->BulkInLock);
		DBGPRINT(RT_DEBUG_ERROR, ("RTUSBBulkReceive: Submit Rx URB failed %d\n", ret));
	}
	else
	{	/* success*/
		ASSERT((pRxContext->InUse == pRxContext->IRPPending));
	}
}



/*
	========================================================================

	Routine Description:
	USB_RxPacket initializes a URB and uses the Rx IRP to submit it
	to USB. It checks if an Rx Descriptor is available and passes the
	the coresponding buffer to be filled. If no descriptor is available
	fails the request. When setting the completion routine we pass our
	Adapter Object as Context.

	Arguments:

	Return Value:
		true			found matched tuple cache
		false			no matched found

	Note:

	========================================================================
*/
#define fRTMP_ADAPTER_NEED_STOP_RX		\
		(fRTMP_ADAPTER_NIC_NOT_EXIST | fRTMP_ADAPTER_HALT_IN_PROGRESS |	\
		 fRTMP_ADAPTER_RADIO_OFF | fRTMP_ADAPTER_RESET_IN_PROGRESS | \
		 fRTMP_ADAPTER_REMOVE_IN_PROGRESS | fRTMP_ADAPTER_BULKIN_RESET)

#define fRTMP_ADAPTER_NEED_STOP_HANDLE_RX	\
		(fRTMP_ADAPTER_NIC_NOT_EXIST | fRTMP_ADAPTER_HALT_IN_PROGRESS |	\
		 fRTMP_ADAPTER_RADIO_OFF | fRTMP_ADAPTER_RESET_IN_PROGRESS | \
		 fRTMP_ADAPTER_REMOVE_IN_PROGRESS)

VOID RTUSBBulkReceive(struct rtmp_adapter *pAd)
{
	PRX_CONTEXT pRxContext;

	/* sanity check */
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NEED_STOP_HANDLE_RX)
					&& !RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_POLL_IDLE))
		return;

	while(1)
	{
		spin_lock_bh(&pAd->BulkInLock);
		pRxContext = &(pAd->RxContext[pAd->NextRxBulkInReadIndex]);
		if (((pRxContext->InUse == false) && (pRxContext->Readable == true)) &&
			(pRxContext->bRxHandling == false))
		{
			pRxContext->bRxHandling = true;
			spin_unlock_bh(&pAd->BulkInLock);

			rtmp_rx_done_handle(pAd);

			/* Finish to handle this bulkIn buffer.*/
			spin_lock_bh(&pAd->BulkInLock);
			pRxContext->BulkInOffset = 0;
			pRxContext->Readable = false;
			pRxContext->bRxHandling = false;
			pAd->ReadPosition = 0;
			pAd->TransferBufferLength = 0;
			INC_RING_INDEX(pAd->NextRxBulkInReadIndex, RX_RING_SIZE);
			spin_unlock_bh(&pAd->BulkInLock);
		}
		else
		{
			spin_unlock_bh(&pAd->BulkInLock);
			break;
		}
	}

	if (!((RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NEED_STOP_RX)
						&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_POLL_IDLE)))))
	{
#ifdef CONFIG_STA_SUPPORT
		if(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF))
			return;
#endif /* CONFIG_STA_SUPPORT */

		DoBulkIn(pAd);
	}

}


/*
	========================================================================

	Routine Description:
		This routine process Rx Irp and call rx complete function.

	Arguments:
		DeviceObject	Pointer to the device object for next lower
						device. DeviceObject passed in here belongs to
						the next lower driver in the stack because we
						were invoked via IoCallDriver in USB_RxPacket
						AND it is not OUR device object
	  Irp				Ptr to completed IRP
	  Context			Ptr to our Adapter object (context specified
						in IoSetCompletionRoutine

	Return Value:
		Always returns STATUS_MORE_PROCESSING_REQUIRED

	Note:
		Always returns STATUS_MORE_PROCESSING_REQUIRED
	========================================================================
*/
USBHST_STATUS RTUSBBulkRxComplete(URBCompleteStatus Status, struct urb *pURB, pregs *pt_regs)
{
	/* use a receive tasklet to handle received packets;*/
	/* or sometimes hardware IRQ will be disabled here, so we can not*/
	/* use spin_lock_bh()/spin_unlock_bh() after IRQ is disabled. :<*/
	PRX_CONTEXT		pRxContext;
	struct rtmp_adapter *pAd;
	struct os_cookie *		pObj;

	pRxContext	= pURB->context;
	pAd 		= pRxContext->pAd;
	pObj 		= pAd->OS_Cookie;

	RTMP_NET_TASK_DATA_ASSIGN(&pObj->rx_done_task, (unsigned long)pURB);
	RTMP_OS_TASKLET_SCHE(&pObj->rx_done_task);

}


static void RTUSBDataBulkOut(struct rtmp_adapter *pAd, ULONG bulkFlag, INT epIdx)
{
	if (RTUSB_TEST_BULK_FLAG(pAd, bulkFlag))
        {
        	if (((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) ||
                     (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
                    ))
                {
                	RTUSBBulkOutDataPacket(pAd, epIdx, pAd->NextBulkOutIndex[epIdx]);
                }
        }
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBKickBulkOut(
	IN	struct rtmp_adapter *pAd)
{
	/* BulkIn Reset will reset whole USB PHY. So we need to make sure fRTMP_ADAPTER_BULKIN_RESET not flaged.*/
	if (!RTMP_TEST_FLAG(pAd ,fRTMP_ADAPTER_NEED_STOP_TX)
		)
	{
		/* 2. PS-Poll frame is next*/
		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_PSPOLL))
		{
			RTUSBBulkOutPsPoll(pAd);
		}

		/* 5. Mlme frame is next*/
		else if ((RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_MLME)) ||
				 (pAd->MgmtRing.TxSwFreeIdx < MGMT_RING_SIZE))
		{
			RTUSBBulkOutMLMEPacket(pAd, pAd->MgmtRing.TxDmaIdx);
		}

		/* 6. Data frame normal is next [BE, BK, VI, VO]*/
		RTUSBDataBulkOut(pAd, fRTUSB_BULK_OUT_DATA_NORMAL,   EDCA_AC0_PIPE);
		RTUSBDataBulkOut(pAd, fRTUSB_BULK_OUT_DATA_NORMAL_2, EDCA_AC1_PIPE);
		RTUSBDataBulkOut(pAd, fRTUSB_BULK_OUT_DATA_NORMAL_3, EDCA_AC2_PIPE);
		RTUSBDataBulkOut(pAd, fRTUSB_BULK_OUT_DATA_NORMAL_4, EDCA_AC3_PIPE);

		/* 7. Null frame is the last*/
		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NULL))
		{

					{
				RTUSBBulkOutNullFrame(pAd);
			}
		}

		/* 8. No data avaliable*/
		else
		{

		}
	}
}

/*
	========================================================================

	Routine Description:
	Call from Reset action after BulkOut failed.
	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBCleanUpDataBulkOutQueue(
	IN	struct rtmp_adapter *pAd)
{
	u8 		Idx;
	PHT_TX_CONTEXT	pTxContext;

	DBGPRINT(RT_DEBUG_TRACE, ("--->CleanUpDataBulkOutQueue\n"));

	for (Idx = 0; Idx < 4; Idx++)
	{
		pTxContext = &pAd->TxContext[Idx];

		pTxContext->CurWritePosition = pTxContext->NextBulkOutPosition;
		pTxContext->LastOne = false;
		spin_lock_bh(&pAd->BulkOutLock[Idx]);
		pAd->BulkOutPending[Idx] = false;
		spin_unlock_bh(&pAd->BulkOutLock[Idx]);
	}

	DBGPRINT(RT_DEBUG_TRACE, ("<---CleanUpDataBulkOutQueue\n"));
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBCleanUpMLMEBulkOutQueue(
	IN	struct rtmp_adapter *pAd)
{
	DBGPRINT(RT_DEBUG_TRACE, ("--->CleanUpMLMEBulkOutQueue\n"));


	DBGPRINT(RT_DEBUG_TRACE, ("<---CleanUpMLMEBulkOutQueue\n"));
}


/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:


	Note:

	========================================================================
*/
VOID	RTUSBCancelPendingIRPs(
	IN	struct rtmp_adapter *pAd)
{
	RTUSBCancelPendingBulkInIRP(pAd);
	RTUSBCancelPendingBulkOutIRP(pAd);
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID RTUSBCancelPendingBulkInIRP(struct rtmp_adapter *pAd)
{
	PRX_CONTEXT pRxContext;
	PCMD_RSP_CONTEXT pCmdRspEventContext = &pAd->CmdRspEventContext;
	UINT i;

	DBGPRINT_RAW(RT_DEBUG_TRACE, ("--->RTUSBCancelPendingBulkInIRP\n"));
	for ( i = 0; i < (RX_RING_SIZE); i++)
	{
		pRxContext = &(pAd->RxContext[i]);
		if(pRxContext->IRPPending == true)
		{
			usb_kill_urb(pRxContext->pUrb);
			pRxContext->IRPPending = false;
			pRxContext->InUse = false;
			/*NdisInterlockedDecrement(&pAd->PendingRx);*/
			/*pAd->PendingRx--;*/
		}
	}

	if (pCmdRspEventContext->IRPPending == true)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("Unlink cmd rsp urb\n"));
		usb_kill_urb(pCmdRspEventContext->pUrb);
		pCmdRspEventContext->IRPPending = false;
		pCmdRspEventContext->InUse = false;
	}

	DBGPRINT_RAW(RT_DEBUG_TRACE, ("<---RTUSBCancelPendingBulkInIRP\n"));
}


/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBCancelPendingBulkOutIRP(
	IN	struct rtmp_adapter *pAd)
{
	PHT_TX_CONTEXT		pHTTXContext;
	PTX_CONTEXT			pMLMEContext;
	PTX_CONTEXT			pNullContext;
	PTX_CONTEXT			pPsPollContext;
	UINT				i, Idx;
/*	unsigned int 		IrqFlags;*/
/*	NDIS_SPIN_LOCK		*pLock;*/
/*	bool				*pPending;*/


/*	pLock = &pAd->BulkOutLock[MGMTPIPEIDX];*/
/*	pPending = &pAd->BulkOutPending[MGMTPIPEIDX];*/

	for (Idx = 0; Idx < 4; Idx++)
	{
		pHTTXContext = &(pAd->TxContext[Idx]);

		if (pHTTXContext->IRPPending == true)
		{

			/* Get the USB_CONTEXT and cancel it's IRP; the completion routine will itself*/
			/* remove it from the HeadPendingSendList and NULL out HeadPendingSendList*/
			/*	when the last IRP on the list has been	cancelled; that's how we exit this loop*/


			usb_kill_urb(pHTTXContext->pUrb);

			/* Sleep 200 microseconds to give cancellation time to work*/
			udelay(200);
		}

		pAd->BulkOutPending[Idx] = false;
	}

	/*spin_lock_bh(pLock);*/
	for (i = 0; i < MGMT_RING_SIZE; i++)
	{
		pMLMEContext = (PTX_CONTEXT)pAd->MgmtRing.Cell[i].AllocVa;
		if(pMLMEContext && (pMLMEContext->IRPPending == true))
		{

			/* Get the USB_CONTEXT and cancel it's IRP; the completion routine will itself*/
			/* remove it from the HeadPendingSendList and NULL out HeadPendingSendList*/
			/*	when the last IRP on the list has been	cancelled; that's how we exit this loop*/


			usb_kill_urb(pMLMEContext->pUrb);
			pMLMEContext->IRPPending = false;

			/* Sleep 200 microsecs to give cancellation time to work*/
			udelay(200);
		}
	}
	pAd->BulkOutPending[MGMTPIPEIDX] = false;
	/*spin_unlock_bh(pLock);*/

	pNullContext = &(pAd->NullContext);
	if (pNullContext->IRPPending == true)
		usb_kill_urb(pNullContext->pUrb);

	pPsPollContext = &(pAd->PsPollContext);
	if (pPsPollContext->IRPPending == true)
		usb_kill_urb(pPsPollContext->pUrb);

	for (Idx = 0; Idx < 4; Idx++)
	{
		spin_lock_bh(&pAd->BulkOutLock[Idx]);
		pAd->BulkOutPending[Idx] = false;
		spin_unlock_bh(&pAd->BulkOutLock[Idx]);
	}
}


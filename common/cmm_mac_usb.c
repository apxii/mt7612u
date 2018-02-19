/****************************************************************************
 * Ralink Tech Inc.
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************/




#include "rt_config.h"
#include <linux/bitfield.h>

static int RTMPAllocUsbBulkBufStruct(
	struct usb_device *udev,
	struct urb **ppUrb,
	void **ppXBuffer,
	INT	bufLen,
	dma_addr_t *pDmaAddr,
	char *pBufName)
{
	*ppUrb = usb_alloc_urb(0, GFP_ATOMIC);
	if (*ppUrb == NULL) {
		DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc urb struct for %s !\n", pBufName));
		return NDIS_STATUS_RESOURCES;
	}

	*ppXBuffer = usb_alloc_coherent(udev, bufLen, GFP_ATOMIC, pDmaAddr);
	if (*ppXBuffer == NULL) {
		DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc Bulk buffer for %s!\n", pBufName));
		return NDIS_STATUS_RESOURCES;
	}

	return NDIS_STATUS_SUCCESS;
}


VOID RTMPResetTxRxRingMemory(struct rtmp_adapter *pAd)
{
	UINT index, i, acidx;
	PTX_CONTEXT pNullContext   = &pAd->NullContext;
	PTX_CONTEXT pPsPollContext = &pAd->PsPollContext;
	PCMD_RSP_CONTEXT pCmdRspEventContext = &pAd->CmdRspEventContext;

	/* Free TxSwQueue Packet*/
	for (index = 0; index < NUM_OF_TX_RING; index++) {
		PQUEUE_ENTRY pEntry;
		struct sk_buff *pPacket;
		PQUEUE_HEADER pQueue;

		spin_lock_bh(&pAd->irq_lock);
		pQueue = &pAd->TxSwQueue[index];
		while (pQueue->Head) {
			pEntry = RemoveHeadQueue(pQueue);
			pPacket = QUEUE_ENTRY_TO_PACKET(pEntry);
			dev_kfree_skb_any(pPacket);
		}
		 spin_unlock_bh(&pAd->irq_lock);
	}

	/* unlink all urbs for the RECEIVE buffer queue.*/
	for (i = 0; i < RX_RING_SIZE; i++) {
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);

		if (pRxContext->pUrb)
			usb_kill_urb(pRxContext->pUrb);
	}

	if (pCmdRspEventContext->pUrb)
		usb_kill_urb(pCmdRspEventContext->pUrb);

	/* unlink PsPoll urb resource*/
	if (pPsPollContext && pPsPollContext->pUrb)
		usb_kill_urb(pPsPollContext->pUrb);

	/* Free NULL frame urb resource*/
	if (pNullContext && pNullContext->pUrb)
		usb_kill_urb(pNullContext->pUrb);


	/* Free mgmt frame resource*/
	for (i = 0; i < MGMT_RING_SIZE; i++) {
		PTX_CONTEXT pMLMEContext = (PTX_CONTEXT)pAd->MgmtRing.Cell[i].AllocVa;

		if (pMLMEContext) {
			if (pMLMEContext->pUrb != NULL) {
				usb_kill_urb(pMLMEContext->pUrb);
				usb_free_urb(pMLMEContext->pUrb);
				pMLMEContext->pUrb = NULL;
			}
		}

		if (pAd->MgmtRing.Cell[i].pNdisPacket != NULL) {
			dev_kfree_skb_any(pAd->MgmtRing.Cell[i].pNdisPacket);
			pAd->MgmtRing.Cell[i].pNdisPacket = NULL;

			if (pMLMEContext)
				pMLMEContext->TransferBuffer = NULL;
		}

	}


	/* Free Tx frame resource*/
	for (acidx = 0; acidx < 4; acidx++) {
		PHT_TX_CONTEXT pHTTXContext = &(pAd->TxContext[acidx]);

		if (pHTTXContext && pHTTXContext->pUrb)
			usb_kill_urb(pHTTXContext->pUrb);
	}

	/* Clear all pending bulk-out request flags.*/
	RTUSB_CLEAR_BULK_FLAG(pAd, 0xffffffff);


}


/*
 * ========================================================================
 * Routine Description:
 *	Calls USB_InterfaceStop and frees memory allocated for the URBs
 *	calls NdisMDeregisterDevice and frees the memory
 *	allocated in VNetInitialize for the Adapter Object
 *
 * Arguments:
 * 	*pAd				the raxx interface data pointer
 *
 * Return Value:
 *	None
 *
 * Note:
 * ========================================================================
 */

VOID	RTMPFreeTxRxRingMemory(struct rtmp_adapter *pAd)
{
	UINT                i, acidx;
	PTX_CONTEXT			pNullContext   = &pAd->NullContext;
	PTX_CONTEXT			pPsPollContext = &pAd->PsPollContext;
	PCMD_RSP_CONTEXT pCmdRspEventContext = &pAd->CmdRspEventContext;
	struct usb_device *udev =  mt7612u_to_usb_dev(pAd);

	DBGPRINT(RT_DEBUG_ERROR, ("---> RTMPFreeTxRxRingMemory\n"));

	/* Free all resources for the RECEIVE buffer queue.*/
	for (i = 0; i < RX_RING_SIZE; i++) {
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);

		if (pRxContext) {
			if (pRxContext->pUrb) {
				usb_kill_urb(pRxContext->pUrb);
				usb_free_urb(pRxContext->pUrb);
				pRxContext->pUrb = NULL;
			}

			if (pRxContext->TransferBuffer) {
				usb_free_coherent(udev ,MAX_RXBULK_SIZE,
						  pRxContext->TransferBuffer,
						  pRxContext->data_dma);
				pRxContext->TransferBuffer = NULL;
			}
		}
	}

	/* Command Response */
	if (pCmdRspEventContext->pUrb) {
		usb_kill_urb(pCmdRspEventContext->pUrb);
		usb_free_urb(pCmdRspEventContext->pUrb);
		pCmdRspEventContext->pUrb = NULL;
	}

	if (pCmdRspEventContext->CmdRspBuffer) {
		usb_free_coherent(udev, CMD_RSP_BULK_SIZE,
				  pCmdRspEventContext->CmdRspBuffer,
				  pCmdRspEventContext->data_dma);
		pCmdRspEventContext->CmdRspBuffer = NULL;
	}

	/* Free PsPoll frame resource*/
	if (pPsPollContext->pUrb) {
		usb_kill_urb(pPsPollContext->pUrb);
		usb_free_urb(pPsPollContext->pUrb);
		pPsPollContext->pUrb = NULL;
	}

	if (pPsPollContext->TransferBuffer) {
		usb_free_coherent(udev, sizeof(TX_BUFFER),
				  pPsPollContext->TransferBuffer,
				  pPsPollContext->data_dma);
		pPsPollContext->TransferBuffer = NULL;
	}

	/* Free NULL frame resource*/
	if (pNullContext->pUrb) {
		usb_kill_urb(pNullContext->pUrb);
		usb_free_urb(pNullContext->pUrb);
		pNullContext->pUrb = NULL;
	}

	if (pNullContext->TransferBuffer) {
		usb_free_coherent(udev, sizeof(TX_BUFFER),
				  pPsPollContext->TransferBuffer,
				  pNullContext->data_dma);
		pPsPollContext->TransferBuffer = NULL;
	}

	/* Free mgmt frame resource*/
	for (i = 0; i < MGMT_RING_SIZE; i++) {
		PTX_CONTEXT pMLMEContext = (PTX_CONTEXT)pAd->MgmtRing.Cell[i].AllocVa;

		if (pMLMEContext) {
			if (pMLMEContext->pUrb != NULL) {
				usb_kill_urb(pMLMEContext->pUrb);
				usb_free_urb(pMLMEContext->pUrb);
				pMLMEContext->pUrb = NULL;
			}
		}

		if (pAd->MgmtRing.Cell[i].pNdisPacket != NULL) {
			dev_kfree_skb_any(pAd->MgmtRing.Cell[i].pNdisPacket);
			pAd->MgmtRing.Cell[i].pNdisPacket = NULL;
			if (pMLMEContext)
				pMLMEContext->TransferBuffer = NULL;
		}
	}

	kfree(pAd->MgmtDescRing.AllocVa);

	/* Free Tx frame resource*/
	for (acidx = 0; acidx < 4; acidx++) {
		PHT_TX_CONTEXT pHTTXContext = &(pAd->TxContext[acidx]);

		if (pHTTXContext) {
			if (pHTTXContext->pUrb) {
				usb_kill_urb(pHTTXContext->pUrb);
				usb_free_urb(pHTTXContext->pUrb);
				pHTTXContext->pUrb = NULL;
			}

			if (pCmdRspEventContext->CmdRspBuffer) {
				usb_free_coherent(udev, sizeof(HTTX_BUFFER),
						  pHTTXContext->TransferBuffer,
						  pHTTXContext->data_dma);
				pHTTXContext->TransferBuffer = NULL;
			}
		}
	}

	if (pAd->FragFrame.pFragPacket)
		dev_kfree_skb_any(pAd->FragFrame.pFragPacket);


	DBGPRINT(RT_DEBUG_ERROR, ("<--- RTMPFreeTxRxRingMemory\n"));
}


/*
 * ========================================================================
 * Routine Description:
 *     Initialize receive data structures.
 *
 * Arguments:
 *     pAd					Pointer to our adapter
 *
 * Return Value:
 *	NDIS_STATUS_SUCCESS
 *	NDIS_STATUS_RESOURCES
 *
 * Note:
 *	Initialize all receive releated private buffer, include those define
 *	in struct rtmp_adapter structure and all private data structures. The major
 *	work is to allocate buffer for each packet and chain buffer to
 *	NDIS packet descriptor.
 * ========================================================================
 */

int NICInitRecv(struct rtmp_adapter *pAd)
{
	u8 i;
	PCMD_RSP_CONTEXT pCmdRspEventContext = &pAd->CmdRspEventContext;

	DBGPRINT(RT_DEBUG_TRACE, ("--> NICInitRecv\n"));


	pAd->PendingRx = 0;
	pAd->NextRxBulkInReadIndex 	= 0;	/* Next Rx Read index*/
	pAd->NextRxBulkInIndex		= 0;	/*RX_RING_SIZE -1;  Rx Bulk pointer*/
	pAd->NextRxBulkInPosition 	= 0;

	for (i = 0; i < (RX_RING_SIZE); i++) {
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);

		ASSERT((pRxContext->TransferBuffer != NULL));
		ASSERT((pRxContext->pUrb != NULL));

		memset(pRxContext->TransferBuffer, 0, MAX_RXBULK_SIZE);

		pRxContext->pAd	= pAd;
		pRxContext->pIrp = NULL;
		pRxContext->InUse = false;
		pRxContext->IRPPending = false;
		pRxContext->Readable	= false;
		pRxContext->bRxHandling = false;
		pRxContext->BulkInOffset = 0;
	}

	pCmdRspEventContext->pAd = pAd;
	pCmdRspEventContext->InUse = false;
	pCmdRspEventContext->Readable = false;
	memset(pCmdRspEventContext->CmdRspBuffer, 0, CMD_RSP_BULK_SIZE);

	DBGPRINT(RT_DEBUG_TRACE, ("<-- NICInitRecv()\n"));

	return NDIS_STATUS_SUCCESS;
}


/*
 * ========================================================================
 * Routine Description:
 *    Initialize transmit data structures.
 *
 * Arguments:
 *     pAd					Pointer to our adapter
 *
 * Return Value:
 *	NDIS_STATUS_SUCCESS
 *	NDIS_STATUS_RESOURCES
 *
 * Note:
 * ========================================================================
 */

int NICInitTransmit(struct rtmp_adapter *pAd)
{
	u8 		i, acidx;
	int     Status = NDIS_STATUS_SUCCESS;
	PTX_CONTEXT		pNullContext   = &(pAd->NullContext);
	PTX_CONTEXT		pPsPollContext = &(pAd->PsPollContext);
	PTX_CONTEXT		pMLMEContext = NULL;
	PVOID			RingBaseVa;
	RTMP_MGMT_RING  *pMgmtRing;
	PVOID pTransferBuffer;
	struct urb *pUrb;
	ra_dma_addr_t data_dma;
	struct usb_device *udev =  mt7612u_to_usb_dev(pAd);

	DBGPRINT(RT_DEBUG_TRACE, ("--> NICInitTransmit\n"));


	/* Init 4 set of Tx parameters*/
	for (acidx = 0; acidx < NUM_OF_TX_RING; acidx++) {
		/* Initialize all Transmit releated queues*/
		InitializeQueueHeader(&pAd->TxSwQueue[acidx]);

		/* Next Local tx ring pointer waiting for buck out*/
		pAd->NextBulkOutIndex[acidx] = acidx;
		pAd->BulkOutPending[acidx] = false; /* Buck Out control flag	*/
	}


	/* TX_RING_SIZE, 4 ACs*/

	for (acidx =  0; acidx < 4; acidx++) {
		PHT_TX_CONTEXT	pHTTXContext = &(pAd->TxContext[acidx]);

		pTransferBuffer = pHTTXContext->TransferBuffer;
		pUrb = pHTTXContext->pUrb;
		data_dma = pHTTXContext->data_dma;

		ASSERT((pTransferBuffer != NULL));
		ASSERT((pUrb != NULL));

		memset(pHTTXContext, 0, sizeof(HT_TX_CONTEXT));
		pHTTXContext->TransferBuffer = pTransferBuffer;
		pHTTXContext->pUrb = pUrb;
		pHTTXContext->data_dma = data_dma;

		memset(pHTTXContext->TransferBuffer->Aggregation, 0, 4);

		pHTTXContext->pAd = pAd;
		pHTTXContext->BulkOutPipeId = acidx;
		pHTTXContext->bRingEmpty = true;
		pHTTXContext->bCopySavePad = false;

		pAd->BulkOutPending[acidx] = false;
	}



	/* MGMT_RING_SIZE*/
	memset(pAd->MgmtDescRing.AllocVa, 0, pAd->MgmtDescRing.AllocSize);
	RingBaseVa = pAd->MgmtDescRing.AllocVa;

	/* Initialize MGMT Ring and associated buffer memory*/
	pMgmtRing = &pAd->MgmtRing;
	for (i = 0; i < MGMT_RING_SIZE; i++) {
		/* link the pre-allocated Mgmt buffer to MgmtRing.Cell*/
		pMgmtRing->Cell[i].AllocSize = sizeof(TX_CONTEXT);
		pMgmtRing->Cell[i].AllocVa = RingBaseVa;
		pMgmtRing->Cell[i].pNdisPacket = NULL;
		pMgmtRing->Cell[i].pNextNdisPacket = NULL;

		/*Allocate URB for MLMEContext*/
		pMLMEContext = (PTX_CONTEXT) pAd->MgmtRing.Cell[i].AllocVa;
		pMLMEContext->pUrb = usb_alloc_urb(0, GFP_ATOMIC);
		if (pMLMEContext->pUrb == NULL) {
			DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc TX MLMEContext[%d] urb!!\n", i));
			Status = NDIS_STATUS_RESOURCES;
			goto err;
		}
		pMLMEContext->pAd = pAd;
		pMLMEContext->SelfIdx = i;

		/* Offset to next ring descriptor address*/
		RingBaseVa = (u8 *) RingBaseVa + sizeof(TX_CONTEXT);
	}
	DBGPRINT(RT_DEBUG_TRACE, ("MGMT Ring: total %d entry allocated\n", i));

	/*pAd->MgmtRing.TxSwFreeIdx = (MGMT_RING_SIZE - 1);*/
	pAd->MgmtRing.TxSwFreeIdx = MGMT_RING_SIZE;
	pAd->MgmtRing.TxCpuIdx = 0;
	pAd->MgmtRing.TxDmaIdx = 0;



	/* NullContext*/

	pTransferBuffer = pNullContext->TransferBuffer;
	pUrb = pNullContext->pUrb;
	data_dma = pNullContext->data_dma;

	memset(pNullContext, 0, sizeof(TX_CONTEXT));
	pNullContext->TransferBuffer = pTransferBuffer;
	pNullContext->pUrb = pUrb;
	pNullContext->data_dma = data_dma;
	pNullContext->pAd = pAd;

	/* PsPollContext*/

	pTransferBuffer = pPsPollContext->TransferBuffer;
	pUrb = pPsPollContext->pUrb;
	data_dma = pPsPollContext->data_dma;
	memset(pPsPollContext, 0, sizeof(TX_CONTEXT));
	pPsPollContext->TransferBuffer = pTransferBuffer;
	pPsPollContext->pUrb = pUrb;
	pPsPollContext->data_dma = data_dma;
	pPsPollContext->pAd = pAd;
	pPsPollContext->LastOne = true;

	DBGPRINT(RT_DEBUG_TRACE, ("<-- NICInitTransmit(Status=%d)\n", Status));

	return Status;

	/* --------------------------- ERROR HANDLE --------------------------- */
err:
	if (pAd->MgmtDescRing.AllocVa) {
		pMgmtRing = &pAd->MgmtRing;
		for (i = 0; i < MGMT_RING_SIZE; i++) {
			pMLMEContext = (PTX_CONTEXT) pAd->MgmtRing.Cell[i].AllocVa;
			if (pMLMEContext) {
				if (pMLMEContext->pUrb) {
					usb_kill_urb(pMLMEContext->pUrb);
					usb_free_urb(pMLMEContext->pUrb);
					pMLMEContext->pUrb = NULL;
				}

				if (pMLMEContext->TransferBuffer) {
					usb_free_coherent(udev, sizeof(TX_BUFFER),
							  pMLMEContext->TransferBuffer,
							  pMLMEContext->data_dma);
					pMLMEContext->TransferBuffer = NULL;
				}

			}
		}
		kfree(pAd->MgmtDescRing.AllocVa);
		pAd->MgmtDescRing.AllocVa = NULL;
	}

	/* Here we didn't have any pre-allocated memory need to free.*/

	return Status;
}


/*
 * ========================================================================
 * Routine Description:
 *    Allocate DMA memory blocks for send, receive.
 *
 * Arguments:
 *    pAd					Pointer to our adapter
 *
 * Return Value:
 *	NDIS_STATUS_SUCCESS
 *	NDIS_STATUS_FAILURE
 *	NDIS_STATUS_RESOURCES
 *
 * Note:
 * ========================================================================
 */
int RTMPAllocTxRxRingMemory(struct rtmp_adapter *pAd)
{
	int Status = NDIS_STATUS_FAILURE;
	PTX_CONTEXT pNullContext   = &(pAd->NullContext);
	PTX_CONTEXT pPsPollContext = &(pAd->PsPollContext);
	PCMD_RSP_CONTEXT pCmdRspEventContext = &(pAd->CmdRspEventContext);
	INT i, acidx;
	struct usb_device *udev =  mt7612u_to_usb_dev(pAd);

	DBGPRINT(RT_DEBUG_TRACE, ("--> RTMPAllocTxRxRingMemory\n"));

	/* Init send data structures and related parameters*/


	/* TX_RING_SIZE, 4 ACs*/

	for (acidx = 0; acidx < 4; acidx++) {
		PHT_TX_CONTEXT	pHTTXContext = &(pAd->TxContext[acidx]);

		memset(pHTTXContext, 0, sizeof(HT_TX_CONTEXT));
		/*Allocate URB and bulk buffer*/
		Status = RTMPAllocUsbBulkBufStruct(udev,
					&pHTTXContext->pUrb,
					(PVOID *)&pHTTXContext->TransferBuffer,
					sizeof(HTTX_BUFFER),
					&pHTTXContext->data_dma,
					"HTTxContext");
		if (Status != NDIS_STATUS_SUCCESS)
			goto err;
	}



	/* MGMT_RING_SIZE*/

	/* Allocate MGMT ring descriptor's memory*/
	pAd->MgmtDescRing.AllocSize = MGMT_RING_SIZE * sizeof(TX_CONTEXT);
	pAd->MgmtDescRing.AllocVa =
		kmalloc(pAd->MgmtDescRing.AllocSize, GFP_ATOMIC);
	if (pAd->MgmtDescRing.AllocVa == NULL) {
		DBGPRINT_ERR(("Failed to allocate a big buffer for MgmtDescRing!\n"));
		Status = NDIS_STATUS_RESOURCES;
		goto err;
	}



	/* NullContext*/

	memset(pNullContext, 0, sizeof(TX_CONTEXT));
	/*Allocate URB*/
	Status = RTMPAllocUsbBulkBufStruct(udev,
				&pNullContext->pUrb,
				(PVOID *)&pNullContext->TransferBuffer,
				sizeof(TX_BUFFER),
				&pNullContext->data_dma,
				"TxNullContext");
	if (Status != NDIS_STATUS_SUCCESS)
		goto err;


	/* PsPollContext*/

	memset(pPsPollContext, 0, sizeof(TX_CONTEXT));
	/*Allocate URB*/
	Status = RTMPAllocUsbBulkBufStruct(udev,
				&pPsPollContext->pUrb,
				(PVOID *)&pPsPollContext->TransferBuffer,
				sizeof(TX_BUFFER),
				&pPsPollContext->data_dma,
				"TxPsPollContext");
	if (Status != NDIS_STATUS_SUCCESS)
		goto err;



	/* Init receive data structures and related parameters*/
	for (i = 0; i < (RX_RING_SIZE); i++) {
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);

		/*Allocate URB*/
		Status = RTMPAllocUsbBulkBufStruct(udev,
				&pRxContext->pUrb,
				(PVOID *)&pRxContext->TransferBuffer,
				MAX_RXBULK_SIZE,
				&pRxContext->data_dma,
				"RxContext");
		if (Status != NDIS_STATUS_SUCCESS)
			goto err;

	}

	/* Init command response event related parameters */
	Status = RTMPAllocUsbBulkBufStruct(udev,
				&pCmdRspEventContext->pUrb,
				(PVOID *)&pCmdRspEventContext->CmdRspBuffer,
				CMD_RSP_BULK_SIZE,
				&pCmdRspEventContext->data_dma,
				"CmdRspEventContext");

	if (Status != NDIS_STATUS_SUCCESS)
		goto err;


	memset(&pAd->FragFrame, 0, sizeof(FRAGMENT_FRAME));
	pAd->FragFrame.pFragPacket =  dev_alloc_skb(RX_BUFFER_NORMSIZE);

	if (pAd->FragFrame.pFragPacket == NULL)
		Status = NDIS_STATUS_RESOURCES;

	DBGPRINT_S(Status, ("<-- RTMPAllocTxRxRingMemory, Status=%x\n", Status));
	return Status;

err:
	Status = NDIS_STATUS_RESOURCES;
	RTMPFreeTxRxRingMemory(pAd);

	return Status;
}


int RTMPInitTxRxRingMemory(struct rtmp_adapter *pAd)
{
	INT				num;
	int 	Status;

	/* Init the CmdQ and CmdQLock*/
	spin_lock_init(&pAd->CmdQLock);
	spin_lock_bh(&pAd->CmdQLock);
	RTInitializeCmdQ(&pAd->CmdQ);
	spin_unlock_bh(&pAd->CmdQLock);


	spin_lock_init(&pAd->MLMEBulkOutLock);
	spin_lock_init(&pAd->BulkInLock);
	spin_lock_init(&pAd->CmdRspLock);
	for (num = 0 ; num < 6; num++)
		spin_lock_init(&pAd->BulkOutLock[num]);

	for (num = 0; num < NUM_OF_TX_RING; num++)
		spin_lock_init(&pAd->TxContextQueueLock[num]);

	NICInitRecv(pAd);


	Status = NICInitTransmit(pAd);

	return Status;

}

/*
 * ========================================================================
 * Routine Description:
 *    Enable DMA.
 *
 * Arguments:
 *	*pAd				the raxx interface data pointer
 *
 * Return Value:
 *	None
 *
 * Note:
 * ========================================================================
 */

VOID RT28XXDMAEnable(struct rtmp_adapter *pAd)
{
	u32 val;

	/* for last packet, PBF might use more than limited, so minus 2 to prevent from error */

	val = FIELD_PREP(MT_USB_DMA_CFG_RX_BULK_AGG_LMT, (MAX_RXBULK_SIZE / 1024) - 3) |
	      FIELD_PREP(MT_USB_DMA_CFG_RX_BULK_AGG_TOUT, 0x80) |
	      MT_USB_DMA_CFG_RX_BULK_EN |
	      MT_USB_DMA_CFG_TX_BULK_EN;

	/* USB1.1 do not use bulk in aggregation */
	if ((pAd->in_max_packet >= 512) && (pAd->usb_ctl.usb_aggregation))
		val |= MT_USB_DMA_CFG_RX_BULK_AGG_EN;
	else {
		DBGPRINT(RT_DEBUG_OFF, ("disable usb rx aggregagion\n"));
	}

	if (IS_MT76x2U(pAd))
		val |= MT_USB_DMA_CFG_RX_DROP_OR_PAD;

	mt76u_sys_write(pAd, U3DMA_WLCFG, val);
}

/********************************************************************
  *
  *	2870 Beacon Update Related functions.
  *
  ********************************************************************/

/*
 * ========================================================================
 * Routine Description:
 *    Write Beacon buffer to Asic.
 *
 * Arguments:
 *	*pAd				the raxx interface data pointer
 *
 * Return Value:
 *	None
 *
 * Note:
 * ========================================================================
 */

VOID RT28xx_UpdateBeaconToAsic(struct rtmp_adapter *pAd,
	INT apidx, ULONG FrameLen, ULONG UpdatePos)
{
	u8 *pBeaconFrame = NULL;
	u8 *ptr;
	UINT i, padding;
	BEACON_SYNC_STRUCT *pBeaconSync = pAd->CommonCfg.pBeaconSync;
	bool bBcnReq = false;
	u8 bcn_idx = 0;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

#ifdef CONFIG_AP_SUPPORT
	if ((apidx < pAd->ApCfg.BssidNum) &&
	    (apidx < MAX_MBSSID_NUM(pAd))) {
		bcn_idx = pAd->ApCfg.MBSSID[apidx].BcnBufIdx;
		pBeaconFrame = (u8 *) pAd->ApCfg.MBSSID[apidx].BeaconBuf;
		bBcnReq = BeaconTransmitRequired(pAd, apidx, &pAd->ApCfg.MBSSID[apidx]);
	}
#endif /* CONFIG_AP_SUPPORT */

	if (pBeaconFrame == NULL) {
		DBGPRINT(RT_DEBUG_ERROR, ("pBeaconFrame is NULL!\n"));
		return;
	}

	if (pBeaconSync == NULL) {
		DBGPRINT(RT_DEBUG_ERROR, ("pBeaconSync is NULL!\n"));
		return;
	}

	if (bBcnReq == false) {
		/* when the ra interface is down, do not send its beacon frame */
		/* clear all zero */
		for (i = 0; i < TXWISize; i += 4)
			mt76u_reg_write(pAd,
					pAd->BeaconOffset[bcn_idx] + i,
					0x00);

		pBeaconSync->BeaconBitMap &= (~(BEACON_BITMAP_MASK & (1 << bcn_idx)));
		memset(pBeaconSync->BeaconTxWI[bcn_idx], 0, TXWISize);
	} else {
		ptr = (u8 *)&pAd->BeaconTxWI;
#ifdef __BIG_ENDIAN
		RTMPWIEndianChange(pAd, ptr, TYPE_TXWI);
#endif
		if (!memcmp(pBeaconSync->BeaconTxWI[bcn_idx], &pAd->BeaconTxWI, TXWISize) == false) {
			/* If BeaconTxWI changed, we need to rewrite the TxWI for the Beacon frames.*/
			pBeaconSync->BeaconBitMap &= (~(BEACON_BITMAP_MASK & (1 << bcn_idx)));
			memmove(pBeaconSync->BeaconTxWI[bcn_idx], &pAd->BeaconTxWI, TXWISize);
		}

		if ((pBeaconSync->BeaconBitMap & (1 << bcn_idx)) != (1 << bcn_idx)) {
			for (i = 0; i < TXWISize; i += 4) {
				u32 dword;

				dword =  *ptr +
					(*(ptr + 1) << 8) +
					(*(ptr + 2) << 16) +
					(*(ptr + 3) << 24);

				mt76u_reg_write(pAd,
						pAd->BeaconOffset[bcn_idx] + i,
						dword);
				ptr += 4;
			}
		}

		ptr = pBeaconSync->BeaconBuf[bcn_idx];
		padding = (FrameLen & 0x03);	/* ULLI : 4 bytes */
		memset((u8 *)(pBeaconFrame + FrameLen), 0, padding);
		FrameLen += padding;

		/* ULLI : WTH ???, are programmers lazy of pointers ?? */

		for (i = 0 ; i < FrameLen /*HW_BEACON_OFFSET*/; i += 4) {
			if (!memcmp(ptr, pBeaconFrame, 4) == false) {
				u32 dword;

				memmove(ptr, pBeaconFrame, 4);

				dword =  *ptr +
					(*(ptr + 1) << 8) +
					(*(ptr + 2) << 16) +
					(*(ptr + 3) << 24);

				mt76u_reg_write(pAd,
						pAd->BeaconOffset[bcn_idx] + TXWISize + i,
						dword);
			}
			ptr += 4;
			pBeaconFrame += 4;
		}


		pBeaconSync->BeaconBitMap |= (1 << bcn_idx);

		/* For AP interface, set the DtimBitOn so that we can send Bcast/Mcast frame out after this beacon frame.*/
#ifdef CONFIG_AP_SUPPORT
		{
			ptr = (u8 *) (pAd->ApCfg.MBSSID[apidx].BeaconBuf + pAd->ApCfg.MBSSID[apidx].TimIELocationInBeacon);
			if ((*(ptr + 4)) & 0x01)
				pBeaconSync->DtimBitOn |= (1 << apidx);
			else
				pBeaconSync->DtimBitOn &= ~(1 << apidx);
		}
#endif /* CONFIG_AP_SUPPORT */
}

}


VOID RTUSBBssBeaconStop(struct rtmp_adapter *pAd)
{
	BEACON_SYNC_STRUCT	*pBeaconSync;
	int i, offset;
	bool	Cancelled = true;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

	pBeaconSync = pAd->CommonCfg.pBeaconSync;
	if (pBeaconSync && pBeaconSync->EnableBeacon) {
		INT NumOfBcn = 0;

#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
			NumOfBcn = pAd->ApCfg.BssidNum + MAX_MESH_NUM;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
			NumOfBcn = MAX_MESH_NUM;
#endif /* CONFIG_STA_SUPPORT */

		RTMPCancelTimer(&pAd->CommonCfg.BeaconUpdateTimer, &Cancelled);

		for (i = 0; i < NumOfBcn; i++) {
			memset(pBeaconSync->BeaconBuf[i], 0, HW_BEACON_OFFSET);
			memset(pBeaconSync->BeaconTxWI[i], 0, TXWISize);

			for (offset = 0; offset < HW_BEACON_OFFSET; offset += 4)
				mt76u_reg_write(pAd,
						pAd->BeaconOffset[i] + offset,
						0x00);

			pBeaconSync->CapabilityInfoLocationInBeacon[i] = 0;
			pBeaconSync->TimIELocationInBeacon[i] = 0;
		}
		pBeaconSync->BeaconBitMap = 0;
		pBeaconSync->DtimBitOn = 0;
	}
}


VOID RTUSBBssBeaconStart(struct rtmp_adapter *pAd)
{
	int apidx;
	BEACON_SYNC_STRUCT	*pBeaconSync;
	UINT8 TXWISize = pAd->chipCap.TXWISize;
/*	LARGE_INTEGER 	tsfTime, deltaTime;*/

	pBeaconSync = pAd->CommonCfg.pBeaconSync;
	if (pBeaconSync && pBeaconSync->EnableBeacon) {
		INT NumOfBcn = 0;

#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
			NumOfBcn = pAd->ApCfg.BssidNum + MAX_MESH_NUM;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
			NumOfBcn = MAX_MESH_NUM;
#endif /* CONFIG_STA_SUPPORT */

		for (apidx = 0; apidx < NumOfBcn; apidx++) {
			u8 CapabilityInfoLocationInBeacon = 0;
			u8 TimIELocationInBeacon = 0;
#ifdef CONFIG_AP_SUPPORT
			IF_DEV_CONFIG_OPMODE_ON_AP(pAd) {
					CapabilityInfoLocationInBeacon = pAd->ApCfg.MBSSID[apidx].CapabilityInfoLocationInBeacon;
					TimIELocationInBeacon = pAd->ApCfg.MBSSID[apidx].TimIELocationInBeacon;
			}
#endif /* CONFIG_AP_SUPPORT */

			memset(pBeaconSync->BeaconBuf[apidx], 0, HW_BEACON_OFFSET);
			pBeaconSync->CapabilityInfoLocationInBeacon[apidx] = CapabilityInfoLocationInBeacon;
			pBeaconSync->TimIELocationInBeacon[apidx] = TimIELocationInBeacon;
			memset(pBeaconSync->BeaconTxWI[apidx], 0, TXWISize);
		}
		pBeaconSync->BeaconBitMap = 0;
		pBeaconSync->DtimBitOn = 0;
		pAd->CommonCfg.BeaconUpdateTimer.Repeat = true;

		pAd->CommonCfg.BeaconAdjust = 0;
		pAd->CommonCfg.BeaconFactor = 0xffffffff / (pAd->CommonCfg.BeaconPeriod << 10);
		pAd->CommonCfg.BeaconRemain = (0xffffffff % (pAd->CommonCfg.BeaconPeriod << 10)) + 1;
		DBGPRINT(RT_DEBUG_TRACE, ("RTUSBBssBeaconStart:BeaconFactor=%d, BeaconRemain=%d!\n",
									pAd->CommonCfg.BeaconFactor, pAd->CommonCfg.BeaconRemain));
		RTMPSetTimer(&pAd->CommonCfg.BeaconUpdateTimer, 10 /*pAd->CommonCfg.BeaconPeriod*/);

	}
}


VOID RTUSBBssBeaconInit(struct rtmp_adapter *pAd)
{
	BEACON_SYNC_STRUCT	*pBeaconSync;
	int i, j;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

	pAd->CommonCfg.pBeaconSync =
		kmalloc(sizeof(BEACON_SYNC_STRUCT), GFP_ATOMIC);

	if (pAd->CommonCfg.pBeaconSync) {
		pBeaconSync = pAd->CommonCfg.pBeaconSync;
		memset(pBeaconSync, 0, sizeof(BEACON_SYNC_STRUCT));
		for (i = 0; i < HW_BEACON_MAX_COUNT(pAd); i++) {
			memset(pBeaconSync->BeaconBuf[i], 0, HW_BEACON_OFFSET);
			pBeaconSync->CapabilityInfoLocationInBeacon[i] = 0;
			pBeaconSync->TimIELocationInBeacon[i] = 0;
			pBeaconSync->BeaconTxWI[i] =
				kmalloc(TXWISize, GFP_ATOMIC);
			if (pBeaconSync->BeaconTxWI[i])
				memset(pBeaconSync->BeaconTxWI[i], 0, TXWISize);
			else
				goto error2;
		}
		pBeaconSync->BeaconBitMap = 0;

		/*RTMPInitTimer(pAd, &pAd->CommonCfg.BeaconUpdateTimer, GET_TIMER_FUNCTION(BeaconUpdateExec), pAd, true);*/
		pBeaconSync->EnableBeacon = true;
	} else
		goto error1;

	return;

error2:
	for (j = 0; j < i; j++)
		kfree(pBeaconSync->BeaconTxWI[j]);

	kfree(pAd->CommonCfg.pBeaconSync);

error1:
	DBGPRINT(RT_DEBUG_ERROR, ("memory are not available\n"));
}


VOID RTUSBBssBeaconExit(struct rtmp_adapter *pAd)
{
	BEACON_SYNC_STRUCT	*pBeaconSync;
	bool	Cancelled = true;
	int i;

	if (pAd->CommonCfg.pBeaconSync) {
		pBeaconSync = pAd->CommonCfg.pBeaconSync;
		pBeaconSync->EnableBeacon = false;
		RTMPCancelTimer(&pAd->CommonCfg.BeaconUpdateTimer, &Cancelled);
		pBeaconSync->BeaconBitMap = 0;

		for (i = 0; i < HW_BEACON_MAX_COUNT(pAd); i++) {
			memset(pBeaconSync->BeaconBuf[i], 0, HW_BEACON_OFFSET);
			pBeaconSync->CapabilityInfoLocationInBeacon[i] = 0;
			pBeaconSync->TimIELocationInBeacon[i] = 0;
			kfree(pBeaconSync->BeaconTxWI[i]);
		}

		kfree(pAd->CommonCfg.pBeaconSync);
		pAd->CommonCfg.pBeaconSync = NULL;
	}
}


/*
 *   ========================================================================
 *   Routine Description:
 *       For device work as AP mode but didn't have TBTT interrupt event, we need a mechanism
 *       to update the beacon context in each Beacon interval. Here we use a periodical timer
 *       to simulate the TBTT interrupt to handle the beacon context update.
 *
 *   Arguments:
 *       SystemSpecific1         - Not used.
 *       FunctionContext         - Pointer to our Adapter context.
 *       SystemSpecific2         - Not used.
 *       SystemSpecific3         - Not used.
 *
 *   Return Value:
 *       None
 *
 *   ========================================================================
 */
VOID BeaconUpdateExec(PVOID SystemSpecific1, PVOID FunctionContext,
		PVOID SystemSpecific2, PVOID SystemSpecific3)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)FunctionContext;
	LARGE_INTEGER	tsfTime_a;		/*, tsfTime_b, deltaTime_exp, deltaTime_ab;*/
	uint32_t	delta, delta2MS, period2US, remain, remain_low, remain_high;
/*	bool			positive;*/

	if (pAd->CommonCfg.IsUpdateBeacon == true) {
		ReSyncBeaconTime(pAd);

#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd) {
			BEACON_SYNC_STRUCT *pBeaconSync = pAd->CommonCfg.pBeaconSync;
			ULONG UpTime;

			/* update channel utilization */
			NdisGetSystemUpTime(&UpTime);

			if (pAd->ApCfg.DtimCount == 0 && pBeaconSync->DtimBitOn) {
				struct os_cookie *pObj;

				pObj = pAd->OS_Cookie;
				RTMP_OS_TASKLET_SCHE(&pObj->tbtt_task);
			}
#ifdef RT_CFG80211_SUPPORT
			RT_CFG80211_BEACON_TIM_UPDATE(pAd);
#else
			APUpdateAllBeaconFrame(pAd);
#endif /* RT_CFG80211_SUPPORT  */
		}
#endif /* CONFIG_AP_SUPPORT */

	}

	tsfTime_a.u.LowPart = mt76u_reg_read(pAd, TSF_TIMER_DW0);
	tsfTime_a.u.HighPart = mt76u_reg_read(pAd, TSF_TIMER_DW1);


	/*
		Calculate next beacon time to wake up to update.

		BeaconRemain = (0xffffffff % (pAd->CommonCfg.BeaconPeriod << 10)) + 1;

		Background: Timestamp (us) % Beacon Period (us) shall be 0 at TBTT

		Formula:	(a+b) mod m = ((a mod m) + (b mod m)) mod m
					(a*b) mod m = ((a mod m) * (b mod m)) mod m

		==> ((HighPart * 0xFFFFFFFF) + LowPart) mod Beacon_Period
		==> (((HighPart * 0xFFFFFFFF) mod Beacon_Period) +
			(LowPart mod (Beacon_Period))) mod Beacon_Period
		==> ((HighPart mod Beacon_Period) * (0xFFFFFFFF mod Beacon_Period)) mod
			Beacon_Period

		Steps:
		1. Calculate the delta time between now and next TBTT;

			delta time = (Beacon Period) - ((64-bit timestamp) % (Beacon Period))

			(1) If no overflow for LowPart, 32-bit, we can calcualte the delta
				time by using LowPart;

				delta time = LowPart % (Beacon Period)

			(2) If overflow for LowPart, we need to care about HighPart value;

				delta time = (BeaconRemain * HighPart + LowPart) % (Beacon Period)

				Ex: if the maximum value is 0x00 0xFF (255), Beacon Period = 100,
					TBTT timestamp will be 100, 200, 300, 400, ...
					when TBTT timestamp is 300 = 1*56 + 44, means HighPart = 1,
					Low Part = 44

		2. Adjust next update time of the timer to (delta time + 10ms).
	*/

	/*positive=getDeltaTime(tsfTime_a, expectedTime, &deltaTime_exp);*/
	period2US = (pAd->CommonCfg.BeaconPeriod << 10);
	remain_high = pAd->CommonCfg.BeaconRemain * tsfTime_a.u.HighPart;
	remain_low = tsfTime_a.u.LowPart % (pAd->CommonCfg.BeaconPeriod << 10);
	remain = (remain_high + remain_low)%(pAd->CommonCfg.BeaconPeriod << 10);
	delta = (pAd->CommonCfg.BeaconPeriod << 10) - remain;

	delta2MS = (delta>>10);
	if (delta2MS > 150) {
		pAd->CommonCfg.BeaconUpdateTimer.TimerValue = 100;
		pAd->CommonCfg.IsUpdateBeacon = false;
	} else {
		pAd->CommonCfg.BeaconUpdateTimer.TimerValue = delta2MS + 10;
		pAd->CommonCfg.IsUpdateBeacon = true;
	}
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd) {
		if ((pAd->CommonCfg.Channel > 14) &&
		    (pAd->CommonCfg.bIEEE80211H == 1) &&
		    (pAd->Dot11_H.RDMode == RD_SWITCHING_MODE))
			ChannelSwitchingCountDownProc(pAd);
	}
#endif /* CONFIG_AP_SUPPORT */
}


/*
 ******************************************************************
 *
 *	2870 Radio on/off Related functions.
 ******************************************************************
*/
VOID RT28xxUsbMlmeRadioOn(struct rtmp_adapter *pAd)
{
	DBGPRINT(RT_DEBUG_TRACE, ("RT28xxUsbMlmeRadioOn()\n"));

	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
		return;

	mt7612u_radio_on(pAd, MLME_RADIO_ON);


	/* Clear Radio off flag*/
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		APStartUp(pAd);
#endif /* CONFIG_AP_SUPPORT */


}


VOID RT28xxUsbMlmeRadioOFF(struct rtmp_adapter *pAd)
{
	DBGPRINT(RT_DEBUG_TRACE, ("RT28xxUsbMlmeRadioOFF()\n"));

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
		return;

#ifdef CONFIG_STA_SUPPORT
	/* Clear PMKID cache.*/
	pAd->StaCfg.SavedPMKNum = 0;
	memset(pAd->StaCfg.SavedPMK, 0, (PMKID_NO * sizeof(BSSID_INFO)));

	/* Link down first if any association exists*/
	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) {
		if (INFRA_ON(pAd) || ADHOC_ON(pAd)) {
			MLME_DISASSOC_REQ_STRUCT DisReq;
			MLME_QUEUE_ELEM *pMsgElem;

			pMsgElem = kmalloc(sizeof(MLME_QUEUE_ELEM), GFP_ATOMIC);
			if (pMsgElem) {
				COPY_MAC_ADDR(&DisReq.Addr, pAd->CommonCfg.Bssid);
				DisReq.Reason =  REASON_DISASSOC_STA_LEAVING;

				pMsgElem->Machine = ASSOC_STATE_MACHINE;
				pMsgElem->MsgType = MT2_MLME_DISASSOC_REQ;
				pMsgElem->MsgLen = sizeof(MLME_DISASSOC_REQ_STRUCT);
				memmove(pMsgElem->Msg, &DisReq, sizeof(MLME_DISASSOC_REQ_STRUCT));

				MlmeDisassocReqAction(pAd, pMsgElem);
				kfree(pMsgElem);

				mdelay(1);
			}
		}
	}
#endif /* CONFIG_STA_SUPPORT */

	/* Set Radio off flag*/
	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd) {
		/* Link down first if any association exists*/
		if (INFRA_ON(pAd) || ADHOC_ON(pAd))
			LinkDown(pAd, false);

		mdelay(10);

		/*==========================================*/
		/* Clean up old bss table*/
		/* because abdroid will get scan table when interface down, so we not clean scan table */
		BssTableInit(&pAd->ScanTab);

	}
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		APStop(pAd);
#endif /* CONFIG_AP_SUPPORT */


	mt7612u_radio_off(pAd, MLME_RADIO_OFF);
}

bool AsicCheckCommandOk(
	IN struct rtmp_adapter *pAd,
	IN u8 	 Command)
{
	uint32_t CmdStatus, CID, i;
	uint32_t ThisCIDMask = 0;
	INT ret;


	ret = down_interruptible(&pAd->reg_atomic);
	if (ret != 0) {
		DBGPRINT(RT_DEBUG_ERROR, ("reg_atomic get failed(ret=%d)\n", ret));
		return false;
	}

	i = 0;
	do {
		CID = mt76u_reg_read(pAd, H2M_MAILBOX_CID);
		if ((CID & CID0MASK) == Command) {
			ThisCIDMask = CID0MASK;
			break;
		} else if ((((CID & CID1MASK)>>8) & 0xff) == Command) {
			ThisCIDMask = CID1MASK;
			break;
		} else if ((((CID & CID2MASK)>>16) & 0xff) == Command) {
			ThisCIDMask = CID2MASK;
			break;
		} else if ((((CID & CID3MASK)>>24) & 0xff) == Command) {
			ThisCIDMask = CID3MASK;
			break;
		}

		udelay(100);
		i++;
	} while (i < 200);

	ret = false;
	CmdStatus = mt76u_reg_read(pAd, H2M_MAILBOX_STATUS);
	if (i < 200) {
		if (((CmdStatus & ThisCIDMask) == 0x1) ||
		    ((CmdStatus & ThisCIDMask) == 0x100) ||
		    ((CmdStatus & ThisCIDMask) == 0x10000) ||
		    ((CmdStatus & ThisCIDMask) == 0x1000000))
			ret = true;
	}
	mt76u_reg_write(pAd, H2M_MAILBOX_STATUS, 0xffffffff);
	mt76u_reg_write(pAd, H2M_MAILBOX_CID, 0xffffffff);

	up(&pAd->reg_atomic);


	return ret;

}


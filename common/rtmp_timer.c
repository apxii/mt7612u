/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2008, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source code is an unpublished work and	the
 * use of a copyright notice does not imply	otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering,	decoding, reverse engineering or in any
 * way altering the	source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

    Module Name:
    rtmp_timer.c

    Abstract:
    task for timer handling

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
    Name          Date            Modification logs
    Shiang Tu	08-28-2008   init version

*/

#include "rt_config.h"


BUILD_TIMER_FUNCTION(MlmePeriodicExec);
/*BUILD_TIMER_FUNCTION(MlmeRssiReportExec);*/
BUILD_TIMER_FUNCTION(AsicRxAntEvalTimeout);
BUILD_TIMER_FUNCTION(APSDPeriodicExec);
BUILD_TIMER_FUNCTION(EnqueueStartForPSKExec);
#ifdef CONFIG_STA_SUPPORT
#endif /* CONFIG_STA_SUPPORT */


BUILD_TIMER_FUNCTION(BeaconUpdateExec);

#ifdef CONFIG_AP_SUPPORT
extern VOID APDetectOverlappingExec(
				IN PVOID SystemSpecific1,
				IN PVOID FunctionContext,
				IN PVOID SystemSpecific2,
				IN PVOID SystemSpecific3);

BUILD_TIMER_FUNCTION(APDetectOverlappingExec);

BUILD_TIMER_FUNCTION(Bss2040CoexistTimeOut);

BUILD_TIMER_FUNCTION(GREKEYPeriodicExec);
BUILD_TIMER_FUNCTION(CMTimerExec);
BUILD_TIMER_FUNCTION(WPARetryExec);
#ifdef AP_SCAN_SUPPORT
BUILD_TIMER_FUNCTION(APScanTimeout);
#endif /* AP_SCAN_SUPPORT */
BUILD_TIMER_FUNCTION(APQuickResponeForRateUpExec);
#ifdef DROP_MASK_SUPPORT
BUILD_TIMER_FUNCTION(drop_mask_timer_action);
#endif /* DROP_MASK_SUPPORT */

#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
BUILD_TIMER_FUNCTION(BeaconTimeout);
BUILD_TIMER_FUNCTION(ScanTimeout);
BUILD_TIMER_FUNCTION(AuthTimeout);
BUILD_TIMER_FUNCTION(AssocTimeout);
BUILD_TIMER_FUNCTION(ReassocTimeout);
BUILD_TIMER_FUNCTION(DisassocTimeout);
BUILD_TIMER_FUNCTION(LinkDownExec);
BUILD_TIMER_FUNCTION(StaQuickResponeForRateUpExec);
BUILD_TIMER_FUNCTION(WpaDisassocApAndBlockAssoc);


BUILD_TIMER_FUNCTION(RtmpUsbStaAsicForceWakeupTimeout);


#endif /* CONFIG_STA_SUPPORT */




BUILD_TIMER_FUNCTION(eTxBfProbeTimerExec);




#ifdef PEER_DELBA_TX_ADAPT
BUILD_TIMER_FUNCTION(PeerDelBATxAdaptTimeOut);
#endif /* PEER_DELBA_TX_ADAPT */

#ifdef RTMP_TIMER_TASK_SUPPORT
static void RtmpTimerQHandle(struct rtmp_adapter *pAd)
{
#if 1
	/* Queue handling based on created timers list */
	int status;
	RALINK_TIMER_STRUCT	*pTimer;
	unsigned long	irqFlag;
	RTMP_OS_TASK *pTask;

	struct list_head *pRscList = &pAd->RscTimerCreateList;
	struct list_head *ptr, *se;

	pTask = &pAd->timerTask;
	while(!RTMP_OS_TASK_IS_KILLED(pTask))
	{
		if (RtmpOSTaskWait(pAd, pTask, &status) == false)
		{
			RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);
			break;
		}

		if (pAd->TimerQ.status == RTMP_TASK_STAT_STOPED)
			break;

		list_for_each_safe(ptr, se, pRscList) {
			RALINK_TIMER_STRUCT *pTimer = list_entry(ptr, RALINK_TIMER_STRUCT, list);

			if (pTimer->Fired){
			/*
				DBGPRINT(RT_DEBUG_TRACE,("%s: Timer %p fired, Repeat=%u, State=%u, TimerValue=%lu\n",__FUNCTION__,
					 pTimer, pTimer->Repeat, pTimer->State, pTimer->TimerValue));
			*/
				pTimer->Fired = false;
				if ((pTimer->handle != NULL) && (!pAd->PM_FlgSuspend))
					pTimer->handle(NULL, (PVOID) pTimer->cookie, NULL, pTimer);
				if ((pTimer->Repeat) && (pTimer->State == false))
					RTMP_OS_Add_Timer(&pTimer->TimerObj, pTimer->TimerValue);
			}
		}

		if (status != 0)
		{
			pAd->TimerQ.status = RTMP_TASK_STAT_STOPED;
			RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);
			break;
		}
	}
#else
	int status;
	RALINK_TIMER_STRUCT	*pTimer;
	RTMP_TIMER_TASK_ENTRY	*pEntry;
	unsigned long	irqFlag;
	RTMP_OS_TASK *pTask;

	pTask = &pAd->timerTask;
	while(!RTMP_OS_TASK_IS_KILLED(pTask))
	{
		pTimer = NULL;

		if (RtmpOSTaskWait(pAd, pTask, &status) == false)
		{
			RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);
			break;
		}

		if (pAd->TimerQ.status == RTMP_TASK_STAT_STOPED)
			break;

		/* event happened.*/
		while(pAd->TimerQ.pQHead)
		{
			spin_lock_irqsave(&pAd->TimerQLock, irqFlag);
			pEntry = pAd->TimerQ.pQHead;
			if (pEntry)
			{
				pTimer = pEntry->pRaTimer;

				/* update pQHead*/
				pAd->TimerQ.pQHead = pEntry->pNext;
				if (pEntry == pAd->TimerQ.pQTail)
					pAd->TimerQ.pQTail = NULL;

				/* return this queue entry to timerQFreeList.*/
				pEntry->pNext = pAd->TimerQ.pQPollFreeList;
				pAd->TimerQ.pQPollFreeList = pEntry;
			}
			spin_unlock_irqrestore(&pAd->TimerQLock, irqFlag);

			if (pTimer)
			{
				if ((pTimer->handle != NULL) && (!pAd->PM_FlgSuspend))
					pTimer->handle(NULL, (PVOID) pTimer->cookie, NULL, pTimer);
				if ((pTimer->Repeat) && (pTimer->State == false))
					RTMP_OS_Add_Timer(&pTimer->TimerObj, pTimer->TimerValue);
			}
		}

		if (status != 0)
		{
			pAd->TimerQ.status = RTMP_TASK_STAT_STOPED;
			RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);
			break;
		}
	}
#endif
}


INT RtmpTimerQThread(
	IN void *Context)
{
	RTMP_OS_TASK	*pTask;
	struct rtmp_adapter *pAd = NULL;


	pTask = (RTMP_OS_TASK *)Context;
	pAd = (struct rtmp_adapter *)RTMP_OS_TASK_DATA_GET(pTask);

	if (pAd == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR,( "%s:: pAd is NULL!\n",__FUNCTION__));
		return 0;
	}

	RtmpTimerQHandle(pAd);

	DBGPRINT(RT_DEBUG_TRACE,( "<---%s\n",__FUNCTION__));

	return 0;
}


RTMP_TIMER_TASK_ENTRY *RtmpTimerQInsert(
	IN struct rtmp_adapter *pAd,
	IN RALINK_TIMER_STRUCT *pTimer)
{
	RTMP_TIMER_TASK_ENTRY *pQNode = NULL, *pQTail;
	unsigned long irqFlags;
	RTMP_OS_TASK	*pTask = &pAd->timerTask;

	spin_lock_irqsave(&pAd->TimerQLock, irqFlags);
	if (pAd->TimerQ.status & RTMP_TASK_CAN_DO_INSERT)
	{
		if(pAd->TimerQ.pQPollFreeList)
		{
			pQNode = pAd->TimerQ.pQPollFreeList;
			pAd->TimerQ.pQPollFreeList = pQNode->pNext;

			pQNode->pRaTimer = pTimer;
			pQNode->pNext = NULL;

			pQTail = pAd->TimerQ.pQTail;
			if (pAd->TimerQ.pQTail != NULL)
				pQTail->pNext = pQNode;
			pAd->TimerQ.pQTail = pQNode;
			if (pAd->TimerQ.pQHead == NULL)
				pAd->TimerQ.pQHead = pQNode;
		}
	}
	spin_unlock_irqrestore(&pAd->TimerQLock, irqFlags);

	if (pQNode)
	{
		RTMP_OS_TASK_WAKE_UP(pTask);
	}

	return pQNode;
}


bool RtmpTimerQRemove(
	IN struct rtmp_adapter *pAd,
	IN RALINK_TIMER_STRUCT *pTimer)
{
	RTMP_TIMER_TASK_ENTRY *pNode, *pPrev = NULL;
	unsigned long irqFlags;

	spin_lock_irqsave(&pAd->TimerQLock, irqFlags);
	if (pAd->TimerQ.status >= RTMP_TASK_STAT_INITED)
	{
		pNode = pAd->TimerQ.pQHead;
		while (pNode)
		{
			if (pNode->pRaTimer == pTimer)
				break;
			pPrev = pNode;
			pNode = pNode->pNext;
		}

		/* Now move it to freeList queue.*/
		if (pNode)
		{
			if (pNode == pAd->TimerQ.pQHead)
				pAd->TimerQ.pQHead = pNode->pNext;
			if (pNode == pAd->TimerQ.pQTail)
				pAd->TimerQ.pQTail = pPrev;
			if (pPrev != NULL)
				pPrev->pNext = pNode->pNext;

			/* return this queue entry to timerQFreeList.*/
			pNode->pNext = pAd->TimerQ.pQPollFreeList;
			pAd->TimerQ.pQPollFreeList = pNode;
		}
	}
	spin_unlock_irqrestore(&pAd->TimerQLock, irqFlags);

	return true;
}


void RtmpTimerQExit(struct rtmp_adapter *pAd)
{
	RTMP_TIMER_TASK_ENTRY *pTimerQ;
	unsigned long irqFlags;

	spin_lock_irqsave(&pAd->TimerQLock, irqFlags);
	while (pAd->TimerQ.pQHead)
	{
		pTimerQ = pAd->TimerQ.pQHead;
		pAd->TimerQ.pQHead = pTimerQ->pNext;
		/* remove the timeQ*/
	}
	pAd->TimerQ.pQPollFreeList = NULL;
	kfree(pAd->TimerQ.pTimerQPoll);
	pAd->TimerQ.pQTail = NULL;
	pAd->TimerQ.pQHead = NULL;
	pAd->TimerQ.status = RTMP_TASK_STAT_STOPED;
	spin_unlock_irqrestore(&pAd->TimerQLock, irqFlags);
}


void RtmpTimerQInit(struct rtmp_adapter *pAd)
{
	int 	i;
	RTMP_TIMER_TASK_ENTRY *pQNode, *pEntry;
	unsigned long irqFlags;

	spin_lock_init(&pAd->TimerQLock);

	memset(&pAd->TimerQ, 0, sizeof(pAd->TimerQ));

	pAd->TimerQ.pTimerQPoll =
		kmalloc(sizeof(RTMP_TIMER_TASK_ENTRY) * TIMER_QUEUE_SIZE_MAX,
			GFP_ATOMIC);
	if (pAd->TimerQ.pTimerQPoll) {
		pEntry = NULL;
		pQNode = (RTMP_TIMER_TASK_ENTRY *)pAd->TimerQ.pTimerQPoll;
		memset(pAd->TimerQ.pTimerQPoll, 0, sizeof(RTMP_TIMER_TASK_ENTRY) * TIMER_QUEUE_SIZE_MAX);

		spin_lock_irqsave(&pAd->TimerQLock, irqFlags);
		for (i = 0 ;i <TIMER_QUEUE_SIZE_MAX; i++)
		{
			pQNode->pNext = pEntry;
			pEntry = pQNode;
			pQNode++;
		}
		pAd->TimerQ.pQPollFreeList = pEntry;
		pAd->TimerQ.pQHead = NULL;
		pAd->TimerQ.pQTail = NULL;
		pAd->TimerQ.status = RTMP_TASK_STAT_INITED;
		spin_unlock_irqrestore(&pAd->TimerQLock, irqFlags);
	}
}
#endif /* RTMP_TIMER_TASK_SUPPORT */


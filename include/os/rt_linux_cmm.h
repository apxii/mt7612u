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
	rt_linux_cmm.h

    Abstract:
	Common OS structure/definition in LINUX whatever OS ABL.

    Revision History:
    Who          When          What
    ---------    ----------    ----------------------------------------------
 */


#ifndef __RT_LINUX_CMM_H__
#define __RT_LINUX_CMM_H__


typedef struct _OS_RSTRUC  {
	u8 *pContent; /* pointer to real structure content */
} OS_RSTRUC;


/*****************************************************************************
 *	OS task related data structure and definitions
 ******************************************************************************/
#define RTMP_OS_TASK_INIT(__pTask, __pTaskName, __pAd)		\
	RtmpOSTaskInit(__pTask, __pTaskName, __pAd);

/* rt_linux.h */
#define RTMP_OS_TASK				OS_TASK

#define RTMP_OS_TASK_GET(__pTask)							\
	(__pTask)

#define RTMP_OS_TASK_DATA_GET(__pTask)						\
	((__pTask)->priv)

#define RTMP_OS_TASK_IS_KILLED(__pTask)						\
	((__pTask)->task_killed)

#ifdef KTHREAD_SUPPORT
#define RTMP_OS_TASK_WAKE_UP(__pTask)						\
	WAKE_UP(pTask);
#else
#define RTMP_OS_TASK_WAKE_UP(__pTask)						\
	up(&(pTask)->taskSema);
#endif /* KTHREAD_SUPPORT */

#ifdef KTHREAD_SUPPORT
#define RTMP_OS_TASK_LEGALITY(__pTask)						\
	if ((__pTask)->kthread_task != NULL)
#else
#define RTMP_OS_TASK_LEGALITY(__pTask)						\
	CHECK_PID_LEGALITY((__pTask)->taskPID)
#endif /* KTHREAD_SUPPORT */



/*****************************************************************************
 * Timer related definitions and data structures.
 ******************************************************************************/

/* rt_linux.h */
#define NDIS_MINIPORT_TIMER			OS_NDIS_MINIPORT_TIMER
#define RTMP_OS_TIMER				OS_TIMER

#define RTMP_OS_FREE_TIMER(__pAd)
#define RTMP_OS_FREE_LOCK(__pAd)
#define RTMP_OS_FREE_TASKLET(__pAd)
#define RTMP_OS_FREE_TASK(__pAd)
#define RTMP_OS_FREE_SEM(__pAd)
#define RTMP_OS_FREE_ATOMIC(__pAd)



/*****************************************************************************
 *	OS file operation related data structure definitions
 ******************************************************************************/
/* if you add any new type, please also modify RtmpOSFileOpen() */
#define RTMP_FILE_RDONLY			0x0F01
#define RTMP_FILE_WRONLY			0x0F02
#define RTMP_FILE_CREAT				0x0F03
#define RTMP_FILE_TRUNC				0x0F04


/* rt_linux.h */
#define RTMP_OS_FS_INFO				OS_FS_INFO


/*****************************************************************************
 *	OS semaphore related data structure and definitions
 ******************************************************************************/

#define RTMP_INT_LOCK							OS_INT_LOCK
#define RTMP_INT_UNLOCK							OS_INT_UNLOCK
#define RTMP_OS_ATOMIC							atomic_t


#define RtmpMLMEUp								OS_RTMP_MlmeUp

#define RTMP_OS_ATMOIC_INIT(__pAtomic, __pAtomicList)
#define RTMP_OS_ATMOIC_DESTROY(__pAtomic)
#define RTMP_THREAD_PID_KILL(__PID)				KILL_THREAD_PID(__PID, SIGTERM, 1)

/*****************************************************************************
 *	OS task related data structure and definitions
 ******************************************************************************/

/* rt_linux.h */
#define RTMP_NET_TASK_STRUCT		OS_NET_TASK_STRUCT
#define PRTMP_NET_TASK_STRUCT		POS_NET_TASK_STRUCT

#define RTMP_OS_TASKLET_SCHE(__pTasklet)							\
		tasklet_hi_schedule(__pTasklet)
#define RTMP_OS_TASKLET_INIT(__pAd, __pTasklet, __pFunc, __Data)	\
		tasklet_init(__pTasklet, __pFunc, __Data)
#define RTMP_OS_TASKLET_KILL(__pTasklet)							\
		tasklet_kill(__pTasklet)

#define RTMP_NET_TASK_DATA_ASSIGN(__Tasklet, __Data)		\
	(__Tasklet)->data = (unsigned long)__Data


/*****************************************************************************
 *	OS definition related data structure and definitions
 ******************************************************************************/

#define RTMP_OS_PCI_VENDOR_ID			PCI_VENDOR_ID
#define RTMP_OS_PCI_DEVICE_ID			PCI_DEVICE_ID

#define ra_dma_addr_t					dma_addr_t

#define PCI_MAP_SINGLE					RtmpDrvPciMapSingle


/***********************************************************************************
 *	Others
 ***********************************************************************************/
#define APCLI_IF_UP_CHECK(pAd, ifidx) (RtmpOSNetDevIsUp((pAd)->ApCfg.ApCliTab[(ifidx)].wdev.if_dev) == true)


#define RTMP_OS_NETDEV_SET_PRIV		RtmpOsSetNetDevPriv
#define RTMP_OS_NETDEV_GET_PRIV		RtmpOsGetNetDevPriv
#define RTMP_OS_NETDEV_SET_WDEV		RtmpOsSetNetDevWdev
#define RTMP_OS_NETDEV_GET_WDEV		RtmpOsGetNetDevWdev
#define RT_DEV_PRIV_FLAGS_GET		RtmpDevPrivFlagsGet
#define RT_DEV_PRIV_FLAGS_SET		RtmpDevPrivFlagsSet

#endif /* __RT_LINUX_CMM_H__ */


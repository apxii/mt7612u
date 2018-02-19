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
	rtmp_init_inf.c

	Abstract:
	Miniport generic portion header file

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/
#include	"rt_config.h"



int rt28xx_init(struct rtmp_adapter *pAd)
{
	int Status;
	UINT index = 0;

	if (!pAd)
		return false;

	if (rtmp_asic_top_init(pAd) != true)
		goto err1;

	DBGPRINT(RT_DEBUG_TRACE, ("MAC[Ver:Rev=0x%08x : 0x%08x]\n",
				pAd->mac_rev, pAd->asic_rev));

	if (mt7612u_mcu_sys_init(pAd) != true)
		goto err1;


	/* reset Adapter flags*/
	RTMP_CLEAR_FLAGS(pAd);

	if (MAX_LEN_OF_MAC_TABLE > MAX_AVAILABLE_CLIENT_WCID(pAd))
	{
		DBGPRINT(RT_DEBUG_ERROR, ("MAX_LEN_OF_MAC_TABLE can not be larger than MAX_AVAILABLE_CLIENT_WCID!!!!\n"));
		goto err1;
	}

#ifdef CONFIG_AP_SUPPORT
	/* Init BssTab & ChannelInfo tabbles for auto channel select.*/
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		AutoChBssTableInit(pAd);
		ChannelInfoInit(pAd);
	}
#endif /* CONFIG_AP_SUPPORT */

	/* Allocate BA Reordering memory*/
	if (ba_reordering_resource_init(pAd, MAX_REORDERING_MPDU_NUM) != true)
		goto err1;

	Status = RTMPInitTxRxRingMemory(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("RTMPAllocTxRxMemory failed, Status[=0x%08x]\n", Status));
		goto err2;
	}

	pAd->CommonCfg.bMultipleIRP = false;
	if (pAd->CommonCfg.bMultipleIRP)
		pAd->CommonCfg.NumOfBulkInIRP = RX_RING_SIZE;
	else
		pAd->CommonCfg.NumOfBulkInIRP = 1;

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE);

	/* initialize MLME*/

	Status = RtmpMgmtTaskInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
		goto err3;

	Status = MlmeInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("MlmeInit failed, Status[=0x%08x]\n", Status));
		goto err4;
	}

	/* Initialize pAd->StaCfg, pAd->ApCfg, pAd->CommonCfg to manufacture default*/
	UserCfgInit(pAd);


	Status = RtmpNetTaskInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
		goto err5;

	CfgInitHook(pAd);

	Status = MeasureReqTabInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("MeasureReqTabInit failed, Status[=0x%08x]\n",Status));
		goto err6;
	}
	Status = TpcReqTabInit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("TpcReqTabInit failed, Status[=0x%08x]\n",Status));
		goto err6;
	}

	/*
		WiFi system operation mode setting base on following partitions:
		1. Parameters from config file
		2. Hardware cap from EEPROM
		3. Chip capabilities in code
	*/
	pAd->RfIcType = RFIC_UNKNOWN;
	Status = RTMPReadParametersHook(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("RTMPReadParametersHook failed, Status[=0x%08x]\n",Status));
		goto err6;
	}
	DBGPRINT(RT_DEBUG_OFF, ("1. Phy Mode = %d\n", pAd->CommonCfg.PhyMode));

	/* We should read EEPROM for all cases */
	NICReadEEPROMParameters(pAd);
	DBGPRINT(RT_DEBUG_OFF, ("2. Phy Mode = %d\n", pAd->CommonCfg.PhyMode));

	/* After operation mode is finialized, init the AP or STA mode */
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		APInitialize(pAd);

	}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd) {
		STAInitialize(pAd);
	}
#endif /* CONFIG_STA_SUPPORT */


	/*Init Ba Capability parameters.*/
	pAd->CommonCfg.DesiredHtPhy.MpduDensity = (u8)pAd->CommonCfg.BACapability.field.MpduDensity;
	pAd->CommonCfg.DesiredHtPhy.AmsduEnable = (unsigned short)pAd->CommonCfg.BACapability.field.AmsduEnable;
	pAd->CommonCfg.DesiredHtPhy.AmsduSize = (unsigned short)pAd->CommonCfg.BACapability.field.AmsduSize;
	pAd->CommonCfg.DesiredHtPhy.MimoPs = (unsigned short)pAd->CommonCfg.BACapability.field.MMPSmode;
	/* UPdata to HT IE*/
	pAd->CommonCfg.HtCapability.HtCapInfo.MimoPs = (unsigned short)pAd->CommonCfg.BACapability.field.MMPSmode;
	pAd->CommonCfg.HtCapability.HtCapInfo.AMsduSize = (unsigned short)pAd->CommonCfg.BACapability.field.AmsduSize;
	pAd->CommonCfg.HtCapability.HtCapParm.MpduDensity = (u8)pAd->CommonCfg.BACapability.field.MpduDensity;

	/* after reading Registry, we now know if in AP mode or STA mode */
	DBGPRINT(RT_DEBUG_OFF, ("3. Phy Mode = %d\n", pAd->CommonCfg.PhyMode));


	/*
		All settle down, now it's time to init asic related parameters
	*/
	/* Init the hardware, we need to init asic before read registry, otherwise mac register will be reset */
	Status = NICInitializeAdapter(pAd, true);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT_ERR(("NICInitializeAdapter failed, Status[=0x%08x]\n", Status));
		if (Status != NDIS_STATUS_SUCCESS)
		goto err6;
	}


	NICInitAsicFromEEPROM(pAd);



	/*
		Do necessary calibration after ASIC initialized
		this's chip variant and may different for different chips
	*/

#ifdef CONFIG_STA_SUPPORT
	/* Initialize the frequency calibration*/
	if (pAd->chipCap.FreqCalibrationSupport)
		FrequencyCalibration(pAd);
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	if (pAd->chipCap.FreqCalibrationSupport)
		InitFrequencyCalibration(pAd);
#endif /* CONFIG_STA_SUPPORT */

	/* Set PHY to appropriate mode and will update the ChannelListNum in this function */
	RTMPSetPhyMode(pAd, pAd->CommonCfg.PhyMode);
	if (pAd->ChannelListNum == 0)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("Wrong configuration. No valid channel found. Check \"ContryCode\" and \"ChannelGeography\" setting.\n"));
		goto err6;
	}

	DBGPRINT(RT_DEBUG_OFF, ("MCS Set = %02x %02x %02x %02x %02x\n", pAd->CommonCfg.HtCapability.MCSSet[0],
           pAd->CommonCfg.HtCapability.MCSSet[1], pAd->CommonCfg.HtCapability.MCSSet[2],
           pAd->CommonCfg.HtCapability.MCSSet[3], pAd->CommonCfg.HtCapability.MCSSet[4]));

	/* APInitialize(pAd);*/

	/*
		Some modules init must be called before APStartUp().
		Or APStartUp() will make up beacon content and call
		other modules API to get some information to fill.
	*/



	if (pAd && (Status != NDIS_STATUS_SUCCESS))
	{
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
		{
			RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE);
		}
	}
	else if (pAd)
	{
		/* Microsoft HCT require driver send a disconnect event after driver initialization.*/
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);

		DBGPRINT(RT_DEBUG_TRACE, ("NDIS_STATUS_MEDIA_DISCONNECT Event B!\n"));

#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		{
			if (pAd->ApCfg.bAutoChannelAtBootup || (pAd->CommonCfg.Channel == 0))
			{
				/* Enable Interrupt first due to we need to scan channel to receive beacons.*/
				RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS);
				RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_REMOVE_IN_PROGRESS);

				/* Support multiple BulkIn IRP,*/
				/* the value on pAd->CommonCfg.NumOfBulkInIRP may be large than 1.*/

				for(index=0; index<pAd->CommonCfg.NumOfBulkInIRP; index++)
				{
					RTUSBBulkReceive(pAd);
					DBGPRINT(RT_DEBUG_TRACE, ("RTUSBBulkReceive!\n" ));
				}

				/* Now Enable RxTx*/
				RTMPEnableRxTx(pAd);
#ifdef MT762x
				// TODO: shiang-usw, check why MT76x2 don't need to set this flag here!
				if (!IS_MT762x(pAd))
#endif /* MT762x */
					RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP);

				/* Let BBP register at 20MHz to do scan */
				mt7612u_bbp_set_bw(pAd, BW_20);

				/* Now we can receive the beacon and do the listen beacon*/
				/* use default BW to select channel*/
				pAd->CommonCfg.Channel = AP_AUTO_CH_SEL(pAd, pAd->ApCfg.AutoChannelAlg);
				pAd->ApCfg.bAutoChannelAtBootup = false;
			}

			/* If WMODE_CAP_N(phymode) and BW=40 check extension channel, after select channel  */
			N_ChannelCheck(pAd);

        		/*
         			We only do this Overlapping BSS Scan when system up, for the
				other situation of channel changing, we depends on station's
				report to adjust ourself.
			*/
			if (pAd->CommonCfg.bForty_Mhz_Intolerant == true)
			{
				DBGPRINT(RT_DEBUG_TRACE, ("Disable 20/40 BSSCoex Channel Scan(BssCoex=%d, 40MHzIntolerant=%d)\n",
											pAd->CommonCfg.bBssCoexEnable,
											pAd->CommonCfg.bForty_Mhz_Intolerant));
			}
			else if(pAd->CommonCfg.bBssCoexEnable == true)
			{
				DBGPRINT(RT_DEBUG_TRACE, ("Enable 20/40 BSSCoex Channel Scan(BssCoex=%d)\n",
							pAd->CommonCfg.bBssCoexEnable));
				APOverlappingBSSScan(pAd);
			}

			RTMP_11N_D3_TimerInit(pAd);
/*			RTMPInitTimer(pAd, &pAd->CommonCfg.Bss2040CoexistTimer, GET_TIMER_FUNCTION(Bss2040CoexistTimeOut), pAd, false);*/


			APStartUp(pAd);
			DBGPRINT(RT_DEBUG_OFF, ("Main bssid = %02x:%02x:%02x:%02x:%02x:%02x\n",
						PRINT_MAC(pAd->ApCfg.MBSSID[BSS0].wdev.bssid)));

			if (IS_MT76x2U(pAd)) {
				mt76x2_reinit_agc_gain(pAd, pAd->hw_cfg.cent_ch);
				mt76x2_reinit_hi_lna_gain(pAd, pAd->hw_cfg.cent_ch);
				mt76x2_get_agc_gain(pAd, true);
			}
		}
#endif /* CONFIG_AP_SUPPORT */


		RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS);
		RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_REMOVE_IN_PROGRESS);

		/* Support multiple BulkIn IRP,*/
		/* the value on pAd->CommonCfg.NumOfBulkInIRP may be large than 1.*/
		for (index=0; index<pAd->CommonCfg.NumOfBulkInIRP; index++)
		{
			RTUSBBulkReceive(pAd);
			DBGPRINT(RT_DEBUG_TRACE, ("RTUSBBulkReceive!\n" ));
		}
	}

	/* Set up the Mac address*/
#if defined(CONFIG_AP_SUPPORT) || defined(CONFIG_STA_SUPPORT)
	DBGPRINT(RT_DEBUG_OFF, ("MT7612U MAC Address = %02x:%02x:%02x:%02x:%02x:%02x\n",
						PRINT_MAC(pAd->CurrentAddress)));
	memmove(&pAd->StaCfg.wdev.if_addr[0], &pAd->CurrentAddress[0], MAC_ADDR_LEN);
	RtmpOSNetDevAddrSet(pAd->OpMode, pAd->net_dev, &pAd->CurrentAddress[0], (u8 *)(pAd->StaCfg.dev_name));
#endif /* CONFIG_STA_SUPPORT */


	/* assign function pointers*/


#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{

#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
		/* send wireless event to wpa_supplicant for infroming interface up.*/
		RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_CUSTOM, RT_INTERFACE_UP, NULL, NULL, 0);
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* WPA_SUPPLICANT_SUPPORT */

	}
#endif /* CONFIG_STA_SUPPORT */

	/* auto-fall back settings */



	if (pAd->CommonCfg.ETxBfTimeout)
	{
		mt76u_reg_write(pAd, TX_TXBF_CFG_3, pAd->CommonCfg.ETxBfTimeout);
	}




	DBGPRINT_S(Status, ("<==== rt28xx_init, Status=%x\n", Status));

	return true;

err6:


	MeasureReqTabExit(pAd);
	TpcReqTabExit(pAd);
err5:
	RtmpNetTaskExit(pAd);
	UserCfgExit(pAd);
err4:
	MlmeHalt(pAd);
	RTMP_AllTimerListRelease(pAd);
err3:
	RtmpMgmtTaskExit(pAd);
err2:
	RTMPResetTxRxRingMemory(pAd);

err1:

	mt7612u_mcu_ctrl_exit(pAd);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd) {
	/* Free BssTab & ChannelInfo tabbles.*/
	AutoChBssTableDestroy(pAd);
	ChannelInfoDestroy(pAd);
	}
#endif /* CONFIG_AP_SUPPORT */


	if(pAd->mpdu_blk_pool.mem)
		kfree(pAd->mpdu_blk_pool.mem); /* free BA pool*/

	DBGPRINT(RT_DEBUG_ERROR, ("!!! rt28xx init fail !!!\n"));
	return false;
}


VOID RTMPDrvOpen(struct rtmp_adapter *pAdSrc)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdSrc;

	RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_MCU_SLEEP);

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP);

//+++Add by shiang for debug
	DBGPRINT(RT_DEBUG_OFF, ("%s(1):Check if PDMA is idle!\n", __FUNCTION__));
	mt7612u_wait_pdma_usecs(pAd, 5, 10);
//---Add by shiang for debug

#ifdef CONFIG_STA_SUPPORT
	/*
		To reduce connection time,
		do auto reconnect here instead of waiting STAMlmePeriodicExec to do auto reconnect.
	*/
	if (pAd->OpMode == OPMODE_STA)
		MlmeAutoReconnectLastSSID(pAd);
#endif /* CONFIG_STA_SUPPORT */
//+++Add by shiang for debug
	DBGPRINT(RT_DEBUG_OFF, ("%s(2):Check if PDMA is idle!\n", __FUNCTION__));
	mt7612u_wait_pdma_usecs(pAd, 5, 10);
//---Add by shiang for debug

#ifdef CONFIG_AP_SUPPORT
#ifdef MULTI_CLIENT_SUPPORT
	pAd->CommonCfg.txRetryCfg = 0;

	{
		uint32_t TxRtyCfg;

		mt76u_reg_read(pAd, TX_RTY_CFG, &TxRtyCfg);
		pAd->CommonCfg.txRetryCfg = TxRtyCfg;
	}
#endif /* MULTI_CLIENT_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */
}


VOID RTMPDrvClose(struct rtmp_adapter *pAd, struct net_device *net_dev)
{
	uint32_t i = 0;


#ifdef RT_CFG80211_SUPPORT
#ifdef CONFIG_AP_SUPPORT
		if (pAd->cfg80211_ctrl.isCfgInApMode == RT_CMD_80211_IFTYPE_AP && RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP))
		{
			CFG80211DRV_DisableApInterface(pAd);
			pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_STATION;
		}
#endif /* CONFIG_AP_SUPPORT */
#endif/*RT_CFG80211_SUPPORT*/

#ifdef BB_SOC
	 if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
	 {
	 	DBGPRINT(RT_DEBUG_TRACE, ("Radio_ON first....\n"));
    		MlmeRadioOn(pAd);
	 }
#endif /* BB_SOC */

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{

		/* If dirver doesn't wake up firmware here,*/
		/* NICLoadFirmware will hang forever when interface is up again.*/
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE))
        {
		    AsicForceWakeup(pAd, true);
        }

		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_REMOVE_IN_PROGRESS);

	}
#endif /* CONFIG_STA_SUPPORT */

	{
		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);
	}

	pAd->CommonCfg.bCountryFlag = false;




	for (i = 0 ; i < NUM_OF_TX_RING; i++)
	{
		while (pAd->DeQueueRunning[i] == true)
		{
			DBGPRINT(RT_DEBUG_TRACE, ("Waiting for TxQueue[%d] done..........\n", i));
			mdelay(1);
		}
	}

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		bool Cancelled = false;
		RTMPCancelTimer(&pAd->CommonCfg.BeaconUpdateTimer, &Cancelled);

		if (pAd->CommonCfg.Bss2040CoexistFlag & BSS_2040_COEXIST_TIMER_FIRED)
		{
			RTMPCancelTimer(&pAd->CommonCfg.Bss2040CoexistTimer, &Cancelled);
			pAd->CommonCfg.Bss2040CoexistFlag  = 0;
		}

		/* PeriodicTimer already been canceled by MlmeHalt() API.*/
		/*RTMPCancelTimer(&pAd->PeriodicTimer,	&Cancelled);*/
	}
#endif /* CONFIG_AP_SUPPORT */

	/* Stop Mlme state machine*/
	MlmeHalt(pAd);

	/* Close net tasklets*/
	RtmpNetTaskExit(pAd);


#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		MacTableReset(pAd);
			MlmeRadioOff(pAd);
	}
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{

		/* Shutdown Access Point function, release all related resources */
		APShutdown(pAd);

	}
#endif /* CONFIG_AP_SUPPORT */

	MeasureReqTabExit(pAd);
	TpcReqTabExit(pAd);

	/* Close kernel threads*/
	RtmpMgmtTaskExit(pAd);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		/* must after RtmpMgmtTaskExit(); Or pAd->pChannelInfo will be NULL */
		/* Free BssTab & ChannelInfo tabbles.*/
		AutoChBssTableDestroy(pAd);
		ChannelInfoDestroy(pAd);
	}
#endif /* CONFIG_AP_SUPPORT */

	/* Free IRQ*/
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE);
	}

	/* Free Ring or USB buffers*/
	RTMPResetTxRxRingMemory(pAd);

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);

	/* Free BA reorder resource*/
	ba_reordering_resource_release(pAd);

	UserCfgExit(pAd); /* must after ba_reordering_resource_release */

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_START_UP);

	/* clear MAC table */
	/* TODO: do not clear spin lock, such as fLastChangeAccordingMfbLock */
	memset(&pAd->MacTab, 0, sizeof(struct mt7612u_mac_table));

	/* release all timers */
	mdelay(2);
	RTMP_AllTimerListRelease(pAd);
}

VOID RTMPInfClose(struct rtmp_adapter *pAd)
{
#ifdef CONFIG_AP_SUPPORT
	pAd->ApCfg.MBSSID[MAIN_MBSSID].bBcnSntReq = false;

	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		/* kick out all STAs behind the bss.*/
		MbssKickOutStas(pAd, MAIN_MBSSID, REASON_DISASSOC_INACTIVE);
	}

	APMakeAllBssBeacon(pAd);
	APUpdateAllBeaconFrame(pAd);
#endif /* CONFIG_AP_SUPPORT */



#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		if (INFRA_ON(pAd) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
		{
			MLME_DISASSOC_REQ_STRUCT	DisReq;
			MLME_QUEUE_ELEM *MsgElem;

			MsgElem = kmalloc(sizeof(MLME_QUEUE_ELEM), GFP_ATOMIC);
			if (MsgElem) {
				COPY_MAC_ADDR(DisReq.Addr, pAd->CommonCfg.Bssid);
				DisReq.Reason =  REASON_DEAUTH_STA_LEAVING;

				MsgElem->Machine = ASSOC_STATE_MACHINE;
				MsgElem->MsgType = MT2_MLME_DISASSOC_REQ;
				MsgElem->MsgLen = sizeof(MLME_DISASSOC_REQ_STRUCT);
				memmove(MsgElem->Msg, &DisReq, sizeof(MLME_DISASSOC_REQ_STRUCT));

				/* Prevent to connect AP again in STAMlmePeriodicExec*/
				pAd->MlmeAux.AutoReconnectSsidLen= 32;
				memset(pAd->MlmeAux.AutoReconnectSsid, 0, pAd->MlmeAux.AutoReconnectSsidLen);

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
				MlmeDisassocReqAction(pAd, MsgElem);
				kfree(MsgElem);
			}

			mdelay(1);
		}

#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
		/* send wireless event to wpa_supplicant for infroming interface down.*/
		RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_CUSTOM, RT_INTERFACE_DOWN, NULL, NULL, 0);
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */

		if (pAd->StaCfg.wpa_supplicant_info.pWpsProbeReqIe)
		{
			kfree(pAd->StaCfg.wpa_supplicant_info.pWpsProbeReqIe);
			pAd->StaCfg.wpa_supplicant_info.pWpsProbeReqIe = NULL;
			pAd->StaCfg.wpa_supplicant_info.WpsProbeReqIeLen = 0;
		}

		if (pAd->StaCfg.wpa_supplicant_info.pWpaAssocIe)
		{
			kfree(pAd->StaCfg.wpa_supplicant_info.pWpaAssocIe);
			pAd->StaCfg.wpa_supplicant_info.pWpaAssocIe = NULL;
			pAd->StaCfg.wpa_supplicant_info.WpaAssocIeLen = 0;
		}
#endif /* WPA_SUPPLICANT_SUPPORT */


	}
#endif /* CONFIG_STA_SUPPORT */
}




struct net_device *RtmpPhyNetDevMainCreate(struct rtmp_adapter *pAd)
{
	struct net_device *pDevNew;
	uint32_t MC_RowID = 0, IoctlIF = 0;
	char *dev_name;

#ifdef HOSTAPD_SUPPORT
	IoctlIF = pAd->IoctlIF;
#endif /* HOSTAPD_SUPPORT */

	dev_name = get_dev_name_prefix(pAd, INT_MAIN);
	pDevNew = RtmpOSNetDevCreate((int32_t)MC_RowID, (uint32_t *)&IoctlIF,
					INT_MAIN, 0, sizeof(struct mt_dev_priv), dev_name);

	printk("%s: %s: pAd=%p, pDevNew=%p, dev_name=%s\n", __FILE__, __func__, pAd, pDevNew, dev_name);

#ifdef HOSTAPD_SUPPORT
	pAd->IoctlIF = IoctlIF;
#endif /* HOSTAPD_SUPPORT */

	return pDevNew;
}

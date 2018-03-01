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

	Abstract:

	Revision History:
	Who 		When			What
	--------	----------		----------------------------------------------
*/

#include "rt_config.h"

#ifdef SCAN_SUPPORT
static INT scan_ch_restore(struct rtmp_adapter *pAd, u8 OpMode)
{
#ifdef CONFIG_STA_SUPPORT
	unsigned short Status;
#endif /* CONFIG_STA_SUPPORT */
	INT bw, ch;

	if (WMODE_CAP(pAd->CommonCfg.PhyMode, WMODE_AC) &&
		(pAd->CommonCfg.Channel > 14) &&
		(pAd->hw_cfg.bbp_bw == BW_80) &&
		(pAd->CommonCfg.vht_bw == VHT_BW_80) &&
		(pAd->CommonCfg.vht_cent_ch != pAd->CommonCfg.CentralChannel)) {
		pAd->CommonCfg.vht_cent_ch = vht_cent_ch_freq(pAd, pAd->CommonCfg.Channel);
	}

	if (pAd->hw_cfg.bbp_bw == BW_80)
		pAd->hw_cfg.cent_ch = pAd->CommonCfg.vht_cent_ch;
	else
		pAd->hw_cfg.cent_ch = pAd->CommonCfg.CentralChannel;

	if (pAd->CommonCfg.BBPCurrentBW != pAd->hw_cfg.bbp_bw)
		mt7612u_bbp_set_bw(pAd, pAd->hw_cfg.bbp_bw);

	AsicSwitchChannel(pAd, pAd->hw_cfg.cent_ch, false);
	AsicLockChannel(pAd, pAd->hw_cfg.cent_ch);

	ch = pAd->hw_cfg.cent_ch;

	switch(pAd->CommonCfg.BBPCurrentBW)
	{
		case BW_80:
			bw = 80;
			break;
		case BW_40:
			bw = 40;
			break;
		case BW_10:
			bw = 10;
			break;
		case BW_20:
		default:
			bw =20;
			break;
	}
	DBGPRINT(RT_DEBUG_TRACE, ("SYNC - End of SCAN, restore to %dMHz channel %d, Total BSS[%02d]\n",
				bw, ch, pAd->ScanTab.BssNr));

#ifdef CONFIG_STA_SUPPORT
	if (OpMode == OPMODE_STA)
	{
		/*
		If all peer Ad-hoc clients leave, driver would do LinkDown and LinkUp.
		In LinkUp, CommonCfg.Ssid would copy SSID from MlmeAux.
		To prevent SSID is zero or wrong in Beacon, need to recover MlmeAux.SSID here.
		*/
		if (ADHOC_ON(pAd))
		{
			memset(pAd->MlmeAux.Ssid, 0, MAX_LEN_OF_SSID);
			pAd->MlmeAux.SsidLen = pAd->CommonCfg.SsidLen;
			memmove(pAd->MlmeAux.Ssid, pAd->CommonCfg.Ssid, pAd->CommonCfg.SsidLen);
		}

		/*
		To prevent data lost.
		Send an NULL data with turned PSM bit on to current associated AP before SCAN progress.
		Now, we need to send an NULL data with turned PSM bit off to AP, when scan progress done
		*/
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED) && (INFRA_ON(pAd)))
		{
			RTMPSendNullFrame(pAd,
								pAd->CommonCfg.TxRate,
								(OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) ? true:false),
								pAd->CommonCfg.bAPSDForcePowerSave ? PWR_SAVE : pAd->StaCfg.Psm);
			DBGPRINT(RT_DEBUG_TRACE, ("%s -- Send null frame\n", __FUNCTION__));
		}

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
        if (pAd->ApCfg.ApCliTab[MAIN_MBSSID].Valid && RTMP_CFG80211_VIF_P2P_CLI_ON(pAd))
        {
                DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_NULL: PWR_ACTIVE SCAN_END\n"));
                RT_CFG80211_P2P_CLI_SEND_NULL_FRAME(pAd, PWR_ACTIVE);
        }
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */

		/* keep the latest scan channel, could be 0 for scan complete, or other channel*/
		pAd->StaCfg.LastScanChannel = pAd->MlmeAux.Channel;

		pAd->StaCfg.ScanChannelCnt = 0;

		/* Suspend scanning and Resume TxData for Fast Scanning*/
		if ((pAd->MlmeAux.Channel != 0) &&
		(pAd->StaCfg.bImprovedScan))	/* it is scan pending*/
		{
			pAd->Mlme.SyncMachine.CurrState = SCAN_PENDING;
			Status = MLME_SUCCESS;
			DBGPRINT(RT_DEBUG_WARN, ("bFastRoamingScan ~~~ Get back to send data ~~~\n"));

			RTMPResumeMsduTransmission(pAd);
		}
		else
		{
			pAd->StaCfg.BssNr = pAd->ScanTab.BssNr;
			pAd->StaCfg.bImprovedScan = false;

			pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
			Status = MLME_SUCCESS;
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_SCAN_CONF, 2, &Status, 0);
			RTMP_MLME_HANDLER(pAd);
		}

	}
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	if (OpMode == OPMODE_AP)
	{
		pAd->Mlme.ApSyncMachine.CurrState = AP_SYNC_IDLE;
		RTMPResumeMsduTransmission(pAd);

#ifdef CON_WPS
		if (pAd->conWscStatus != CON_WPS_STATUS_DISABLED)
		{
			MlmeEnqueue(pAd, AP_SYNC_STATE_MACHINE, APMT2_MLME_SCAN_COMPLETE, 0, NULL,0 );
			RTMP_MLME_HANDLER(pAd);
		}
#endif /* CON_WPS*/

		/* iwpriv set auto channel selection*/
		/* scanned all channels*/
		if (pAd->ApCfg.bAutoChannelAtBootup==true)
		{
			pAd->CommonCfg.Channel = SelectBestChannel(pAd, pAd->ApCfg.AutoChannelAlg);
			pAd->ApCfg.bAutoChannelAtBootup = false;
			N_ChannelCheck(pAd);
			APStop(pAd);
			APStartUp(pAd);
		}

		if (((pAd->CommonCfg.Channel > 14) &&
			(pAd->CommonCfg.bIEEE80211H == true) &&
			RadarChannelCheck(pAd, pAd->CommonCfg.Channel)))
		{
			if (pAd->Dot11_H.InServiceMonitorCount)
			{
				pAd->Dot11_H.RDMode = RD_NORMAL_MODE;
				AsicEnableBssSync(pAd);
			}
			else
			{
				pAd->Dot11_H.RDMode = RD_SILENCE_MODE;
			}
		}
		else
		{
			AsicEnableBssSync(pAd);
		}
	}

#endif /* CONFIG_AP_SUPPORT */


	return true;
}



static INT scan_active(struct rtmp_adapter *pAd, u8 OpMode, u8 ScanType)
{
	u8 *frm_buf = NULL;
	HEADER_802_11 Hdr80211;
	ULONG FrameLen = 0;
	u8 SsidLen = 0;
#ifdef CONFIG_STA_SUPPORT
	unsigned short Status;
#endif /* CONFIG_STA_SUPPORT */


	frm_buf = kmalloc(MGMT_DMA_BUFFER_SIZE, GFP_ATOMIC);
	if (frm_buf == NULL) {
		DBGPRINT(RT_DEBUG_TRACE, ("SYNC - ScanNextChannel() allocate memory fail\n"));
#ifdef CONFIG_STA_SUPPORT
		if (OpMode == OPMODE_STA)
		{
			pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
			Status = MLME_FAIL_NO_RESOURCE;
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_SCAN_CONF, 2, &Status, 0);
		}
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
		if (OpMode == OPMODE_AP)
			pAd->Mlme.ApSyncMachine.CurrState = AP_SYNC_IDLE;
#endif /* CONFIG_AP_SUPPORT */
		return false;
	}

	if (ScanType == SCAN_2040_BSS_COEXIST)
	{
		DBGPRINT(RT_DEBUG_INFO, ("SYNC - SCAN_2040_BSS_COEXIST !! Prepare to send Probe Request\n"));
	}

	/* There is no need to send broadcast probe request if active scan is in effect.*/
	SsidLen = 0;
#ifndef APCLI_CONNECTION_TRIAL
	if ((ScanType == SCAN_ACTIVE) || (ScanType == FAST_SCAN_ACTIVE)
		)
		SsidLen = pAd->MlmeAux.SsidLen;
#endif /* APCLI_CONNECTION_TRIAL */

	{
#ifdef CONFIG_AP_SUPPORT
		/*IF_DEV_CONFIG_OPMODE_ON_AP(pAd) */
		if (OpMode == OPMODE_AP)
		{
			MgtMacHeaderInitExt(pAd, &Hdr80211, SUBTYPE_PROBE_REQ, 0, BROADCAST_ADDR,
								pAd->ApCfg.MBSSID[0].wdev.bssid,
								BROADCAST_ADDR);
		}
#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
		/*IF_DEV_CONFIG_OPMODE_ON_STA(pAd) */
		if (OpMode == OPMODE_STA)
		{
			MgtMacHeaderInit(pAd, &Hdr80211, SUBTYPE_PROBE_REQ, 0, BROADCAST_ADDR,
								pAd->CurrentAddress,
								BROADCAST_ADDR);
		}
#endif /* CONFIG_STA_SUPPORT */

		MakeOutgoingFrame(frm_buf,               &FrameLen,
						  sizeof(HEADER_802_11),    &Hdr80211,
						  1,                        &SsidIe,
						  1,                        &SsidLen,
						  SsidLen,			        pAd->MlmeAux.Ssid,
						  1,                        &SupRateIe,
						  1,                        &pAd->CommonCfg.SupRateLen,
						  pAd->CommonCfg.SupRateLen,  pAd->CommonCfg.SupRate,
						  END_OF_ARGS);

		if (pAd->CommonCfg.ExtRateLen)
		{
			ULONG Tmp;
			MakeOutgoingFrame(frm_buf + FrameLen,            &Tmp,
							  1,                                &ExtRateIe,
							  1,                                &pAd->CommonCfg.ExtRateLen,
							  pAd->CommonCfg.ExtRateLen,          pAd->CommonCfg.ExtRate,
							  END_OF_ARGS);
			FrameLen += Tmp;
		}
	}
	if (WMODE_CAP_N(pAd->CommonCfg.PhyMode))
	{
		ULONG	Tmp;
		u8 HtLen;
		u8 BROADCOM[4] = {0x0, 0x90, 0x4c, 0x33};
#ifdef __BIG_ENDIAN
		HT_CAPABILITY_IE HtCapabilityTmp;
#endif
		if (pAd->bBroadComHT == true)
		{
			HtLen = pAd->MlmeAux.HtCapabilityLen + 4;
#ifdef __BIG_ENDIAN
			memmove(&HtCapabilityTmp, &pAd->MlmeAux.HtCapability, SIZE_HT_CAP_IE);
			*(unsigned short *)(&HtCapabilityTmp.HtCapInfo) = SWAP16(*(unsigned short *)(&HtCapabilityTmp.HtCapInfo));
#ifdef UNALIGNMENT_SUPPORT
			{
				EXT_HT_CAP_INFO extHtCapInfo;

				memmove((u8 *)(&extHtCapInfo), (u8 *)(&HtCapabilityTmp.ExtHtCapInfo), sizeof(EXT_HT_CAP_INFO));
				*(unsigned short *)(&extHtCapInfo) = cpu2le16(*(unsigned short *)(&extHtCapInfo));
				memmove((u8 *)(&HtCapabilityTmp.ExtHtCapInfo), (u8 *)(&extHtCapInfo), sizeof(EXT_HT_CAP_INFO));
			}
#else
			*(unsigned short *)(&HtCapabilityTmp.ExtHtCapInfo) = cpu2le16(*(unsigned short *)(&HtCapabilityTmp.ExtHtCapInfo));
#endif /* UNALIGNMENT_SUPPORT */

			MakeOutgoingFrame(frm_buf + FrameLen,          &Tmp,
							1,                                &WpaIe,
							1,                                &HtLen,
							4,                                &BROADCOM[0],
							pAd->MlmeAux.HtCapabilityLen,     &HtCapabilityTmp,
							END_OF_ARGS);
#else
			MakeOutgoingFrame(frm_buf + FrameLen,          &Tmp,
							1,                                &WpaIe,
							1,                                &HtLen,
							4,                                &BROADCOM[0],
							pAd->MlmeAux.HtCapabilityLen,     &pAd->MlmeAux.HtCapability,
							END_OF_ARGS);
#endif /* __BIG_ENDIAN */
		}
		else
		{
			HtLen = sizeof(HT_CAPABILITY_IE);
#ifdef __BIG_ENDIAN
			memmove(&HtCapabilityTmp, &pAd->CommonCfg.HtCapability, SIZE_HT_CAP_IE);
			*(unsigned short *)(&HtCapabilityTmp.HtCapInfo) = SWAP16(*(unsigned short *)(&HtCapabilityTmp.HtCapInfo));
#ifdef UNALIGNMENT_SUPPORT
			{
				EXT_HT_CAP_INFO extHtCapInfo;

				memmove((u8 *)(&extHtCapInfo), (u8 *)(&HtCapabilityTmp.ExtHtCapInfo), sizeof(EXT_HT_CAP_INFO));
				*(unsigned short *)(&extHtCapInfo) = cpu2le16(*(unsigned short *)(&extHtCapInfo));
				memmove((u8 *)(&HtCapabilityTmp.ExtHtCapInfo), (u8 *)(&extHtCapInfo), sizeof(EXT_HT_CAP_INFO));
			}
#else
			*(unsigned short *)(&HtCapabilityTmp.ExtHtCapInfo) = cpu2le16(*(unsigned short *)(&HtCapabilityTmp.ExtHtCapInfo));
#endif /* UNALIGNMENT_SUPPORT */

			MakeOutgoingFrame(frm_buf + FrameLen,          &Tmp,
							1,                                &HtCapIe,
							1,                                &HtLen,
							HtLen,                            &HtCapabilityTmp,
							END_OF_ARGS);
#else
			MakeOutgoingFrame(frm_buf + FrameLen,          &Tmp,
							1,                                &HtCapIe,
							1,                                &HtLen,
							HtLen,                            &pAd->CommonCfg.HtCapability,
							END_OF_ARGS);
#endif /* __BIG_ENDIAN */
		}
		FrameLen += Tmp;

		if ((pAd->MlmeAux.Channel <= 14) && (pAd->CommonCfg.bBssCoexEnable == true))
		{
			ULONG Tmp;
			HtLen = 1;
			MakeOutgoingFrame(frm_buf + FrameLen,            &Tmp,
							  1,					&ExtHtCapIe,
							  1,					&HtLen,
							  1,          			&pAd->CommonCfg.BSSCoexist2040.word,
							  END_OF_ARGS);

			FrameLen += Tmp;
		}
	}


	if (WMODE_CAP_AC(pAd->CommonCfg.PhyMode) &&
		(pAd->MlmeAux.Channel > 14)) {
		FrameLen += build_vht_ies(pAd, (u8 *)(frm_buf + FrameLen), SUBTYPE_PROBE_REQ);
	}


#ifdef CONFIG_STA_SUPPORT
#ifdef WPA_SUPPLICANT_SUPPORT
	if ((OpMode == OPMODE_STA) &&
		(pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP != WPA_SUPPLICANT_DISABLE) &&
		(pAd->StaCfg.wpa_supplicant_info.WpsProbeReqIeLen != 0))
	{
		ULONG 		WpsTmpLen = 0;

		MakeOutgoingFrame(frm_buf + FrameLen,              &WpsTmpLen,
						pAd->StaCfg.wpa_supplicant_info.WpsProbeReqIeLen,
						pAd->StaCfg.wpa_supplicant_info.pWpsProbeReqIe,
						END_OF_ARGS);

		FrameLen += WpsTmpLen;
	}
#endif /* WPA_SUPPLICANT_SUPPORT */
	if ((OpMode == OPMODE_STA) &&
		(pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP != WPA_SUPPLICANT_DISABLE) &&
		CFG80211DRV_OpsScanRunning(pAd))
	{
		ULONG 		ExtraIeTmpLen = 0;

		MakeOutgoingFrame(frm_buf + FrameLen,              &ExtraIeTmpLen,
						pAd->cfg80211_ctrl.ExtraIeLen,	pAd->cfg80211_ctrl.pExtraIe,
						END_OF_ARGS);

		FrameLen += ExtraIeTmpLen;
	}
#endif /*CONFIG_STA_SUPPORT*/


	MiniportMMRequest(pAd, 0, frm_buf, FrameLen);

#ifdef CONFIG_STA_SUPPORT
	if (OpMode == OPMODE_STA)
	{
		/*
			To prevent data lost.
			Send an NULL data with turned PSM bit on to current associated AP when SCAN in the channel where
			associated AP located.
		*/
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED) &&
			(INFRA_ON(pAd)) &&
			(pAd->CommonCfg.Channel == pAd->MlmeAux.Channel))
		{
			RTMPSendNullFrame(pAd,
						  pAd->CommonCfg.TxRate,
						  (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) ? true:false),
						  PWR_SAVE);
			DBGPRINT(RT_DEBUG_TRACE, ("ScanNextChannel():Send PWA NullData frame to notify the associated AP!\n"));
		}
	}
#endif /* CONFIG_STA_SUPPORT */

	kfree(frm_buf);

	return true;
}


/*
	==========================================================================
	Description:
		Scan next channel
	==========================================================================
 */
VOID ScanNextChannel(struct rtmp_adapter *pAd, u8 OpMode)
{
	u8 ScanType = pAd->MlmeAux.ScanType;
	UINT ScanTimeIn5gChannel = SHORT_CHANNEL_TIME;
	bool ScanPending = false;
	RALINK_TIMER_STRUCT *sc_timer = NULL;
	UINT stay_time = 0;

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		if (MONITOR_ON(pAd))
			return;
	}

	ScanPending = ((pAd->StaCfg.bImprovedScan) && (pAd->StaCfg.ScanChannelCnt>=7));
#endif /* CONFIG_STA_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
	/* Since the Channel List is from Upper layer */
	if (CFG80211DRV_OpsScanRunning(pAd))
		pAd->MlmeAux.Channel = CFG80211DRV_OpsScanGetNextChannel(pAd);
#endif /* CONFIG_STA_SUPPORT */
	if ((pAd->MlmeAux.Channel == 0) || ScanPending)
	{
		scan_ch_restore(pAd, OpMode);
	}
#ifdef CONFIG_STA_SUPPORT
	else if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST) &&
		(OpMode == OPMODE_STA))
	{
		pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
		MlmeCntlConfirm(pAd, MT2_SCAN_CONF, MLME_FAIL_NO_RESOURCE);
	}
#endif /* CONFIG_STA_SUPPORT */
	else
	{
#ifdef CONFIG_STA_SUPPORT
		if (OpMode == OPMODE_STA)
		{
			/* BBP and RF are not accessible in PS mode, we has to wake them up first*/
			if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE))
				AsicForceWakeup(pAd, true);

			/* leave PSM during scanning. otherwise we may lost ProbeRsp & BEACON*/
			if (pAd->StaCfg.Psm == PWR_SAVE)
				RTMP_SET_PSM_BIT(pAd, PWR_ACTIVE);
		}
#endif /* CONFIG_STA_SUPPORT */

		AsicSwitchChannel(pAd, pAd->MlmeAux.Channel, true);
		AsicLockChannel(pAd, pAd->MlmeAux.Channel);

#ifdef CONFIG_STA_SUPPORT
		if (OpMode == OPMODE_STA)
		{
			bool bScanPassive = false;
			if (pAd->MlmeAux.Channel > 14)
			{
				if ((pAd->CommonCfg.bIEEE80211H == 1)
					&& RadarChannelCheck(pAd, pAd->MlmeAux.Channel))
					bScanPassive = true;
			}

			if (bScanPassive)
			{
				ScanType = SCAN_PASSIVE;
				ScanTimeIn5gChannel = MIN_CHANNEL_TIME;
			}
		}

#endif /* CONFIG_STA_SUPPORT */

		/* Check if channel if passive scan under current regulatory domain */
		if (CHAN_PropertyCheck(pAd, pAd->MlmeAux.Channel, CHANNEL_PASSIVE_SCAN) == true)
			ScanType = SCAN_PASSIVE;


		if (OpMode == OPMODE_AP)
			sc_timer = &pAd->MlmeAux.APScanTimer;
		else
			sc_timer = &pAd->MlmeAux.ScanTimer;

		/* We need to shorten active scan time in order for WZC connect issue */
		/* Chnage the channel scan time for CISCO stuff based on its IAPP announcement */
		if (ScanType == FAST_SCAN_ACTIVE)
			stay_time = FAST_ACTIVE_SCAN_TIME;
		else /* must be SCAN_PASSIVE or SCAN_ACTIVE*/
		{
#ifdef CONFIG_STA_SUPPORT
			pAd->StaCfg.ScanChannelCnt++;
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
			if ((OpMode == OPMODE_AP) && (pAd->ApCfg.bAutoChannelAtBootup))
				stay_time = AUTO_CHANNEL_SEL_TIMEOUT;
			else
#endif /* CONFIG_AP_SUPPORT */
			if (WMODE_CAP_2G(pAd->CommonCfg.PhyMode) &&
				WMODE_CAP_5G(pAd->CommonCfg.PhyMode))
			{
				if (pAd->MlmeAux.Channel > 14)
					stay_time = ScanTimeIn5gChannel;
				else
					stay_time = MIN_CHANNEL_TIME;
			}
			else
				stay_time = MAX_CHANNEL_TIME;
		}

		RTMPSetTimer(sc_timer, stay_time);

		if (SCAN_MODE_ACT(ScanType))
		{
			if (scan_active(pAd, OpMode, ScanType) == false)
				return;

#ifdef CONFIG_AP_SUPPORT
#endif /* CONFIG_AP_SUPPORT */
		}

		/* For SCAN_CISCO_PASSIVE, do nothing and silently wait for beacon or other probe reponse*/

#ifdef CONFIG_STA_SUPPORT
		if (OpMode == OPMODE_STA)
			pAd->Mlme.SyncMachine.CurrState = SCAN_LISTEN;
#endif /* CONFIG_STA_SUPPORT */
#ifdef CONFIG_AP_SUPPORT
		if (OpMode == OPMODE_AP)
			pAd->Mlme.ApSyncMachine.CurrState = AP_SCAN_LISTEN;
#endif /* CONFIG_AP_SUPPORT */
	}
}


bool ScanRunning(struct rtmp_adapter *pAd)
{
	bool	rv = false;

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
			if ((pAd->Mlme.SyncMachine.CurrState == SCAN_LISTEN) || (pAd->Mlme.SyncMachine.CurrState == SCAN_PENDING))
				rv = true;
		}
#endif /* CONFIG_STA_SUPPORT */
#ifdef CONFIG_AP_SUPPORT
#ifdef AP_SCAN_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
			rv = ((pAd->Mlme.ApSyncMachine.CurrState == AP_SCAN_LISTEN) ? true : false);
#endif /* AP_SCAN_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */

	return rv;
}


#if defined(CONFIG_STA_SUPPORT)
VOID BuildEffectedChannelList(
	IN struct rtmp_adapter *pAd)
{
	u8 	EChannel[11];
	u8 	i, j, k;
	u8 	UpperChannel = 0, LowerChannel = 0;

	memset(EChannel, 0, 11);
	DBGPRINT(RT_DEBUG_TRACE, ("BuildEffectedChannelList:CtrlCh=%d,CentCh=%d,AuxCtrlCh=%d,AuxExtCh=%d\n",
								pAd->CommonCfg.Channel, pAd->CommonCfg.CentralChannel,
								pAd->MlmeAux.AddHtInfo.ControlChan,
								pAd->MlmeAux.AddHtInfo.AddHtInfo.ExtChanOffset));

	/* 802.11n D4 11.14.3.3: If no secondary channel has been selected, all channels in the frequency band shall be scanned. */
	{
		for (k = 0;k < pAd->ChannelListNum;k++)
		{
			if (pAd->ChannelList[k].Channel <=14 )
			pAd->ChannelList[k].bEffectedChannel = true;
		}
		return;
	}

	i = 0;
	/* Find upper and lower channel according to 40MHz current operation. */
	if (pAd->CommonCfg.CentralChannel < pAd->CommonCfg.Channel)
	{
		UpperChannel = pAd->CommonCfg.Channel;
		LowerChannel = pAd->CommonCfg.CentralChannel-2;
	}
	else if (pAd->CommonCfg.CentralChannel > pAd->CommonCfg.Channel)
	{
		UpperChannel = pAd->CommonCfg.CentralChannel+2;
		LowerChannel = pAd->CommonCfg.Channel;
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("LinkUP 20MHz . No Effected Channel \n"));
		/* Now operating in 20MHz, doesn't find 40MHz effected channels */
		return;
	}

	DeleteEffectedChannelList(pAd);

	DBGPRINT(RT_DEBUG_TRACE, ("BuildEffectedChannelList!LowerChannel ~ UpperChannel; %d ~ %d \n", LowerChannel, UpperChannel));

	/* Find all channels that are below lower channel.. */
	if (LowerChannel > 1)
	{
		EChannel[0] = LowerChannel - 1;
		i = 1;
		if (LowerChannel > 2)
		{
			EChannel[1] = LowerChannel - 2;
			i = 2;
			if (LowerChannel > 3)
			{
				EChannel[2] = LowerChannel - 3;
				i = 3;
			}
		}
	}
	/* Find all channels that are between  lower channel and upper channel. */
	for (k = LowerChannel;k <= UpperChannel;k++)
	{
		EChannel[i] = k;
		i++;
	}
	/* Find all channels that are above upper channel.. */
	if (UpperChannel < 14)
	{
		EChannel[i] = UpperChannel + 1;
		i++;
		if (UpperChannel < 13)
		{
			EChannel[i] = UpperChannel + 2;
			i++;
			if (UpperChannel < 12)
			{
				EChannel[i] = UpperChannel + 3;
				i++;
			}
		}
	}
	/*
	    Total i channels are effected channels.
	    Now find corresponding channel in ChannelList array.  Then set its bEffectedChannel= true
	*/
	for (j = 0;j < i;j++)
	{
		for (k = 0;k < pAd->ChannelListNum;k++)
		{
			if (pAd->ChannelList[k].Channel == EChannel[j])
			{
				pAd->ChannelList[k].bEffectedChannel = true;
				DBGPRINT(RT_DEBUG_TRACE,(" EffectedChannel[%d]( =%d)\n", k, EChannel[j]));
				break;
			}
		}
	}
}


VOID DeleteEffectedChannelList(
	IN struct rtmp_adapter *pAd)
{
	u8 	i;
	/*Clear all bEffectedChannel in ChannelList array. */
 	for (i = 0; i < pAd->ChannelListNum; i++)
	{
		pAd->ChannelList[i].bEffectedChannel = false;
	}
}

VOID ScanParmFill(
	IN struct rtmp_adapter *pAd,
	IN OUT MLME_SCAN_REQ_STRUCT *ScanReq,
	IN STRING Ssid[],
	IN u8 SsidLen,
	IN u8 BssType,
	IN u8 ScanType)
{
	memset(ScanReq->Ssid, 0, MAX_LEN_OF_SSID);
	ScanReq->SsidLen = SsidLen;
	memmove(ScanReq->Ssid, Ssid, SsidLen);
	ScanReq->BssType = BssType;
	ScanReq->ScanType = ScanType;
}
#endif /* defined(CONFIG_STA_SUPPORT) */


#endif /* SCAN_SUPPORT */


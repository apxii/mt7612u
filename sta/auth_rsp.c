/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	auth_rsp.c

	Abstract:

	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
	John		2004-10-1		copy from RT2560
*/
#include "rt_config.h"

/*
    ==========================================================================
    Description:
        authentication state machine init procedure
    Parameters:
        Sm - the state machine

	IRQL = PASSIVE_LEVEL

    ==========================================================================
 */
VOID AuthRspStateMachineInit(
	IN struct rtmp_adapter *pAd,
	IN PSTATE_MACHINE Sm,
	IN STATE_MACHINE_FUNC Trans[])
{
	StateMachineInit(Sm, Trans, MAX_AUTH_RSP_STATE, MAX_AUTH_RSP_MSG,
			 (STATE_MACHINE_FUNC) Drop, AUTH_RSP_IDLE,
			 AUTH_RSP_MACHINE_BASE);

	/* column 1 */
	StateMachineSetAction(Sm, AUTH_RSP_IDLE, MT2_PEER_DEAUTH,
			      (STATE_MACHINE_FUNC) PeerDeauthAction);

	/* column 2 */
	StateMachineSetAction(Sm, AUTH_RSP_WAIT_CHAL, MT2_PEER_DEAUTH,
			      (STATE_MACHINE_FUNC) PeerDeauthAction);

}

/*
    ==========================================================================
    Description:

	IRQL = DISPATCH_LEVEL

    ==========================================================================
*/
VOID PeerAuthSimpleRspGenAndSend(
	IN struct rtmp_adapter *pAd,
	IN PHEADER_802_11 pHdr80211,
	IN unsigned short Alg,
	IN unsigned short Seq,
	IN unsigned short Reason,
	IN unsigned short Status)
{
	HEADER_802_11 AuthHdr;
	ULONG FrameLen = 0;
	u8 *pOutBuffer = NULL;

	if (Reason != MLME_SUCCESS) {
		DBGPRINT(RT_DEBUG_TRACE, ("Peer AUTH fail...\n"));
		return;
	}

	/*Get an unused nonpaged memory */
	pOutBuffer = kmalloc(MGMT_DMA_BUFFER_SIZE, GFP_ATOMIC);
	if (pOutBuffer == NULL)
		return;

	DBGPRINT(RT_DEBUG_TRACE, ("Send AUTH response (seq#2)...\n"));
	MgtMacHeaderInit(pAd, &AuthHdr, SUBTYPE_AUTH, 0, pHdr80211->Addr2,
						pAd->CurrentAddress,
						pAd->MlmeAux.Bssid);
	MakeOutgoingFrame(pOutBuffer, &FrameLen, sizeof (HEADER_802_11),
			  &AuthHdr, 2, &Alg, 2, &Seq, 2, &Reason, END_OF_ARGS);
	MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);
	kfree(pOutBuffer);
}

/*
    ==========================================================================
    Description:

	IRQL = DISPATCH_LEVEL

    ==========================================================================
*/
VOID PeerDeauthAction(
	IN struct rtmp_adapter *pAd,
	IN PMLME_QUEUE_ELEM Elem)
{
	u8 Addr1[MAC_ADDR_LEN];
	u8 Addr2[MAC_ADDR_LEN];
	u8 Addr3[MAC_ADDR_LEN];
	unsigned short Reason;
	bool bDoIterate = false;

	if (PeerDeauthSanity(pAd, Elem->Msg, Elem->MsgLen, Addr1, Addr2, Addr3, &Reason)) {
		if (INFRA_ON(pAd)
		    && (MAC_ADDR_EQUAL(Addr1, pAd->CurrentAddress)
			|| MAC_ADDR_EQUAL(Addr1, BROADCAST_ADDR))
		    && MAC_ADDR_EQUAL(Addr2, pAd->CommonCfg.Bssid)
		    && MAC_ADDR_EQUAL(Addr3, pAd->CommonCfg.Bssid)
		    ) {
			struct rtmp_wifi_dev *wdev = &pAd->StaCfg.wdev;

			DBGPRINT(RT_DEBUG_TRACE,
				 ("AUTH_RSP - receive DE-AUTH from our AP (Reason=%d)\n",
				  Reason));

			if (Reason == REASON_4_WAY_TIMEOUT)
				RTMPSendWirelessEvent(pAd,
						      IW_PAIRWISE_HS_TIMEOUT_EVENT_FLAG,
						      NULL, 0, 0);

			if (Reason == REASON_GROUP_KEY_HS_TIMEOUT)
				RTMPSendWirelessEvent(pAd,
						      IW_GROUP_HS_TIMEOUT_EVENT_FLAG,
						      NULL, 0, 0);


#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
			RtmpOSWrielessEventSend(pAd->net_dev,
						RT_WLAN_EVENT_CGIWAP, -1, NULL,
						NULL, 0);
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */

			/* send wireless event - for deauthentication */
			RTMPSendWirelessEvent(pAd, IW_DEAUTH_EVENT_FLAG, NULL,
					      BSS0, 0);



#ifdef WPA_SUPPLICANT_SUPPORT
			if ((pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP != WPA_SUPPLICANT_DISABLE)
			    && (wdev->AuthMode == Ndis802_11AuthModeWPA2)
			    && (wdev->PortSecured == WPA_802_1X_PORT_SECURED))
				pAd->StaCfg.wpa_supplicant_info.bLostAp = true;
#endif /* WPA_SUPPLICANT_SUPPORT */

			/*
			   Some customer would set AP1 & AP2 same SSID, AuthMode & EncrypType but different WPAPSK,
			   therefore we need to do iterate here.
			 */
			if ((wdev->PortSecured == WPA_802_1X_PORT_NOT_SECURED)
			    &&
			    ((wdev->AuthMode == Ndis802_11AuthModeWPAPSK)
			     || (wdev->AuthMode == Ndis802_11AuthModeWPA2PSK))
			    )
				bDoIterate = true;

			LinkDown(pAd, true);

			if (bDoIterate) {
				pAd->MlmeAux.BssIdx++;
				IterateOnBssTab(pAd);
			}

		}
	} else {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("AUTH_RSP - PeerDeauthAction() sanity check fail\n"));
	}
}

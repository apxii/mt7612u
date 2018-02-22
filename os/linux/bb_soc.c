#include "rt_config.h"

#include "os/bb_soc.h"
//#include "rtmp_timer.h"
//#include "rt_config.h"

/*
    ========================================================================

    Routine Description:
       Trendchip DMT Trainning status detect

    Arguments:
        data                     Point to RTMP_ADAPTER

    Return Value:
        NONE

    Note:

    ========================================================================
*/

static u8 dslStateChg=0;
VOID PeriodicPollingModeDetect(
	IN PVOID SystemSpecific1,
    IN PVOID FunctionContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3){
  unsigned long irqFlags;
  u8 modem_status=0;
  struct rtmp_adapter *pAd = (struct rtmp_adapter *)FunctionContext;
  struct os_cookie *_pObj = pAd->OS_Cookie;

  os_TCIfQuery(0x0002, &modem_status, NULL);

  if ((modem_status==0x08)||(modem_status==0x10))
  {
  	if(!(dslStateChg & (1<<0))){
		RTMP_INT_LOCK(&pAd->irq_lock, irqFlags);
                dslStateChg|=(1<<0);
		RTMP_INT_UNLOCK(&pAd->irq_lock, irqFlags);
	// disable enterrupt
	tc3162_disable_irq(_pObj->pci_dev->irq);
	}
	schedule_work(&pAd->Pollingmode.PollingDataBH);
	//slow down the POLLING MODE DETECT while the dmt in wait init state
  	//pAd->PollingModeDetect.expires = jiffies + POLLING_MODE_DETECT_INTV;

	RTMPModTimer(&pAd->Pollingmode.PollingModeDetect, 10);
  	pAd->Pollingmode.PollingModeDetectRunning = true;
	return;
  }
  else{
	if(dslStateChg & (1<<0)){
 	tc3162_enable_irq(_pObj->pci_dev->irq);
  		RTMP_INT_LOCK(&pAd->irq_lock, irqFlags);
  		dslStateChg &=~(1<<0);
  		RTMP_INT_UNLOCK(&pAd->irq_lock, irqFlags);
	}
  }
  RTMPModTimer(&pAd->Pollingmode.PollingModeDetect, POLLING_MODE_DETECT_INTV);
  pAd->Pollingmode.PollingModeDetectRunning = true;
  return;
}


VOID PollingModeIsr(struct work_struct *work)
{

	PBBUPollingMode pPollingmode=container_of(work, BBUPollingMode, PollingDataBH);
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pPollingmode->pAd_va;
	struct net_device *net_dev = pAd->net_dev;
	rt2860_interrupt(0, net_dev);
}


VOID BBUPollingModeClose(IN struct rtmp_adapter *pAd){
	bool 		Cancelled;

	pAd->Pollingmode.PollingModeDetectRunning = false;
	RTMPCancelTimer(&pAd->Pollingmode.PollingModeDetect, &Cancelled);
}

BUILD_TIMER_FUNCTION(PeriodicPollingModeDetect);


VOID BBUPollingModeInit(IN struct rtmp_adapter *pAd){

	spin_lock_init(&pAd->Pollingmode.PollingModeLock);//for polling mode

	RTMPInitTimer(pAd, &pAd->Pollingmode.PollingModeDetect, GET_TIMER_FUNCTION(PeriodicPollingModeDetect), pAd, false);
	pAd->Pollingmode.PollingModeDetectRunning = false;
}

VOID BBUPollingModeStart(IN struct rtmp_adapter *pAd){

	if (pAd->Pollingmode.PollingModeDetectRunning == false)
	{
	    printk("jiffies=%08lx, POLLING_MODE_DETECT_INTV=%d\r\n", jiffies, POLLING_MODE_DETECT_INTV);
	    RTMPSetTimer(&pAd->Pollingmode.PollingModeDetect, POLLING_MODE_DETECT_INTV);
	}
	// init a BH task here
	INIT_WORK(&(pAd->Pollingmode.PollingDataBH), PollingModeIsr);

}

VOID BBUPrepareMAC(IN struct rtmp_adapter *pAd, u8 *macaddr ){
	u8 FourByteOffset = 0;
	u8 NWlanExt = 0;

	FourByteOffset = macaddr[5]%4;
	DBGPRINT(RT_DEBUG_WARN, ("\r\nFourByteOffset is %d", FourByteOffset));
	NWlanExt = pAd->ApCfg.BssidNum;
	DBGPRINT(RT_DEBUG_WARN, ("\r\nNWlanExt is %d", NWlanExt));


	switch(NWlanExt){
		case 1:
			break;
		case 2:
			switch(FourByteOffset){
				case 1:
				case 3:
					macaddr[5]--;
					break;
				case 0:
				case 2:
					break;
			}
			break;
		case 3:
		case 4:
			switch(FourByteOffset){
				case 0:
					break;
				case 1:
					macaddr[5]--;
					break;
				case 2:
					macaddr[5] -= 2;
					break;
				case 3:
					macaddr[5] -= 3;
					break;
			}
			break;
		default:
			break;
	}

		DBGPRINT(RT_DEBUG_WARN,("current MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
			macaddr[0], macaddr[1],
			macaddr[2], macaddr[3],
			macaddr[4], macaddr[5]));
	/*generate bssid from cpe mac address end, merge from linos, 20100208*/
}

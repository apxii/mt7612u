/****************************************************************************

    Module Name:
	rt_os_util.h

	Abstract:
	All function prototypes are provided from UTIL modules.

	Note:
	But can not use any OS key word and compile option here.
	All functions are provided from UTIL modules.

    Revision History:
    Who          When          What
    ---------    ----------    ----------------------------------------------

***************************************************************************/

#ifndef __RT_OS_UTIL_H__
#define __RT_OS_UTIL_H__

/* ============================ rt_linux.c ================================== */
/* General */
VOID RtmpUtilInit(VOID);

/* OS Time */
void RTMP_GetCurrentSystemTime(LARGE_INTEGER *time);

void RTMP_GetCurrentSystemTick(ULONG *pNow);

VOID RtmpOsWait(uint32_t Time);
uint32_t RtmpOsTimerAfter(ULONG a, ULONG b);
uint32_t RtmpOsTimerBefore(ULONG a, ULONG b);
VOID RtmpOsGetSystemUpTime(ULONG *pTime);
uint32_t RtmpOsTickUnitGet(VOID);



int AdapterBlockAllocateMemory(struct rtmp_adapter **ppAd, uint32_t SizeOfpAd);

VOID *RtmpOsVmalloc(ULONG Size);
VOID RtmpOsVfree(VOID *pMem);

ULONG RtmpOsCopyFromUser(VOID *to, const void *from, ULONG n);
ULONG RtmpOsCopyToUser(VOID *to, const void *from, ULONG n);

bool RtmpOsStatsAlloc(VOID **ppStats, VOID **ppIwStats);

/* OS Packet */
struct sk_buff *RtmpOSNetPktAlloc(VOID *pReserved, int size);

int RTMPAllocateNdisPacket(
	IN	VOID					*pReserved,
	OUT struct sk_buff *		*ppPacket,
	IN	u8 *				pHeader,
	IN	UINT					HeaderLen,
	IN	u8 *				pData,
	IN	UINT					DataLen);

void RTMP_QueryPacketInfo(
	IN  struct sk_buff *pPacket,
	OUT PACKET_INFO *pPacketInfo,
	OUT u8 **pSrcBufVA,
	OUT	UINT *pSrcBufLen);

struct sk_buff *DuplicatePacket(
	IN	struct net_device *pNetDev,
	IN	struct sk_buff *pPacket,
	IN	u8 FromWhichBSSID);

struct sk_buff *duplicate_pkt(
	IN	struct net_device *pNetDev,
	IN	u8 *pHeader802_3,
    IN  UINT HdrLen,
	IN	u8 *pData,
	IN	ULONG DataSize,
	IN	u8 FromWhichBSSID);

struct sk_buff *duplicate_pkt_with_TKIP_MIC(
	IN	VOID					*pReserved,
	IN	struct sk_buff *		pOldPkt);

struct sk_buff *duplicate_pkt_with_VLAN(
	IN	struct net_device *			pNetDev,
	IN	unsigned short 				VLAN_VID,
	IN	unsigned short 				VLAN_Priority,
	IN	u8 *				pHeader802_3,
    IN  UINT            		HdrLen,
	IN	u8 *				pData,
	IN	ULONG					DataSize,
	IN	u8 				FromWhichBSSID,
	IN	u8 				*TPID);

typedef void (*RTMP_CB_8023_PACKET_ANNOUNCE)(
			IN	struct rtmp_adapter *pAdr,
			IN	struct sk_buff *pPacket,
			IN	u8 		OpMode);

bool RTMPL2FrameTxAction(
	IN  struct rtmp_adapter *pAd,
	IN	struct net_device *			pNetDev,
	IN	RTMP_CB_8023_PACKET_ANNOUNCE _announce_802_3_packet,
	IN	u8 				apidx,
	IN	u8 *				pData,
	IN	uint32_t 				data_len,
	IN	u8 		OpMode);

struct sk_buff *ExpandPacket(
	IN	VOID					*pReserved,
	IN	struct sk_buff *		pPacket,
	IN	uint32_t 				ext_head_len,
	IN	uint32_t 				ext_tail_len);

struct sk_buff *ClonePacket(struct sk_buff *pPacket, u8 *pData,ULONG DataSize);

void wlan_802_11_to_802_3_packet(
	IN	struct net_device *			pNetDev,
	IN	u8 				OpMode,
	IN	unsigned short 				VLAN_VID,
	IN	unsigned short 				VLAN_Priority,
	IN	struct sk_buff *		pRxPacket,
	IN	u8 				*pData,
	IN	ULONG					DataSize,
	IN	u8 *				pHeader802_3,
	IN  u8 				FromWhichBSSID,
	IN	u8 				*TPID);


u8 VLAN_8023_Header_Copy(
	IN	unsigned short 				VLAN_VID,
	IN	unsigned short 				VLAN_Priority,
	IN	u8 *				pHeader802_3,
	IN	UINT            		HdrLen,
	OUT u8 *				pData,
	IN	u8 				FromWhichBSSID,
	IN	u8 				*TPID);

VOID RtmpOsPktBodyCopy(
	IN	struct net_device *			pNetDev,
	IN	struct sk_buff *		pNetPkt,
	IN	ULONG					ThisFrameLen,
	IN	u8 *				pData);

INT RtmpOsIsPktCloned(struct sk_buff *pNetPkt);
struct sk_buff *RtmpOsPktCopy(struct sk_buff *pNetPkt);
struct sk_buff *RtmpOsPktClone(struct sk_buff *pNetPkt);

VOID RtmpOsPktDataPtrAssign(struct sk_buff *pNetPkt, u8 *pData);

VOID RtmpOsPktLenAssign(struct sk_buff *pNetPkt, LONG Len);
VOID RtmpOsPktTailAdjust(struct sk_buff *pNetPkt, UINT removedTagLen);

u8 *RtmpOsPktTailBufExtend(struct sk_buff *pNetPkt, UINT len);
u8 *RtmpOsPktHeadBufExtend(struct sk_buff *pNetPkt, UINT len);
VOID RtmpOsPktReserve(struct sk_buff *pNetPkt, UINT len);

VOID RtmpOsPktProtocolAssign(struct sk_buff *pNetPkt);
VOID RtmpOsPktInfPpaSend(struct sk_buff *pNetPkt);
VOID RtmpOsPktRcvHandle(struct sk_buff *pNetPkt);
VOID RtmpOsPktNatMagicTag(struct sk_buff *pNetPkt);
VOID RtmpOsPktNatNone(struct sk_buff *pNetPkt);
VOID RtmpOsPktInit(struct sk_buff *pNetPkt, struct net_device *pNetDev, u8 *buf, unsigned short len);

struct sk_buff *RtmpOsPktIappMakeUp(struct net_device *pNetDev, UINT8 *pMac);

bool RtmpOsPktOffsetInit(VOID);

uint16_t RtmpOsNtohs(uint16_t Value);
uint16_t RtmpOsHtons(uint16_t Value);
uint32_t RtmpOsNtohl(uint32_t Value);
uint32_t RtmpOsHtonl(uint32_t Value);

/* OS File */
RTMP_OS_FD RtmpOSFileOpen(char *pPath,  int flag, int mode);
int RtmpOSFileClose(RTMP_OS_FD osfd);
void RtmpOSFileSeek(RTMP_OS_FD osfd, int offset);
int RtmpOSFileRead(RTMP_OS_FD osfd, char *pDataPtr, int readLen);
int RtmpOSFileWrite(RTMP_OS_FD osfd, char *pDataPtr, int writeLen);

int32_t RtmpOsFileIsErr(VOID *pFile);

void RtmpOSFSInfoChange(RTMP_OS_FS_INFO *pOSFSInfoOrg, bool bSet);

/* OS Network Interface */
int RtmpOSNetDevAddrSet(
	IN u8 OpMode,
	IN struct net_device *pNetDev,
	IN u8 *pMacAddr,
	IN u8 *dev_name);

void RtmpOSNetDevClose(struct net_device *pNetDev);
void RtmpOSNetDevFree(struct net_device *pNetDev);
INT RtmpOSNetDevAlloc(struct net_device **new_dev_p, uint32_t privDataSize);


#ifdef CONFIG_STA_SUPPORT
INT RtmpOSNotifyRawData(struct net_device *pNetDev, u8 *buf, INT len, ULONG type, unsigned short proto);

#endif /* CONFIG_STA_SUPPORT */

struct net_device *RtmpOSNetDevGetByName(struct net_device *pNetDev, char *pDevName);

void RtmpOSNetDeviceRefPut(struct net_device *pNetDev);

INT RtmpOSNetDevDestory(VOID *pReserved, struct net_device *pNetDev);
void RtmpOSNetDevDetach(struct net_device *pNetDev);
int RtmpOSNetDevAttach(u8 OpMode, struct net_device *pNetDev,
		       struct RTMP_OS_NETDEV_OP_HOOK *pDevOpHook);

void RtmpOSNetDevProtect(
	IN bool lock_it);

struct net_device *RtmpOSNetDevCreate(
	IN	int32_t 				MC_RowID,
	IN	uint32_t 				*pIoctlIF,
	IN	INT 					devType,
	IN	INT						devNum,
	IN	INT						privMemSize,
	IN	char *				pNamePrefix);

bool RtmpOSNetDevIsUp(struct net_device *pDev);

unsigned char *RtmpOsNetDevGetPhyAddr(struct net_device *pDev);

VOID RtmpOsNetQueueStart(struct net_device *pDev);
VOID RtmpOsNetQueueStop(struct net_device *pDev);
VOID RtmpOsNetQueueWake(struct net_device *pDev);

VOID RtmpOsSetPktNetDev(VOID *pPkt, struct net_device *pDev);

char *RtmpOsGetNetDevName(struct net_device *pDev);

uint32_t RtmpOsGetNetIfIndex(struct net_device *pDev);

VOID RtmpOsSetNetDevPriv(struct net_device *pDev, struct rtmp_adapter *pPriv);
struct rtmp_adapter *RtmpOsGetNetDevPriv(struct net_device *pDev);

VOID RtmpOsSetNetDevWdev(struct net_device *net_dev, struct rtmp_wifi_dev *wdev);
struct rtmp_wifi_dev *RtmpOsGetNetDevWdev(struct net_device  *pDev);

unsigned short RtmpDevPrivFlagsGet(struct net_device *pDev);
VOID RtmpDevPrivFlagsSet(struct net_device *pDev, unsigned short PrivFlags);

VOID RtmpOsSetNetDevType(struct net_device *pDev, unsigned short Type);
VOID RtmpOsSetNetDevTypeMonitor(struct net_device *pDev);
u8 get_sniffer_mode(struct net_device *pDev);
VOID set_sniffer_mode(struct net_device *pDev, u8 mode);

/* OS Semaphore */
VOID RtmpOsCmdUp(RTMP_OS_TASK *pCmdQTask);
VOID RtmpOsMlmeUp(RTMP_OS_TASK *pMlmeQTask);

/* OS Task */
int RtmpOSTaskKill(RTMP_OS_TASK *pTaskOrg);

INT RtmpOSTaskNotifyToExit(RTMP_OS_TASK *pTaskOrg);

VOID RtmpOSTaskCustomize(RTMP_OS_TASK *pTaskOrg);

int RtmpOSTaskAttach(
	IN	RTMP_OS_TASK *pTaskOrg,
	IN	RTMP_OS_TASK_CALLBACK	fn,
	IN	ULONG arg);

int RtmpOSTaskInit(
	IN	RTMP_OS_TASK *pTaskOrg,
	IN	char *pTaskName,
	IN	VOID *pPriv);

bool RtmpOSTaskWait(
	IN	VOID *pReserved,
	IN	RTMP_OS_TASK *pTaskOrg,
	IN	int32_t *pStatus);

int32_t RtmpThreadPidKill(RTMP_OS_PID	 PID);

/* OS Timer */
VOID RTMP_SetPeriodicTimer(
	IN	struct timer_list *pTimerOrg,
	IN	unsigned long timeout);

VOID RTMP_OS_Init_Timer(
	IN	VOID *pReserved,
	IN	struct timer_list *pTimerOrg,
	IN	TIMER_FUNCTION function,
	IN	PVOID data);

VOID RTMP_OS_Add_Timer(struct timer_list *pTimerOrg, unsigned long timeout);
VOID RTMP_OS_Mod_Timer(struct timer_list *pTimerOrg, unsigned long timeout);
VOID RTMP_OS_Del_Timer(struct timer_list *pTimerOrg, bool *pCancelled);

/* OS I/O */
VOID RTMP_PCI_Writel(ULONG Value, VOID *pAddr);
VOID RTMP_PCI_Writew(ULONG Value, VOID *pAddr);
VOID RTMP_PCI_Writeb(ULONG Value, VOID *pAddr);
ULONG RTMP_PCI_Readl(VOID *pAddr);
ULONG RTMP_PCI_Readw(VOID *pAddr);
ULONG RTMP_PCI_Readb(VOID *pAddr);

int RtmpOsPciConfigReadWord(
	IN	VOID					*pDev,
	IN	uint32_t 				Offset,
	OUT uint16_t 				*pValue);

int RtmpOsPciConfigWriteWord(VOID *pDev, uint32_t Offset, uint16_t Value);
int RtmpOsPciConfigReadDWord(VOID *pDev, uint32_t Offset, uint32_t *pValue);
int RtmpOsPciConfigWriteDWord(VOID *pDev, uint32_t Offset, uint32_t Value);

int RtmpOsPciFindCapability(VOID *pDev, INT Cap);

VOID *RTMPFindHostPCIDev(VOID *pPciDevSrc);

int RtmpOsPciMsiEnable(VOID *pDev);
VOID RtmpOsPciMsiDisable(VOID *pDev);

/* OS Wireless */
ULONG RtmpOsMaxScanDataGet(VOID);

/* OS Interrutp */
int32_t RtmpOsIsInInterrupt(VOID);

/* OS Utility */
void hex_dump(char *str, unsigned char *pSrcBufVA, unsigned int SrcBufLen);

typedef VOID (*RTMP_OS_SEND_WLAN_EVENT)(
	IN	struct rtmp_adapter			*pAdSrc,
	IN	unsigned short 				Event_flag,
	IN	u8 *					pAddr,
	IN  u8 				BssIdx,
	IN	CHAR					Rssi);

VOID RtmpOsSendWirelessEvent(
	IN	struct rtmp_adapter	*pAd,
	IN	unsigned short 		Event_flag,
	IN	u8 *			pAddr,
	IN	u8 		BssIdx,
	IN	CHAR			Rssi,
	IN	RTMP_OS_SEND_WLAN_EVENT pFunc);

#ifdef CONFIG_AP_SUPPORT
void SendSignalToDaemon(
	IN	INT sig,
	IN	RTMP_OS_PID	 pid,
	IN	unsigned long pid_no);
#endif /* CONFIG_AP_SUPPORT */

int RtmpOSWrielessEventSend(
	IN	struct net_device *			pNetDev,
	IN	uint32_t 				eventType,
	IN	INT						flags,
	IN	u8 *				pSrcMac,
	IN	u8 *				pData,
	IN	uint32_t 				dataLen);

int RtmpOSWrielessEventSendExt(
	IN	struct net_device *			pNetDev,
	IN	uint32_t 				eventType,
	IN	INT						flags,
	IN	u8 *				pSrcMac,
	IN	u8 *				pData,
	IN	uint32_t 				dataLen,
	IN	uint32_t 				family);

UINT RtmpOsWirelessExtVerGet(VOID);

VOID RtmpDrvAllMacPrint(
	IN VOID						*pReserved,
	IN uint32_t 				*pBufMac,
	IN uint32_t 				AddrStart,
	IN uint32_t 				AddrEnd,
	IN uint32_t 				AddrStep);

VOID RtmpDrvAllE2PPrint(
	IN	VOID					*pReserved,
	IN	unsigned short 				*pMacContent,
	IN	uint32_t 				AddrEnd,
	IN	uint32_t 				AddrStep);

VOID RtmpDrvAllRFPrint(
	IN VOID *pReserved,
	IN u8 *pBuf,
	IN uint32_t BufLen);

VOID RtmpOsWlanEventSet(
	IN	VOID					*pReserved,
	IN	bool					*pCfgWEnt,
	IN	bool					FlgIsWEntSup);

uint16_t RtmpOsGetUnaligned(uint16_t *pWord);

uint32_t RtmpOsGetUnaligned32(uint32_t *pWord);

ULONG RtmpOsGetUnalignedlong(ULONG *pWord);

long RtmpOsSimpleStrtol(
	IN	const char				*cp,
	IN	char 					**endp,
	IN	unsigned int			base);


/* ============================ rt_os_util.c ================================ */
VOID RtmpDrvRateGet(
	IN VOID *pReserved,
	IN UINT8 MODE,
	IN UINT8 ShortGI,
	IN UINT8 BW,
	IN UINT8 MCS,
	IN UINT8 Antenna,
	OUT uint32_t *pRate);

char * rtstrchr(const char * s, int c);

char *  WscGetAuthTypeStr(unsigned short authFlag);

char *  WscGetEncryTypeStr(unsigned short encryFlag);

unsigned short WscGetAuthTypeFromStr(char *arg);

unsigned short WscGetEncrypTypeFromStr(char *arg);

VOID RtmpMeshDown(
	IN VOID *pDrvCtrlBK,
	IN bool WaitFlag,
	IN bool (*RtmpMeshLinkCheck)(IN VOID *pAd));

unsigned short RtmpOsNetPrivGet(struct net_device *pDev);

bool RtmpOsCmdDisplayLenCheck(
	IN	uint32_t 				LenSrc,
	IN	uint32_t 				Offset);

VOID    WpaSendMicFailureToWpaSupplicant(
	IN	struct net_device *			pNetDev,
	IN const u8 *src_addr,
	IN bool bUnicast,
	IN INT key_id,
	IN const u8 *tsc);

int wext_notify_event_assoc(
	IN	struct net_device *			pNetDev,
	IN	u8 				*ReqVarIEs,
	IN	uint32_t 				ReqVarIELen);

VOID    SendAssocIEsToWpaSupplicant(
	IN	struct net_device *			pNetDev,
	IN	u8 				*ReqVarIEs,
	IN	uint32_t 				ReqVarIELen);

/* ============================ rt_rbus_pci_util.c ========================== */
void RtmpAllocDescBuf(
	IN PPCI_DEV pPciDev,
	IN UINT Index,
	IN ULONG Length,
	IN bool Cached,
	OUT VOID **VirtualAddress,
	OUT PNDIS_PHYSICAL_ADDRESS	PhysicalAddress);

void RtmpFreeDescBuf(
	IN PPCI_DEV pPciDev,
	IN ULONG Length,
	IN VOID *VirtualAddress,
	IN NDIS_PHYSICAL_ADDRESS	PhysicalAddress);

void RTMP_AllocateFirstTxBuffer(
	IN PPCI_DEV pPciDev,
	IN UINT Index,
	IN ULONG Length,
	IN bool Cached,
	OUT VOID **VirtualAddress,
	OUT PNDIS_PHYSICAL_ADDRESS	PhysicalAddress);

void RTMP_FreeFirstTxBuffer(
	IN	PPCI_DEV				pPciDev,
	IN	ULONG					Length,
	IN	bool					Cached,
	IN	PVOID					VirtualAddress,
	IN	NDIS_PHYSICAL_ADDRESS	PhysicalAddress);

struct sk_buff *RTMP_AllocateRxPacketBuffer(
	IN	VOID					*pReserved,
	IN	VOID					*pPciDev,
	IN	ULONG					Length,
	IN	bool					Cached,
	OUT	PVOID					*VirtualAddress,
	OUT	PNDIS_PHYSICAL_ADDRESS	PhysicalAddress);

ra_dma_addr_t linux_pci_map_single(void *pPciDev, void *ptr, size_t size, int sd_idx, int direction);

void linux_pci_unmap_single(void *pPciDev, ra_dma_addr_t dma_addr, size_t size, int direction);

/* ============================ rt_usb_util.c =============================== */
typedef VOID (*USB_COMPLETE_HANDLER)(VOID *);

/*struct urb *rausb_alloc_urb(int iso_packets); */


/* CFG80211 */
#ifdef RT_CFG80211_SUPPORT
typedef struct __CFG80211_BAND {

	UINT8 RFICType;
	UINT8 MpduDensity;
	UINT8 TxStream;
	UINT8 RxStream;
	uint32_t MaxTxPwr;
	uint32_t MaxBssTable;

	uint16_t RtsThreshold;
	uint16_t FragmentThreshold;
	uint32_t RetryMaxCnt; /* bit0~7: short; bit8 ~ 15: long */
	bool FlgIsBMode;
} CFG80211_BAND;

VOID CFG80211OS_UnRegister(
	IN VOID						*pCB,
	IN VOID						*pNetDev);

bool CFG80211_SupBandInit(
	IN VOID						*pCB,
	IN CFG80211_BAND 			*pBandInfo,
	IN VOID						*pWiphyOrg,
	IN VOID						*pChannelsOrg,
	IN VOID						*pRatesOrg);

bool CFG80211OS_SupBandReInit(
	IN VOID						*pCB,
	IN CFG80211_BAND 			*pBandInfo);

VOID CFG80211OS_RegHint(
	IN VOID						*pCB,
	IN u8 				*pCountryIe,
	IN ULONG					CountryIeLen);

VOID CFG80211OS_RegHint11D(
	IN VOID						*pCB,
	IN u8 				*pCountryIe,
	IN ULONG					CountryIeLen);

bool CFG80211OS_BandInfoGet(
	IN VOID						*pCB,
	IN VOID						*pWiphyOrg,
	OUT VOID					**ppBand24,
	OUT VOID					**ppBand5);

uint32_t CFG80211OS_ChanNumGet(
	IN VOID						*pCB,
	IN VOID						*pWiphyOrg,
	IN uint32_t 				IdBand);

bool CFG80211OS_ChanInfoGet(
	IN VOID						*pCB,
	IN VOID						*pWiphyOrg,
	IN uint32_t 				IdBand,
	IN uint32_t 				IdChan,
	OUT uint32_t 				*pChanId,
	OUT uint32_t 				*pPower,
	OUT bool					*pFlgIsRadar);

bool CFG80211OS_ChanInfoInit(
	IN VOID						*pCB,
	IN uint32_t 				InfoIndex,
	IN u8 				ChanId,
	IN u8 				MaxTxPwr,
	IN bool					FlgIsNMode,
	IN bool					FlgIsBW20M);

VOID CFG80211OS_Scaning(
	IN VOID						*pCB,
	IN uint32_t 				ChanId,
	IN u8 				*pFrame,
	IN uint32_t 				FrameLen,
	IN int32_t 				RSSI,
	IN bool					FlgIsNMode,
	IN UINT8					BW);

VOID CFG80211OS_ScanEnd(
	IN VOID						*pCB,
	IN bool					FlgIsAborted);

void CFG80211OS_ConnectResultInform(
	IN VOID						*pCB,
	IN u8 				*pBSSID,
	IN u8 				*pReqIe,
	IN uint32_t 				ReqIeLen,
	IN u8 				*pRspIe,
	IN uint32_t 				RspIeLen,
	IN u8 				FlgIsSuccess);

void CFG80211OS_P2pClientConnectResultInform(
	IN struct net_device *				pNetDev,
	IN u8 				*pBSSID,
	IN u8 				*pReqIe,
	IN uint32_t 				ReqIeLen,
	IN u8 				*pRspIe,
	IN uint32_t 				RspIeLen,
	IN u8 				FlgIsSuccess);

bool CFG80211OS_RxMgmt(IN struct net_device *pNetDev, IN int32_t freq, IN u8 *frame, IN uint32_t len);
VOID CFG80211OS_TxStatus(IN struct net_device *pNetDev, IN int32_t cookie, 	IN u8 *frame, IN uint32_t len, IN bool ack);
VOID CFG80211OS_NewSta(IN struct net_device *pNetDev, IN const u8 *mac_addr, IN const u8 *assoc_frame, IN uint32_t assoc_len);
VOID CFG80211OS_DelSta(IN struct net_device *pNetDev, IN const u8 *mac_addr);
VOID CFG80211OS_MICFailReport(IN struct net_device *pNetDev, IN const u8 *src_addr, IN bool unicast, IN INT key_id, IN const u8 *tsc );
VOID CFG80211OS_Roamed(struct net_device *pNetDev, IN u8 *pBSSID,
					   u8 *pReqIe, uint32_t ReqIeLen, u8 *pRspIe, uint32_t RspIeLen);
VOID CFG80211OS_RecvObssBeacon(VOID *pCB, const u8 *pFrame, INT frameLen, INT freq);
#endif /* RT_CFG80211_SUPPORT */




/* ================================ EXTERN ================================== */
extern u8 SNAP_802_1H[6];
extern u8 SNAP_BRIDGE_TUNNEL[6];
extern u8 EAPOL[2];
extern u8 TPID[];
extern u8 IPX[2];
extern u8 APPLE_TALK[2];
extern u8 NUM_BIT8[8];
extern ULONG RTPktOffsetData, RTPktOffsetLen, RTPktOffsetCB;

extern ULONG OS_NumOfMemAlloc, OS_NumOfMemFree;

extern uint32_t RalinkRate_Legacy[];
extern uint32_t RalinkRate_HT_1NSS[Rate_BW_MAX][Rate_GI_MAX][Rate_MCS];
extern uint32_t RalinkRate_VHT_1NSS[Rate_BW_MAX][Rate_GI_MAX][Rate_MCS];
extern UINT8 newRateGetAntenna(UINT8 MCS);


#ifdef PLATFORM_UBM_IPX8
#include "vrut_ubm.h"
#endif /* PLATFORM_UBM_IPX8 */

int32_t  RtPrivIoctlSetVal(VOID);

void RtmpOsSpinLockIrqSave(spinlock_t *lock, unsigned long *flags);
void RtmpOsSpinUnlockIrqRestore(spinlock_t *lock, unsigned long *flags);
void RtmpOsSpinLockIrq(spinlock_t *lock);
void RtmpOsSpinUnlockIrq(spinlock_t *lock);
void OS_LOAD_CODE_FROM_BIN(unsigned char **image, char *bin_name, void *inf_dev, uint32_t *code_len);

#endif /* __RT_OS_UTIL_H__ */

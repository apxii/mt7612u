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
	mcu_and.h

	Abstract:

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/

#ifndef __MCU_AND_H__
#define __MCU_AND_H__

#include "mcu.h"

#include <linux/interrupt.h>
#include <linux/list.h>

struct rtmp_adapter;
struct _BANK_RF_REG_PAIR;
struct _R_M_W_REG;
struct _RF_R_M_W_REG;

#define CPU_CTL					0x0704
#define CLOCK_CTL				0x0708
#define RESET_CTL				0x070C
#define INT_LEVEL				0x0718
#define COM_REG0				0x0730
#define COM_REG1				0x0734
#define COM_REG2				0x0738
#define COM_REG3				0x073C
#define PCIE_REMAP_BASE1		0x0740
#define PCIE_REMAP_BASE2		0x0744
#define PCIE_REMAP_BASE3		0x0748
#define PCIE_REMAP_BASE4		0x074C
#define LED_CTRL				0x0770
#define LED_TX_BLINK_0			0x0774
#define LED_TX_BLINK_1			0x0778
#define LED0_S0				0x077C
#define LED0_S1				0x0780
#define SEMAPHORE_00			0x07B0
#define SEMAPHORE_01			0x07B4
#define SEMAPHORE_02			0x07B8
#define SEMAPHORE_03			0x07BC

#define MCU_WAIT_ACK_CMD_THRESHOLD 0x0f
#define MCU_RX_CMD_THRESHOLD 0x0f





enum cmd_msg_state {
	ILLEGAL,
	TX_START,
	TX_KICKOUT_FAIL,
	TX_TIMEOUT_FAIL,
	TX_RETRANSMIT,
	TX_DONE,
	WAIT_CMD_OUT,
	WAIT_CMD_OUT_AND_ACK,
	WAIT_ACK,
	RX_START,
	RX_RECEIVE_FAIL,
	RX_DONE,
};

enum {
	MCU_INIT,
	MCU_TX_HALT,
	MCU_RX_HALT,
};

enum {
	CMD_ACK,
};

enum cmd_msg_error_type {
	error_tx_kickout_fail,
	error_tx_timeout_fail,
	error_rx_receive_fail,
};

struct mt7612u_mcu_ctrl {
	u8 cmd_seq;
	unsigned long flags;
	RTMP_NET_TASK_STRUCT cmd_msg_task;
	spinlock_t txq_lock;
	DL_LIST txq;
	spinlock_t rxq_lock;
	DL_LIST rxq;
	spinlock_t ackq_lock;
	DL_LIST ackq;
	spinlock_t kickq_lock;
	DL_LIST kickq;
	spinlock_t rx_doneq_lock;
	DL_LIST rx_doneq;
	unsigned long tx_kickout_fail_count;
	unsigned long tx_timeout_fail_count;
	unsigned long rx_receive_fail_count;
	bool power_on;
	bool dpd_on;
};


struct cmd_msg;

struct cmd_msg_cb {
	struct cmd_msg *msg;
};

#define USB_END_PADDING 4
#define UPLOAD_PATCH_UNIT 2048
#define PATCH_INFO_SIZE 30
#define FW_INFO_SIZE 32
#define IV_SIZE 0x40
#define GET_SEMAPHORE_RETRY_MAX 600
#define UPLOAD_FW_UNIT 14592
#define UPLOAD_FW_TIMEOUT 1000
#define CMD_MSG_CB(pkt) ((struct cmd_msg_cb *)(GET_OS_PKT_CB(pkt)))
#define CMD_MSG_RETRANSMIT_TIMES 3
#define CMD_MSG_TIMEOUT 500

struct cmd_msg {
	DL_LIST list;
	u8 type;
	u8 seq;
	u16 rsp_payload_len;
	bool need_wait;
	bool need_rsp;
	bool need_retransmit;

	 struct completion ack_done;
	char *rsp_payload;
	enum cmd_msg_state state;
	void *priv;
	struct sk_buff *skb;
	struct urb *urb;
	int retransmit_times;
};

/*
 * Calibration ID
 */

enum mt7612u_mcu_calibration {
	R_CALIBRATION_7662 = 1,
	TEMP_SENSOR_CALIBRATION_7662,
	RXDCOC_CALIBRATION_7662,
	RC_CALIBRATION_7662,
	SX_LOGEN_CALIBRATION_7662,
	LC_CALIBRATION_7662,
	TX_LOFT_CALIBRATION_7662,
	TXIQ_CALIBRATION_7662,
	TSSI_CALIBRATION_7662,
	TSSI_COMPENSATION_7662,
	DPD_CALIBRATION_7662,
	RXIQC_FI_CALIBRATION_7662,
	RXIQC_FD_CALIBRATION_7662,
};

enum CALIBRATION_TYPE {
	FULL_CALIBRATION,
	PARTIAL_CALIBRATION,
};

enum SWITCH_CHANNEL_STAGE {
	NORMAL_OPERATING = 0x01,
	SCANNING = 0x02,
	TEMPERATURE_CHANGE = 0x04,
};

/*
 * Function set ID
 */
enum FUN_ID {
	Q_SELECT = 1,
	BW_SETTING = 2,
	USB2_SW_DISCONNECT = 2,
	USB3_SW_DISCONNECT = 3,
	LOG_FW_DEBUG_MSG = 4,
	GET_FW_VERSION = 5,
};

/*
 * Command response RX Ring selection
 */
enum RX_RING_ID {
	RX_RING0,
	RX_RING1,
};

enum BW_SETTING {
	BW20 = 1,
	BW40 = 2,
	BW10 = 4,
	BW80 = 8,
};

/*
 * FW debug message parameters
 */
enum FW_DEBUG_SETTING {
	DISABLE_DEBUG_MSG = 0,
	DEBUG_MSG_TO_UART,
	DEBUG_MSG_TO_HOST,
};

enum CR_TYPE {
	RF_CR,
	BBP_CR,
	RF_BBP_CR,
	HL_TEMP_CR_UPDATE,
};

enum TEMPERATURE_LEVEL_7662 {
	NORMAL_TEMP_7662, /* 0~60 */
	LOW_TEMP_7662, /* < 0 */
	HIGH_TEMP_7662, /* > 60 */
};

#define LOAD_CR_MODE_MASK (0xff)
#define LOAD_CR_MODE(p) (((p) & 0xff))
#define LOAD_CR_TEMP_LEVEL_MASK (0xff << 8)
#define LOAD_CR_TEMP_LEVEL(p) (((p) & 0xff) << 8)
#define LOAD_CR_CHL_MASK (0xff << 16)
#define LOAD_CR_CHL(p) (((p) & 0xff) << 16)

#define SC_PARAM1_CHL_MASK (0xff)
#define SC_PARAM1_CHL(p) (((p) & 0xff))
#define SC_PARAM1_SCAN_MASK (0xff << 8)
#define SC_PARAM1_SCAN(p) (((p) & 0xff) << 8)
#define SC_PARAM1_BW_MASK (0xff << 16)
#define SC_PARAM1_BW(p) (((p) & 0xff) << 16)

#define SC_PARAM2_TR_SETTING_MASK (0xffff)
#define SC_PARAM2_TR_SETTING(p) (((p) & 0xffff))
#define SC_PARAM2_EXTENSION_CHL_MASK (0xff << 16)
#define SC_PARAM2_EXTENSION_CHL(p) (((p) & 0xff) << 16)

#define TSSI_PARAM2_SLOPE0_MASK (0xff)
#define TSSI_PARAM2_SLOPE0(p) (((p) & 0xff))
#define TSSI_PARAM2_SLOPE1_MASK (0xff << 8)
#define TSSI_PARAM2_SLOPE1(p) (((p) & 0xff) << 8)
#define TSSI_PARAM2_OFFSET0_MASK (0xff << 16)
#define TSSI_PARAM2_OFFSET0(p) (((p) & 0xff) << 16)
#define TSSI_PARAM2_OFFSET1_MASK (0xff << 24)
#define TSSI_PARAM2_OFFSET1(p) (((p) & 0xff) << 24)

/*
 * Command type table
 */
enum mcu_cmd_type {
	CMD_FUN_SET_OP = 1,
	CMD_LOAD_CR,
	CMD_INIT_GAIN_OP = 3,
	CMD_DYNC_VGA_OP = 6,
	CMD_TDLS_CH_SW = 7,
	CMD_BURST_WRITE = 8,
	CMD_READ_MODIFY_WRITE,
	CMD_RANDOM_READ,
	CMD_BURST_READ,
	CMD_RANDOM_WRITE = 12,
	CMD_LED_MODE_OP = 16,
	CMD_POWER_SAVING_OP = 20,
	CMD_WOW_CONFIG,
	CMD_WOW_QUERY,
	CMD_WOW_FEATURE = 24,
	CMD_CARRIER_DETECT_OP = 28,
	CMD_RADOR_DETECT_OP,
	CMD_SWITCH_CHANNEL_OP,
	CMD_CALIBRATION_OP,
	CMD_BEACON_OP,
	CMD_ANTENNA_OP
};

/*
 * Event type table
 */
enum EVENT_TYPE {
	CMD_DONE,
	CMD_ERROR,
	CMD_RETRY,
	EVENT_PWR_RSP,
	EVENT_WOW_RSP,
	EVENT_CARRIER_DETECT_RSP,
	EVENT_DFS_DETECT_RSP,
};

enum mcu_skb_state {
	ILLEAGAL = 0,
	MCU_CMD_START,
	MCU_CMD_DONE,
	MCU_RSP_START,
	MCU_RSP_DONE,
	MCU_RSP_CLEANUP,
	UNLINK_START,
};

struct mcu_skb_data {
	enum mcu_skb_state state;
};


struct mt7612u_tssi_comp{
		uint32_t pa_mode;
		uint32_t tssi_slope_offset;
};

enum CALIBRATION_TEST_TYPE {
	CAL_ROBUST_TEST=0,

};

int mt7612u_mcu_sys_init(struct rtmp_adapter *pAd);
void mt7612u_mcu_usb_fw_init(struct rtmp_adapter *ad);
void mt7612u_mcu_ctrl_init(struct rtmp_adapter *ad);
void mt7612u_mcu_ctrl_exit(struct rtmp_adapter *ad);
int mt7612u_mcu_read_modify_write(struct rtmp_adapter *ad, struct _R_M_W_REG *reg_pair, u32 num);
int mt7612u_mcu_random_write(struct rtmp_adapter *ad, struct rtmp_reg_pair *reg_pair, u32 num);
int mt7612u_mcu_fun_set(struct rtmp_adapter *ad, u32 fun_id, u32 param);
int mt7612u_mcu_pwr_saving(struct rtmp_adapter *ad, u32 op, u32 level);
void mt7612u_mcu_calibration(struct rtmp_adapter *ad, enum mt7612u_mcu_calibration cal_id, u32 param);
void mt7612u_mcu_tssi_comp(struct rtmp_adapter *ad, struct mt7612u_tssi_comp *param);
int usb_rx_cmd_msgs_receive(struct rtmp_adapter *ad);
void pci_kick_out_cmd_msg_complete(struct sk_buff *net_pkt);
int mt7612u_mcu_load_cr(struct rtmp_adapter *ad, u32 cr_type, UINT8 temp_level, UINT8 channel);
int mt7612u_mcu_switch_channel(struct rtmp_adapter *ad, u8 channel, bool scan, unsigned int bw, unsigned int tx_rx_setting, u8 bbp_ch_idx);
int mt7612u_mcu_init_gain(struct rtmp_adapter *ad, UINT8 channel, bool force_mode, unsigned int gain_from_e2p);
int mt7612u_mcu_dynamic_vga(struct rtmp_adapter *ad, UINT8 channel, bool mode, bool ext, int rssi, unsigned int false_cca);
int mt7612u_mcu_led_op(struct rtmp_adapter *ad, u32 led_idx, u32 link_status);

#define MT_INBAND_PACKET_MAX_LEN	192	/* must be 48 multible */
#define MT_COMMAND_BULK_OUT_ADDR	0x8
#define MT_COMMAND_RSP_BULK_IN_ADDR	0x85

#define MAX_CALIBRATION_WAIT_TIME						100

#endif /* __MCU_AND_H__ */


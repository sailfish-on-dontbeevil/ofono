/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2009-2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __ISIMODEM_NETWORK_H
#define __ISIMODEM_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#define PN_NETWORK		0x0A
#define NETWORK_TIMEOUT		5
#define NETWORK_SCAN_TIMEOUT	180
#define NETWORK_SET_TIMEOUT	240
#define NET_INVALID_TIME	0x64

enum net_message_id {
	NET_SET_REQ = 0x07,
	NET_SET_RESP = 0x08,
	NET_RSSI_GET_REQ = 0x0B,
	NET_RSSI_GET_RESP = 0x0C,
	NET_RSSI_IND = 0x1E,
	NET_TIME_IND = 0x27,
	NET_RAT_IND = 0x35,
	NET_RAT_REQ = 0x36,
	NET_RAT_RESP = 0x37,
	NET_REG_STATUS_GET_REQ = 0xE0,
	NET_REG_STATUS_GET_RESP = 0xE1,
	NET_REG_STATUS_IND = 0xE2,
	NET_AVAILABLE_GET_REQ = 0xE3,
	NET_AVAILABLE_GET_RESP = 0xE4,
	NET_OPER_NAME_READ_REQ = 0xE5,
	NET_OPER_NAME_READ_RESP = 0xE6,
	NET_COMMON_MESSAGE = 0xF0
};

enum net_subblock {
	NET_REG_INFO_COMMON = 0x00,
	NET_OPERATOR_INFO_COMMON = 0x02,
	NET_RSSI_CURRENT = 0x04,
	NET_GSM_REG_INFO = 0x09,
	NET_DETAILED_NETWORK_INFO = 0x0B,
	NET_GSM_OPERATOR_INFO = 0x0C,
	NET_TIME_INFO = 0x10,
	NET_GSM_BAND_INFO = 0x11,
	NET_RAT_INFO = 0x2C,
	NET_AVAIL_NETWORK_INFO_COMMON = 0xE1,
	NET_OPER_NAME_INFO = 0xE7
};

enum net_reg_status {
	NET_REG_STATUS_HOME = 0x00,
	NET_REG_STATUS_ROAM = 0x01,
	NET_REG_STATUS_ROAM_BLINK = 0x02,
	NET_REG_STATUS_NOSERV = 0x03,
	NET_REG_STATUS_NOSERV_SEARCHING = 0x04,
	NET_REG_STATUS_NOSERV_NOTSEARCHING = 0x05,
	NET_REG_STATUS_NOSERV_NOSIM = 0x06,
	NET_REG_STATUS_POWER_OFF = 0x08,
	NET_REG_STATUS_NSPS = 0x09,
	NET_REG_STATUS_NSPS_NO_COVERAGE = 0x0A,
	NET_REG_STATUS_NOSERV_SIM_REJECTED_BY_NW = 0x0B
};

enum net_network_status {
	NET_OPER_STATUS_UNKNOWN = 0x00,
	NET_OPER_STATUS_AVAILABLE = 0x01,
	NET_OPER_STATUS_CURRENT = 0x02,
	NET_OPER_STATUS_FORBIDDEN = 0x03
};

enum net_network_pref {
	NET_GSM_HOME_PLMN = 0x00,
	NET_GSM_PREFERRED_PLMN = 0x01,
	NET_GSM_FORBIDDEN_PLMN = 0x02,
	NET_GSM_OTHER_PLMN = 0x03,
	NET_GSM_NO_PLMN_AVAIL = 0x04
};

enum net_umts_available {
	NET_UMTS_NOT_AVAILABLE = 0x00,
	NET_UMTS_AVAILABLE = 0x01
};

enum net_band_info {
	NET_GSM_BAND_900_1800 = 0x00,
	NET_GSM_BAND_850_1900 = 0x01,
	NET_GSM_BAND_INFO_NOT_AVAIL = 0x02,
	NET_GSM_BAND_ALL_SUPPORTED_BANDS = 0x03,
	NET_GSM_BAND_850_LOCKED = 0xB0,
	NET_GSM_BAND_900_LOCKED = 0xA0,
	NET_GSM_BAND_1800_LOCKED = 0xA1,
	NET_GSM_BAND_1900_LOCKED = 0xB1,
};

enum net_gsm_cause {
	NET_GSM_IMSI_UNKNOWN_IN_HLR = 0x02,
	NET_GSM_ILLEGAL_MS = 0x03,
	NET_GSM_IMSI_UNKNOWN_IN_VLR = 0x04,
	NET_GSM_IMEI_NOT_ACCEPTED = 0x05,
	NET_GSM_ILLEGAL_ME = 0x06,
	NET_GSM_GPRS_SERVICES_NOT_ALLOWED = 0x07,
	NET_GSM_GPRS_AND_NON_GPRS_NA = 0x08,
	NET_GSM_MS_ID_CANNOT_BE_DERIVED = 0x09,
	NET_GSM_IMPLICITLY_DETACHED = 0x0A,
	NET_GSM_PLMN_NOT_ALLOWED = 0x0B,
	NET_GSM_LA_NOT_ALLOWED = 0x0C,
	NET_GSM_ROAMING_NOT_IN_THIS_LA = 0x0D,
	NET_GSM_GPRS_SERV_NA_IN_THIS_PLMN = 0x0E,
	NET_GSM_NO_SUITABLE_CELLS_IN_LA = 0x0F,
	NET_GSM_MSC_TEMP_NOT_REACHABLE = 0x10,
	NET_GSM_NETWORK_FAILURE = 0x11,
	NET_GSM_MAC_FAILURE = 0x14,
	NET_GSM_SYNCH_FAILURE = 0x15,
	NET_GSM_CONGESTION = 0x16,
	NET_GSM_AUTH_UNACCEPTABLE = 0x17,
	NET_GSM_SERV_OPT_NOT_SUPPORTED = 0x20,
	NET_GSM_SERV_OPT_NOT_SUBSCRIBED = 0x21,
	NET_GSM_SERV_TEMP_OUT_OF_ORDER = 0x22,
	NET_GSM_RETRY_ENTRY_NEW_CELL_LOW = 0x30,
	NET_GSM_RETRY_ENTRY_NEW_CELL_HIGH = 0x3F,
	NET_GSM_SEMANTICALLY_INCORRECT = 0x5F,
	NET_GSM_INVALID_MANDATORY_INFO = 0x60,
	NET_GSM_MSG_TYPE_NONEXISTENT = 0x61,
	NET_GSM_CONDITIONAL_IE_ERROR = 0x64,
	NET_GSM_MSG_TYPE_WRONG_STATE = 0x65,
	NET_GSM_PROTOCOL_ERROR_UNSPECIFIED = 0x6F
};

enum net_cs_type {
	NET_CS_GSM = 0x00
};

enum net_rat_name {
	NET_GSM_RAT = 0x01,
	NET_UMTS_RAT = 0x02
};

enum net_rat_type {
	NET_CURRENT_RAT = 0x00,
	NET_SUPPORTED_RATS = 0x01
};

enum net_measurement_type {
	NET_CURRENT_CELL_RSSI = 0x02
};

enum net_search_mode {
	NET_MANUAL_SEARCH = 0x00
};

enum net_oper_name_type {
	NET_HARDCODED_LATIN_OPER_NAME = 0x00
};

enum net_select_mode {
	NET_SELECT_MODE_UNKNOWN = 0x00,
	NET_SELECT_MODE_MANUAL = 0x01,
	NET_SELECT_MODE_AUTOMATIC = 0x02,
	NET_SELECT_MODE_USER_RESELECTION = 0x03,
	NET_SELECT_MODE_NO_SELECTION = 0x04
};

enum net_isi_cause {
	NET_CAUSE_OK = 0x00,
	NET_CAUSE_COMMUNICATION_ERROR = 0x01,
	NET_CAUSE_INVALID_PARAMETER = 0x02,
	NET_CAUSE_NO_SIM = 0x03,
	NET_CAUSE_SIM_NOT_YET_READY = 0x04,
	NET_CAUSE_NET_NOT_FOUND = 0x05,
	NET_CAUSE_REQUEST_NOT_ALLOWED = 0x06,
	NET_CAUSE_CALL_ACTIVE = 0x07,
	NET_CAUSE_SERVER_BUSY = 0x08,
	NET_CAUSE_SECURITY_CODE_REQUIRED = 0x09,
	NET_CAUSE_NOTHING_TO_CANCEL = 0x0A,
	NET_CAUSE_UNABLE_TO_CANCEL = 0x0B,
	NET_CAUSE_NETWORK_FORBIDDEN = 0x0C,
	NET_CAUSE_REQUEST_REJECTED = 0x0D,
	NET_CAUSE_CS_NOT_SUPPORTED = 0x0E,
	NET_CAUSE_PAR_INFO_NOT_AVAILABLE = 0x0F,
	NET_CAUSE_NOT_DONE = 0x10,
	NET_CAUSE_NO_SELECTED_NETWORK = 0x11,
	NET_CAUSE_REQUEST_INTERRUPTED = 0x12,
	NET_CAUSE_TOO_BIG_INDEX = 0x14,
	NET_CAUSE_MEMORY_FULL = 0x15,
	NET_CAUSE_SERVICE_NOT_ALLOWED = 0x16,
	NET_CAUSE_NOT_SUPPORTED_IN_TECH = 0x17
};

#ifdef __cplusplus
};
#endif

#endif /* !__ISIMODEM_NETWORK_H */

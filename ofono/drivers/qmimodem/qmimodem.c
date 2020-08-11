/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2011-2012  Intel Corporation. All rights reserved.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#define OFONO_API_SUBJECT_TO_CHANGE
#include <ofono/plugin.h>

#include "qmimodem.h"
#include <plugins/qmiextvoice.h>

static int qmimodem_init(void)
{
	qmi_devinfo_init();
	qmi_netreg_init();
	if (getenv("OFONO_QMI_EXTVOICE")) {
		qmiext_voicecall_init();
	} else {
		qmi_voicecall_init();
	}
	qmi_sim_legacy_init();
	qmi_sim_init();
	qmi_sms_init();
	qmi_ussd_init();
	qmi_gprs_init();
	qmi_gprs_context_init();
	qmi_lte_init();
	qmi_radio_settings_init();
	qmi_location_reporting_init();
	qmi_netmon_init();

	return 0;
}

static void qmimodem_exit(void)
{
	qmi_netmon_exit();
	qmi_location_reporting_exit();
	qmi_radio_settings_exit();
	qmi_lte_exit();
	qmi_gprs_context_exit();
	qmi_gprs_exit();
	qmi_ussd_exit();
	qmi_sms_exit();
	qmi_sim_exit();
	qmi_sim_legacy_exit();
	if (getenv("OFONO_QMI_EXTVOICE")) {
		qmiext_voicecall_exit();
	} else {
		qmi_voicecall_exit();
	}
	qmi_netreg_exit();
	qmi_devinfo_exit();
}

OFONO_PLUGIN_DEFINE(qmimodem, "Qualcomm QMI modem driver", VERSION,
		OFONO_PLUGIN_PRIORITY_DEFAULT, qmimodem_init, qmimodem_exit)

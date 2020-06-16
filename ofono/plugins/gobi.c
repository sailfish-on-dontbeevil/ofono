/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2008-2012  Intel Corporation. All rights reserved.
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

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <sailfish_manager.h>

#define OFONO_API_SUBJECT_TO_CHANGE
#include <ofono/plugin.h>
#include <ofono/modem.h>
#include <ofono/devinfo.h>
#include <ofono/netreg.h>
#include <ofono/netmon.h>
#include <ofono/phonebook.h>
#include <ofono/voicecall.h>
#include <ofono/sim.h>
#include <ofono/stk.h>
#include <ofono/sms.h>
#include <ofono/ussd.h>
#include <ofono/gprs.h>
#include <ofono/gprs-context.h>
#include <ofono/lte.h>
#include <ofono/radio-settings.h>
#include <ofono/location-reporting.h>
#include <ofono/log.h>
#include <ofono/message-waiting.h>

#include <drivers/qmimodem/qmi.h>
#include <drivers/qmimodem/dms.h>
#include <drivers/qmimodem/wda.h>
#include <drivers/qmimodem/util.h>

#define GOBI_DMS	(1 << 0)
#define GOBI_NAS	(1 << 1)
#define GOBI_WMS	(1 << 2)
#define GOBI_WDS	(1 << 3)
#define GOBI_PDS	(1 << 4)
#define GOBI_PBM	(1 << 5)
#define GOBI_UIM	(1 << 6)
#define GOBI_CAT	(1 << 7)
#define GOBI_CAT_OLD	(1 << 8)
#define GOBI_VOICE	(1 << 9)
#define GOBI_WDA	(1 << 10)

static struct sailfish_slot_driver_reg *slot_gobi_driver_reg = NULL;
static char *imei = "123456789012345";

struct gobi_data {
	struct qmi_device *device;
	struct qmi_service *dms;
	unsigned long features;
	unsigned int discover_attempts;
	uint8_t oper_mode;
};

static void gobi_debug(const char *str, void *user_data)
{
	const char *prefix = user_data;

	ofono_info("%s%s", prefix, str);
}

/*IMEI CALLBACK*/
static void gobi_get_ids_cb(struct qmi_result *result, void *user_data)
{
	char *str;
	struct cb_data *cbd = user_data;
	ofono_devinfo_query_cb_t cb = cbd->cb;

	str = qmi_result_get_string(result, QMI_DMS_RESULT_ESN);
	if (!str || strcmp(str, "0") == 0) {
		str = qmi_result_get_string(result, QMI_DMS_RESULT_IMEI);
		if (!str) {
			CALLBACK_WITH_FAILURE(cb, NULL, cbd->data);
			return;
		} else {
			ofono_info("Got IMEI %s", str);
			imei = str;
		}
    }
}

static int gobi_probe(struct ofono_modem *modem)
{
	struct gobi_data *data;

	DBG("%p", modem);

	data = g_try_new0(struct gobi_data, 1);
	if (!data)
		return -ENOMEM;

	ofono_modem_set_data(modem, data);

	return 0;
}

static void gobi_remove(struct ofono_modem *modem)
{
	struct gobi_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	ofono_modem_set_data(modem, NULL);

	qmi_service_unref(data->dms);

	qmi_device_unref(data->device);

	g_free(data);
}

static void shutdown_cb(void *user_data)
{
	struct ofono_modem *modem = user_data;
	struct gobi_data *data = ofono_modem_get_data(modem);

	DBG("");

	data->discover_attempts = 0;

	qmi_device_unref(data->device);
	data->device = NULL;

	ofono_modem_set_powered(modem, FALSE);
}

static void shutdown_device(struct ofono_modem *modem)
{
	struct gobi_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	qmi_service_unref(data->dms);
	data->dms = NULL;

	qmi_device_shutdown(data->device, shutdown_cb, modem, NULL);
}

static void power_reset_cb(struct qmi_result *result, void *user_data)
{
	struct ofono_modem *modem = user_data;

	DBG("");

	if (qmi_result_set_error(result, NULL)) {
		shutdown_device(modem);
		return;
	}

	ofono_modem_set_powered(modem, TRUE);
}

static void get_oper_mode_cb(struct qmi_result *result, void *user_data)
{
	struct ofono_modem *modem = user_data;
	struct gobi_data *data = ofono_modem_get_data(modem);
	struct qmi_param *param;
	uint8_t mode;

	DBG("");

	if (qmi_result_set_error(result, NULL)) {
		shutdown_device(modem);
		return;
	}

	if (!qmi_result_get_uint8(result, QMI_DMS_RESULT_OPER_MODE, &mode)) {
		shutdown_device(modem);
		return;
	}

	data->oper_mode = mode;

	/*
	 * Telit QMI LTE modem must remain online. If powered down, it also
	 * powers down the sim card, and QMI interface has no way to bring
	 * it back alive.
	 */
	if (ofono_modem_get_boolean(modem, "AlwaysOnline")) {
		ofono_modem_set_powered(modem, TRUE);
		return;
	}

	switch (data->oper_mode) {
	case QMI_DMS_OPER_MODE_ONLINE:
		param = qmi_param_new_uint8(QMI_DMS_PARAM_OPER_MODE,
					QMI_DMS_OPER_MODE_PERSIST_LOW_POWER);
		if (!param) {
			shutdown_device(modem);
			return;
		}

		if (qmi_service_send(data->dms, QMI_DMS_SET_OPER_MODE, param,
					power_reset_cb, modem, NULL) > 0)
			return;

		shutdown_device(modem);
		break;
	default:
		ofono_modem_set_powered(modem, TRUE);
		break;
	}
}

static void get_caps_cb(struct qmi_result *result, void *user_data)
{
	struct ofono_modem *modem = user_data;
	struct gobi_data *data = ofono_modem_get_data(modem);
	const struct qmi_dms_device_caps *caps;
	uint16_t len;
	uint8_t i;

	DBG("");

	if (qmi_result_set_error(result, NULL))
		goto error;

	caps = qmi_result_get(result, QMI_DMS_RESULT_DEVICE_CAPS, &len);
	if (!caps)
		goto error;

        ofono_info("service capabilities %d", caps->data_capa);
        ofono_info("sim supported %d", caps->sim_supported);

        for (i = 0; i < caps->radio_if_count; i++)
                DBG("radio = %d", caps->radio_if[i]);

	if (qmi_service_send(data->dms, QMI_DMS_GET_OPER_MODE, NULL,
					get_oper_mode_cb, modem, NULL) > 0)
		return;

error:
	shutdown_device(modem);
}

static void create_dms_cb(struct qmi_service *service, void *user_data)
{
	struct ofono_modem *modem = user_data;
	struct gobi_data *data = ofono_modem_get_data(modem);

	DBG("");

	if (!service)
		goto error;

	data->dms = qmi_service_ref(service);
	/*Get modem IMEI*/
    qmi_service_send(data->dms, QMI_DMS_GET_IDS, NULL,
                     gobi_get_ids_cb, modem, NULL);
	
	if (qmi_service_send(data->dms, QMI_DMS_GET_CAPS, NULL,
					get_caps_cb, modem, NULL) > 0)
		return;

error:
	shutdown_device(modem);
}

static void create_shared_dms(void *user_data)
{
	struct ofono_modem *modem = user_data;
	struct gobi_data *data = ofono_modem_get_data(modem);

	qmi_service_create_shared(data->device, QMI_SERVICE_DMS,
				  create_dms_cb, modem, NULL);
}

static void discover_cb(void *user_data)
{
	struct ofono_modem *modem = user_data;
	struct gobi_data *data = ofono_modem_get_data(modem);
	uint16_t major, minor;

	DBG("");

	if (qmi_device_has_service(data->device, QMI_SERVICE_DMS))
		data->features |= GOBI_DMS;
	if (qmi_device_has_service(data->device, QMI_SERVICE_NAS))
		data->features |= GOBI_NAS;
	if (qmi_device_has_service(data->device, QMI_SERVICE_WMS))
		data->features |= GOBI_WMS;
	if (qmi_device_has_service(data->device, QMI_SERVICE_WDS))
		data->features |= GOBI_WDS;
	if (qmi_device_has_service(data->device, QMI_SERVICE_WDA))
		data->features |= GOBI_WDA;
	if (qmi_device_has_service(data->device, QMI_SERVICE_PDS))
		data->features |= GOBI_PDS;
	if (qmi_device_has_service(data->device, QMI_SERVICE_PBM))
		data->features |= GOBI_PBM;
	if (qmi_device_has_service(data->device, QMI_SERVICE_UIM))
		data->features |= GOBI_UIM;
	if (qmi_device_has_service(data->device, QMI_SERVICE_CAT))
		data->features |= GOBI_CAT;
	if (qmi_device_get_service_version(data->device,
				QMI_SERVICE_CAT_OLD, &major, &minor))
		if (major > 0)
			data->features |= GOBI_CAT_OLD;
	if (qmi_device_has_service(data->device, QMI_SERVICE_VOICE))
			data->features |= GOBI_VOICE;

	if (!(data->features & GOBI_DMS)) {
		if (++data->discover_attempts < 3) {
			qmi_device_discover(data->device, discover_cb,
								modem, NULL);
			return;
		}

		shutdown_device(modem);
		return;
	}

	if (qmi_device_is_sync_supported(data->device))
		qmi_device_sync(data->device, create_shared_dms, modem);
	else
		create_shared_dms(modem);
}

static int gobi_enable(struct ofono_modem *modem)
{
	struct gobi_data *data = ofono_modem_get_data(modem);
	const char *device;
	int fd;

	DBG("%p", modem);

	device = ofono_modem_get_string(modem, "Device");
	if (!device)
		return -EINVAL;

	fd = open(device, O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return -EIO;

	data->device = qmi_device_new(fd);
	if (!data->device) {
		close(fd);
		return -ENOMEM;
	}

	if (getenv("OFONO_QMI_DEBUG"))
		qmi_device_set_debug(data->device, gobi_debug, "QMI: ");

	qmi_device_set_close_on_unref(data->device, true);

	qmi_device_discover(data->device, discover_cb, modem, NULL);

	return -EINPROGRESS;
}

static void power_disable_cb(struct qmi_result *result, void *user_data)
{
	struct ofono_modem *modem = user_data;

	DBG("");

	shutdown_device(modem);
}

static int gobi_disable(struct ofono_modem *modem)
{
	struct gobi_data *data = ofono_modem_get_data(modem);
	struct qmi_param *param;

	DBG("%p", modem);

	qmi_service_cancel_all(data->dms);
	qmi_service_unregister_all(data->dms);

	/*
	 * Telit QMI modem must remain online. If powered down, it also
	 * powers down the sim card, and QMI interface has no way to bring
	 * it back alive.
	 */
	if (ofono_modem_get_boolean(modem, "AlwaysOnline"))
		goto out;

	param = qmi_param_new_uint8(QMI_DMS_PARAM_OPER_MODE,
					QMI_DMS_OPER_MODE_PERSIST_LOW_POWER);
	if (!param)
		return -ENOMEM;

	if (qmi_service_send(data->dms, QMI_DMS_SET_OPER_MODE, param,
					power_disable_cb, modem, NULL) > 0)
		return -EINPROGRESS;

out:
	shutdown_device(modem);

	return -EINPROGRESS;
}

static void set_online_cb(struct qmi_result *result, void *user_data)
{
	struct cb_data *cbd = user_data;
	ofono_modem_online_cb_t cb = cbd->cb;

	DBG("");

	if (qmi_result_set_error(result, NULL))
		CALLBACK_WITH_FAILURE(cb, cbd->data);
	else
		CALLBACK_WITH_SUCCESS(cb, cbd->data);
}

static void gobi_set_online(struct ofono_modem *modem, ofono_bool_t online,
				ofono_modem_online_cb_t cb, void *user_data)
{
	struct gobi_data *data = ofono_modem_get_data(modem);
	struct cb_data *cbd = cb_data_new(cb, user_data);
	struct qmi_param *param;
	uint8_t mode;

	ofono_info("%p %s", modem, online ? "online" : "offline");

	if (online)
		mode = QMI_DMS_OPER_MODE_ONLINE;
	else
		mode = QMI_DMS_OPER_MODE_LOW_POWER;

	param = qmi_param_new_uint8(QMI_DMS_PARAM_OPER_MODE, mode);
	if (!param)
		goto error;

	if (qmi_service_send(data->dms, QMI_DMS_SET_OPER_MODE, param,
					set_online_cb, cbd, g_free) > 0)
		return;

	qmi_param_free(param);

error:
	CALLBACK_WITH_FAILURE(cb, cbd->data);

	g_free(cbd);
}

static void gobi_pre_sim(struct ofono_modem *modem)
{
	struct gobi_data *data = ofono_modem_get_data(modem);
	const char *sim_driver = NULL;

	DBG("%p", modem);

	ofono_devinfo_create(modem, 0, "qmimodem", data->device);

	if (data->features & GOBI_UIM)
		sim_driver = "qmimodem";
	else if (data->features & GOBI_DMS)
		sim_driver = "qmimodem-legacy";

	if (ofono_modem_get_boolean(modem, "ForceSimLegacy"))
		sim_driver = "qmimodem-legacy";

	ofono_info("Use modem %s", sim_driver);
	
	
	if (sim_driver)
		ofono_sim_create(modem, 0, sim_driver, data->device);

	if (data->features & GOBI_VOICE) {
		ofono_voicecall_create(modem, 0, "qmimodem", data->device);
	}

	if (data->features & GOBI_PDS)
		ofono_location_reporting_create(modem, 0, "qmimodem",
							data->device);
}

static void gobi_post_sim(struct ofono_modem *modem)
{
	struct gobi_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	ofono_lte_create(modem, 0, "qmimodem", data->device);

	if (data->features & GOBI_CAT)
		ofono_stk_create(modem, 0, "qmimodem", data->device);
	else if (data->features & GOBI_CAT_OLD)
		ofono_stk_create(modem, 1, "qmimodem", data->device);

	if (data->features & GOBI_PBM)
		ofono_phonebook_create(modem, 0, "qmimodem", data->device);

	if (data->features & GOBI_NAS)
		ofono_radio_settings_create(modem, 0, "qmimodem", data->device);

	if (data->features & GOBI_WMS)
		ofono_sms_create(modem, 0, "qmimodem", data->device);

	if ((data->features & GOBI_WMS) && (data->features & GOBI_UIM) &&
			!ofono_modem_get_boolean(modem, "ForceSimLegacy")) {
		struct ofono_message_waiting *mw =
					ofono_message_waiting_create(modem);

		if (mw)
			ofono_message_waiting_register(mw);
	}
}

static void gobi_post_online(struct ofono_modem *modem)
{
	struct gobi_data *data = ofono_modem_get_data(modem);
	struct ofono_gprs *gprs;
	struct ofono_gprs_context *gc;

	ofono_info("Post online %p", modem);

	if (data->features & GOBI_NAS) {
		ofono_netreg_create(modem, 0, "qmimodem", data->device);
		ofono_netmon_create(modem, 0, "qmimodem", data->device);
	} else {
		ofono_info("NO NAS");
	}

	if (data->features & GOBI_VOICE) {
		ofono_ussd_create(modem, 0, "qmimodem", data->device);
	}  else {
		ofono_info("NO VOISE");
	}

	if (data->features & GOBI_WDS) {
		gprs = ofono_gprs_create(modem, 0, "qmimodem", data->device);
		gc = ofono_gprs_context_create(modem, 0, "qmimodem",
							data->device);

		if (gprs && gc) {
			ofono_gprs_add_context(gprs, gc);
		}
	}  else {
		ofono_info("NO WDS");
	}
}

/* sailfish_slot_driver callbacks */

typedef struct sailfish_slot_manager_impl {
	struct sailfish_slot_manager *handle;
	guint start_timeout_id;
	GSList *slots;
} slot_gobi_plugin;

typedef struct sailfish_slot_impl {
	struct sailfish_slot *handle;
	struct ofono_watch *watch;
	struct ofono_modem *modem;
	slot_gobi_plugin *plugin;
	gulong sim_watch_id;
	gulong uicc_event_id;
	gboolean sim_inserted;
	char *path;
	char *usbdev;
	char *manufacturer;
	char *model;
	char *revision;
	char *imei;
	int port_speed;
	int frame_size;
	guint disconnect_id;
	GIOChannel *channel;
	GHashTable *options;
	guint start_timeout;
	guint start_timeout_id;
	guint retry_init_id;
	guint setup_id;
} slot_gobi_slot;


static slot_gobi_plugin *slot_gobi_plugin_create(struct sailfish_slot_manager *m)
{
	slot_gobi_plugin *plugin = g_new0(slot_gobi_plugin, 1);

	ofono_info("CREATE SFOS MANAGER PLUGIN");
	plugin->handle = m;

	return plugin;
	
}

static void slot_gobi_slot_enabled_changed(slot_gobi_slot *slot)
{
	ofono_info("Enable slot changed");
	int err = 0;
	ofono_info("Slot %d", slot->handle->enabled);
	
	if(slot->handle->enabled) {
		ofono_info("Enable slot");
		slot->modem = ofono_modem_create("quectelqmi_0", "quectelqmi");
		if(slot->modem) {
			err = ofono_modem_register(slot->modem);
		}
		
		ofono_info("Modem error status %d", err);
		
		if (!err) {
			ofono_error("Error %d registering %s modem", err,
				"quectelqmi");
			//ofono_modem_remove(slot->modem);
			//slot->modem = NULL;
		}
	} else {
		ofono_info("Disable slot");
		ofono_modem_remove(slot->modem);
		slot->modem = NULL;
	}
}

static guint slot_gobi_plugin_start(slot_gobi_plugin *plugin)
{
	slot_gobi_slot *slot = g_new0(slot_gobi_slot, 1);
	
	plugin->slots = g_slist_insert(plugin->slots, slot, 0);

	slot->imei = imei;
	
	slot->handle = sailfish_manager_slot_add(plugin->handle, slot,
				"/quectelqmi_0", (OFONO_RADIO_ACCESS_MODE_GSM | OFONO_RADIO_ACCESS_MODE_UMTS | OFONO_RADIO_ACCESS_MODE_LTE),
				slot->imei, "00", SAILFISH_SIM_STATE_PRESENT);
	
//	slot_gobi_slot_enabled_changed(slot);
		
	return 0;
}

static void slot_gobi_plugin_cancel_start(slot_gobi_plugin *plugin, guint id)
{
	ofono_info("slot_gobi_plugin_cancel_start");
	ofono_info("%u", id);
	g_source_remove(id);
}

static void slot_gobi_plugin_free(slot_gobi_plugin *plugin)
{
	ofono_info("slot_gobi_plugin_free");
	g_free(plugin);
}

static void slot_gobi_slot_set_data_role(slot_gobi_slot *slot,
						enum sailfish_data_role role)
{
	ofono_info("slot_gobi_slot_set_data_role");
	ofono_info("%d", role);
}

static void slot_gobi_slot_free(slot_gobi_slot *slot)
{
//TODO add functionality
	ofono_info("slot_gobi_slot_free");
}

static const struct sailfish_slot_driver slot_gobi_driver = {
	.name = "slot_gobi",
	.manager_create =		slot_gobi_plugin_create,
	.manager_start = 		slot_gobi_plugin_start,
	.manager_cancel_start = slot_gobi_plugin_cancel_start,
	.manager_free = 		slot_gobi_plugin_free,
	.slot_enabled_changed = slot_gobi_slot_enabled_changed,
	.slot_set_data_role = 	slot_gobi_slot_set_data_role,
	.slot_free = 			slot_gobi_slot_free
};

/* end of sailfish_slot_driver callbacks*/

static struct ofono_modem_driver gobi_driver = {
	.name		= "gobi",
	.probe		= gobi_probe,
	.remove		= gobi_remove,
	.enable		= gobi_enable,
	.disable	= gobi_disable,
	.set_online	= gobi_set_online,
	.pre_sim	= gobi_pre_sim,
	.post_sim	= gobi_post_sim,
	.post_online	= gobi_post_online,
};

static int gobi_init(void)
{
	slot_gobi_driver_reg = sailfish_slot_driver_register(&slot_gobi_driver);
	return ofono_modem_driver_register(&gobi_driver);
}

static void gobi_exit(void)
{
	ofono_modem_driver_unregister(&gobi_driver);
}

OFONO_PLUGIN_DEFINE(gobi, "Qualcomm Gobi modem driver", VERSION,
			OFONO_PLUGIN_PRIORITY_DEFAULT, gobi_init, gobi_exit)

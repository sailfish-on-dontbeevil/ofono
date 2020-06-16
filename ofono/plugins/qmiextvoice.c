/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2011-2012  Intel Corporation. All rights reserved.
 *  Copyright (C) 2017 Alexander Couzens <lynxis@fe80.eu>
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



#include "qmiextvoice.h"

#include <drivers/qmimodem/qmimodem.h>
#include <drivers/qmimodem/qmi.h>
#include <drivers/qmimodem/dms.h>
#include <drivers/qmimodem/wda.h>
#include <drivers/qmimodem/voice.h>

struct qmi_voicecall_data {
	struct qmi_service *voice;
	uint16_t major;
	uint16_t minor;
	GSList *call_list;
	struct voicecall_static *vs;
	struct ofono_phone_number dialed;
};

enum call_direction qmiext_to_ofono_direction(uint8_t qmi_direction) {
	return qmi_direction - 1;
}

enum parse_error qmiext_voice_ind_call_status(
		struct qmi_result *qmi_result,
		struct qmiext_voice_all_call_status_ind *result)
{
	int err = NONE;
	int offset;
	uint16_t len;
	const struct qmiext_voice_remote_party_number *remote_party_number;
	const struct qmiext_voice_call_information *call_information;

	/* mandatory */
	call_information = qmi_result_get(qmi_result, 0x01, &len);
	if (call_information)
	{
		/* verify the length */
		if (len < sizeof(call_information->size))
			return INVALID_LENGTH;

		if (len != call_information->size * sizeof(struct qmiext_voice_call_information_instance)
			    + sizeof(call_information->size))
			return INVALID_LENGTH;
		result->call_information_set = 1;
		result->call_information = call_information;
	} else
		return MISSING_MANDATORY;

	/* mandatory */
	remote_party_number = qmi_result_get(qmi_result, 0x10, &len);
	if (remote_party_number) {
		const struct qmiext_voice_remote_party_number_instance *instance;
		int instance_size = sizeof(struct qmiext_voice_remote_party_number_instance);
		int i;

		/* verify the length */
		if (len < sizeof(remote_party_number->size))
			return INVALID_LENGTH;

		for (i = 0, offset = sizeof(remote_party_number->size);
		     offset <= len && i < 16 && i < remote_party_number->size; i++)
		{
			if (offset == len) {
				break;
			} else if (offset + instance_size > len) {
				return INVALID_LENGTH;
			}

			instance = (void *)remote_party_number + offset;
			result->remote_party_number[i] = instance;
			offset += sizeof(struct qmiext_voice_remote_party_number_instance) + instance->number_size;
		}
		result->remote_party_number_set = 1;
		result->remote_party_number_size = remote_party_number->size;
	} else
		return MISSING_MANDATORY;

	return err;
}

enum parse_error qmiext_voice_dial_call_parse(
		struct qmi_result *qmi_result,
		struct qmiext_voice_dial_call_result *result)
{
	int err = NONE;

	/* mandatory */
	if (qmi_result_get_uint8(qmi_result, 0x10, &result->call_id))
		result->call_id_set = 1;
	else
		err = MISSING_MANDATORY;

	return err;
}

enum parse_error qmiext_voice_answer_call_parse(
		struct qmi_result *qmi_result,
		struct qmiext_voice_answer_call_result *result)
{
	int err = NONE;

	/* optional */
	if (qmi_result_get_uint8(qmi_result, 0x10, &result->call_id))
		result->call_id_set = 1;

	return err;
}

enum parse_error qmiext_voice_end_call_parse(
		struct qmi_result *qmi_result,
		struct qmiext_voice_end_call_result *result)
{
	int err = NONE;

	/* optional */
	if (qmi_result_get_uint8(qmi_result, 0x10, &result->call_id))
		result->call_id_set = 1;

	return err;
}

int qmiext_to_ofono_status(uint8_t status, int *ret) {
	int err = 0;
	switch (status) {
	case QMI_CALL_STATE_DISCONNECTING:
		*ret = CALL_STATUS_DISCONNECTED;
		break;
	case QMI_CALL_STATE_HOLD:
		*ret = CALL_STATUS_HELD;
		break;
	case QMI_CALL_STATE_WAITING:
		*ret = CALL_STATUS_WAITING;
		break;
	case QMI_CALL_STATE_ORIG:
		*ret = CALL_STATUS_DIALING;
		break;
	case QMI_CALL_STATE_INCOMING:
		*ret = CALL_STATUS_INCOMING;
		break;
	case QMI_CALL_STATE_CONV:
		*ret = CALL_STATUS_ACTIVE;
		break;
	case QMI_CALL_STATE_CC_IN_PROG:
		*ret = CALL_STATUS_DIALING;
		break;
	case QMI_CALL_STATE_ALERTING:
		*ret = CALL_STATUS_ALERTING;
		break;
	case QMI_CALL_STATE_SETUP:
		/* FIXME: unsure if _SETUP is dialing or not */
		DBG("QMI_CALL_STATE_SETUP unsupported");
		err = 1;
		break;
	case QMI_CALL_STATE_IDLE:
		DBG("QMI_CALL_STATE_IDLE unsupported");
		err = 1;
		break;
	case QMI_CALL_STATE_END:
		DBG("QMI_CALL_STATE_END unsupported");
		err = 1;
		break;
	default:
		err = 1;
	}
	return err;
}

int qmiext_voice_end_call(
		struct qmiext_voice_end_call_arg *arg,
		struct qmi_service *service,
		qmi_result_func_t func,
		void *user_data,
		qmi_destroy_func_t destroy)
{
	struct qmi_param *param = NULL;

	param = qmi_param_new();
	if (!param)
		goto error;

	if (arg->call_id_set) {
		if (!qmi_param_append_uint8(
					param,
					0x1,
					arg->call_id))
			goto error;
	}

	if (qmi_service_send(service,
			     0x21,
			     param,
			     func,
			     user_data,
			     destroy) > 0)
		return 0;
error:
	g_free(param);
	return 1;
}

int qmiext_voice_dial_call(
		struct qmiext_voice_dial_call_arg *arg,
		struct qmi_service *service,
		qmi_result_func_t func,
		void *user_data,
		qmi_destroy_func_t destroy)
{
	struct qmi_param *param = NULL;

	param = qmi_param_new();
	if (!param)
		goto error;

	if (arg->calling_number_set) {
		if (!qmi_param_append(param,
				 0x1,
				 strlen(arg->calling_number),
				 arg->calling_number))
			goto error;
	}

	if (arg->call_type_set)
		qmi_param_append_uint8(param, 0x10, arg->call_type);

	if (qmi_service_send(service,
			     0x20,
			     param,
			     func,
			     user_data,
			     destroy) > 0)
		return 0;
error:
	DBG("qmiext_voice_dial_call ERROR");
	g_free(param);
	return 1;
}

int qmiext_voice_answer_call(
		struct qmiext_voice_answer_call_arg *arg,
		struct qmi_service *service,
		qmi_result_func_t func,
		void *user_data,
		qmi_destroy_func_t destroy)
{
	struct qmi_param *param = NULL;

	param = qmi_param_new();
	if (!param)
		goto error;

	if (arg->call_id_set) {
		if (!qmi_param_append_uint8(
					param,
					0x1,
					arg->call_id))
			goto error;
	}

	if (qmi_service_send(service,
			     0x22,
			     param,
			     func,
			     user_data,
			     destroy) > 0)
		return 0;
error:
	g_free(param);
	return 1;
}

const char *qmiext_voice_call_state_name(enum qmiext_voice_call_state value)
{
	switch (value) {
		_(QMI_CALL_STATE_IDLE);
		_(QMI_CALL_STATE_ORIG);
		_(QMI_CALL_STATE_INCOMING);
		_(QMI_CALL_STATE_CONV);
		_(QMI_CALL_STATE_CC_IN_PROG);
		_(QMI_CALL_STATE_ALERTING);
		_(QMI_CALL_STATE_HOLD);
		_(QMI_CALL_STATE_WAITING);
		_(QMI_CALL_STATE_DISCONNECTING);
		_(QMI_CALL_STATE_END);
		_(QMI_CALL_STATE_SETUP);
	}
	return "QMI_CALL_STATE_<UNKNOWN>";
}

gint qmiext_at_util_call_compare_by_id(gconstpointer a, gconstpointer b)
{
	const struct ofono_call *call = a;
	unsigned int id = GPOINTER_TO_UINT(b);

	if (id < call->id)
		return -1;

	if (id > call->id)
		return 1;

	return 0;
}

gint qmiext_at_util_call_compare(gconstpointer a, gconstpointer b)
{
	const struct ofono_call *ca = a;
	const struct ofono_call *cb = b;

	if (ca->id < cb->id)
		return -1;

	if (ca->id > cb->id)
		return 1;

	return 0;
}

gint qmiext_at_util_call_compare_by_status(gconstpointer a, gconstpointer b)
{
	const struct ofono_call *call = a;
	int status = GPOINTER_TO_INT(b);

	if (status != call->status)
		return 1;

	return 0;
}

void qmiext_at_util_call_list_notify(struct ofono_voicecall *vc,
	        GSList **call_list,
	        GSList *calls)
{
    GSList *old_calls = *call_list;
    GSList *new_calls = calls;
    struct ofono_call *new_call, *old_call;

    while (old_calls || new_calls) {
	old_call = old_calls ? old_calls->data : NULL;
	new_call = new_calls ? new_calls->data : NULL;

	/* we drop disconnected calls and treat them as not existent */
	if (new_call && new_call->status == CALL_STATUS_DISCONNECTED) {
	    new_calls = new_calls->next;
	    calls = g_slist_remove(calls, new_call);
	    g_free(new_call);
	    continue;
	}

	if (old_call &&
		(new_call == NULL ||
		(new_call->id > old_call->id))) {
	    ofono_voicecall_disconnected(
			vc,
			old_call->id,
			OFONO_DISCONNECT_REASON_LOCAL_HANGUP,
			NULL);
	    old_calls = old_calls->next;
	} else if (new_call &&
	       (old_call == NULL ||
	       (new_call->id < old_call->id))) {

	    /* new call, signal it */
	    if (new_call->type == 0)
		ofono_voicecall_notify(vc, new_call);

	    new_calls = new_calls->next;
	} else {
	    if (memcmp(new_call, old_call, sizeof(*new_call))
		    && new_call->type == 0)
		ofono_voicecall_notify(vc, new_call);

	    new_calls = new_calls->next;
	    old_calls = old_calls->next;
	}
    }

    g_slist_free_full(*call_list, g_free);
    *call_list = calls;
}

void qmiext_at_util_call_list_dial_callback(struct ofono_voicecall *vc,
		   GSList **call_list,
		   const struct ofono_phone_number *ph,
		   int call_id)
{
    GSList *list;
    struct ofono_call *call;

    /* list_notify could be triggered before this call back is handled */
    list = g_slist_find_custom(*call_list,
		   GINT_TO_POINTER(call_id),
		   qmiext_at_util_call_compare_by_id);

    if (list && list->data) {
	call = list->data;
	DBG("Call id %d already known. In state %s(%d)",
	    call_id, call_status_to_string(call->status),
	    call->status);
	return;
    }

    call = g_new0(struct ofono_call, 1);
    call->id = call_id;

    memcpy(&call->called_number, ph, sizeof(*ph));
    call->direction = CALL_DIRECTION_MOBILE_ORIGINATED;
    call->status = CALL_STATUS_DIALING;
    call->type = 0; /* voice */

    *call_list = g_slist_insert_sorted(*call_list,
		        call,
		        qmiext_at_util_call_compare);
    ofono_voicecall_notify(vc, call);
}

static void all_call_status_ind(struct qmi_result *result, void *user_data)
{
	DBG("all_call_status_ind");

	struct ofono_voicecall *vc = user_data;
	struct qmi_voicecall_data *vd = ofono_voicecall_get_data(vc);
	GSList *calls = NULL;
	int i;
	int size = 0;
	struct qmiext_voice_all_call_status_ind status_ind;


	if (qmiext_voice_ind_call_status(result, &status_ind) != NONE) {
		DBG("Parsing of all call status indication failed");
		return;
	}

	if (!status_ind.remote_party_number_set || !status_ind.call_information_set) {
		DBG("Some required fields are not set");
		return;
	}

	size = status_ind.call_information->size;
	if (!size) {
		DBG("No call informations received!");
		return;
	}

	/* expect we have valid fields for every call */
	if (size != status_ind.remote_party_number_size)  {
		DBG("Not all fields have the same size");
		return;
	}

	for (i = 0; i < size; i++) {
		struct qmiext_voice_call_information_instance call_info;
		struct ofono_call *call;
		const struct qmiext_voice_remote_party_number_instance *remote_party = status_ind.remote_party_number[i];
		int number_size;

		call_info = status_ind.call_information->instance[i];
		call = g_new0(struct ofono_call, 1);
		call->id = call_info.id;
		call->direction = qmiext_to_ofono_direction(call_info.direction);
		call->status = 1;

		if (qmiext_to_ofono_status(call_info.state, &call->status)) {
			if(call_info.state == QMI_CALL_STATE_END) {
				struct ofono_error error = {
					OFONO_ERROR_TYPE_NO_ERROR, 0
				};
				ofono_voicecall_disconnected(vc, call->id, 0, &error);
				continue;
			}
			DBG("Ignore call id %d, because can not convert QMI state 0x%x to ofono.",
			    call_info.id, call_info.state);
			continue;
		}
		
		DBG("Call %d in state %s(%d)",
		    call_info.id,
		    qmiext_voice_call_state_name(call_info.state),
		    call_info.state);

		call->type = 0; /* always voice */
		number_size = remote_party->number_size;
		strncpy(call->phone_number.number, remote_party->number,
				number_size);
		/* FIXME: set phone_number_type */

		if (strlen(call->phone_number.number) > 0)
			call->clip_validity = 0;
		else
			call->clip_validity = 2;

		calls = g_slist_insert_sorted(calls, call, qmiext_at_util_call_compare);
	}

	qmiext_at_util_call_list_notify(vc, &vd->call_list, calls);
}

static void create_voice_cb(struct qmi_service *service, void *user_data)
{
	struct ofono_voicecall *vc = user_data;
	struct qmi_voicecall_data *data = ofono_voicecall_get_data(vc);

	DBG("create_voice_cb");

	if (!service) {
		ofono_error("Failed to request Voice service");
		ofono_voicecall_remove(vc);
		return;
	}

	if (!qmi_service_get_version(service, &data->major, &data->minor)) {
		ofono_error("Failed to get Voice service version");
		ofono_voicecall_remove(vc);
		return;
	}

	data->voice = qmi_service_ref(service);

	/* FIXME: we should call indication_register to ensure we get notified on call events.
	 * We rely at the moment on the default value of notifications
	 */
	qmi_service_register(data->voice, QMI_VOICE_IND_ALL_STATUS,
			     all_call_status_ind, vc, NULL);

	ofono_voicecall_register(vc);
}

static int qmiext_voicecall_probe(struct ofono_voicecall *vc,
					unsigned int vendor, void *user_data)
{
	struct qmi_device *device = user_data;
	struct qmi_voicecall_data *data;

	DBG("");

	data = g_new0(struct qmi_voicecall_data, 1);

	ofono_voicecall_set_data(vc, data);

	qmi_service_create(device, QMI_SERVICE_VOICE,
					create_voice_cb, vc, NULL);

	return 0;
}

static void qmiext_voicecall_remove(struct ofono_voicecall *vc)
{
	struct qmiext_voicecall_data *data = ofono_voicecall_get_data(vc);
	DBG("QMI Ext Voicecall plugin remove");
	ofono_voicecall_set_data(vc, NULL);
	qmi_service_unregister_all(data->voice);
	qmi_service_unref(data->voice);
	g_free(data);
}

static void dial_cb(struct qmi_result *result, void *user_data)
{
	struct cb_data *cbd = user_data;
	struct ofono_voicecall *vc = cbd->user;
	struct qmiext_voicecall_data *vd = ofono_voicecall_get_data(vc);
	ofono_voicecall_cb_t cb = cbd->cb;
	uint16_t error;
	struct qmiext_voice_dial_call_result dial_result;

	if (qmi_result_set_error(result, &error)) {
		DBG("QMI Error %d", error);
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		return;
	}

	if (NONE != qmiext_voice_dial_call_parse(result, &dial_result)) {
		DBG("Received invalid Result");
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		return;
	}

	if (!dial_result.call_id_set) {
		DBG("Didn't receive a call id");
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		return;
	}

	DBG("New call QMI id %d", dial_result.call_id);
	qmiext_at_util_call_list_dial_callback(vc,
				      &vd->call_list,
				      &vd->dialed,
				      dial_result.call_id);


	/* FIXME: create a timeout on this call_id */
	CALLBACK_WITH_SUCCESS(cb, cbd->data);
}

static void dial(struct ofono_voicecall *vc, const struct ofono_phone_number *ph,
		enum ofono_clir_option clir, ofono_voicecall_cb_t cb,
		void *data)
{
	DBG("dial");
	struct qmi_voicecall_data *vd = ofono_voicecall_get_data(vc);
	struct cb_data *cbd = cb_data_new(cb, data);
	struct qmiext_voice_dial_call_arg arg;

	cbd->user = vc;
	arg.calling_number_set = true;
	arg.calling_number = ph->number;
	memcpy(&vd->dialed, ph, sizeof(*ph));

	arg.call_type_set = true;
	arg.call_type = QMI_CALL_TYPE_VOICE_FORCE;

	if (!qmiext_voice_dial_call(
				&arg,
				vd->voice,
				dial_cb,
				cbd,
				g_free))
		return;

	CALLBACK_WITH_FAILURE(cb, data);
	g_free(cbd);
}

static void answer_cb(struct qmi_result *result, void *user_data)
{
	struct cb_data *cbd = user_data;
	ofono_voicecall_cb_t cb = cbd->cb;
	uint16_t error;
	struct qmiext_voice_answer_call_result answer_result;

	if (qmi_result_set_error(result, &error)) {
		DBG("QMI Error %d", error);
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		return;
	}

	/* TODO: what happens when calling it with no active call or wrong caller id? */
	if (NONE != qmiext_voice_answer_call_parse(result, &answer_result)) {
		DBG("Received invalid Result");
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		return;
	}

	CALLBACK_WITH_SUCCESS(cb, cbd->data);
}

static void answer(struct ofono_voicecall *vc, ofono_voicecall_cb_t cb, void *data)
{
	struct qmi_voicecall_data *vd = ofono_voicecall_get_data(vc);
	struct cb_data *cbd = cb_data_new(cb, data);
	struct qmiext_voice_answer_call_arg arg;
	struct ofono_call *call;
	GSList *list;

	DBG("");
	cbd->user = vc;

	list = g_slist_find_custom(vd->call_list,
				   GINT_TO_POINTER(CALL_STATUS_INCOMING),
				   qmiext_at_util_call_compare_by_status);

	if (list == NULL) {
		DBG("Can not find a call to answer");
		goto err;
	}

	call = list->data;

	arg.call_id_set = true;
	arg.call_id = call->id;

	if (!qmiext_voice_answer_call(
				&arg,
				vd->voice,
				answer_cb,
				cbd,
				g_free))
		return;
err:
	CALLBACK_WITH_FAILURE(cb, data);
	g_free(cbd);
}

static void end_cb(struct qmi_result *result, void *user_data)
{
	struct cb_data *cbd = user_data;
	ofono_voicecall_cb_t cb = cbd->cb;
	uint16_t error;
	struct qmiext_voice_end_call_result end_result;

	if (qmi_result_set_error(result, &error)) {
		DBG("QMI Error %d", error);
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		return;
	}

	if (NONE != qmiext_voice_end_call_parse(result, &end_result)) {
		DBG("Received invalid Result");
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		return;
	}

	CALLBACK_WITH_SUCCESS(cb, cbd->data);
}

static void release_specific(struct ofono_voicecall *vc, int id,
		ofono_voicecall_cb_t cb, void *data)
{
	struct qmi_voicecall_data *vd = ofono_voicecall_get_data(vc);
	struct cb_data *cbd = cb_data_new(cb, data);
	struct qmiext_voice_end_call_arg arg;

	DBG("");
	cbd->user = vc;

	arg.call_id_set = true;
	arg.call_id = id;

	if (!qmiext_voice_end_call(&arg,
				vd->voice,
				end_cb,
				cbd,
				g_free))
		return;

	CALLBACK_WITH_FAILURE(cb, data);
	g_free(cbd);
}

static void hangup_active(struct ofono_voicecall *vc,
		ofono_voicecall_cb_t cb, void *data)
{
	struct qmi_voicecall_data *vd = ofono_voicecall_get_data(vc);
	struct ofono_call *call;
	GSList *list = NULL;
	enum call_status active[] = {
		CALL_STATUS_ACTIVE,
		CALL_STATUS_DIALING,
		CALL_STATUS_ALERTING
	};
	int i;

	DBG("");
	for (i = 0; i < ARRAY_SIZE(active); i++) {
		list = g_slist_find_custom(vd->call_list,
					   GINT_TO_POINTER(CALL_STATUS_ACTIVE),
					   qmiext_at_util_call_compare_by_status);

		if (list)
			break;
	}

	if (list == NULL) {
		DBG("Can not find a call to hang up");
		CALLBACK_WITH_FAILURE(cb, data);
		return;
	}

	call = list->data;
	release_specific(vc, call->id, cb, data);
}

static struct ofono_voicecall_driver driver = {
	.name		= "qmimodem",
	.probe		= qmiext_voicecall_probe,
	.remove		= qmiext_voicecall_remove,
	.dial		= dial,
	.answer		= answer,
	.hangup_active  = hangup_active,
	.release_specific  = release_specific,
};

void qmiext_voicecall_init(void)
{
	DBG("Use extended QMI voice interface");
	ofono_voicecall_driver_register(&driver);
}

void qmiext_voicecall_exit(void)
{
	ofono_voicecall_driver_unregister(&driver);
}

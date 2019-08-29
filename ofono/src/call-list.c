/*
 *
 *  oFono - Open Source Telephony
 *
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
 */

#include <glib.h>

#include "common.h"

#include <ofono/types.h>
#include <ofono/log.h>
#include <ofono/voicecall.h>
#include <ofono/call-list.h>

#include <string.h>

void ofono_call_list_dial_callback(struct ofono_voicecall *vc,
				   GSList **call_list,
				   const struct ofono_phone_number *ph,
				   int call_id)
{
	GSList *list;
	struct ofono_call *call;

	/* list_notify could be triggered before this call back is handled */
	list = g_slist_find_custom(*call_list,
				   GINT_TO_POINTER(call_id),
				   ofono_call_compare_by_id);

	if (list && list->data) {
		call = list->data;
		DBG("Call id %d already known. In state %s(%d)",
		    call_id, ofono_call_status_to_string(call->status),
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
					    ofono_call_compare);
	ofono_voicecall_notify(vc, call);
}

void ofono_call_list_notify(struct ofono_voicecall *vc,
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
						OFONO_DISCONNECT_REASON_UNKNOWN,
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

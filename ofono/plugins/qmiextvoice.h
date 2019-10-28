#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gatchat.h>
#include <gatresult.h>

#include <common.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

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
#include <ofono/radio-settings.h>
#include <ofono/location-reporting.h>
#include <ofono/log.h>
#include <ofono/message-waiting.h>

/*Some enums and structs*/
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define QMI_VOICE_IND_ALL_STATUS 0x2e
#define _(X) case X: return #X

struct qmiext_voice_dial_call_result {
	bool call_id_set;
	uint8_t call_id;
};

enum qmiext_voice_call_type {
	QMI_CALL_TYPE_VOICE = 0x0,
	QMI_CALL_TYPE_VOICE_FORCE,
};

enum qmiext_voice_call_state {
	QMI_CALL_STATE_IDLE = 0x0,
	QMI_CALL_STATE_ORIG,
	QMI_CALL_STATE_INCOMING,
	QMI_CALL_STATE_CONV,
	QMI_CALL_STATE_CC_IN_PROG,
	QMI_CALL_STATE_ALERTING,
	QMI_CALL_STATE_HOLD,
	QMI_CALL_STATE_WAITING,
	QMI_CALL_STATE_DISCONNECTING,
	QMI_CALL_STATE_END,
	QMI_CALL_STATE_SETUP
};

enum parse_error {
	NONE = 0,
	MISSING_MANDATORY = 1,
	INVALID_LENGTH = 2,
};

struct qmiext_voice_answer_call_result {
	bool call_id_set;
	uint8_t call_id;
};

struct qmiext_voice_dial_call_arg {
	bool calling_number_set;
	const char *calling_number;
	bool call_type_set;
	uint8_t call_type;
};

struct qmiext_voicecall_data {
	struct qmi_service *voice;
	uint16_t major;
	uint16_t minor;
	GSList *call_list;
	struct voicecall_static *vs;
	struct ofono_phone_number dialed;
};

struct qmiext_voice_answer_call_arg {
	bool call_id_set;
	uint8_t call_id;
};

struct qmiext_voice_end_call_arg {
	bool call_id_set;
	uint8_t call_id;
};

struct qmiext_voice_end_call_result {
	bool call_id_set;
	uint8_t call_id;
};

struct qmiext_voice_all_call_status_ind {
	bool call_information_set;
	const struct qmiext_voice_call_information *call_information;
	bool remote_party_number_set;
	uint8_t remote_party_number_size;
	const struct qmiext_voice_remote_party_number_instance *remote_party_number[16];
};

struct qmiext_voice_call_information_instance {
	uint8_t id;
	uint8_t state;
	uint8_t type;
	uint8_t direction;
	uint8_t mode;
	uint8_t multipart_indicator;
	uint8_t als;
} __attribute__((__packed__));

struct qmiext_voice_call_information {
	uint8_t size;
	struct qmiext_voice_call_information_instance instance[0];
} __attribute__((__packed__)) ;

struct qmiext_voice_remote_party_number_instance {
	uint8_t call_id;
	uint8_t presentation_indicator;
	uint8_t number_size;
	char number[0];
} __attribute__((__packed__));

struct qmiext_voice_remote_party_number {
	uint8_t size;
	struct qmiext_voice_remote_party_number_instance instance[0];
} __attribute__((__packed__));

extern void qmiext_voicecall_init(void);
extern void qmiext_voicecall_exit(void);

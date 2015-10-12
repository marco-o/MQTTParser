#include "mqttparser.h"
#include <string.h>
#include <stdarg.h>

void mqtt_packet_push_byte(mqtt_packet_t *self, uint8_t *data);
void mqtt_packet_push_word(mqtt_packet_t *self, uint16_t *data);
void mqtt_packet_push_length(mqtt_packet_t *self, int *data);
void mqtt_packet_push_text(mqtt_packet_t *self, mqtt_text_t *data);
void mqtt_packet_push_message(mqtt_packet_t *self, mqtt_text_t *data);

void mqtt_packet_pop_byte(mqtt_packet_t *self, uint8_t *data);
void mqtt_packet_pop_word(mqtt_packet_t *self, uint16_t *data);
void mqtt_packet_pop_length(mqtt_packet_t *self, int *data);
void mqtt_packet_pop_text(mqtt_packet_t *self, mqtt_text_t *data);
void mqtt_packet_pop_message(mqtt_packet_t *self, mqtt_text_t *data);

typedef void(*mqtt_exx_byte_t)   (mqtt_packet_t *, uint8_t *);
typedef void(*mqtt_exx_word_t)   (mqtt_packet_t *, uint16_t *);
typedef void(*mqtt_exx_length_t) (mqtt_packet_t *, int *);
typedef void(*mqtt_exx_text_t)   (mqtt_packet_t *, mqtt_text_t *);
typedef void(*mqtt_exx_message_t)(mqtt_packet_t *, mqtt_text_t *);

typedef struct mqtt_exchanger_s
{
	mqtt_exx_byte_t    byte_exx;
	mqtt_exx_word_t    word_exx;
	mqtt_exx_length_t  length_exx;
	mqtt_exx_text_t    text_exx;
	mqtt_exx_message_t message_exx;
} mqtt_exchanger_t;

static mqtt_exchanger_t writer =
{
	mqtt_packet_push_byte,
	mqtt_packet_push_word,
	mqtt_packet_push_length,
	mqtt_packet_push_text,
	mqtt_packet_push_message
};

static mqtt_exchanger_t reader =
{
	mqtt_packet_pop_byte,
	mqtt_packet_pop_word,
	mqtt_packet_pop_length,
	mqtt_packet_pop_text,
	mqtt_packet_pop_message
};

void mqtt_packet_init(mqtt_packet_t *self, uint8_t *data, int size)
{
	self->data = data;
	self->size = size;
	self->head = 0;
}

void mqtt_packet_push_byte(mqtt_packet_t *self, uint8_t *data)
{
	if (self->head < self->size - 1)
		self->data[self->head] = *data;
	self->head++;
}

void mqtt_packet_push_word(mqtt_packet_t *self, uint16_t *data)
{
	if (self->head < self->size - 2)
	{
		self->data[self->head++] = (uint8_t)(*data >> 8);
		self->data[self->head++] = (uint8_t)(*data & 0xFF);
	}
	else
		self->head += 2;
}

void mqtt_packet_push_length(mqtt_packet_t *self, int *data)
{
	int value = *data;
	do
	{
		self->data[self->head++] = value & 0xff;
		value /= 0x80;
	} while (value > 0);
}

void mqtt_packet_push_text(mqtt_packet_t *self, mqtt_text_t *data)
{
	mqtt_packet_push_word(self, &data->length);
	mqtt_packet_push_message(self, data);
}

void mqtt_packet_push_message(mqtt_packet_t *self, mqtt_text_t *data)
{
	if (self->head + data->length <= self->size)
		memcpy(self->data + self->head, data->text, data->length);
	self->head += data->length;
}

void mqtt_packet_pop_byte(mqtt_packet_t *self, uint8_t *data)
{
	if (self->head < self->size - 1)
		*data = self->data[self->head];
	self->head++;
}

void mqtt_packet_pop_word(mqtt_packet_t *self, uint16_t *data)
{
	if (self->head < self->size - 1)
	{
		*data = self->data[self->head++];
		*data = (*data << 8) | self->data[self->head++];
	}
	else
		self->head += 2;
}

void mqtt_packet_pop_length(mqtt_packet_t *self, int *data)
{
	int i;
	int value = 0;
	for (i = 0; i < 4; i++)
	{
		uint8_t tmp = self->data[self->head++];
		value = (value << 7) + (tmp & 0x7f);
		if ((tmp & 0x80) == 0)
			break;
	}
	*data = value;
}

void mqtt_packet_pop_text(mqtt_packet_t *self, mqtt_text_t *data)
{
	mqtt_packet_pop_word(self, &data->length);
	if (self->head + data->length <= self->size)
		data->text = self->data + self->head;
	self->head += data->length;
}

void mqtt_packet_pop_message(mqtt_packet_t *self, mqtt_text_t *data)
{
	data->length = (uint16_t)(self->size - self->head);
	data->text = self->data + self->head;
	self->head = self->size;
}

void mqtt_basic_exx(mqtt_packet_t *packet, mqtt_basic_t *data, mqtt_exchanger_t *exx)
{
	exx->byte_exx(packet, &data->byte1);
	exx->byte_exx(packet, &data->byte2);
}

void mqtt_publish_variable_exx(mqtt_packet_t *packet, int qos, mqtt_publish_variable_t *data, mqtt_exchanger_t *exx)
{
	exx->text_exx(packet, &data->topic);
	if (qos)
		exx->word_exx(packet, &data->packetid);
}

void mqtt_connect_variable_exx(mqtt_packet_t *packet, mqtt_connect_variable_t *data, mqtt_exchanger_t *exx)
{
	exx->text_exx(packet, &data->marker);
	exx->byte_exx(packet, &data->level);
	exx->byte_exx(packet, &data->flags);
	exx->word_exx(packet, &data->keepalive);
}

void mqtt_connect_payload_exx(mqtt_packet_t *packet, 
								mqtt_connect_variable_t *ctrl,
								mqtt_connect_payload_t *data,
								mqtt_exchanger_t *exx)
{
	exx->text_exx(packet, &data->client_id);
	if (ctrl->flags & 0x40)
	{
		exx->text_exx(packet, &data->will_topic);
		exx->text_exx(packet, &data->will_message);
	}
	if (ctrl->flags & 0x80)
	{
		exx->text_exx(packet, &data->username);
		exx->text_exx(packet, &data->password);
	}
}


void mqtt_subscribe_payload_exx(mqtt_packet_t *packet, int cmd, mqtt_subscribe_payload_t *data, mqtt_exchanger_t *exx)
{
	int count;
	for (count = 0; count < data->count ; count++)
	{
		if (exx == &reader && packet->head == packet->size)
		{
			data->count = count;
		    break ;
		}

		switch (cmd)
		{
		case SUBSCRIBE:
			exx->text_exx(packet, &data->items[count].topic);
			exx->byte_exx(packet, &data->items[count].qos);
			break;
		case UNSUBSCRIBE:
			exx->text_exx(packet, &data->items[count].topic);
			break;
		case SUBACK:
			exx->byte_exx(packet, &data->items[count].ack);
			break;
		}
	}
}

void mqtt_message_exx(mqtt_packet_t *packet, mqtt_message_t *data, mqtt_exchanger_t *exx)
{
	int cmd = data->header.ctrl >> 4;
	switch (cmd)
	{
	case CONNECT:
		mqtt_connect_variable_exx(packet, &data->variable.connect, exx);
		mqtt_connect_payload_exx(packet, &data->variable.connect, &data->payload.connect, exx);
		break;
	case CONNACK:
		mqtt_basic_exx(packet, &data->variable.connack, exx);
		break;
	case DISCONNECT:
		break;
	case PUBLISH:
		mqtt_publish_variable_exx(packet, (data->header.ctrl >> 1) & 0x03, &data->variable.publish, exx);
		exx->message_exx(packet, &data->payload.publish);
		break;
	case SUBSCRIBE:
	case UNSUBSCRIBE:
	case SUBACK:
		exx->word_exx(packet, &data->variable.msgid);
		mqtt_subscribe_payload_exx(packet, cmd, &data->payload.subscribe, exx);
		break;
	case UNSUBACK:
	case PUBACK:
	case PUBREC:
	case PUBREL:
	case PUBCOMP:
		exx->word_exx(packet, &data->variable.msgid);
		break;
	}
}

int mqtt_message_peek(mqtt_message_t *data, mqtt_packet_t *packet)
{
	int result;
	mqtt_packet_pop_byte(packet, &data->header.ctrl);
	mqtt_packet_pop_length(packet, &data->header.length);
	result = packet->head + data->header.length;
	packet->head = 0;
	return result;
}

void mqtt_message_read(mqtt_message_t *data, mqtt_packet_t *packet)
{
	mqtt_packet_pop_byte(packet, &data->header.ctrl);
	mqtt_packet_pop_length(packet, &data->header.length);
	if (packet->size > packet->head + data->header.length)
		packet->size = packet->head + data->header.length; // to exit subscribe/unsubs loops and publish msg
	mqtt_message_exx(packet, data, &reader);
}

void mqtt_message_write(mqtt_message_t *data, mqtt_packet_t *packet)
{
	mqtt_packet_t length;

	mqtt_packet_init(&length, NULL, 0);
	/* The following call writes nothing, just computes packet length */
	mqtt_message_exx(&length, data, &writer);
	data->header.length = length.head;
	mqtt_packet_push_byte(packet, &data->header.ctrl);
	mqtt_packet_push_length(packet, &data->header.length);
	mqtt_message_exx(packet, data, &writer);
}

void mqtt_text_init(mqtt_text_t *self, const char *text)
{
	self->text = (uint8_t *)text;
	self->length = (uint16_t)strlen(text);
}

void mqtt_connect_build(mqtt_message_t *self, const char *client_id, int clean, uint16_t keepalive)
{
	mqtt_connect_variable_t *variable = &self->variable.connect;
	mqtt_connect_payload_t *connect = &self->payload.connect;

	memset(self, 0, sizeof(mqtt_message_t));
	self->header.ctrl = (CONNECT << 4);
	variable->flags = (clean ? 2 : 0) ;
	variable->keepalive = keepalive;
	variable->level = 4;
	mqtt_text_init(&variable->marker, "MQTT");
	mqtt_text_init(&connect->client_id, client_id);
}

void mqtt_disconnect_build(mqtt_message_t *self)
{
	memset(self, 0, sizeof(mqtt_message_t));
	self->header.ctrl = (DISCONNECT << 4);
	self->header.length = 0;
}

void mqtt_connect_credentials(mqtt_message_t *self, const char *username, const char *password, int passlen)
{
	mqtt_connect_payload_t *connect = &self->payload.connect;
	self->variable.connect.flags |= 0xC0;
	mqtt_text_init(&connect->username, username);
	connect->password.text = (uint8_t *)password;
	connect->password.length = (uint16_t)passlen;
}

void mqtt_connect_will(mqtt_message_t *self, int will_retain, int will_qos,
	                   const char *will_topic, int topic_len,
					   const char *will_message, int msg_len)
{
	mqtt_connect_payload_t *connect = &self->payload.connect;
	self->variable.connect.flags |= (0x40 | (will_retain ? 0x20 : 0) | (will_qos << 3));
	connect->will_topic.length   = (uint16_t )topic_len;
	connect->will_topic.text     = (uint8_t *)will_topic;
	connect->will_message.length = (uint16_t )msg_len;
	connect->will_message.text   = (uint8_t *)will_message;
}

void mqtt_publish_build(mqtt_message_t *self, 
	                    int qos, int retain, int *msgid, 
						const char *topic, const char *msg, int msglen)
{
	memset(self, 0, sizeof(mqtt_message_t));
	self->header.ctrl = (uint8_t)((PUBLISH << 4) | (qos << 1) | (retain ? 1 : 0));
	if (msgid)
	{
		self->variable.publish.packetid = (uint16_t)*msgid;
		(*msgid)++;
	}
	mqtt_text_init(&self->variable.publish.topic, topic);
	self->payload.publish.text   = (uint8_t *)msg;
	self->payload.publish.length = (uint16_t )msglen;
}

void mqtt_pub_xxx_build(mqtt_message_t *self, int ack, int msgid)
{
	self->header.ctrl = (uint8_t)((ack << 4));
	self->variable.msgid = (uint16_t)msgid;
}

typedef const char *charptr_t;
void mqtt_subscribe_build_ex(mqtt_message_t *self, int cmd, uint16_t *msgid, va_list arg)
{
	mqtt_subscribe_payload_t *subs = &self->payload.subscribe;
	self->header.ctrl = (uint8_t)((cmd << 4) | 2);
	self->variable.msgid = (uint16_t)*msgid;
	(*msgid)++;
	for (subs->count = 0; subs->count < MAX_SUBSCRIBE_ITEMS ; subs->count++)
	{
		charptr_t topic = va_arg(arg, charptr_t);
		if (topic == NULL)
			break;
		if (cmd == SUBSCRIBE)
		subs->items[subs->count].qos = (uint8_t)va_arg(arg, int);
		mqtt_text_init(&subs->items[subs->count].topic, topic);
	}
}

void mqtt_va_subscribe_build(mqtt_message_t *self, uint16_t *msgid, ...)
{
	va_list arg;

	va_start(arg, msgid);
	mqtt_subscribe_build_ex(self, SUBSCRIBE, msgid, arg);
}

void mqtt_va_unsubscribe_build(mqtt_message_t *self, uint16_t *msgid, ...)
{
	va_list arg;

	va_start(arg, msgid);
	mqtt_subscribe_build_ex(self, UNSUBSCRIBE, msgid, arg);
}

void mqtt_subscribe_build(mqtt_message_t *self, uint16_t *msgid, const char *topic, int qos)
{
	mqtt_va_subscribe_build(self, msgid, topic, qos, NULL);
}

void mqtt_unsubscribe_build(mqtt_message_t *self, uint16_t *msgid, const char *topic)
{
	mqtt_va_unsubscribe_build(self, msgid, topic, NULL);
}
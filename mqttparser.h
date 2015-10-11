#ifndef mqttparser_H
#define mqttparser_H

#include <inttypes.h>

/* MTT commands */
enum mqtt_message_e
{
	CONNECT = 1, // Required
	CONNACK = 2, // None
	PUBLISH = 3, // Optional
	PUBACK = 4, // None
	PUBREC = 5, // None
	PUBREL = 6, // None
	PUBCOMP = 7, // None
	SUBSCRIBE = 8, // Required
	SUBACK = 9, // Required
	UNSUBSCRIBE = 10, // Required
	UNSUBACK = 11, // None
	PINGREQ = 12, // None mqtt - v3.1.1 - os 29 October 2014
	PINGRESP = 13, // None
	DISCONNECT = 14, // None
};

/* Data structure used for read/write */
typedef struct mqtt_packet_s
{
	uint8_t *data;
	int      head;
	int      size;
} mqtt_packet_t;

/* MQTT fixed header */
typedef struct mqtt_fixed_header_s
{
	uint8_t ctrl;
	int     length;
} mqtt_fixed_header_t;

typedef struct mqtt_text_s
{
	uint16_t length;
	uint8_t *text;
} mqtt_text_t;

typedef struct mqtt_basic_s
{
	uint8_t byte1;
	uint8_t byte2;
} mqtt_basic_t;

typedef struct mqtt_connect_variable_s
{
	mqtt_text_t marker;
	uint8_t     level;
	uint8_t     flags;
	uint16_t    keepalive;
} mqtt_connect_variable_t;

typedef struct mqtt_publish_variable_s
{
	mqtt_text_t topic;
	uint16_t    packetid;
} mqtt_publish_variable_t;

typedef union mqtt_variable_header_s
{
	mqtt_connect_variable_t connect;
	mqtt_publish_variable_t publish;
	mqtt_basic_t            connack;
	uint16_t                msgid;
} mqtt_variable_header_t;

typedef struct mqtt_subscribe_item_payload_s
{
	mqtt_text_t topic;
	uint8_t     qos;
	uint8_t     ack;
} mqtt_subscribe_item_payload_t;

#define MAX_SUBSCRIBE_ITEMS 4

typedef struct mqtt_subscribe_payload_s
{
	int                           count;
	mqtt_subscribe_item_payload_t items[MAX_SUBSCRIBE_ITEMS]; // how to deal with multiple subscriptions?
} mqtt_subscribe_payload_t;

typedef struct mqtt_connect_payload_s
{
	mqtt_text_t client_id;
	mqtt_text_t will_topic;
	mqtt_text_t will_message;
	mqtt_text_t username;
	mqtt_text_t password;
} mqtt_connect_payload_t;

typedef union mqtt_payload_s
{
	mqtt_connect_payload_t   connect;
	mqtt_text_t              publish;
	mqtt_subscribe_payload_t subscribe; 
} mqtt_payload_t;

typedef struct mqtt_message_s
{
	mqtt_fixed_header_t    header;
	mqtt_variable_header_t variable;
	mqtt_payload_t         payload;
} mqtt_message_t;

// initializes a packet
void mqtt_packet_init(mqtt_packet_t *, uint8_t *data, int size);

void mqtt_connect_build(mqtt_message_t *, const char *client_id, int clean, uint16_t keepalive);
void mqtt_connect_userpass(mqtt_message_t *, const char *username, const char *password, int passlen);
void mqtt_connect_will(mqtt_message_t *self, int will_retain, int will_qos,
						const char *will_topic, int topic_len,
						const char *will_message, int msg_len) ;
void mqtt_disconnect_build(mqtt_message_t *);

void mqtt_publish_build(mqtt_message_t *self, 
	                    int qos, int retain, int *msgid, 
						const char *topic, const char *msg, int msglen) ;
void mqtt_pub_xxx_build(mqtt_message_t *self, int ack, int msgid);

/* General subscription functions*/
void mqtt_va_subscribe_build(mqtt_message_t *self, uint16_t *msgid, ...);
void mqtt_va_unsubscribe_build(mqtt_message_t *self, uint16_t *msgid, ...);
/* Utility for single topic */
void mqtt_subscribe_build(mqtt_message_t *self, uint16_t *msgid, const char *topic, int qos);
void mqtt_unsubscribe_build(mqtt_message_t *self, uint16_t *msgid, const char *topic);

/* Functions to encode/decode a message to/from a packet */
void mqtt_message_read(mqtt_message_t *data, mqtt_packet_t *packet);
void mqtt_message_write(mqtt_message_t *data,mqtt_packet_t *packet);

#endif
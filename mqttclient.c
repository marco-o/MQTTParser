#include "mqttparser.h"
#include <string.h>
#ifdef WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#define close closesocket
#endif


/* Definition os  few message handlers */
typedef struct mqtt_client_s mqtt_client_t;
typedef void(*mqtt_on_connect_t)(mqtt_client_t *);
typedef void(*mqtt_on_publish_t)(mqtt_client_t *, const mqtt_text_t *topic, const mqtt_text_t *message);

struct mqtt_client_s
{
	int               socket;
	uint16_t          msgid;
	uint8_t           buffer[256];
	uint8_t           buffer_in[256];
	mqtt_message_t    connectmsg;
	mqtt_on_connect_t on_connect;
	mqtt_on_publish_t on_publish;
} ;

void mqtt_client_init(mqtt_client_t *self, const char *client_id, int clean, uint16_t keepalive)
{
	memset(self, 0, sizeof(mqtt_client_t));
	mqtt_connect_build(&self->connectmsg, client_id, clean, keepalive);
	self->msgid = 1;
}

void mqtt_client_credentials(mqtt_client_t *self, const char *username, const char *password, int passlen)
{
	mqtt_connect_credentials(&self->connectmsg, username, password, passlen);
}

void mqtt_client_callbacks(mqtt_client_t *self, mqtt_on_connect_t on_connect, mqtt_on_publish_t on_publish)
{
	self->on_connect = on_connect;
	self->on_publish = on_publish;
}

int mqtt_client_send(mqtt_client_t *self, mqtt_message_t *message)
{
	mqtt_packet_t packet;
	mqtt_packet_init(&packet, self->buffer, sizeof(self->buffer));
	mqtt_message_write(message, &packet);
	if (packet.head > packet.size)
		return packet.size;
	send(self->socket, (const char *)packet.data, packet.head, 0);
	return 0;
}

int mqtt_client_connect(mqtt_client_t *self, const char *host, const char *port)
{
	struct addrinfo hints, *servinfo, *p;
	int rv, sockfd = -1;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) 
		return -1;

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			sockfd = -1;
			continue;
		}

		break;
	}
	freeaddrinfo(servinfo); // all done with this structure
	if (sockfd > 0)
	{
		self->socket = sockfd;
		mqtt_client_send(self, &self->connectmsg);
	}
	return sockfd;
}

int mqtt_client_loop(mqtt_client_t *self)
{
	int len;
	for (len = 0; len < sizeof(self->buffer_in);)
	{
		int read = recv(self->socket, (char *)self->buffer_in + len, sizeof(self->buffer_in) - len, 0);
		if (read > 0)
		{
			mqtt_packet_t packet;
			mqtt_message_t message;
			len += read;
			mqtt_packet_init(&packet, self->buffer_in, len);
			if (mqtt_message_peek(&message, &packet) == len)
			{
				mqtt_message_read(&message, &packet);
				switch (message.header.ctrl >> 4)
				{
				case PUBLISH:
					if (self->on_publish)
						self->on_publish(self, &message.variable.publish.topic, &message.payload.publish);
					break;
				case CONNACK:
					if (self->on_connect)
						self->on_connect(self);
					break;
				}
				len = 0;
			}
		}
		else
			break; // close connection?
	} 
	return 0;
}

void mqtt_client_shutdown(mqtt_client_t *self)
{
	mqtt_message_t message;
	mqtt_disconnect_build(&message);
	mqtt_client_send(self, &message);

	close(self->socket);
}

void on_test_connect(mqtt_client_t *self)
{
	mqtt_message_t message;
	mqtt_subscribe_build(&message, &self->msgid, "abc", 1);
	mqtt_client_send(self, &message);
	printf("CONNAC received\n");
}

void on_test_publish(mqtt_client_t *self, const mqtt_text_t *topic, const mqtt_text_t *message)
{
	printf("PUBLISH  %*s -> %*s\n", topic->length, topic->text, message->length, message->text);
}

void mqtt_client_test(const char *host, const char *port)
{
	mqtt_client_t client;
	mqtt_client_init(&client, "test", 0, 300);
	mqtt_client_callbacks(&client, on_test_connect, on_test_publish);
	if (mqtt_client_connect(&client, host, port))
		mqtt_client_loop(&client);
}
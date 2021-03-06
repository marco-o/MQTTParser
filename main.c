#include "mqttparser.h"
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#ifdef WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

static const char *msg_name[] = 
{
    "",
    "CONNECT", // Required
    "CONNACK", // None
    "PUBLISH", // Optional
    "PUBACK", // None
    "PUBREC", // None
    "PUBREL", // None
    "PUBCOMP", // None
    "SUBSCRIBE", // Required
    "SUBACK", // Required
    "UNSUBSCRIBE", // Required
    "UNSUBACK", // None
    "PINGREQ",
    "PINGRESP",
    "DISCONNECT",
};

int client_connect(const char *host, const char *port)
{
    struct addrinfo hints, *servinfo, *p;
    int rv, sockfd = -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            sockfd = -1;
            perror("client: connect");
            continue;
        }

        break;
    }
    freeaddrinfo(servinfo); // all done with this structure
    return sockfd;
}

int mqtt_send_message(mqtt_message_t *self, int sock)
{
    uint8_t buffer[128];
    mqtt_packet_t packet;

    mqtt_packet_init(&packet, buffer, sizeof(buffer));
    mqtt_message_write(self, &packet);
    if (packet.head > packet.size)
        return -1;
    send(sock, (const char *)packet.data, packet.head, 0);
    return 0;
}

int client_test(const char *host, const char *port)
{
    int sockfd;
    mqtt_message_t message;
    mqtt_packet_t packet;
    uint8_t buffer[128];
    uint16_t msgid = 1;

    if ((sockfd = client_connect(host, port)) < 0)
        return -1;

    mqtt_connect_build(&message, "test", 1, 300);
    mqtt_send_message(&message, sockfd);
    /*
    // waiting connack
    mqtt_packet_init(&packet, buffer, sizeof(buffer));
    packet.size = recv(sockfd, (char *)packet.data, packet.size, 0);
    mqtt_message_read(&message, &packet);
    if (message.header.ctrl == (CONNACK << 4))
        printf("Received CONNACK: %02x %02x\n", message.variable.connack.byte1, message.variable.connack.byte2);
    */
    // publish something
    mqtt_publish_build(&message, 0, 0, NULL, "abc", "def", 3);
    mqtt_send_message(&message, sockfd);

    // subscribe
    mqtt_va_subscribe_build(&message, &msgid, "abc", 1, "xyz", 2, NULL);
    mqtt_send_message(&message, sockfd);

    int count;
    for (count = 0; count < 8; ++count)
    {
        mqtt_packet_init(&packet, buffer, sizeof(buffer));
        packet.size = recv(sockfd, (char *)packet.data, packet.size, 0);
        mqtt_message_read(&message, &packet);
        printf("Packet in: msg = %s\n", msg_name[message.header.ctrl >> 4]);
        switch (message.header.ctrl >> 4)
        {
        case PUBLISH:
            {
                char topic[20] = { 0 };
                char msgtext[40] = {0};
                strncpy(topic, (const char *)message.variable.publish.topic.text, message.variable.publish.topic.length);
                strncpy(msgtext, (const char *)message.payload.publish.text, message.payload.publish.length);
                printf("Data in: %s %s\n", topic, msgtext);
            }
            if ((message.header.ctrl >> 1) & 3)
            {
                mqtt_pub_xxx_build(&message, PUBACK, message.variable.publish.packetid);
                mqtt_send_message(&message, sockfd);
            }
            break;
        }
    }

    mqtt_disconnect_build(&message);
    mqtt_send_message(&message, sockfd);

    close(sockfd);
    return 0;
}

int main(int argc, char *argv[])
{
    int i ;
    const char *host = "localhost" ;
    const char *port = "1883" ;
#ifdef WIN32
    WSADATA wsaData;   // if this doesn't work

    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }
#endif
    for (i = 1; i < argc; i++)
        if (strncmp("--port=", argv[i], 7) == 0)
            port = argv[i] + 7 ;
        else if (strncmp("--host=", argv[i], 7) == 0)
            host = argv[i] + 7 ;
        else if (strcmp("--loop", argv[i]) == 0)
            mqtt_client_test(host, port);
        else if (strcmp("--client", argv[i]) == 0)
            return client_test(host, port);
    return 0;
}




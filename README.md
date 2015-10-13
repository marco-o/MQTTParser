# MQTTParser
A simple C library for MQTT 3.1.1 message encoding/decoding

The aim of this project is provide a simple library for building MQTT messages on very small 
devices where not even malloc/free are avaiable. This makes the design attractive for embedded devices.

Usage is very simple. For example to publish a message one can do as follows:

    // build message publishing 'def' on topic abc
    mqtt_message_t message;
    mqtt_publish_build(&message, 0, 0, NULL, "abc", "def", 3);
      
Topic and message are not copied so source data must stay there until packet is build.
This is done by providing an external buffer

    mqtt_packet_t packet;
    uint8_t buffer[128];

    mqtt_packet_init(&packet, buffer, sizeof(buffer)); /* Build a packet object */
    mqtt_message_write(self, &packet);                 /* Write the message to the packet */
    
At this point packet is ready and can be sent over e.g. a socket with the following call:

    send(sock, (const char *)packet.data, packet.head, 0);

If the provided buffer is not big enough for the message then packet.head will contain the required size for
the buffer. Anyway no data is written outside the available space.

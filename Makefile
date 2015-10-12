CC=gcc
CFLAGS=-I. -Wall
OBJ = main.o mqttparser.o mqttclient.o

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

mqttest: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)
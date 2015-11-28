CC=gcc
#CFLAGS=-c -O -g -Wall -I/usr/include/mysql
#LDFLAGS=-L/usr/lib/mysql -lmysqlclient
CFLAGS=-c -O -g -Wall
LDFLAGS=
SOURCES=ds18b20.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=ds18b20

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o ds18b20


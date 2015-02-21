TARGET	 = kovcamd
OBJECTS  = daemon.o 
CC       = gcc
CFLAGS   = -pipe -Wall -std=gnu99 -O3 -mfpu=neon 
LDFLAGS  = -lpthread

all        : kovcam $(TARGET)

kovcam     : 
	cd camera; $(MAKE)

$(TARGET)  : $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(CFLAGS) $(LDFLAGS)

$(OBJECTS) : 

clear clean :
	rm -rf *.o $(TARGET)
	cd camera; $(MAKE) clear


TARGET	 = kovcam
OBJECTS  = main.o v4l2sonien.o x264sonien.o CircularBuffer.o CircularQueue.o 
CC       = gcc
CFLAGS   = -pipe -Wall -std=gnu99 -O3 -mfpu=neon 
LDFLAGS  = -lpthread -lx264


$(TARGET) : $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(CFLAGS) $(LDFLAGS)

$(OBJECTS) : 

clear clean :
	rm -rf *.o $(TARGET)


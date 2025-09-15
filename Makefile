CC=cc
CFLAGS=-std=c99 -O2 -Wall -Wextra
TARGET=memsim

all: $(TARGET)
$(TARGET): memsim.c
	$(CC) $(CFLAGS) -o $@ $<
clean:
	rm -f $(TARGET)

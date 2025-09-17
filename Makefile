CC=cc
CFLAGS=-std=c99 -O2 -Wall -Wextra
TARGET=memsim

all: $(TARGET)
$(TARGET): memsim.c
	$(CC) $(CFLAGS) -o $@ $<




# testing targets
TRACE ?= traces/trace1
FRAMES ?= 10
DEBUG ?= debug

fifo: $(TARGET)
	./$(TARGET) $(TRACE) $(FRAMES) fifo $(DEBUG)

lru: $(TARGET)
	./$(TARGET) $(TRACE) $(FRAMES) lru $(DEBUG)

clock: $(TARGET)
	./$(TARGET) $(TRACE) $(FRAMES) clock $(DEBUG)

rand: $(TARGET)
	./$(TARGET) $(TRACE) $(FRAMES) rand $(DEBUG)


clean:
	rm -f $(TARGET)

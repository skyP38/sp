CC = gcc
USE_ASM ?= 1
CFLAGS = -Wall -Wextra -DUSE_ASM=$(USE_ASM) -O2
TARGET = lab
SRC = lab1.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) $(TARGET)_c

.PHONY: clean c_version
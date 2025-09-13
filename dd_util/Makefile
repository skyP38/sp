CC = gcc
CFLAGS = -Wall -Wextra -DUSE_ASM -O2
TARGET = lab
SRC = lab.c

# asm
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

# c
c_version: $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET)_c $<

clean:
	rm -f $(TARGET) $(TARGET)_c

.PHONY: clean c_version
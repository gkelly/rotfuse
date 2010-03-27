# Copyright 2009 Garret Kelly. All Rights Reserved.
# Author: gkelly@gkelly.org (Garret Kelly)

TARGET		= rotfuse
SRC 		= $(wildcard *.c)
OBJ 		= $(patsubst %.c,%.o,$(SRC))

CFLAGS  = -g -O3 -Wall `pkg-config --cflags fuse`
LDFLAGS = `pkg-config --libs fuse`

.PHONY: all clean

all: $(TARGET)

clean:
	@rm -f $(TARGET) $(OBJ)
	@echo "Clean."

$(TARGET): $(OBJ)
	@echo "LINK $@"
	@$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	@echo "CC   $^"
	@$(CC) -c $(CFLAGS) -o $@ $^

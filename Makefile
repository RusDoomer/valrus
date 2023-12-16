CC = gcc
CFLAGS = -Wall -Wno-char-subscripts -O2
SRC_DIR = src

main: $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) $(SRC_DIR)/main.c -o ruslyzer
	
clean:
	rm -f ruslyzer *.o

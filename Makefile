CC = gcc
CFLAGS = -Wall -Wextra -Wno-char-subscripts -Wno-implicit-fallthrough -O2
SRC_DIR = src

main: $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) $(SRC_DIR)/main.c -o ruslyzer
	
clean:
	rm -f ruslyzer *.o

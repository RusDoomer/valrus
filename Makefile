CC = gcc
CFLAGS = -Wall -Wno-char-subscripts -O2 -g -fsanitize=address
SRC_DIR = src

main: $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) $(SRC_DIR)/main.c -o valrus
	
clean:
	rm -f valrus *.o corpora/ngram/*

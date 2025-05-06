# Makefile for echoclient and echoserveri

CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread

# 디렉토리 및 파일
SRCDIR = .
TARGETS = echoclient echoserveri

all: $(TARGETS)

echoclient: $(SRCDIR)/echoclient.c $(SRCDIR)/csapp.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

echoserveri: $(SRCDIR)/echoserveri.c $(SRCDIR)/csapp.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
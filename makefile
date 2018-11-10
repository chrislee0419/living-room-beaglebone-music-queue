OUTFILE = musicPlayer
OUTDIR = $(HOME)/cmpt433/public/music_player

TESTDIR = tests

CC_C = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -g -std=c99 -D _POSIX_C_SOURCE=200809L -D_GNU_SOURCE -Werror
LFLAGS = -Llibs
IFLAGS = -I/usr/local/include
LIBS = -lpthread -lasound -lmad -lfaad -logg -lopus -lcurl

#SRCS = main.c network.c control.c audio.c downloader.c disp.c
SRCS = $(wildcard *.c) $(wildcard decoders/*.c)

all: app node script

app:
	mkdir -p $(OUTDIR)
	$(CC_C) $(CFLAGS) $(IFLAGS) $(SRCS) $(LFLAGS) $(LIBS) -o $(OUTDIR)/$(OUTFILE)

test: streamtest

streamtest:
	gcc $(CFLAGS) $(IFLAGS) stream.c $(TESTDIR)/streamtest.c -lcurl -o $(TESTDIR)/streamtest

node:
	mkdir -p $(OUTDIR)/music-player-nodejs-copy/
	cp -R nodejs/* $(OUTDIR)/music-player-nodejs-copy/

script:
	mkdir -p $(OUTDIR)/music-player-services
	cp scripts/install.sh $(OUTDIR)
	cp scripts/*.service $(OUTDIR)/music-player-services

clean:
	rm -f $(OUTDIR)/$(OUTFILE)
	rm -R $(OUTDIR)/music-player-nodejs-copy/
	rm -f $(OUTDIR)/install.sh
	rm -R $(OUTDIR)/music-player-services/
	rm $(TESTDIR)/streamtest

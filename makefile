OUTFILE = musicPlayer
OUTDIR = $(HOME)/cmpt433/public/myApps

CC_C = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -g -std=c99 -D _POSIX_C_SOURCE=200809L -Werror
LFLAGS = -L$(HOME)/cmpt433/public/asound_lib_bbg
LIBS = -lpthread -lasound

SRCS = main.c network.c control.c audio.c downloader.c
#SRCS = $(wildcard *.c)

all: app node

app:
	$(CC_C) $(CFLAGS) $(SRCS) $(LFLAGS) $(LIBS) -o $(OUTDIR)/$(OUTFILE)

node:
	mkdir -p $(OUTDIR)/music-player-nodejs-copy/
	cp -R nodejs/* $(OUTDIR)/music-player-nodejs-copy/

clean:
	rm $(OUTDIR)/$(OUTFILE)
	rm -R $(OUTDIR)/music-player-nodejs-copy/

audio:
	$(CC_C) $(CFLAGS)  $(LFLAGS) audio.c mp3towav.c -lpthread -lasound -o$(OUTDIR)/audio

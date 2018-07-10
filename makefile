OUTFILE = musicPlayer
OUTDIR = $(HOME)/cmpt433/public/myApps

CC_C = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -g -std=c99 -D _POSIX_C_SOURCE=200809L -Werror -pthread

LFLAGS = -L$(HOME)/cmpt433/public/asound_lib_BBB

SRCS = main.c network.c control.c
#SRCS = $(wildcard *.c)

all: app node

app:
	$(CC_C) $(CFLAGS) $(SRCS) -o $(OUTDIR)/$(OUTFILE)

node:
	mkdir -p $(OUTDIR)/music-player-nodejs-copy/
	cp -R nodejs/* $(OUTDIR)/music-player-nodejs-copy/

clean:
	rm $(OUTDIR)/$(OUTFILE)
	rm -R $(OUTDIR)/music-player-nodejs-copy/

audio:
	$(CC_C) $(CFLAGS)  $(LFLAGS) audio.c mp3towav.c -lpthread -lasound -o$(OUTDIR)/audio

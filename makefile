OUTFILE = musicPlayer
OUTDIR = $(HOME)/cmpt433/public/myApps

CC_C = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -g -std=c99 -D _POSIX_C_SOURCE=200809L -Werror -pthread

LFLAGS = -L$(HOME)/cmpt433/public/asound_lib_BBB

SRCS = $(wildcard *.c)


all: app

app:
	$(CC_C) $(CFLAGS) $(SRCS) -o $(OUTDIR)/$(OUTFILE)

clean:
	rm $(OUTDIR)/$(OUTFILE)

audio:
	$(CC_C) $(CFLAGS)  $(LFLAGS) -pg mp3towav.c playwav.c -lpthread -lasound -o$(OUTDIR)/mp3towav

CFLAGS = # -Os
USE_SHM = 1 # Use the shared memory extension?

all:
	$(CC) $(CFLAGS) -O3 -Wall -DUSE_SHM=$(USE_SHM) oaim.c -lX11 -lXtst -lXext -o oaim

debug:
	$(CC) $(CFLAGS) -g -Wall -Wextra -DUSE_SHM=$(USE_SHM) oaim.c -lX11 -lXtst -lXext -o oaim

install: all
	sudo cp oaim /bin

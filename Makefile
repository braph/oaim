CFLAGS = -Os
USE_SHM = 1 # Use the shared memory extension?

all:
	$(CC) $(CFLAGS) -Wall -DUSE_SHM=$(USE_SHM) oaim.c -lX11 -lXtst -lXext -o oaim

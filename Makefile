CFLAGS = -O2

all:
	$(CC) $(CFLAGS) -lX11 -lXtst oaim.c -o oaim

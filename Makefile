all:
	$(CC) oaim.c -O2 -lX11 -lXtst -o oaim

all:
	$(CC) oaim.c -O2 -lImlib2 -lX11 -lgiblib -lXtst -o oaim

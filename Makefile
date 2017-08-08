PROGNAME = sharpye
SRC = sharpye.c
CC = cc

build:
	$(CC) -std=c99 -levent $(SRC) -o $(PROGNAME)
	ls

clean:
	rm -rf $(PROGNAME) *.o *.swp
	ls




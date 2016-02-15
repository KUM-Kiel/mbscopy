mbscopy: mbscopy.c
	$(CC) -o mbscopy -Wall -O3 mbscopy.c

clean:
	rm -rf mbscopy

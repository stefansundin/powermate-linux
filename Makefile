powermate: main.c
	gcc -o powermate main.c -O2 -s

# sudo apt-get install libc6-dev-i386
powermate32: main.c
	gcc -o powermate32 main.c -O2 -s -m32

all: servidor

servidor: fonte/ep1-servidor-exemplo.c
	gcc -o servidor fonte/ep1-servidor-exemplo.c

clear:
	if [ -e servidor ]; then rm servidor; fi

default: build

build:
	gcc -o webserver rio_package.h rio_package.c webserver.h webserver.c

run: build
	./webserver
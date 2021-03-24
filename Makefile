
CFLAGS = -Wall -g 

CC     = gcc $(CFLAGS)

all : bl_server bl_client


bl_server.o : bl_server.c  blather.h

	$(CC) -c $<

server_funcs.o : server_funcs.c  blather.h

	$(CC) -c $<

util.o : util.c  blather.h

	$(CC) -c $<

bl_client.o : bl_client.c blather.h
	$(CC) -c $<

simpio.o : simpio.c blather.h
	$(CC) -c $<

bl_server : bl_server.o server_funcs.o  util.o 
	$(CC) -o bl_server  bl_server.o server_funcs.o  util.o
	@echo bl_server is ready

bl_client : bl_client.o simpio.o util.o
	$(CC) -o bl_client bl_client.o simpio.o util.o -lpthread 
	@echo bl_client is ready

clean:
	@echo Cleaning up object files
	rm -f *.o bl_server
	rm -f *.o bl_client
	rm -rf test-results
include test_Makefile


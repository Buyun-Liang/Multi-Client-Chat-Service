
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include "blather.h"

//REPEAT:
//  check all sources
//  handle a join request if one is ready
//  for each client{
//    if the client is ready handle data from it
//  }
//}

// Function run when a SIGINT is sent to the program
void handle_SIGINT(int sig_num) {
  
	dbg_printf("Try to SIGINT\n");
	fflush(stdout);
}

// Function run when a SIGTERM is sent to the program
void handle_SIGTERM(int sig_num) {
  
	dbg_printf("Try to SIGTERM\n");
	fflush(stdout);
}

int main(int argc, char *argv[]){


	// Set handling functions for programs
  
	struct sigaction my_sa = {};               // portable signal handling setup with sigaction()
	sigemptyset(&my_sa.sa_mask);               // don't block any other signals during handling
	my_sa.sa_flags = SA_RESTART;               // always restart system calls on signals possible
	my_sa.sa_handler = handle_SIGTERM;         // run function handle_SIGTERM
	sigaction(SIGTERM, &my_sa, NULL);          // register SIGTERM with given action
	my_sa.sa_handler = handle_SIGINT;          // run function handle_SIGINT
	sigaction(SIGINT,  &my_sa, NULL);          // register SIGINT with given action
	
	if(argc < 2){
   
	       	printf("usage: %s <name>\n",argv[0]);
		exit(1);
  
	}

	int perms =  O_RDWR; 
	server_t server_stack={};
	server_t *server = &server_stack;
	server_start(server, argv[1], perms); // start server 

	while(1){
		server_check_sources(server); 
		
		if (server_join_ready(server)) {
			server_handle_join(server); // new client join
		}
		
		for (int idx = 0 ; idx < server -> n_clients ; idx++ ){
			
			dbg_printf("\nbl_server: before check client ready\n");
			if (server_client_ready(server, idx)){
				dbg_printf("\nbl_server: before handle client\n");
				server_handle_client(server,idx); // client msg
			}
		}

	}

return 0;
}

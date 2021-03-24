// To facilitate operations of bl_server main program, complete the server_funcs.c file which provides service routines that mainly manipulate server_t structures. Each of these has a purpose to serve in the ultimate goal of the server.


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
#include <poll.h>

#include "blather.h"

#define PREAD  0                                                     // read and write ends of pipes
#define PWRITE 1

// Gets a pointer to the client_t struct at the given index. If the
// index is beyond n_clients, the behavior of the function is
// unspecified and may cause a program crash.

client_t *server_get_client(server_t *server, int idx){

	return   &(server->client[idx]) ;

};

// Initializes and starts the server with the given name. A join fifo
// called "server_name.fifo" should be created. Removes any existing
// file of that name prior to creation. Opens the FIFO and stores its
// file descriptor in join_fd.
//
// ADVANCED: create the log file "server_name.log" and write the
// initial empty who_t contents to its beginning. Ensure that the
// log_fd is position for appending to the end of the file. Create the
// POSIX semaphore "/server_name.sem" and initialize it to 1 to
// control access to the who_t portion of the log.
//
// LOG Messages:
// log_printf("BEGIN: server_start()\n");              // at beginning of function
// log_printf("END: server_start()\n");                // at end of function
//
void server_start(server_t *server, char *server_name, int perms){

	log_printf("BEGIN: server_start()\n");              // at beginning of function

	char server_name_fifo_stack[MAXNAME];	
	char *server_name_fifo = server_name_fifo_stack;
	
	sprintf(server_name_fifo, "%s.fifo", server_name);           // fifo named after pid (mildly unsafe naming)
	remove(server_name_fifo);                                       // remove old server FIFO, ensures starting in a good state
	mkfifo(server_name_fifo , S_IRUSR | S_IWUSR);                    // create server FIFO for client requests
	
	int join_fd_new = open(server_name_fifo, perms);               // open FIFO with perms

	if (join_fd_new == -1){
		perror("getcwd() error");
	}	

	dbg_printf("serverfd is %d\n", join_fd_new);	
	server -> join_fd = join_fd_new;
	log_printf("END: server_start()\n");                // at end of function

}

// Shut down the server. Close the join FIFO and unlink (remove) it so
// that no further clients can join. Send a BL_SHUTDOWN message to all
// clients and proceed to remove all clients in any order.
//
// ADVANCED: Close the log file. Close the log semaphore and unlink
// it.
//
// LOG Messages:
// log_printf("BEGIN: server_shutdown()\n");           // at beginning of function
// log_printf("END: server_shutdown()\n");             // at end of function

void server_shutdown(server_t *server){

	log_printf("BEGIN: server_shutdown()\n");           // at beginning of function
	close(server->join_fd);

	char server_name_fifo_stack[MAXNAME];
        char *server_name_fifo = server_name_fifo_stack;
        sprintf(server_name_fifo, "%s.fifo", server -> server_name);           // fifo named 
        remove(server_name_fifo);                                       // remove server FIFO

	mesg_t mesg_actual={};
	mesg_t *mesg=&mesg_actual;
	mesg->kind=BL_SHUTDOWN;
	server_broadcast(server, mesg);
	log_printf("END: server_shutdown()\n");             // at end of function
	exit(0);
};

// Adds a client to the server according to the parameter join which
// should have fileds such as name filed in.  The client data is
// copied into the client[] array and file descriptors are opened for
// its to-server and to-client FIFOs. Initializes the data_ready field
// for the client to 0. Returns 0 on success and non-zero if the
// server as no space for clients (n_clients == MAXCLIENTS).
//
// LOG Messages:
// log_printf("BEGIN: server_add_client()\n");         // at beginning of function
// log_printf("END: server_add_client()\n");           // at end of function

int server_add_client(server_t *server, join_t *join){

	log_printf("BEGIN: server_add_client()\n");         // at beginning of function

	if (server -> n_clients == MAXCLIENTS){
		return -1;
	}
	else{
		client_t client_new = { .data_ready = 0    };
		strcpy(client_new.name, join -> name);
		strcpy(client_new.to_client_fname, join -> to_client_fname);
		strcpy(client_new.to_server_fname, join -> to_server_fname);
		
		client_new.to_client_fd = open(join->to_client_fname, O_WRONLY); // open to_client fifo
                client_new.to_server_fd = open(join->to_server_fname, O_RDONLY); // open to server fifo
	
		dbg_printf(" First OPEN: to server fd is: %d ", client_new.to_server_fd);

		server -> client[(server -> n_clients) ] = client_new; 
		server -> n_clients += 1;

		log_printf("END: server_add_client()\n");           // at end of function
		return 0;
	}
}


// Remove the given client likely due to its having departed or
// disconnected. Close fifos associated with the client and remove
// them.  Shift the remaining clients to lower indices of the client[]
// preserving their order in the array; decreases n_clients.
//
int server_remove_client(server_t *server, int idx){

	for (int i = idx; i < (server->n_clients)-1; i++ ){
		client_t *client_old = server_get_client(server,i); 
		client_t *client_new = server_get_client(server,i+1);
		client_t client_tmp = *client_new;
		*client_old = client_tmp;
	};

	server -> n_clients -= 1;
	return 0;
};

// Send the given message to all clients connected to the server by
// writing it to the file descriptors associated with them.
//
// ADVANCED: Log the broadcast message unless it is a PING which
// should not be written to the log.

int server_broadcast(server_t *server, mesg_t *mesg){

	for (int idx = 0; idx < server -> n_clients; idx++){
		
		client_t *client_new = server_get_client(server,idx);
		write(client_new -> to_client_fd, mesg, sizeof(mesg_t));     // write msg to fd of clients
	}
	return 0;
};



// Checks all sources of data for the server to determine if any are
// ready for reading. Sets the servers join_ready flag and the
// data_ready flags of each of client if data is ready for them.
// Makes use of the poll() system call to efficiently determine
// which sources are ready.
//
// LOG Messages:
// log_printf("BEGIN: server_check_sources()\n");             // at beginning of function
// log_printf("poll()'ing to check %d input sources\n",...);  // prior to poll() call
// log_printf("poll() completed with return value %d\n",...); // after poll() call
// log_printf("poll() interrupted by a signal\n");            // if poll interrupted by a signal
// log_printf("join_ready = %d\n",...);                       // whether join queue has data
// log_printf("client %d '%s' data_ready = %d\n",...)         // whether client has data ready
// log_printf("END: server_check_sources()\n");               // at end of function

void server_check_sources(server_t *server){

	log_printf("BEGIN: server_check_sources()\n");             // at beginning of function
	log_printf("poll()'ing to check %d input sources\n", server->n_clients+1);  // prior to poll() call

	struct pollfd pfds[1 + server -> n_clients];               // array of structures for poll, 1 per fd to be monitored

	pfds[0].fd =  server -> join_fd;    // populate first entry with join_fd
	pfds[0].events = POLLIN;                                           // check for ready to read() without blocking

	for (int idx = 0; idx < server -> n_clients; idx++){
		
		client_t *client_new = server_get_client(server,  idx);
		pfds[idx+1].fd     =  client_new -> to_server_fd;
  		pfds[idx+1].events = POLLIN;                                           // check for ready to read() without blocking
	};

	int ret = poll(pfds, 1 + server -> n_clients, -1);       // block until OS notifies input is ready
	log_printf("poll() completed with return value %d\n", ret); // after poll() call

	if (ret == -1){
		 if(errno == EINTR){
		 	log_printf("poll() interrupted by a signal\n");            // if poll interrupted by a signal
		 	log_printf("END: server_check_sources()\n");               // at end of function
			server_shutdown(server);
		 }
	}

	else{
		if( pfds[0].revents & POLLIN ){                                  // join fifo had input as indicated by 'revents'
			server -> join_ready = 1;
		}
		log_printf("join_ready = %d\n", server -> join_ready);                       // whether join queue has data	
		for (int idx = 0; idx < server -> n_clients; idx++){
			
			if( pfds[idx+1].revents & POLLIN ){                      // to_server fifo had input as indicated by 'revents'       
				client_t *client_new = server_get_client(server,  idx);
				client_new -> data_ready = 1;
			}
			
			client_t *client_new = server_get_client(server,  idx);	
			log_printf("client %d '%s' data_ready = %d\n",idx, client_new -> name, client_new -> data_ready );  // whether client has data ready
	
		};
	}
	log_printf("END: server_check_sources()\n");               // at end of function

};

// Return the join_ready flag from the server which indicates whether
// a call to server_handle_join() is safe.

int server_join_ready(server_t *server){

	return server -> join_ready;
};

// Call this function only if server_join_ready() returns true. Read a
// join request and add the new client to the server. After finishing,
// set the servers join_ready flag to 0.
//
// LOG Messages:
// log_printf("BEGIN: server_handle_join()\n");               // at beginnning of function
// log_printf("join request for new client '%s'\n",...);      // reports name of new client
// log_printf("END: server_handle_join()\n");                 // at end of function

int server_handle_join(server_t *server){

	log_printf("BEGIN: server_handle_join()\n");               // at beginnning of function
	
	join_t join_actual={};
	join_t *join_request = & join_actual;

	read( server -> join_fd, join_request, sizeof(join_t));              // read a single request from the requests FIFO	

	log_printf("join request for new client '%s'\n",join_request -> name) ;      // reports name of new client
	server_add_client(server, join_request);
	server -> join_ready = 0;
	
	log_printf("END: server_handle_join()\n");                 // at end of function

	mesg_t mesg_actual={};
        mesg_t *msg_new=&mesg_actual;
	msg_new -> kind = BL_JOINED;
	strcpy(msg_new -> name, join_request -> name);

	server_broadcast(server, msg_new);

	return 0;
};

// Return the data_ready field of the given client which indicates
// whether the client has data ready to be read from it.

int server_client_ready(server_t *server, int idx){

	client_t *client_new = server_get_client(server,  idx);
	return client_new -> data_ready;
};

// Process a message from the specified client. This function should
// only be called if server_client_ready() returns true. Read a
// message from to_server_fd and analyze the message kind. Departure
// and Message types should be broadcast to all other clients.  Ping
// responses should only change the last_contact_time below. Behavior
// for other message types is not specified. Clear the client's
// data_ready flag so it has value 0.
//
// ADVANCED: Update the last_contact_time of the client to the current
// server time_sec.
//
// LOG Messages:
// log_printf("BEGIN: server_handle_client()\n");           // at beginning of function
// log_printf("client %d '%s' DEPARTED\n",                  // indicates client departed
// log_printf("client %d '%s' MESSAGE '%s'\n",              // indicates client message
// log_printf("END: server_handle_client()\n");             // at end of function

int server_handle_client(server_t *server, int idx){


	log_printf("BEGIN: server_handle_client()\n");           // at beginning of function

	mesg_t user_mesg_actual;
	mesg_t *msg_new=&user_mesg_actual;

	client_t *client_new = server_get_client(server,  idx);
	dbg_printf(" to server fd is: %d ", client_new -> to_server_fd);
	
	int nread = read( client_new -> to_server_fd, msg_new, sizeof(mesg_t));              // read a message from the to_server FIFO
	if(nread != sizeof(mesg_t)){
     	
	       	dbg_printf("SERVER: read %d bytes from to_server.fifo; empty pipe, exiting\n",nread);
		exit(0);
    	}

	dbg_printf("\nhandle client, %d bytes type %d msg received from %s :%s \n",nread, msg_new -> kind, msg_new -> name, msg_new->body);

	if (msg_new -> kind == BL_MESG){

		log_printf("client %d '%s' MESSAGE '%s'\n", idx,  msg_new -> name, msg_new->body)  ;           // indicates client message
		dbg_printf("\nhandle client, before broadcast \n");
		server_broadcast(server, msg_new);

	}
	else if(msg_new -> kind == BL_DEPARTED){

		log_printf("client %d '%s' DEPARTED\n",  idx,  msg_new -> name, msg_new->body    );            // indicates client departed
		mesg_t dp_mesg_actual={};
        	mesg_t *dp_mesg=&dp_mesg_actual;
        	dp_mesg->kind=BL_DEPARTED;
		strcpy(dp_mesg->name,msg_new -> name );

		server_remove_client(server, idx);	
		server_broadcast(server, dp_mesg);

	}
	
	client_new -> data_ready = 0;
	log_printf("END: server_handle_client()\n");             // at end of function
	return 0;
};


void server_tick(server_t *server);
// ADVANCED: Increment the time for the server

void server_ping_clients(server_t *server);
// ADVANCED: Ping all clients in the server by broadcasting a ping.

void server_remove_disconnected(server_t *server, int disconnect_secs);
// ADVANCED: Check all clients to see if they have contacted the
// server recently. Any client with a last_contact_time field equal to
// or greater than the parameter disconnect_secs should be
// removed. Broadcast that the client was disconnected to remaining
// clients.  Process clients from lowest to highest and take care of
// loop indexing as clients may be removed during the loop
// necessitating index adjustments.

void server_write_who(server_t *server);
// ADVANCED: Write the current set of clients logged into the server
// to the BEGINNING the log_fd. Ensure that the write is protected by
// locking the semaphore associated with the log file. Since it may
// take some time to complete this operation (acquire semaphore then
// write) it should likely be done in its own thread to preven the
// main server operations from stalling.  For threaded I/O, consider
// using the pwrite() function to write to a specific location in an
// open file descriptor which will not alter the position of log_fd so
// that appends continue to write to the end of the file.

void server_log_message(server_t *server, mesg_t *mesg);
// ADVANCED: Write the given message to the end of log file associated
// with the server.







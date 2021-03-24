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
#include <pthread.h>

#include "blather.h"

simpio_t simpio_actual={};
simpio_t *simpio = &simpio_actual;

client_t client_actual={};
client_t *client = &client_actual;

pthread_t user_thread;          // thread managing user input
pthread_t server_thread;

// thread to manage user input
void *user(void *arg){
	while(!simpio->end_of_input){
		simpio_reset(simpio);                                         
		iprintf(simpio, "");                                          // print prompt
		while(!simpio->line_ready && !simpio->end_of_input){          // read until line is complete
			simpio_get_char(simpio);
		}
    
		dbg_printf("\n Before a line is ready\n");
		if(simpio->line_ready){

    			dbg_printf("\nYou entered: %s\n",simpio->buf);
			mesg_t user_mesg_actual={};
			mesg_t *user_mesg=&user_mesg_actual;              
			user_mesg->kind=BL_MESG;
			strcpy(user_mesg->body, simpio->buf);
			strcpy(user_mesg->name, client->name); // msg send to server
   
			int nwrite = write(client->to_server_fd, &user_mesg_actual, sizeof(mesg_t));       //send mesg to server
			dbg_printf("\n  %d bytes type %d msg  from %s :%s \n",nwrite, user_mesg -> kind, user_mesg -> name, user_mesg->body);
		}
	}

	mesg_t user_depart_actual={};                                        //send depart mesg to server
	mesg_t *user_depart= &user_depart_actual;
	user_depart->kind=BL_DEPARTED;
	strcpy(user_depart->name, client->name);
	
	write(client->to_server_fd, user_depart, sizeof(mesg_t));
  
	pthread_cancel(server_thread); // kill the server thread
	return NULL;
}

//  thread to listen to the info from the server.
void *server(void *arg){
    mesg_t server_mesg_acutal={};
    mesg_t *server_mesg= &server_mesg_acutal;
    dbg_printf("\n In server Thread, Step 1: begin read to_client msg\n");
    do{
        read(client->to_client_fd, server_mesg, sizeof(mesg_t));           //read mesg from server and act accordingly 
        switch (server_mesg->kind)
        {
        case BL_MESG:
            iprintf(simpio, "\n[%s] : %s\n",server_mesg->name,server_mesg->body);
            break;
        
        case BL_JOINED:
            iprintf(simpio,"-- %s JOINED --\n",server_mesg->name);
            break;
        
        case BL_DEPARTED:
            iprintf(simpio,"-- %s DEPARTED --\n",server_mesg->name);
            break;

        case BL_SHUTDOWN:
            iprintf(simpio,"\n!!! server is shutting down !!!\n");
            break;
        
        // case BL_DISCONNECTED:
        //     iprintf(simpio,"--%s DISCONNECTED--",server_mesg->name);
        //     break;
        
        // case BL_PING:
        //     mesg_t *ping;
        //     ping->kind=BL_PING;
        //     write(client->to_server_fd, &ping, sizeof(mesg_t));
        //     break;
        
        default:
            break;
        }
    }while(server_mesg -> kind != BL_SHUTDOWN);
  
  pthread_cancel(user_thread);

  return NULL;
}

int main(int argc, char *argv[]){               
                                                                         
	//get server and client name

  if(argc<2)
  {
      printf("not enough arg\n");
      exit(1);
  }

  dbg_printf("initialize\n");

  sprintf(client->name, "%s", argv[2]);
  char server_name_stack[MAXNAME];	
  char *server_name_fifo = server_name_stack;
  sprintf(server_name_fifo, "%s.fifo", argv[1]);

  dbg_printf("server_name_fifo is %s \n", server_name_fifo);

  sprintf(client->to_client_fname, "%d_%s.fifo",getpid(),"to_client");              //create fifos
  mkfifo(client->to_client_fname,  S_IRUSR | S_IWUSR);
  sprintf(client->to_server_fname, "%d_%s.fifo",getpid(),"to_server");
  mkfifo(client->to_server_fname, S_IRUSR | S_IWUSR);

  client->to_client_fd=open(client->to_client_fname,O_RDWR);                        // open fifos
  client->to_server_fd=open(client->to_server_fname,O_RDWR);

  join_t join_actual={};
  join_t *join=&join_actual;
  int serverfd = open(server_name_fifo,O_WRONLY);
  
  dbg_printf("serverfd is %d\n",serverfd);
  
  strcpy(join->name, client->name);                            //write client info to server fifo
  strcpy(join->to_client_fname, client->to_client_fname);
  strcpy(join->to_server_fname, client->to_server_fname);
  dbg_printf("before write\n");
  
  write(serverfd, join, sizeof(join_t));


  dbg_printf("after write\n");
  char prompt[MAXNAME];
  snprintf(prompt, MAXNAME, "%s>> ",argv[2]); // create a prompt string
  simpio_set_prompt(simpio, prompt);         // set the prompt
  simpio_reset(simpio);                      // initialize io
  simpio_noncanonical_terminal_mode();       // set the terminal into a compatible mode

  dbg_printf("creates  user and server thread\n");

  pthread_create(&user_thread,   NULL, user,   NULL);     // start user thread to read input
  pthread_create(&server_thread, NULL, server, NULL);
  
  pthread_join(user_thread, NULL);
  pthread_join(server_thread, NULL);

  simpio_reset_terminal_mode();
  printf("\n");                 // newline just to make returning to the terminal prettier
 
  close(client->to_client_fd);
  close(client->to_server_fd);
  close(serverfd);

  dbg_printf("end of client main\n");
  return 0;
}
  

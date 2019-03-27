/* Author: Cara Scott
 * Date: November 25, 2018
 * Class: CS 360
 * Purpose: basically my own chat room initiator 
 * http://web.eecs.utk.edu/~huangj/cs360/360/labs/lab8/lab.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/types.h>
#include <utime.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include "fields.h"
#include "dllist.h"
#include "jrb.h"
#include "jval.h"
#include "socketfun.h"

/* --- Structs --- */

//struct that stores the user's data
typedef struct User {
	char *name;
	time_t join_time;
	time_t last_talk_time;
	time_t disconnect_time;
	int fd;
} User;

//struct to hold all relavant information
typedef struct Thread_struct {
	JRB tree_of_info;
	pthread_mutex_t *lock;
	int sock;
	int port;
	char *host;
	int num_users;
	int tmp_fd;
} Thread_struct;

/* --- Function prototypes --- */

//thread functions
void * console_func(void *c);
void * input_func(void *i);
void * thread_func(void *t);
void broadcast(Thread_struct *TS, char *buf_input); 
void sig_pipe_handler(int dummy); 

//Huang functions
void send_bytes(char *p, int len, int fd);
void send_string(char *s, int fd);
int receive_bytes(char *p, int len, int fd);
int receive_string(char *s, int size, int fd);
void *from_socket(void *v);

/* --- Int Main --- */

int main(int argc, char *argv[]) {

	//error check the command line arguments
	if (argc < 3 || argc > 3) {
		printf("usage: jtalk_server host port\n");
		exit(0);
	}

	char *host = argv[1];
	int port = atoi(argv[2]);
	Thread_struct TS;

	if (port < 5000) {
		printf("Must use a port >= 5000\n");
		exit(0);
	}

	signal( SIGPIPE, sig_pipe_handler );

	//store host and port in struct
	TS.port = port;
	TS.host = host;
	TS.num_users = 0;

	//Locksmith stuff
	pthread_mutex_t *lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(lock, NULL);
	TS.lock = lock;

	pthread_t console, input;
	TS.tree_of_info = make_jrb();

	//store socket in tree
	int sock = serve_socket(host, port);
	TS.sock = sock;

	//create the thread for input
	pthread_create(&input, NULL, input_func, &TS);

	console_func(&TS);

	//Clean up my dudes
	
	jrb_free_tree(TS.tree_of_info);

	return;
}//end of int main

//Runs console commands
void *console_func(void *c) {
	IS is;
	is = new_inputstruct(NULL); //NULL signifies receiving from stdin
	char *host;
	int port;
	Thread_struct *TS = (Thread_struct *) c;
	pthread_mutex_lock(TS->lock);
	host = TS->host;
	port = TS->port;
	pthread_mutex_unlock(TS->lock);

	printf("Jtalk_server_console> ");

	while (get_line(is) >= 0) {

		//prints out all of the current users
		if (strcmp(is->fields[0], "TALKERS") == 0) {
			printf("Jtalk server on %s port %d\n", host, port);

			int i;
			JRB tmp;

			for (i = 1; ; i++) {
				tmp = jrb_find_int(TS->tree_of_info, i);
				if (tmp == NULL) {
					break;
				}
				else {
					User *user = (User *) tmp->val.v;
					if (user->disconnect_time == 0) {
						printf("%-7d%-11s last talked at %s", i, user->name, ctime(&(user->last_talk_time)));
					}
				}
			}
		}

		//Prints out all of the users that have been on the server
		else if (strcmp(is->fields[0], "ALL") == 0) {
			printf("Jtalk server on %s port %d\n", host, port);

			int i;
			JRB tmp;

			for (i = 1; ; i++) {
				tmp = jrb_find_int(TS->tree_of_info, i);
				if (tmp == NULL) {
					break;
				}
				else {
					User *user = (User *) tmp->val.v;
					char *status = "LIVE";
					if (user->disconnect_time != 0) status = "DEAD";
					printf("%d      %s  %s\n", i, user->name, status);
					printf("   Joined      at %s", ctime(&(user->join_time)));
					printf("   Last talked at %s", ctime(&(user->last_talk_time)));
					if (strcmp(status, "DEAD") == 0) printf("   Quit        at %s", ctime(&(user->disconnect_time)));
				}
			}
		}
		
		//error catch
		else {
			printf("Unknown console command: ");
			int h;
			for (h = 0; h < is->NF; h++) {
				printf("%s ", is->fields[h]);
			}
			printf("\n");
		}

		//reprint out the input prompt
		printf("Jtalk_server_console> ");
	}

	jettison_inputstruct(is);
}

//Handles creating each new thread
void *input_func(void *i) {
	void *t;
	int fd, sock;
	Thread_struct *TS = (Thread_struct *) i;
	pthread_mutex_lock(TS->lock);
	sock = TS->sock; 
	pthread_mutex_unlock(TS->lock);

	for ( ; ; ) { 
		pthread_t *thread;
		thread = malloc(sizeof(pthread_t));

		fd = accept_connection(sock);

		pthread_mutex_lock(TS->lock);
		TS->tmp_fd = fd;
		TS->num_users++;
		pthread_mutex_unlock(TS->lock);

		//create the thread for each user
		pthread_create(thread, NULL, thread_func, TS);
	}
}

//takes care of each user's thread
void * thread_func(void *t) {
	Thread_struct *TS = (Thread_struct *) t;

	//fill in struct information 	
	User *user = (User *) malloc(sizeof(User));
		
	//Fill in fd and times
	user->fd = TS->tmp_fd;

	//receive string
	char buf_name[64];
	char buf_name_tmp[64];
	if (receive_string(buf_name, sizeof(buf_name), user->fd) < 0) printf("RETURNED -1\n");
	strcpy(buf_name_tmp, buf_name);
	const char *colon = ":";
	char *name = strtok(buf_name, colon);
	user->name = strdup(name);

	//sending string to say who joined
	send_string(buf_name_tmp, user->fd);
	int y;
	for (y = 1; y <= TS->num_users; y++) {
		JRB temp = jrb_find_int(TS->tree_of_info, y);
		if (temp != NULL) {
			User *user1 = (User *) temp->val.v;
			if (user1->disconnect_time == 0) send_string(buf_name_tmp, user1->fd);
		}
	}
	
	//Fill in times
	user->join_time = time(NULL);
	user->last_talk_time = time(NULL);
	user->disconnect_time = 0;

	int k;
	JRB temp;

	//Figure out which number of user this is
	for (k = 1;  ; k++) {
		temp = jrb_find_int(TS->tree_of_info, k);
		if (temp == NULL) break;
	}

	//insert into the JRB
	jrb_insert_int(TS->tree_of_info, k, new_jval_v(user));
	char buf_input[1100];

	for ( ; ; ) {
		int val = receive_string(buf_input, sizeof(buf_input), user->fd);
		//checks if user left
		if (val == -1) {
			user->disconnect_time = time(NULL);
			char *output = malloc(75);
			sprintf(output, "%s has quit\n", user->name);
			broadcast(TS, output);
			pthread_exit(NULL);
		}

		user->last_talk_time = time(NULL);

		broadcast(TS, buf_input);
	}
}

//function to broadcast message to everyone
void broadcast(Thread_struct *TS, char *buf_input) {
	int z;
	for (z = 1; z <= TS->num_users; z++) {
		JRB temp_jrb = jrb_find_int(TS->tree_of_info, z);
		if (temp_jrb != NULL) {
			User *user2 = (User *) temp_jrb->val.v;
			if (user2->disconnect_time == 0) send_string(buf_input, user2->fd);
		}
	}
}

//function to handle sig pipe
void sig_pipe_handler(int dummy) {
	signal(SIGPIPE, sig_pipe_handler );
}

//Huang's functions

void send_bytes(char *p, int len, int fd)
{
  char *ptr;
  int i;

  ptr = p;
  while(ptr < p+len) {
    i = write(fd, ptr, p-ptr+len);
    if (i < 0) {
      perror("write");
      return;
    }
    ptr += i;
  }
}

void send_string(char *s, int fd)
{
  int len;

  len = strlen(s);
  send_bytes((char *) &len, sizeof(int), fd);
  send_bytes(s, len, fd);
}

int receive_bytes(char *p, int len, int fd)
{
  char *ptr;
  int i;

  ptr = p;
  while(ptr < p+len) {
    i = read(fd, ptr, p-ptr+len);
    if (i == 0) return -1;      /* If the socket closes, exit */
    if (i < 0) {
      perror("read");
      return 0;
    }
    ptr += i;
  }
  return 0;
}

int receive_string(char *s, int size, int fd)
{
  int len;

  
  int k = receive_bytes((char *) &len, 4, fd);
  if (k < 0) return -1;
  if (len > size-1) {
    fprintf(stderr, "Receive string: string too small (%d vs %d)\n", len, size);
    return -1;
  }
  int val = receive_bytes(s, len, fd);
  s[len] = '\0';
  return val;
}

void *from_socket(void *v)
{
  int fd, *fdp;
  char s[1100];

  fdp = (int *) v;
  fd = *fdp;

  while(1) { 
    receive_string(s, 1100, fd); 
    printf("%s", s);
  }
  return NULL;
}


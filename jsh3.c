/* Author: Cara Scott
 * Date: November 25, 2018
 * Class: CS 360
 * Purpose: basically my own shell program
 * http://web.eecs.utk.edu/~huangj/cs360/360/labs/lab7/lab.html
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

#include "fields.h"
#include "dllist.h"
#include "jrb.h"
#include "jval.h"

/* --- Function prototypes --- */

void error_msg(char *input);

int NF(int NF_to_null, int current_NF); 

int parsing_for_carrots(IS is, int pipes[]);

int looking_for_pipes(IS is, int pipe_parse[]);

int fork_things(IS is, int pipes[], int and_flag, JRB waiting_tree);

int spawn_process(char *tmp_p, char **tmp_if, int pipefd1[], int pipefd2[]); 

/* --- Int Main --- */

int main(int argc, char *argv[]) {
	IS is;
	is = new_inputstruct(NULL);
	JRB waiting_tree = make_jrb();
	int input = 0;
	int fork_return, wait_return, status;
	int pipes[2] = {0, 1};

	if (argc > 1) {
		if (strcmp(argv[1], "-") == 0) {
			input = 1;
		}
	}

	if (input == 0) {
		printf("jsh3: ");
	}

	while (get_line(is) >= 0) {
		//variables and what not
		int pipe_parse[100] = {0};
		pipes[0] = 0;
		pipes[1] = 1;
		int pipefd1[2] = {-1, -1};
		int pipefd2[2] = {-1, -1};
		is->fields[is->NF] = NULL;
		int and_flag = 0;

		//check if command is exit and if there is an & at the end
		if (is->NF < 1) {
			if (input == 0) {
				printf("jsh3: ");
			}
			continue;
		}
		if (strcmp(is->fields[0], "exit") == 0) exit(0);
		if (strcmp(is->fields[is->NF-1], "&") == 0) {
			and_flag = 1;
			is->fields[is->NF-1] = NULL;
		}

		//Parse for pipes
		int are_there_pipes = looking_for_pipes(is, pipe_parse);

		if (are_there_pipes == 1) {
			int k;

			if (pipe(pipefd2) < 0) {
				perror("pipe for pipe2fd");
				exit(1);
			}

			//First command
			//Parse for carrots
			int num_to_null = parsing_for_carrots(is, pipes);
			int tmp_pipes[2];
			tmp_pipes[0] = pipes[0];
			tmp_pipes[1] = pipes[1];
			int tmp1[2] = {tmp_pipes[0], -1};

			//Spawn process
			int sp_fork = spawn_process(is->fields[0], is->fields, tmp1, pipefd2); 
			jrb_insert_int(waiting_tree, sp_fork, new_jval_v(NULL));

			if (tmp1[0] > 2) close(tmp1[0]);
			if (tmp1[1] > 2) close(tmp1[1]);

			//for loop for inner processes
			for (k = 1; k < (pipe_parse[0]); k++) {
				if (pipefd1[0] > 2) close(pipefd1[0]);
				if (pipefd1[1] > 2) close(pipefd1[1]);

				pipefd1[0] = pipefd2[0];
				pipefd1[1] = pipefd2[1];

				if (pipe(pipefd2) < 0) {
					perror("pipe for pipe2fd");
					exit(1);
				}

				//Spawn process
				sp_fork = spawn_process(is->fields[pipe_parse[k]], &(is->fields[pipe_parse[k]]), pipefd1, pipefd2); 
				jrb_insert_int(waiting_tree, sp_fork, new_jval_v(NULL));
			}

			//Transition between for loop and last command
			if (pipefd1[0] > 2) close(pipefd1[0]);
			if (pipefd1[1] > 2) close(pipefd1[1]);

			pipefd1[0] = pipefd2[0];
			pipefd1[1] = pipefd2[1];

			//last command
			int tmp2[2] = {-1, tmp_pipes[1]};

			//spawn process
			sp_fork = spawn_process(is->fields[pipe_parse[k]], &(is->fields[pipe_parse[k]]), pipefd1, tmp2); 
			jrb_insert_int(waiting_tree, sp_fork, new_jval_v(NULL));

			if (tmp2[0] > 2) close(tmp2[0]);
			if (tmp2[1] > 2) close(tmp2[1]);

			//close all of the pipes
			if (pipefd1[0] > 2) close(pipefd1[0]);
			if (pipefd1[1] > 2) close(pipefd1[1]);
			if (pipefd2[0] > 2) close(pipefd2[0]);
			if (pipefd2[1] > 2) close(pipefd2[1]);

			if (and_flag == 1) {
				//clean up zombies with a JRB
				while (!jrb_empty(waiting_tree)){
					wait_return = wait(&status);
					JRB tmp = jrb_find_int(waiting_tree, wait_return);
					//clean up memory used in the JRB
					if (tmp) {
						free(tmp->val.v);
						jrb_delete_node(tmp);
					}
				}
			}
		}
		//if there are no pipes
		else {
			//Parse for carrots
			int num_to_null = parsing_for_carrots(is, pipes);

			//Nulls out the first carrot if there is one
			if (num_to_null != -1) {
				is->fields[num_to_null] = NULL;
			}
			
			fork_things(is, pipes, and_flag, waiting_tree);
		}

		//re-print out jshell input line header
		if (input == 0) {
			printf("jsh3: ");
		}
	}//end of while

	//clean up
	jrb_free_tree(waiting_tree);
	jettison_inputstruct(is);

	exit(0);
}//end of int main

/* I made so many functions! Be proud */

//prints out error message that the action requested cannot be done 
// and exits the child process
void error_msg(char *input) {
	perror(input);
	exit(1);
}

//Returns the smallest number above -1
int NF(int NF_to_null, int current_NF) {
	int num = NF_to_null;
	if (num == -1) {
		return current_NF;
	}
	else if (current_NF < num) {
		return current_NF;
	}
	else return num;
}

//added nulls
int parsing_for_carrots(IS is, int pipes[]) {
	//check for a '<', '>', and '>>'
	int i;
	int NF_to_null = -1;
	for (i = 0; i < is->NF; i++) {
		//check for opening a file for input
		if (is->fields[i] != NULL) {
			if (strcmp(is->fields[i], "<") == 0) {
				is->fields[i] = NULL;
				NF_to_null = NF(NF_to_null, i);

				int in = open(is->fields[i+1], O_RDONLY, 0644);
				if (in < 0) {
					perror("open");
					exit(1);
				}
				pipes[0] = in;
			}
			//check for opening a file for output
			else if (strcmp(is->fields[i], ">") == 0) {
				is->fields[i] = NULL;

				NF_to_null = NF(NF_to_null, i);

				int out = open(is->fields[i+1], O_WRONLY | O_TRUNC | O_CREAT, 0644);
				if (out < 0) {
					perror("open");
					exit(1);
				}
				pipes[1] = out;
			}
			//check for appending a file for output
			else if (strcmp(is->fields[i], ">>") == 0) {
				is->fields[i] = NULL;

				NF_to_null = NF(NF_to_null, i);

				int out = open(is->fields[i+1], O_WRONLY | O_APPEND, 0644);
				if (out < 0) {
					perror("open");
					exit(1);
				}
				pipes[1] = out;
			}
		}
	}

	//returns number of the first carrot
	return NF_to_null;	
}

//Goes through the given command and finds all of the pipes
//It then replaces each pipe with NULL and records where the file is in the given array
int looking_for_pipes(IS is, int pipe_parse[]) {
	int i;
	int j = 1;
	int num_of_pipes = 0;

	//variable that shows if any pipes exist
	int mario = 0;

	for (i = 0; i < is->NF; i++) {
		if (is->fields[i] != NULL) {
			if (strcmp(is->fields[i], "|") == 0) {
				mario = 1;
				num_of_pipes++;
				pipe_parse[j] = i + 1;
				j++;
				is->fields[i] = NULL;
			}
		}
	}

	pipe_parse[0] = num_of_pipes;

	return mario;
}

int fork_things(IS is, int pipes[], int and_flag, JRB waiting_tree) {
	int fork_return, wait_return, status;

	fork_return = fork();

	//in the child
	if (fork_return == 0) {
		if (pipes[0] != 0) {
			if (dup2(pipes[0], 0) < 0) {
				perror("dup2");
				exit(1);
			}
			close(pipes[0]);
		}
		if (pipes[1] != 1) {
			if (dup2(pipes[1], 1) < 0) {
				perror("dup2");
				exit(1);
			}
			close(pipes[1]);
		}
		//prepare the variables for the exec call
		char *tmp_p = is->fields[0];
		char **tmp_if = is->fields;
		execvp(tmp_p, tmp_if);
		error_msg(is->fields[0]);
	}

	//in the non-waiting parent
	else if (and_flag == 1) {
		//close all the personally created pipes
		if (pipes[0] != 0) close(pipes[0]);
		if (pipes[1] != 1) close(pipes[1]);
		//add the zombie child to the waiting tree
		jrb_insert_int(waiting_tree, fork_return, new_jval_v(NULL));
	}

	//in the waiting parent
	else if (and_flag == 0) {
		//close all the personally created pipes
		if (pipes[0] != 0) close(pipes[0]);
		if (pipes[1] != 1) close(pipes[1]);

		//add the priviledged child to the waiting tree
		jrb_insert_int(waiting_tree, fork_return, new_jval_v(NULL));

		//clean up zombies with a JRB
		while (!jrb_empty(waiting_tree)){
			wait_return = wait(&status);
			JRB tmp = jrb_find_int(waiting_tree, wait_return);
			//clean up memory used in the JRB
			if (tmp) {
				free(tmp->val.v);
				jrb_delete_node(tmp);
				if (fork_return == wait_return) break;
			}
		}
	}
}

//spawn process for piping
int spawn_process(char *tmp_p, char **tmp_if, int pipefd1[], int pipefd2[]) {
	int fork_return, wait_return, status;

	fork_return = fork();

	//in the child
	if (fork_return == 0) {
		if (pipefd1[0] > 2) {
			if (dup2(pipefd1[0], 0) < 0) {
				perror("dup2");
				exit(1);
			}
			if (pipefd1[0] > 2) close(pipefd1[0]);
		}
		if (pipefd2[1] > 2) {
			if (dup2(pipefd2[1], 1) < 0) {
				perror("dup2");
				exit(1);
			}
			if (pipefd2[1] > 2) close(pipefd2[1]);
		}
		if (pipefd1[1] > 2) close(pipefd1[1]);
		if (pipefd2[0] > 2) close(pipefd2[0]);

		//prepare the variables for the exec call
		execvp(tmp_p, tmp_if);
		error_msg(tmp_p);
	}

	return fork_return;
}

/* Author: Cara Scott
 * Date: September 30, 2018
 * Class: CS 360
 * Purpose: basically my own makefile
 * http://web.eecs.utk.edu/~huangj/cs360/360/labs/lab3/lab.html
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "fields.h"
#include "dllist.h"
#include "jrb.h"
#include "jval.h"

typedef struct Make {
	Dllist c_files;
	Dllist h_files;
	Dllist libraries;
	Dllist flags;
	Dllist o_files;
	char *executable;

} Make;

/* --- Functions --- */

//This code actually remakes the .o files by creating the ``gcc -c'' string and calling system() on it.
void remake_cfile(Make *make, char *cfile){
	Dllist tmp;
	char *command = (char *) malloc(1000);
	strcat(command, "gcc -c ");

	//add the flags to the string
	dll_traverse(tmp, make->flags) {
		strcat(command, tmp->val.s);
		strcat(command, " ");
	}
	strcat(command, cfile);
	printf("%s\n", command);

	if (system(command) != 0) {
		fprintf(stderr, "Command failed.  Exiting\n");
		exit(1);
	}

	//clean up the string memory
	free(command);
}

//goes through each line of the file and sends the information to the correct function
//based on the first letter in the line
void read_file(IS is, Make *make) {
	int i;
	int exec_num = 0;

	while (get_line(is) >= 0) {
		if (is->NF != 0) {
			if ((*is->fields[0]) == 'C') {
				reading_lists(is, make->c_files);
			}
			else if ((*is->fields[0]) == 'H') {
				reading_lists(is, make->h_files);
			}
			else if ((*is->fields[0]) == 'F') {
				reading_lists(is, make->flags);
			}
			else if ((*is->fields[0]) == 'E') {
				exec_num++;

				//error checks that there is one and only one E file
				if (exec_num > 1) {
					fprintf(stderr, "fmakefile (%d) cannot have more than one E line\n", is->line);
					exit(1);
				}
				if (!(is->fields[1])) {
					fprintf(stderr, "fmakefile (%d) empty E line\n", is->line);
					exit(1);
				}

				make->executable = strdup(is->fields[1]);
			}
			else if ((*is->fields[0]) == 'L'){
				reading_lists(is, make->libraries);
			}
			else { //if the letter is not recognized, an error is flagged
				fprintf(stderr, "fakemake (%d): Lines must start with C, H, E, F or L\n", is->line);
				exit(1);
			}
		}
	}
	if (exec_num == 0) {
		fprintf(stderr, "No executable specified\n");
		exit(1);
	}
}

//adds all of the files in the rest of the line to the given list 
reading_lists(IS is, Dllist list){
	int i;
	Dllist dnode;

	for(i = 1; i < is->NF; i++){
		dll_append(list, new_jval_s(strdup(is->fields[i])));		
	}
}

//function to assist in cleaning up memory
void empty_list(Dllist list){
	Dllist tmp;

	dll_traverse(tmp, list) free(tmp->val.s);

	free_dllist(list);
}

//This code traverses the H dlist, and calls stat on each file. It flags an error if the file doesn't exist. Otherwise, it returns the maximum value of st_mtime to main
int process_hfiles(Make *make, Dllist files) {
	Dllist tmp;
	struct stat statbuf;
	int max_value = -1;

	dll_traverse(tmp, files) {
		if(stat(tmp->val.s, &statbuf) == 0) {
			int time = statbuf.st_mtime;
			if(time > max_value) max_value = time;
		}
		else {
			fprintf(stderr, "fmakefile: %s: No such file or directory\n", tmp->val.s);
			exit(-1);
		}
	}
	return max_value;
}

//This code traverses the C dlist, and calls stat on each file. It flags an error if the file doesn't exist. Otherwise, it looks for the .o file. If that file doesn't exist, or is less recent than the C file or the maximum st_mtime of the header files, then the file needs to be remade.
int process_cfiles(Make *make, int max_htime) {
	Dllist tmp; 
	struct stat statbuf;
	struct stat statbuf_tmp;
	int max_value = -1;
	char *dot_o_file = (char *) malloc(1000);
	int changed_file_num = 0;

	dll_traverse(tmp, make->c_files){
		if(stat(tmp->val.s, &statbuf) == 0) {
			//create the .o file name     
			dot_o_file[0] = '\0';
			strncat(dot_o_file, tmp->val.s, (strlen(tmp->val.s) - 1));
			strcat(dot_o_file, "o");
			dll_append(make->o_files, new_jval_s(strdup(dot_o_file)));	

			//try to find the .o file and get the information on it
			if(stat(dot_o_file, &statbuf_tmp) == 0) {
				//the most recent time would be larger
				if(statbuf_tmp.st_mtime < max_htime) { 					
					changed_file_num++;
					remake_cfile(make, tmp->val.s);
				}
				else if(statbuf_tmp.st_mtime < statbuf.st_mtime) {
					changed_file_num++;
					remake_cfile(make, tmp->val.s);
				}
				else {
					//then the file does not need to be updated
				}
			}
			else {
				changed_file_num++;
				remake_cfile(make, tmp->val.s);
			}
		}
		else {
			fprintf(stderr, "fmakefile: %s: No such file or directory\n", tmp->val.s);
			exit(-1);
		}
	}
	//clean-up string memory
	free(dot_o_file);

	if (stat(make->executable, &statbuf) == 0){
		//less recent the .o files
		int max_otime = process_hfiles(make, make->o_files);

		if (max_otime > statbuf.st_mtime) changed_file_num++;
	}
	else changed_file_num++;
	

	if (changed_file_num != 0) {
		char *command = (char *) malloc(1000);
		command[0] = '\0';
		strcat(command, "gcc -o ");

		strcat(command, make->executable);
		strcat(command, " ");

		//add the flags to the string
		dll_traverse(tmp, make->flags) {
			strcat(command, tmp->val.s);
			strcat(command, " ");
		}
		//add the .o files to the string
		dll_traverse(tmp, make->o_files) {
			strcat(command, tmp->val.s);
			strcat(command, " ");
		}
		if (make->libraries != NULL) { 
			dll_traverse(tmp, make->libraries) {
				strcat(command, tmp->val.s);
				strcat(command, " ");
			}
		}
		command[strlen(command) - 1] = '\0';
		printf("%s\n", command);

		if (system(command) != 0) {
			fprintf(stderr, "Command failed.  Fakemake exiting\n");
			exit(1);
		}

		//clean up the string memory
		free(command);
	}

	return changed_file_num;
}

void clean_up(Make *make){
/* --- Clean-up my dudes --- */
	
	if (make->c_files != NULL) empty_list(make->c_files);
	if (make->h_files != NULL) empty_list(make->h_files);
	if (make->flags != NULL) empty_list(make->flags);
	if (make->libraries != NULL) empty_list(make->libraries);
	if (make->o_files != NULL) empty_list(make->o_files);
	free(make->executable);
	free(make);
}


/* --- Main --- */

int main(int argc, char *argv[]){
	//Creating all the variables and what nots
	char *file;
	FILE *input_file;
	IS is;
	Make *make;
	int max_htime;

	//if no description file specified then fmakefile 
	//if specified file doesn't exist then exit with an error
	if(argc == 1){
		file = "fmakefile";
		is = new_inputstruct(file);
		if(is == NULL) {
			fprintf(stderr, argv[1]); //TODO
			printf("first one");
			exit(1);
		}
	}
	//error checking for given file
	if(argc == 2){
		file = argv[1];
		//fields example for error checking
		is = new_inputstruct(argv[1]);
		if(is == NULL) {
			fprintf(stderr, argv[1]); //TODO
			printf("second one");
			exit(1);
		}
	}

	//make memory for each needed aspect in my struct and read in the information
	make = malloc(sizeof(Make));
	make->c_files = new_dllist();
	make->h_files = new_dllist();
	make->libraries = new_dllist();
	make->flags = new_dllist();
	make->o_files = new_dllist();
	read_file(is, make);

	if(make->h_files != NULL) max_htime = process_hfiles(make, make->h_files);
	int changed_file_num = process_cfiles(make, max_htime);

	if(changed_file_num == 0) {
		printf("%s up to date\n", make->executable);
		clean_up(make);
		jettison_inputstruct(is);
		exit(1);
	}

	clean_up(make);
	jettison_inputstruct(is);

	exit(0);
}

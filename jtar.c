/* Author: Cara Scott
 * Date: October 7, 2018
 * Class: CS 360
 * Purpose: basically my own tar
 * http://web.eecs.utk.edu/~huangj/cs360/360/labs/lab4/lab.html
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

#include "fields.h"
#include "dllist.h"
#include "jrb.h"
#include "jval.h"

/* --- FUNCTIONS --- */

void process_files(char *argument, char *command, JRB rp_jrb, JRB inodes_jrb) {
	struct stat buffer;
	int status;
	Dllist tmp;
	JRB node;
	bool var;

	status = lstat(argument, &buffer);

	if (status == -1) {
		fprintf(stderr, "This didn't open (inside process_files func): %s\n", argument);
		exit(1);
	}

	// check if this is a regular file
	if (S_ISREG(buffer.st_mode)) {
		
		// check the real path JRB tree
		char actualpath[PATH_MAX+1];
		char *rp = realpath(argument, actualpath);
		node = jrb_find_str(rp_jrb, rp);

		if (node == NULL) {
			//if it does not exist in inode tree
			jrb_insert_str(rp_jrb, strdup(rp), new_jval_v(NULL));
			//process as normal
			// check the inode JRB tree
			struct stat statbuf;
			if (stat(argument, &statbuf) == 0) {
				int inode = statbuf.st_ino;
				node = jrb_find_int(inodes_jrb, inode);
				if (node == NULL) {
					// it does not exist
					jrb_insert_int(inodes_jrb, inode, new_jval_s(strdup(argument)));
					//print the information out to stdout
					//format: F<number of hard links>;name;stuct;contents
					int k;
					printf("F");
					int a = 1;
					int *b = &a;
					fwrite(b, sizeof(int), 1, stdout);
					printf("\n");
					fwrite(argument, (strlen(argument) + 1), 1, stdout);
					printf("\n");
					k = fwrite(&statbuf, sizeof(struct stat), 1, stdout);
					if (k != 1) {
						fprintf(stderr, "Moving the struct info didn't work; in func, file, inode\n");
						exit(1);
					}
					printf("\n");
					//printing the contents
					FILE *file_pointer;
					file_pointer = fopen(argument, "r");
					char buffer[statbuf.st_size];
					fread(buffer, statbuf.st_size, 1, file_pointer);
					fwrite(&buffer, statbuf.st_size, 1, stdout);
					fclose(file_pointer);
				}
			
				else {//this is a hard link
					//print the information out to stdout
					//format: F<number of hard links>;name;name1;stuct;
					int k;
					printf("F");
					int a = 2;
					int *b = &a;
					fwrite(b, sizeof(int), 1, stdout);
					printf("\n");
					fwrite(node->val.s, (strlen(node->val.s) + 1), 1, stdout);
					printf("\n");
					fwrite(argument, (strlen(argument) + 1), 1, stdout);
					printf("\n");
					k = fwrite(&statbuf, sizeof(struct stat), 1, stdout);
					if (k != 1) {
						fprintf(stderr, "Moving the struct info didn't work; in func, file, inode\n");
						exit(1);
					}
					printf("\n");
				}
			}
		}
		else {
			//ignore because it's a duplicate 
		}
	}

	// checks if this is a directory
	else if (S_ISDIR(buffer.st_mode)) {
		//Goes through the contents of the directory
		DIR *directory;
		struct dirent *dir;
		directory = opendir(argument);
		Dllist files = new_dllist();
		if (directory) {
			for (dir = readdir(directory); dir != NULL; dir = readdir(directory)) {
				if (strcmp(dir->d_name, ".") == 0) continue;
				else if (strcmp(dir->d_name, "..") == 0) continue;
				else {
					//creates the path to the file and adds it to a Dllist
					char *path = malloc(PATH_MAX + 1);
					sprintf(path, "%s/%s", argument, dir->d_name);
					dll_append(files, new_jval_s(path));
				}
			}
			closedir(directory);
		}

		//print out the directory name and it's stat struct
		struct stat statbuf;
		stat(argument, &statbuf);
		printf("D");
		int a = strlen(argument);
		int *b = &a;
		fwrite(b, sizeof(int), 1, stdout);
		printf("\n");

		fwrite(argument, (strlen(argument) + 1), 1, stdout);
		printf("\n");
		int k = fwrite(&statbuf, sizeof(struct stat), 1, stdout);
		if (k != 1) {
			fprintf(stderr, "Moving the struct info didn't work; in func, file, inode\n");
			exit(1);
		}
		printf("\n");

		// Recursively call the contents of the Dllist
		dll_traverse(tmp, files) {
			process_files(jval_s(tmp->val), command, rp_jrb, inodes_jrb);
			//clean up the strdup
			free(jval_s(tmp->val));
		}
		free_dllist(files);
	}

	else {
		fprintf(stderr, "jtar: %s: No such file or directory\n", argument);			
		exit(1);
	}
}

int newline_error(char *check) {
	if (strcmp(check, "\n") == 0) {
		return 0;
	}
	else {
		fprintf(stderr, "Jtar file in wrong format\n");
		exit(1);
	}
}

void endl_error() {
	char sc[2];
	fread(sc, 1, 1, stdin);
	sc[1] = '\0';
	newline_error(sc);
}

//format: D<size of name>;name;stuct;contents
//format: F<number of hard links>;name1;name2;struct
int x_option(Dllist directories, Dllist dir_struct){
	char buffer[2];
	fread(buffer, 1, 1, stdin);
	buffer[1] = '\0';

	//if the line is a directory
	if (strcmp(buffer, "D") == 0){
		//read in the size of the directory name
		char filename_size[5];
		fread(filename_size, sizeof(int), 1, stdin);
		filename_size[4] = '\0';

		//error check the newline after D<size of filename>
		endl_error();

		//read in filename
		char filename[(*filename_size) + 1];
		fread(filename, *filename_size + 1, 1, stdin);

		//error check the newline after filename
		endl_error();

		struct stat statbuf;
		fread(&statbuf, sizeof(struct stat), 1, stdin);

		//error check the newline after struct info
		endl_error();

		//now that everything is read in, we need to make the directory
		mkdir(filename, 0777);

		struct stat *p = malloc(sizeof(struct stat));
		memcpy(p, &statbuf, sizeof(struct stat));

		dll_append(directories, new_jval_s(strdup(filename)));
		dll_append(dir_struct, new_jval_v(p));

		return 1;
	}

	//if the line is a file
	else if (strcmp(buffer, "F") == 0){

		//read in the number of hard links
		char num_hard_links[5];
		fread(num_hard_links, sizeof(int), 1, stdin);
		num_hard_links[4] = '\0';

		//error check the semicolon after F<size of filename>
		endl_error();

		if (*num_hard_links == 1) {
			//read in filename char by char
			char char_buf[2];
			char_buf[1] = '\0';
			char filename[PATH_MAX + 1];
			fread(char_buf, 1, 1, stdin);
			int c = 0;
			while (strcmp(char_buf, "\n") != 0) {
				filename[c] = char_buf[0];
				c++;
				fread(char_buf, 1, 1, stdin);
			}

			struct stat statbuf;
			fread(&statbuf, sizeof(struct stat), 1, stdin);

			//error check the semicolon after struct info
			endl_error();

			//read in the contents of the file
			FILE *file_pointer;
			file_pointer = fopen(filename, "w");
			if (file_pointer == NULL) {
				printf("ERROR NUM: %d\n", errno);
				perror("Error");
				exit(1);
			}
			char buffer[statbuf.st_size];
			fread(buffer, statbuf.st_size, 1, stdin);
			fwrite(buffer, statbuf.st_size, 1, file_pointer);
			fclose(file_pointer);

			chmod(filename,statbuf.st_mode);
			struct utimbuf time;
			time.actime = statbuf.st_atime;
			time.modtime = statbuf.st_mtime;
			utime(filename, &time);

			return 1;
		}
		else if (*num_hard_links == 2) {
			//read in first filename char by char
			char char_buf[2];
			char_buf[1] = '\0';
			char filename[PATH_MAX + 1];
			fread(char_buf, 1, 1, stdin);
			int c = 0;
			while (strcmp(char_buf, "\n") != 0) {
				filename[c] = char_buf[0];
				c++;
				fread(char_buf, 1, 1, stdin);
			}

			//read in second filename char by char
			char char_buf2[2];
			char_buf2[1] = '\0';
			char filename2[PATH_MAX + 1];
			fread(char_buf2, 1, 1, stdin);
			int ch = 0;
			while (strcmp(char_buf2, "\n") != 0) {
				filename2[ch] = char_buf2[0];
				ch++;
				fread(char_buf2, 1, 1, stdin);
			}

			struct stat statbuf;
			fread(&statbuf, sizeof(struct stat), 1, stdin);

			//error check the semicolon after struct info
			endl_error();

			link(filename, filename2);

			return 1;
		}
	}

	//check if it is the end of the file
	else if (strcmp(buffer, "E") == 0) return 2;
}



/* --- MAIN --- */



int main(int argc, char *argv[]){
	//Creating all the variables and what nots
	IS is;
	char *command;
	JRB real_paths = make_jrb(); // deals with duplicates (same file in the same place)
	JRB inodes = make_jrb(); // deals with hard links (store file name and inodes but not conents)
	Dllist directories;
	directories = new_dllist();
	Dllist dir_struct;
	dir_struct = new_dllist();

	//parse command line arguments and flag errors
	if (strcmp(argv[1], "c") == 0) {
		command = "c";
		int i;

		// sends each command line argument to the process files command
		for (i = 2; i < argc; i++) {
			process_files(argv[i], command, real_paths, inodes);
		}
		printf("E");
	}

	else if (strcmp(argv[1], "x") == 0) {
		command = "x";
		while (x_option(directories, dir_struct) != 2);
		Dllist dnode_struct = dll_first(dir_struct);
		Dllist dnode_dir;
		dll_traverse(dnode_dir, directories) {
			chmod(dnode_dir->val.s, ((struct stat *)dnode_struct->val.v)->st_mode);
			struct utimbuf time;
			time.actime = ((struct stat *)dnode_struct->val.v)->st_atime;
			time.modtime = ((struct stat *)dnode_struct->val.v)->st_mtime;
			
			int check = utime(dnode_dir->val.s, &time);
			if (check != 0) {
				printf("Errno: %d\n", errno);
				printf("AHHHHHHH IT DIDN'T WORK!\n");
			}
			dnode_struct = dnode_struct->flink; 
		}
	}

	else if (strcmp(argv[1], "cv") == 0) {
		command = "cv";
		int i;

		// sends each command line argument to the process files command
		for (i = 2; i < argc; i++) {
			process_files(argv[i], command, real_paths, inodes);
		}
		printf("E");
	}

	else if (strcmp(argv[1], "xv") == 0) {
		command = "xv";
		//x_option();
	}

	//if the input command is incorrect
	else {
		fprintf(stderr, "usage: jtar [cxv] [files ...]\n");
		exit(1);
	}


/* --- Clean-up --- */
	jrb_free_tree(real_paths);
	jrb_free_tree(inodes);
	free_dllist(directories);
	free_dllist(dir_struct);

	//exit(0);
	return 0;
}

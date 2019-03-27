// Cara Scott
// Lab 1: Red-Black Trees
// 9/8/18
// Lab that creates a family tree using a Red-Black Tree.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fields.h"
#include "jval.h"
#include "dllist.h"
#include "jrb.h"

typedef struct Person {	
	char *Name;
	char *Sex;
	struct Person *Father;
	struct Person *Mother;
	int Visited;
	int Printed;
	Dllist Children;
} Person; // name of the struct

int is_descendant(Person *per, JRB people, Dllist dnode) {
	if (per->Visited == 1) return 0; //this means we've already visited him or her and found no issues
	if (per->Visited == 2) {
		 //there's a CYCLE!
		 fprintf(stderr, "Bad input -- cycle in specification\n");
		 exit(-1);
	}
	per->Visited = 2;
	dll_traverse(dnode, per->Children) {
		Person *child_per = (Person *) dnode->val.v;
		if (is_descendant(child_per, people, dnode)) return 1;
	}
	per->Visited = 1;
	return 0;
}



/* Int Main */




int main() {

	/* Definitions and Such */

	JRB people, node; 	
	Dllist dnode, dnode_child, dll_tmp, toprint; // to help traverse a dllist
	Person *per, *tmp_per, *dll_per, *child_per; //pointer to the Person struct
	IS is; //from fields.h
	int i, n;
	people = make_jrb(); //returns a pointer to the root node of a tree
	char *name;
	char readname[200];
	int line_num = 0;

	is = new_inputstruct(NULL); 

	/* Filling in the structs */

	while (get_line(is) >= 0) {

		line_num++;

		//error check the input that there is something there
		if (strlen(is->text1) == 1) {
			continue;
		}

		if ((strcmp(is->fields[0], "PERSON")) == 0) {

			//Constructing the full name of the person
			strcpy(readname, is->fields[1]);
			for (i = 2; i < is->NF; i++) { 
				strcat(readname, " ");
				strcat(readname, is->fields[i]);
			}

			//Checks to see if the person is already present in the tree
			node = jrb_find_str(people, readname);

			if (node == NULL) { //aka if the person does not exist
				per = malloc(sizeof(Person));
				per->Name = strdup(readname);
				per->Father = NULL;
				per->Mother = NULL;
				per->Sex = strdup("Unknown");
				per->Visited = 0;
				per->Printed = 0;
				per->Children = new_dllist();
				jrb_insert_str(people, per->Name, new_jval_v(per));
			}
			else { //if the person already exists
				per = (Person *) node->val.v; //val.v is a void pointer
			}
		}

/* Parentage */

		else if ((strcmp(is->fields[0], "FATHER")) == 0) {

			//Constructing the father's name
			strcpy(readname, is->fields[1]);
			for (i = 2; i < is->NF; i++) {//took out is->NF-3 for all 
				strcat(readname, " ");
				strcat(readname, is->fields[i]);
			}
			//printf("FATHER: %s\n", readname);

			//See if the father exists
			node = jrb_find_str(people, readname);

			if (node == NULL) { //aka if the father does not exist
				tmp_per = malloc(sizeof(Person));
				tmp_per->Name = strdup(readname);
				tmp_per->Father = NULL;
				tmp_per->Mother = NULL;
				tmp_per->Sex = strdup("M");//strdup("Unknown");
				tmp_per->Visited = 0;
				tmp_per->Printed = 0;
				tmp_per->Children = new_dllist();
				dll_append(tmp_per->Children, new_jval_v(per));
				jrb_insert_str(people, tmp_per->Name, new_jval_v(tmp_per));
			}
			else { //if the father already exists, reset the pointer
				tmp_per = (Person *) node->val.v; //val.v is a void pointer
				dll_append(tmp_per->Children, new_jval_v(per));
				tmp_per->Sex = strdup("M");
			}	
			//add who the father is
			per->Father = tmp_per;
		}
		else if ((strcmp(is->fields[0], "MOTHER")) == 0) {

			//char readname[200];
			//Constructing the mother's name
			strcpy(readname, is->fields[1]);
			for (i = 2; i < is->NF; i++) {//took out is->NF-3 for all 
				strcat(readname, " ");
				strcat(readname, is->fields[i]);
			}
			//printf("MOTHER: %s\n", readname);

			//See if the mother exists
			node = jrb_find_str(people, readname);

			if (node == NULL) { //aka if the mother does not exist
				tmp_per = malloc(sizeof(Person));
				tmp_per->Name = strdup(readname);
				tmp_per->Father = NULL;
				tmp_per->Mother = NULL;
				tmp_per->Sex = strdup("F"); //strdup("Unknown");
				tmp_per->Visited = 0;
				tmp_per->Printed = 0;
				tmp_per->Children = new_dllist();
				dll_append(tmp_per->Children, new_jval_v(per));
				jrb_insert_str(people, tmp_per->Name, new_jval_v(tmp_per));
			}
			else { //if the mother already exists, reset the pointer
				tmp_per = (Person *) node->val.v; //val.v is a void pointer
				dll_append(tmp_per->Children, new_jval_v(per));
				tmp_per->Sex = strdup("F");
			}	
			//add who the mother is
			per->Mother = tmp_per;
		}

/* Parental of */

		else if ((strcmp(is->fields[0], "FATHER_OF")) == 0) {

			//Constructing the child's name
			strcpy(readname, is->fields[1]);
			for (i = 2; i < is->NF; i++) {
				strcat(readname, " ");
				strcat(readname, is->fields[i]);
			}

			//See if the child exists
			node = jrb_find_str(people, readname);

			if (node == NULL) { //aka if the child does not exist
				tmp_per = malloc(sizeof(Person));
				tmp_per->Name = strdup(readname);
				tmp_per->Father = per;
				tmp_per->Mother = NULL;
				tmp_per->Sex = strdup("Unknown");
				tmp_per->Visited = 0;
				tmp_per->Printed = 0;
				tmp_per->Children = new_dllist();
				jrb_insert_str(people, tmp_per->Name, new_jval_v(tmp_per));
			}
			else { //if the child already exists, reset the pointer
				tmp_per = (Person *) node->val.v; //val.v is a void pointer
				tmp_per->Father = per;
			}
			
			int is_child_in = -1;
			//check to see if the child is already in the parent's list
			dll_traverse(dnode, per->Children){
				is_child_in = strcmp(((Person *)dnode->val.v)->Name, readname);
			}

			//Add the child to the Children list if they aren't there
			if (is_child_in != 0) {
				dll_append(per->Children, new_jval_v(tmp_per));
			}
			per->Sex = strdup("M");//change the father's sex
		}

		else if ((strcmp(is->fields[0], "MOTHER_OF")) == 0) {

			//Constructing the child's name
			strcpy(readname, is->fields[1]);
			for (i = 2; i < is->NF; i++) {//took out is->NF-3 for all 
				strcat(readname, " ");
				strcat(readname, is->fields[i]);
			}

			//See if the child exists
			node = jrb_find_str(people, readname);

			if (node == NULL) { //aka if the child does not exist
				tmp_per = malloc(sizeof(Person));
				tmp_per->Name = strdup(readname);
				tmp_per->Mother = per;//switched per->Name
				tmp_per->Father = NULL;
				tmp_per->Sex = strdup("Unknown");
				tmp_per->Visited = 0;
				tmp_per->Printed = 0;
				tmp_per->Children = new_dllist();
				jrb_insert_str(people, tmp_per->Name, new_jval_v(tmp_per));
			}
			else { //if the child already exists
				tmp_per = (Person *) node->val.v; //val.v is a void pointer
				tmp_per->Mother = per;
			}

			//Add the child to the Children list
			dll_append(per->Children, new_jval_v(tmp_per));
			per->Sex = strdup("F");//change the mother's sex
		}

		else if ((strcmp(is->fields[0], "SEX")) == 0) {
			//if ((!((strcmp(per->Sex, "Unknown")) == 0)) || (!(strcmp(per->Sex, is->fields[0]) == 0))) { 
			//	fprintf(stderr, "Bad input - sex mismatch on line %d\n", line_num);
			//	exit(-1);
			//}
			per->Sex = strdup(is->fields[1]);
		}
		// error check that the first word is a valid input (ex. PERSON)
		else {
			fprintf(stderr, "3: Unknown key: %s\n", is->fields[0]);
			exit(-1);
		}
	}

/* Check if there is a cycle */

	jrb_traverse(node, people) {
		per = ((Person *) node->val.v);
		if(is_descendant(per, people, dnode)) {
			fprintf(stderr, "Bad input -- cycle in specification\n");
			exit(-1);
		}
	}


/* Printing out in the super special way */


	toprint = new_dllist();

	//add all people who don't have parents to the queue
	jrb_traverse(node, people) {
		per = (Person *) node->val.v;
		if (per->Father == NULL && per->Mother == NULL) {
			dll_append(toprint, new_jval_v(per));
		}
	}

	int father_print, mother_print;
	while(!(dll_empty(toprint))){

		dnode = dll_first(toprint);
		dll_delete_node(dll_first(toprint));

		dll_per = (Person *) dnode->val.v;
		if(dll_per->Printed == 0){

			// to avoid asking for non existant data, set the parent printed info before hand
			if(dll_per->Father == NULL) father_print = 0;
			else father_print = dll_per->Father->Printed;
			if(dll_per->Mother == NULL) mother_print = 0;
			else mother_print = dll_per->Mother->Printed;

			if((dll_per->Father == NULL && dll_per->Mother == NULL) || (father_print == 1 && dll_per->Mother == NULL) || (dll_per->Father == NULL && mother_print == 1) || (father_print == 1 && mother_print == 1)){
				per = (Person *) dnode->val.v;
				printf("%s\n", per->Name); 
				per->Printed = 1;
				if ((strcmp(per->Sex, "M")) == 0) {
					printf("  Sex: Male\n");
				}
				else if ((strcmp(per->Sex, "F")) == 0) {
					printf("  Sex: Female\n");
				}
				else {
					printf("  Sex: Unknown\n");
				}
				if (per->Father == NULL) {
					printf("  Father: Unknown\n");
				}
				else {
					printf("  Father: %s\n", per->Father->Name);//else unknown
				}
				if (per->Mother == NULL) {
					printf("  Mother: Unknown\n");
				}
				else {
					printf("  Mother: %s\n", per->Mother->Name);//else unknown
				}
				//check if the children dllist is empty
				if (dll_empty(per->Children)) {
					printf("  Children: None\n\n");//if no kids
				}
				else {
					printf("  Children:\n");
					dll_traverse(dnode_child, per->Children){
						child_per = (Person *) dnode_child->val.v;
						printf("    %s\n", child_per->Name);
						dll_append(toprint, new_jval_v(child_per));
					}
					printf("\n");
				}
				//for all of p's children, put the child at the end of doprint	
			}//end if
		} //end if
	} //end while


/* Clean up, my dudes. I want this code to shine like THE TOP OF THE CHRYSLER BUILDING */

	//this goes bit by bit through a person struct and deletes the pointers
	jrb_traverse(node, people) {
		per = (Person *) node->val.v;
		free(per->Name);
		if(per->Sex != NULL) free(per->Sex);
		if(per->Children != NULL) free_dllist(per->Children);
		free(per);
	}
	jrb_free_tree(people);//this deletes the tree

	jettison_inputstruct(is); //this attempts to avoid memory leaks

	exit(0);
}

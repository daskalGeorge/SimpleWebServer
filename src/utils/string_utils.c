/*
 * string_utils.c
 *
 *  Created on: Sep 22, 2016
 *      Author: wtrwhl
 */

#include "string_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int replace_char(char * str, char a, char b){
	if(str == NULL) return 0;
	int length = strlen(str);
	while(length != 0){
		if(str[length] == a)
			str[length] = b;
		length--;
	}
	return 1;
}

char ** split_str(char * str, int * out, char del){
	char ** dlist;
	char line[1024];
	int len = strlen(str);
	int i, j = 0, index = 0;
	*out = 0;
	dlist = (char **) malloc(sizeof(char *));
	for(i=0; i<len; i++){
		if(str[i] == del){
			line[j] = '\0';
			dlist[index] = (char *) malloc((strlen(line) + 1) * sizeof(char));
			strcpy(dlist[index], line);
			index++;
			(*out)++;
			j = 0;
			dlist = (char **) realloc(dlist, (index + 1) * sizeof(char *));
		}
		else{
			line[j] = str[i];
			j++;
		}
	}

	return dlist;
}

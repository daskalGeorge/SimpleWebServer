/*
 * webserver.c
 *
 *  Created on: Sep 20, 2016
 *      Author: wtrwhl
 */

#include "webserver.h"
#include "utils/string_utils.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#define PATH_MAX 1024

struct webserver_request {
    enum {REQUEST_POST, REQUEST_GET, REQUST_HEAD} method;
    char *uri;
    char *host;
    char *args;
};

struct webserver_response {
    int status;
    char *body;
    unsigned long body_length;
    char *content_type;
    char *content_encoding;
};

char * get_index(char * token);
int parse_lines(int client_socket, struct webserver_request *request);
int parse_method(char request_line[], struct webserver_request *request);
int accept_connection(struct webserver_connection server_connection);
int handle_connection(struct webserver_connection, int client_socket);
void create_response(int, struct webserver_response *, struct webserver_request, char *, unsigned long, int);
int respond(int client_socket, struct webserver_response *response);
void respond_with_error(int client_socket, int http_status);
int http_message(int http_status, char **http_message);
int get_content_GET(char* root_path, struct webserver_request *request, char **file_content, unsigned long *file_length);
int get_cgi_content(char * root_path, struct webserver_request *request, char **file_content, unsigned long *file_length);
char* get_content_type_from_filepath(char *file_path);
void logger(struct webserver_request *request, int http_status);

struct webserver_connection webserver_connect(int port_number, char *root) {

    struct webserver_connection server_connection;
    server_connection.status = 0;
    server_connection.socket = socket(PF_INET, SOCK_STREAM, 0);

    if (setsockopt(server_connection.socket, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)) < 0) {

        server_connection.status = -1;
        return server_connection;
    }

    server_connection.path = malloc(strlen(root) + 1);
    strcpy(server_connection.path, root);

    struct sockaddr_in socket_address;
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(port_number);
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int length = sizeof(socket_address);
    if (bind(server_connection.socket, (struct sockaddr*) &socket_address, length) < 0) {

        server_connection.status = -1;
        return server_connection;
    }

    if (listen(server_connection.socket, 5) < 0) {
        server_connection.status = -1;
    }

    return server_connection;
};

void webserver_disconnect(struct webserver_connection server_connection) {
    close(server_connection.socket);
}

void webserver_listen(struct webserver_connection server_connection) {

    printf("Starts listening for new connections...\n");

    while (1) {

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        fd_set set;

        FD_ZERO(&set);
        FD_SET(server_connection.socket, &set);

        int status = select(server_connection.socket + 1, &set, NULL, NULL, &timeout);
        if (status > 0) {
            accept_connection(server_connection);
        }
    }
}

int accept_connection(struct webserver_connection server_connection) {

    int client_socket;
    struct sockaddr_in client_address;
    socklen_t client_length;

    client_socket = accept(server_connection.socket, (struct sockaddr*) &client_address, &client_length);
    if (client_socket < 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {

        handle_connection(server_connection, client_socket);
        exit(0);
    } else {
        close(client_socket);
    }

    return 0;
}

int handle_connection(struct webserver_connection server_connection, int client_socket) {

    struct webserver_request request;
    int status;

    status = parse_lines(client_socket, &request);
    if (status == 0) {

        char *file_content;
        unsigned long * file_length = malloc(sizeof(unsigned long));



		struct webserver_response response;
		if(request.method == REQUEST_GET){
			status = get_content_GET(server_connection.path, &request, &file_content, file_length);
			create_response(status, &response, request, file_content, *file_length, client_socket);
		} else
		if(request.method == REQUEST_POST){
			status = get_cgi_content(server_connection.path, &request, &file_content, file_length);
			create_response(status, &response, request, file_content, *file_length, client_socket);
		}
		respond(client_socket, &response);



        free(file_content);
        free(file_length);
    }

    logger(&request, status);

    close(client_socket);

    return 0;
}

int parse_lines(int client_socket, struct webserver_request *request) {

    char request_line[1024];

    while (1) {

        bzero(request_line, 1024);
        if (read(client_socket, request_line, 1023) < 0) {
            return -1;
        }
        char ** tmp; int len;
        tmp = split_str(request_line, &len, '\n');
        //printf("Request Line 1: %s, num of lines = %d\n", tmp[0], len );

        int status = parse_method(tmp[0], request);

        if (status > 0) {
            respond_with_error(client_socket, status);
        }

        if(request->method == REQUEST_POST){
        	//printf("REQ_POST %s\n", request_line);

        }

        return status;
    }

    return 0;
}

int parse_method(char request_line[], struct webserver_request *request) {

    char *token;
    int token_counter = 0;
    token = strtok(request_line, " ");

    while (token != NULL) {

        if (token_counter == 0) {

            if (strcmp(token, "GET") == 0) {
                request->method = REQUEST_GET;
            } else
            if (strcmp(token, "HEAD") == 0){
                request->method = REQUST_HEAD;
            } else
			if(strcmp(token, "POST") == 0) {
				request->method = REQUEST_POST;
            } else {
            	return 400;
            }

        } else if (token_counter == 1 && request->method == REQUEST_GET) {

        	if(strcmp(token, "/") == 0){
        		request->uri = get_index(token);
        		request->args = NULL;
        	} else{
				request->uri = malloc(strlen(token) + 1);
				strcpy(request->uri, token);

				char * tmpURI = request->uri;
				request->uri = strsep(&tmpURI, "?");

				if(replace_char(tmpURI, '&', ' ')){
					request->args = tmpURI;
				} else{
					request->args = NULL;
				}
        	}
        }


        token = strtok(NULL, " ");
        token_counter++;
    }

    free(token);

    return 0;
}

int parse_POST_args(char request_line[], struct webserver_request *request){

	if(request_line == NULL){
		return 500;
	}
	else{
		request->args = request_line;
	}

	return 0;
}

char * get_index(char * token){
	char index_page[] = "index.html";
	char * uri;
	uri = malloc(strlen(token) + strlen(index_page) + 1);
	strcat(uri, token);
	strcat(uri, index_page);
	return uri;
}

int http_message(int http_status, char **http_message) {

    *http_message = malloc(30);

    switch (http_status) {
        case 200:

            strcpy(*http_message, "OK");
            break;

        case 201:

            strcpy(*http_message, "Created");
            break;

        case 400:

            strcpy(*http_message, "Bad Request");
            break;

        case 401:

            strcpy(*http_message, "Unauthorized");
            break;

        case 403:

            strcpy(*http_message, "Forbidden");
            break;

        case 404:

            strcpy(*http_message, "Not Found");
            break;

        case 405:

            strcpy(*http_message, "Method Not Allowed");
            break;

        case 500:

            strcpy(*http_message, "Internal Server Error");
            break;

        default:

            return -1;
            break;
    }

    return 0;
}

void create_response(int status, struct webserver_response * response, struct webserver_request request, char * file_content, unsigned long file_length, int client_socket){
	if (status == 0) {
		status = 200;
		char *content_type = get_content_type_from_filepath(request.uri);

		response->status = status;
		response->body = file_content;
		response->body_length = file_length;
		response->content_type = content_type;
		response->content_encoding = NULL;

		if (strcmp(content_type, "text/html") == 0) {
			response->content_encoding = strdup("utf-8");
		}
		free(content_type);
	} else if (status > 0) {
		respond_with_error(client_socket, status);
	} else {
		respond_with_error(client_socket, 500);
	}
}

int respond(int client_socket, struct webserver_response *response) {

    char *http_status_message;
    char response_headers[200];

    http_message(response->status, &http_status_message);
    size_t content_type_length = strlen(response->content_type + 11);
    if (response->content_encoding != NULL) {
        content_type_length += strlen(response->content_encoding);
    }
    char *content_type = malloc(content_type_length);
    strcpy(content_type, response->content_type);

    if (response->content_encoding != NULL) {

        strcat(content_type, "; charset=");
        strcat(content_type, response->content_encoding);
    }

    sprintf(
        response_headers,
        "HTTP/1.1 %d %s\nContent-Type: %s\nContent-Length: %lu\nServer: Webserver C\r\n\r\n",
        response->status,
        http_status_message,
        content_type,
        response->body_length
    );


    write(client_socket, response_headers, strlen(response_headers));
    write(client_socket, response->body, response->body_length);
    write(client_socket, "\r\n", strlen("\r\n"));

    free(http_status_message);
    free(content_type);

    return 0;
}

void respond_with_error(int client_socket, int http_status) {

    char *message;
    char *html = malloc(100);

    http_message(http_status, &message);
    sprintf(html, "<html><body>%s</body></html>", message);

    struct webserver_response response;
    response.status = http_status;
    response.body = html;
    response.body_length = strlen(html);
    response.content_type = strdup("text/html");
    response.content_encoding = strdup("utf-8");

    respond(client_socket, &response);

    free(message);
    free(html);
}

int get_content_GET(char *root_path, struct webserver_request *request, char **file_content, unsigned long *file_length) {

	// check for arguments
	if(request->args){
		int status;
		status = get_cgi_content(root_path, request, file_content, file_length);
		return status;
	}

	// get uri
    char *file_path = malloc(strlen(root_path) + strlen(request->uri) + 1);
    strcpy(file_path, root_path);
    strcat(file_path, request->uri);
    //

    // get requested file
 //   *file_content = malloc(0);

    if (access(file_path, F_OK) == -1) {
        return 404;
    }

    struct stat s;
    if (stat(file_path, &s) == 0) {
        if (s.st_mode & S_IFDIR) {
            return 404;
        }
    } else {
        return 404;
    }

    FILE *file;
    file = fopen(file_path, "rb");
    if (!file) {
        return -1;
    }

    fseek(file, 0, SEEK_END);
    *file_length = ftell(file);
    fseek(file, 0, SEEK_SET);

 //   free(*file_content);
    *file_content = malloc(*file_length + 1);
    fread(*file_content, 1, *file_length, file);

    fclose(file);
    free(file_path);
    //

    return 0;
}

int get_cgi_content(char * root_path, struct webserver_request *request, char **file_content, unsigned long *file_length){

	FILE *file;

	char *file_path;
	file_path = malloc(strlen(root_path) + strlen(request->uri) + 1);
	strcpy(file_path, root_path);
	strcat(file_path, request->uri);

//    *file_content = malloc(0);

    if (access(file_path, F_OK) == -1) {
        return 404;
    }

    struct stat s;
    if (stat(file_path, &s) == 0) {
        if (s.st_mode & S_IFDIR) {
            return 404;
        }
    } else {
        return 404;
    }

    char * php_query;
    char php_com[] = "php-cgi ";
    php_query = (char *) malloc(strlen(php_com) + strlen(file_path) + 1 + strlen(request->args) + 1);

    strcpy(php_query, php_com);
    strcat(php_query, file_path);
    strcat(php_query, " ");
    strcat(php_query, request->args);


	file = popen(php_query, "r");
	if (!file)
		return -1;


	size_t len = 0;
	int ignore_lines = 2;
	char * line_content;
	*file_content = (char *) malloc(1);
	*file_content[0] = '\0';
	while (getline(&line_content, &len, file) != -1){

		if(ignore_lines-- > 0)continue;

	    //fputs(*file_content, stdout);
		*file_content = realloc(*file_content, strlen(*file_content) + strlen(line_content));
		strcat(*file_content, line_content);
	}

	*file_length = strlen(*file_content);

	pclose(file);
	free(line_content);
	free(file_path);
	free(php_query);


	return 0;
}

char* get_content_type_from_filepath(char *file_path) {

    char *file_path_copy = strdup(file_path);
    char *token;
    char *extension;
    char *content_type = malloc(20);

    extension = malloc(0);
    while ((token = strsep(&file_path_copy, ".")) != NULL) {

        free(extension);
        extension = strdup(token);
    }

    free(file_path_copy);
    free(token);

    if (strcmp(extension, "htm") == 0 || strcmp(extension, "html") == 0) {
        strcpy(content_type, "text/html");
    } else if (strcmp(extension, "jpg") == 0) {
        strcpy(content_type, "image/jpeg");
    } else if (strcmp(extension, "png") == 0) {
        strcpy(content_type, "image/png");
    } else if (strcmp(extension, "gif") == 0) {
        strcpy(content_type, "image/gif");
    }

    return content_type;
}

void logger(struct webserver_request *request, int http_status) {

    char method[5];

    switch (request->method) {
        case REQUEST_GET:

            strcpy(method, "GET");
            break;

        case REQUEST_POST:

            strcpy(method, "POST");
            break;

        case REQUST_HEAD:
        	strcpy(method, "HEAD");
			break;
    }

    printf("%s %s %d\n", method, request->uri, http_status);

    free(request->uri);
}

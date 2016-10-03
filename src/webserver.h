/*
 * webserver.h
 *
 *  Created on: Sep 20, 2016
 *      Author: wtrwhl
 */

#ifndef WEBSERVER_H_
#define WEBSERVER_H_


#include <stdio.h>

struct webserver_connection {
    int status;
    int socket;
    char *path;
};

struct webserver_connection webserver_connect(int port_number, char *root);
void webserver_listen(struct webserver_connection);
void webserver_disconnect(struct webserver_connection);



#endif /* WEBSERVER_H_ */

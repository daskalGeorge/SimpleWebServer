/*
 * main.c
 *
 *  Created on: Sep 20, 2016
 *      Author: wtrwhl
 */

#include "webserver.h"
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, const char * argv[]) {

    struct webserver_connection connection;
    connection = webserver_connect(8888, "MyTestSite");

    if (connection.status != 0) return EXIT_FAILURE;

    webserver_listen(connection);

    return EXIT_SUCCESS;
}

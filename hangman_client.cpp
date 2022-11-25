/* +---+
   |   |
   O   |
  /|\  |
  / \  |
       |
========

Gonçalo Nunes - ist199229
Mateus Pinho - ist199282
*/

// main function that takes two arguments from the command line
// the first argument is the server's IP address
// the second argument is the server's port number

// this should have something in it but i dont remember how include files work
#include "hangman_client.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <unistd.h>

// TODO don't add \n from hint to file

/* Constants */
#define BLOCK_SIZE 256

/* Global variables */
int move_number = 1;

int start_new_game(std::string id);
int valid_id(std::string id);
int receive_message();

int main(int argc, char *argv[]) {
    int fd, errorcode;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    char buffer[BLOCK_SIZE];

    // check if the number of arguments is correct
    if (argc != 3)
    {
        printf("Input Error: Incorrect number of arguments.\nExpected 2 arguments, received %d\n", argc - 1);
        printf("\nTry running the program in the following format: %s <server IP> <server port>\n\n", argv[0]);
        exit(1);
    }

    // get the server's IP address and port number
    char *server_ip = argv[1];
    char *server_port = argv[2];

    // create a socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) exit(1);

    // set the server's address
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errorcode = getaddrinfo(server_ip, server_port, &hints, &res);
    if (errorcode != 0) exit(1); 

    /* main program code */
    printf("Welcome to Hangman!\n");

    freeaddrinfo(res);
    close(fd);
}

int start_new_game(std::string id) {

    if (!valid_id(id)) {
        // TODO send error message
        return -1;
    }

    // Connect to server

    // Send ID and new game request

    // Receive message

    // Set word size

    // Set maximum number of errors

    return 0; // 0 is a placeholder
}

int valid_id(std::string id) {
    int i = 0;

    if (id.size() != 6) {
        return -1;
    }

    while (id[i] != '\0' || id[i] != '\n') {
        if (!isdigit(id[i])) {
            return -1;
        }
        i++;
    }
    return 0;
}
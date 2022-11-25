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

// TODO don't add \n from hint to file

/* Constants */
#define BLOCK_SIZE 256

/* Global variables */
int move_number = 1;

int start_new_game(std::string id);
int valid_id(std::string id);
int receive_message(int fd, socklen_t addrlen, sockaddr_in addr, char buffer[]);
int send_message(int fd, char message[], size_t buf_size, struct addrinfo res);

int main(int argc, char *argv[]) {
    int fd, errorcode;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;    // TODO custom name for structs
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
    int server_port = atoi(argv[2]);

    // create a socket

}

int receive_message(int fd, socklen_t addrlen, struct sockaddr_in addr, char buffer[]) {
    addrlen = sizeof(addr);
    ssize_t n = recvfrom(fd, buffer, BLOCK_SIZE, 0, (struct sockaddr*)&addr, &addrlen);
    
    if(n==-1) { 
        /* Error */
        printf("Error (receive_message): An error occured while receiving a message.\n");
        printf("    error message was received from ssize_t recvfrom(int sockfd, void *restrict buf, size_t len, int flags, struct sockaddr *restrict src_addr, socklen_t *restrict addrlen)\n");
        exit(1);    // TODO error message
    }

    return 0;
}

int send_message(int fd, char message[], size_t buf_size, struct addrinfo *res) {
    ssize_t n = sendto(fd, message, buf_size, 0, res->ai_addr, res->ai_addrlen);

    if(n == -1) {
        /* Error */
        printf("Error (send_message): An error occured while sending a message.\n");
        printf("    error message was received from ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)\n");
        exit(1);    // TODO error message
    }

    return 0;
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
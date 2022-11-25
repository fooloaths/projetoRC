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
#include <iostream>

// TODO don't add \n from hint to file

/* Constants */
#define BLOCK_SIZE 256

/* Global variables */
int move_number = 1;

int start_new_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr);
int valid_id(std::string id);
int receive_message(int fd, socklen_t addrlen, sockaddr_in addr, char* buffer, size_t buf_size);
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
    
    while (1) {
        std::string input;
        std::getline(std::cin, input);

        // split input in two strings using space as delimiter, doesnt account for malformed input
        std::string command = input.substr(0 , input.find(' '));
        std::string message = input.substr(input.find(' ') + 1, input.length());
        // check if the command is equal to "start"
        if (command == "start") {
            start_new_game(message, fd, res, addr);
        }
    }

    freeaddrinfo(res);
    close(fd);
}

int receive_message(int fd, struct sockaddr_in addr, char* buffer, size_t buf_size) {
    socklen_t addrlen = sizeof(addr);
    ssize_t n = recvfrom(fd, buffer, buf_size, 0, (struct sockaddr*)&addr, &addrlen);

    if(n==-1) { 
        /* Error */
        printf("Error (receive_message): An error occured while receiving a message.\n");
        printf("    error message was received from ssize_t recvfrom(int sockfd, void *restrict buf, size_t len, int flags, struct sockaddr *restrict src_addr, socklen_t *restrict addrlen)\n");
        exit(1);    // TODO error message
    }

    return 0;
}

int send_message(int fd, const char* message, size_t buf_size, struct addrinfo *res) {
    ssize_t n = sendto(fd, message, buf_size, 0, res->ai_addr, res->ai_addrlen);

    if(n == -1) {
        /* Error */
        printf("Error (send_message): An error occured while sending a message.\n");
        printf("    error message was received from ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)\n");
        exit(1);    // TODO error message
    }

    return 0;
}

int start_new_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr) {

    if (!valid_id(id)) {
        // TODO send error message
        return -1;
    }

    // Send ID and new game request
    std::string message = "SNG " + id + "\n";
    send_message(fd, message.c_str(), message.length(), res);

    // TODO Receive messageww
    // ! fix the buffer being block size
    char buffer[BLOCK_SIZE];
    receive_message(fd, addr, buffer, BLOCK_SIZE);
    std::string response = buffer;

    // split input in four strings using space as delimiter, doesnt account for malformed input
    // remove \n from response
    response.pop_back();

    std::string response_command = response.substr(0 , response.find(' '));
    if (response_command == "ERR") {
        // TODO send error message
        return -1;
    }
    
    std::string status = response.substr(response.find(' ') + 1, response.find(' ', response.find(' ') + 1));
    std::string n_letters = response.substr(response.find(' ', response.find(' ') + 1) + 1, response.find(' ', response.find(' ', response.find(' ') + 1) + 1));
    std::string max_errors = response.substr(response.find(' ', response.find(' ', response.find(' ') + 1) + 1) + 1, response.length());

    // convert number of letters into a number of underscores to print
    int n_letters_int = std::stoi(n_letters);
    std::string underscores = "";
    for (int i = 0; i < n_letters_int; i++) {
        underscores += "_ ";
    }

    // ! code down sucks
    // // if (status != "OK") {
    // //     // TODO send error message
    // //     return -1;
    // // }

    printf("New game started (max %s errors): %s\n", max_errors.c_str(), underscores.c_str());
    
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
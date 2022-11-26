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
#define INPUT 128 // TODO rethink this value
#define START "start"
#define SG "sg"
#define SCOREBOARD "scoreboard"
#define SB "sb"
#define PLAY "play"
#define PL "pl"
#define GUESS "guess"
#define GW "gw"
#define HINT "hint"
#define H "h"
#define STATE "state"
#define ST "st"
#define QUIT "quit"
#define EXIT "exit"
#define TRUE 1
#define FALSE 0
#define COMMAND_SIZE 11 // Largest command keyword is scoreboard. Add 1 for '\0'
#define QUT "QUT"
#define PLG "PLG"
#define RLG "RLG"
#define OK "OK"
#define WIN "WIN"
#define DUP "DUP"
#define NOK "NOK"
#define OVR "OVR"
#define INV "INV"
#define ERR "ERR"
#define VICTORY_MESSAGE "WELL DONE! You guessed: "

/* Global variables */
int move_number = 1;
std::string player_id;
std::string word = "_____";

int start_new_game(std::string id);
int valid_id(std::string id);
int receive_message(int fd, socklen_t addrlen, sockaddr_in addr, char buffer[]);
int send_message(int fd, char message[], size_t buf_size, struct addrinfo *res);
int exit_game(char message[], int fd, size_t buf_size, struct addrinfo *res);
std::string play_result(char message[]);
int play_aux_ok(char message[], char letter);
int play(int fd, socklen_t addrlen, struct sockaddr_in addr, char message[], char letter);
std::string format_word();


// TODO add checks to see if the move_numbers make sense when communicating with the server
// TODO sprintf is always used alongside send_message. Maybe abstract it

int main(int argc, char *argv[]) {
    int fd, errorcode;
    char* err;
    //ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;    // TODO custom name for structs
    struct sockaddr_in addr;
    char buffer[BLOCK_SIZE];

    // Check if the number of arguments is correct
    if (argc != 3)
    {
        printf("Input Error: Incorrect number of arguments.\nExpected 2 arguments, received %d\n", argc - 1);
        printf("\nTry running the program in the following format: %s <server IP> <server port>\n\n", argv[0]);
        exit(1);
    }

    // Get the server's IP address and port number
    char *server_ip = argv[1];
    char *server_port = argv[2];

    // Create a socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        exit(1);
    }

    // Set the server's address
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errorcode = getaddrinfo(server_ip, server_port, &hints, &res);
    if (errorcode != 0) {
        exit(1); // TODO set error message + close socket
    }

    /* Main program code */
    char* input = (char *) malloc(INPUT * sizeof(char));
    if (input == NULL) {
        exit(1); // TODO error message + close socket
    }

    char* command = (char *) malloc(COMMAND_SIZE * sizeof(char));
    if (input == NULL) {
        exit(1); // TODO error message + close socket
    }

    /* Client application loop */
    
    int terminate = FALSE;
    while (terminate == FALSE) {
        err = fgets(input, INPUT, stdin);
        if (err == NULL) {
            // TODO maybe check ferror too
            printf("Error (main): Failed to correctly read input with fgets.");
        }

        // check if the command is equal to "start"

        if (!strcmp(command, START)) {
            //start_new_game(message);
        }
        else if (!strcmp(command, QUIT) || !strcmp(command, EXIT)) {
            exit_game(buffer, fd, BLOCK_SIZE, res);

            if (!strcmp(command, EXIT)) {
                terminate = TRUE;
            }
        }
        else {
            printf("Error (main): Invalid command\n");
        }
    }


    free(input);
    free(command);
    freeaddrinfo(res);
    close(fd);
}

int receive_message(int fd, socklen_t addrlen, struct sockaddr_in addr, char buffer[]) {
    addrlen = sizeof(addr);
    ssize_t n = recvfrom(fd, buffer, BLOCK_SIZE, 0, (struct sockaddr*)&addr, &addrlen);
    
    if(n==-1) { 
        /* Error */
        printf("Error (receive_message): An error occured while receiving a message.\n");
        printf("    error message was received from ssize_t recvfrom(int sockfd, void *restrict buf, size_t len, int flags, struct sockaddr *restrict src_addr, socklen_t *restrict addrlen)\n");
        return -1;    // TODO error message
    }

    return 0;
}

int send_message(int fd, char message[], size_t buf_size, struct addrinfo *res) {
    ssize_t n = sendto(fd, message, buf_size, 0, res->ai_addr, res->ai_addrlen);

    if(n == -1) {
        /* Error */
        printf("Error (send_message): An error occured while sending a message.\n");
        printf("    error message was received from ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)\n");
        return -1;    // TODO error message
    }

    return 0;
}

std::string format_word() {
    int len = word.length();

    std::string formatted_word;
    int i;
    for (i = 0; i < len; i++) {
        formatted_word.push_back(toupper(word[i]));
        formatted_word.push_back(' ');
    }
    formatted_word.pop_back();

    return formatted_word;
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

// TODO maybe rename to play_status
std::string play_result(char message[]) {
    int i = 4; // Skip reply signature (RLG) and space

    std::string status;
    while (message[i] != ' ') {
        status.push_back(message[i]);
        i++;
    }

    return status;
}

int play_aux_ok(char message[], char letter) {
    int i = 8; // Skip to trial number () RLG OK
    int trial_number = 0;
    while (message[i] != ' ') {
        trial_number = trial_number * 10 + (message[i] - '0');
        i++;
    }
    i++; // Skip space

    if (trial_number != move_number) {
        return -1;
    }


    int pos = 0;
    while (message[i] != '\0') {
        if (message[i] == ' ') {
            // Update knowledge of word;
            word[pos] = letter;
            pos = 0;
        }
        else {
            pos = pos * 10 + (message[i] - '\0');
        }

        i++;
    }

    // Print current knowledge of the word
    printf("Word: %s\n", format_word().c_str());

    return 0;
}

int play(int fd, socklen_t addrlen, struct addrinfo *res, struct sockaddr_in addr,
        char message[], size_t buf_size, char letter) {

    // Send message
    int err = sprintf(message, "%s %s %c %d", QUT, player_id.c_str(), letter, move_number);
    if (err < 0) {
        printf("Error (play): An error occured while preparing the exit message\n");
    }

    err = send_message(fd, message, buf_size, res);
    if (err == -1) {
        printf("Error (play): An error occured while sending the exit message\n");
    }

    // Receive message
    err = receive_message(fd, addrlen, addr, message);
    if (err == -1) {
        printf("Error (play): An error occured while receiving a message from the server\n");
    }

    // Check if play was successful
    std::string status = play_result(message); // TODO maybe use switch case
    if (status.compare(OK)) {
        if (play_aux_ok(message, letter) == -1) {
            printf("The trial numbers don't match\n");
        }
    }
    else if (status.compare(WIN)) {
        printf("%s%s", VICTORY_MESSAGE, format_word().c_str());
    }
    else if (status.compare(DUP)) {
        
    }
    else if (status.compare(NOK)) {
        
    }
    else if (status.compare(OVR)) {
        
    }
    else if (status.compare(INV)) {
        
    }
    else if (status.compare(ERR)) {
        
    }
    else {

    }

    return 0;
}

int exit_game(char message[], int fd, size_t buf_size, struct addrinfo *res) {

    int err = sprintf(message, "%s %s", QUT, player_id.c_str());
    if (err < 0) {
        printf("Error (exit_game): An error occured while preparing the exit message\n");
    }

    err = send_message(fd, message, buf_size, res);
    if (err == -1) {
        printf("Error (exit_game): An error occured while sending the exit message\n");
    }

    // TODO close TCP connections

    return 0;
}
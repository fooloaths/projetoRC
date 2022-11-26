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

int start_new_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr);
//int valid_id(std::string id);
int receive_message(int fd, sockaddr_in addr, char* buffer, size_t buf_size);
int send_message(int fd, char message[], size_t buf_size, struct addrinfo *res);
int exit_game(int fd, struct addrinfo *res);
std::string play_result(std::string message);
int play_aux_ok(std::string message, char letter);
int play(int fd, struct sockaddr_in addr, char message[], char letter);
std::string format_word();



// TODO add checks to see if the move_numbers make sense when communicating with the server
// TODO sprintf is always used alongside send_message. Maybe abstract it


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
    if (fd == -1) {
        exit(1);
    }

    // set the server's address
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errorcode = getaddrinfo(server_ip, server_port, &hints, &res);
    if (errorcode != 0) {
        exit(1); // TODO set error message + close socket
    }

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
    // Send ID and new game request
    player_id = id;
    std::string message = "SNG " + id + "\n";
    send_message(fd, message.c_str(), message.length(), res);

    // TODO Receive message
    // ! fix the buffer being block size
    char buffer[BLOCK_SIZE];
    receive_message(fd, addr, buffer, BLOCK_SIZE);
    std::string response = buffer;

    // split input in four strings using space as delimiter, doesnt account for malformed input
    // remove \n from response
    response.pop_back();

    std::string response_command = response.substr(0 , response.find(' '));
    if (response_command == "ERR") {
        printf("Error.\n");
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

// TODO maybe rename to play_status
std::string play_result(std::string message) {
    int i = 4; // Skip reply signature (RLG) and space

    std::string status;
    while (message[i] != ' ') {
        status.push_back(message[i]);
        i++;
    }

    return status;
}

int play_aux_ok(std::string message, char letter) {
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

int play(int fd, struct addrinfo *res, struct sockaddr_in addr,
        char buffer[], size_t buf_size, char letter) {

    // Send message
    std::string message = PLG + player_id + letter + std::to_string(move_number) + '\n';
    // // int err = sprintf(message, "%s %s %c %d", QUT, player_id.c_str(), letter, move_number);
    /*if (err < 0) {
        printf("Error (play): An error occured while preparing the exit message\n");
    }*/

    int err = send_message(fd, message.c_str(), message.length(), res);
    if (err == -1) {
        printf("Error (play): An error occured while sending the exit message\n");
    }

    // Receive message
    err = receive_message(fd, addr, buffer, buf_size);
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
        printf("%s%s\n", VICTORY_MESSAGE, format_word().c_str());
    }
    else if (status.compare(DUP)) {

    }
    else if (status.compare(NOK)) {
        
    }
    else if (status.compare(OVR)) {
        printf("The letter guessed %c was incorrect. There are no more attempts available\n");        
    }
    else if (status.compare(INV)) {
        
    }
    else if (status.compare(ERR)) {
        
    }
    else {

    }

    return 0;
}

int exit_game(int fd, struct addrinfo *res) {

    std::string message = QUT + player_id + '\n';

    int err = send_message(fd, message.c_str(), message.length(), res);
    if (err == -1) {
        printf("Error (exit_game): An error occured while sending the exit message\n");
    }

    // TODO close TCP connections

    return 0;
}

// // int valid_id(std::string id) {
// //     int i = 0;

// //     if (id.length() != 6) {
// //         return -1;
// //     }

// //     while (id[i] != '\0' || id[i] != '\n') {
// //         if (!isdigit(id[i])) {
// //             return -1;
// //         }
// //         i++;
// //     }
// //     return 0;
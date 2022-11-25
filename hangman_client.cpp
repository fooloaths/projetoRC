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

// TODO don't add \n from hint to file

/* Constants */
#define BLOCK_SIZE 256

/* Global variables */
int move_number = 1;

int start_new_game(string id);
int valid_id(string id);

int main(int argc, char *argv[])
{
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
}

int start_new_game(int id) {

    return 0; // 0 is a placeholder
}
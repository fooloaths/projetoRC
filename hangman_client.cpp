/* 
hangman client
gonçalo nunes
mateus pinho
*/

// main function that takes two arguments from the command line
// the first argument is the server's IP address
// the second argument is the server's port number

// this should have something in it but i dont remember how include files work
#include "hangman_client.hpp"
#include <stdio.h>
#include <stdlib.h>

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
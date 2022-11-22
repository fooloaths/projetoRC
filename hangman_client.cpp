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
        printf("Usage: %s <Server IP> <Server Port>\n", argv[0]);
        exit(1);
    }

    // get the server's IP address and port number
    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    // print the server's IP address and port number
    printf("Server IP: %s, Server Port: %d\n", server_ip, server_port);
}
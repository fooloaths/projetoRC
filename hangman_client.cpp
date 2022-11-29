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
#include <vector>

/* Global variables */
int move_number = 1;
std::string player_id;
std::string word = "";

void start_new_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr);
void receive_message(int fd, sockaddr_in addr, char* buffer, size_t buf_size);
void send_message(int fd, char message[], size_t buf_size, struct addrinfo *res);
int exit_game(std::string id, int fd, struct addrinfo *res);
std::string get_status(std::string message);
std::string play_aux_ok(const char* message, std::string letter);
int play(std::string letter, int fd, struct addrinfo *res, struct sockaddr_in add);
std::string format_word(std::string word_to_format = word);
int guess(std::string word, int fd, struct addrinfo *res, struct sockaddr_in addr);

// TODO add checks to see if the move_numbers make sense when communicating with the server
// TODO sprintf is always used alongside send_message. Maybe abstract it

int main(int argc, char *argv[]) {
    int fd, errorcode;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;

    // check if the number of arguments is correct
    if (argc != 3) exit(1);

    // get the server's IP address and port number
    char *server_ip = argv[1];
    char *server_port = argv[2];

    // create a socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) exit(1);

    // set the server's address
    if (memset(&hints, 0, sizeof(hints)) == NULL) exit(1);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errorcode = getaddrinfo(server_ip, server_port, &hints, &res);
    if (errorcode != 0) {
        exit(1); 
        freeaddrinfo(res);
        close(fd);
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
        } else if (command == "play" || command == "pl") {
            // TODO CHECK NUMBER OF MOVES
            play(message, fd, res, addr);
        } else if (command == "quit") {
            exit_game(message, fd, res);
        } else if (command == "exit") {
            exit_game(message, fd, res);
            break;
        } else if (command == "guess" || command == "gw") {
            // TODO CHECK NUMBER OF MOVES
            guess(message, fd, res, addr);
        } else {
            printf("Input Error: Invalid command.\n");
        }
    }

    freeaddrinfo(res);
    close(fd);
}

std::string format_word(std::string word_to_format) {
    int len = word_to_format.length();

    std::string formatted_word;
    int i;
    for (i = 0; i < len; i++) {
        formatted_word.push_back(toupper(word_to_format[i]));
        formatted_word.push_back(' ');
    }
    formatted_word.pop_back();

    return formatted_word;
}

void receive_message(int fd, struct sockaddr_in addr, char* buffer, size_t buf_size) {
    socklen_t addrlen = sizeof(addr);
    if (recvfrom(fd, buffer, buf_size, 0, (struct sockaddr*)&addr, &addrlen) == -1) exit(1);
}

void send_message(int fd, const char* message, size_t buf_size, struct addrinfo *res) {
    if (sendto(fd, message, buf_size, 0, res->ai_addr, res->ai_addrlen) == -1) exit(1);
}

void start_new_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr) {
    player_id = id;
    std::string message = "SNG " + id + "\n";
    send_message(fd, message.c_str(), message.length(), res);

    char buffer[BLOCK_SIZE];
    receive_message(fd, addr, buffer, BLOCK_SIZE);
    std::string response = buffer;

    // remove \n from response
    response.pop_back();    
    std::string response_command = response.substr(0 , response.find(' '));
    if (response_command == "ERR") {
        printf("el servidor no esta muy bueno ya?\n");
        exit(1);
    }
    
    std::string status = get_status(response);
    std::string n_letters = response.substr(response.find(' ', response.find(' ') + 1) + 1, response.find(' ', response.find(' ', response.find(' ') + 1) + 1));
    std::string max_errors = response.substr(response.find(' ', response.find(' ', response.find(' ') + 1) + 1) + 1, response.length());

    // convert number of letters into a number of underscores to print
    int n_letters_int = std::stoi(n_letters);
    for (int i = 0; i < n_letters_int; i++) {
        word += "_";
    }

    if (status != "OK") {
        printf("el servidor no esta muy bueno ya?\n");
        exit(1);
    }

    std::string formatted_word = format_word();
    printf("New game started (max %s errors): %s\n", max_errors.c_str(), formatted_word.c_str());
}

// TODO maybe rename to play_status
std::string get_status(std::string message) {
    int i = 4; // Skip reply signature (RLG) and space

    std::string status;
    while (message[i] != ' ') {
        status.push_back(message[i]);
        i++;
    }

    return status;
}

// function to return the positions of underscores in a word
std::vector<int> get_underscore_positions(std::string message) {
    std::vector<int> positions;
    int i = 0;
    for (char c : message) {
        if (c == '_') {
            positions.push_back(i);
        }
        i++;
    }

    return positions;
}

std::string play_win_ok(const char* message, std::string letter) {
    std::vector<int> positions = get_underscore_positions(word);

    // if there are no underscores, there is a bug somewhere
    if (positions.size() == 0) {
        printf("Error: No underscores in word.\n");
        exit(1);
    }

    // replace all underscores with the letter
    for (int i : positions) {
        word[i] = letter[0];
    }

    return word;
}

std::string play_aux_ok(const char* message, std::string letter) {
    std::string word_pos = message;
    word_pos.pop_back();
    word_pos = word_pos.substr(word_pos.find(' ', word_pos.find(' ', word_pos.find(' ') + 1) + 1) + 1, word_pos.length());

    // split word_pos into two strings by the first space
    int n = atoi(word_pos.substr(0, word_pos.find(' ')).c_str());
    std::string positions = word_pos.substr(word_pos.find(' ') + 1, word_pos.length());

    // transform word_pos into a vector of ints
    if (positions != "") {
        std::vector<int> word_pos_int;
        std::string word_pos_int_str;
        for (int i = 0; i < positions.length(); i++) {
            if (positions[i] == ' ') {
                word_pos_int.push_back(std::stoi(word_pos_int_str));
                word_pos_int_str.clear();
            }
            else {
                word_pos_int_str.push_back(positions[i]);
            }
        }
        word_pos_int.push_back(std::stoi(word_pos_int_str));

        // update word
        for (int i = 0; i < word_pos_int.size(); i++) {
            word.at(word_pos_int.at(i) - 1) = letter[0];
        }
    }
    return word;
    
}

int guess(std::string guess_word, int fd, struct addrinfo *res, struct sockaddr_in addr) {
    char buffer[BLOCK_SIZE];
    size_t buf_size = BLOCK_SIZE;

    // Send message
    std::string message = "PWG " + player_id + ' ' + guess_word + ' ' +  std::to_string(move_number) + '\n';

    send_message(fd, message.c_str(), message.length(), res);
    receive_message(fd, addr, buffer, buf_size);

    std::string response = buffer;
    // remove all characters after \n
    response = response.substr(0, response.find('\n') + 1);
    const char* buf = response.c_str();

    std::string status = get_status(buf);
    move_number++;
    if (status.compare(WIN) == 0) {
        printf("YOU ARE A GENIUS!!! THE WORD WAS %s\n", format_word(guess_word).c_str());
    } else if (status.compare(NOK) == 0) {
        printf("YOU ARE WRONG!!!\n");
    } else if (status.compare(OVR) == 0) {
        printf("YOU LOST!!! YOU ARE DUMB!!!\n");
    } else if (status.compare(INV) == 0) {
        printf("MESSAGE LOST IN TRAFFIC!!!\n");
    } else {
        printf("ERROR!!!\n");
        move_number--;
    }
    return 0;
}

int play(std::string letter, int fd, struct addrinfo *res, struct sockaddr_in addr) {
    // !! TODO fix buffer being block size
    char buffer[BLOCK_SIZE];
    size_t buf_size = BLOCK_SIZE;
    
    if (letter.length() != 1 || !isalpha(letter[0])) {
        printf("Error (play): The letter must be a single character.\n");
        return -1;
    }

    std::string message = "PLG " + player_id + ' ' + letter + ' ' + std::to_string(move_number) + '\n';
    send_message(fd, message.c_str(), message.length(), res);
    receive_message(fd, addr, buffer, buf_size);
    
    std::string response = buffer;
    // remove all characters after the first \n
    response = response.substr(0, response.find('\n') + 1);
    const char* buf = response.c_str();

    std::string status = get_status(buf); // TODO maybe use switch case
    move_number++;
    if (status.compare(OK) == 0) {
        word = play_aux_ok(buf, letter);
        printf("YOU ARE RIGHT!!! THE WORD NOW IS %s\n", format_word().c_str());
    } else if (status.compare(WIN) == 0) {
        word = play_win_ok(buf, letter);
        printf("YOU ARE A GENIUS!!! THE WORD WAS %s\n", format_word().c_str());
    } else if (status.compare(DUP) == 0) {
        printf("YOU ALREADY TRIED THIS LETTER!!!\n");
        move_number--;
    } else if (status.compare(NOK) == 0) {
        printf("YOU ARE WRONG!!!\n");
    } else if (status.compare(OVR) == 0) {
        printf("YOU LOST!!! YOU ARE DUMB!!!\n");
    } else if (status.compare(INV) == 0) {
        printf("MESSAGE LOST IN TRAFFIC!!!\n");
    } else {
        printf("ERROR!!!\n");
        move_number--;
    }
    return 0;
}

int exit_game(std::string id, int fd, struct addrinfo *res) {
    std::string message = "QUT " + id + '\n';

    send_message(fd, message.c_str(), message.length(), res);

    // TODO server reply

    return 0;
}
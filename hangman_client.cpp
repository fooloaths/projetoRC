//
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <tuple>
#include <fstream>

/* Definitions */
#define SOCKET_TIMEOUT 60 // seconds

/* Global variables */
int move_number = 0;
std::string player_id = "";
std::string word = "";

/* Constants */
const auto GSIP = "127.0.0.1";
const auto GSPORT = "58046";

void start_new_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr);
std::string receive_message(int fd, sockaddr_in addr, size_t buf_size);
void send_message(int fd, char message[], size_t buf_size, struct addrinfo *res);
void exit_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr);
std::string get_status(std::string message);
std::string play_aux_ok(std::string message, std::string letter);
void play(std::string letter, int fd, struct addrinfo *res, struct sockaddr_in add);
std::string format_word(std::string word_to_format = word);
void scoreboard(const char *server_ip, const char *server_port);
void guess(std::string word, int fd, struct addrinfo *res, struct sockaddr_in addr);
void reveal_word(int fd, struct addrinfo *res, struct sockaddr_in addr);
void status(const char *server_ip, const char *server_port);
void hint(const char *server_ip, const char *server_port);
void status_aux_ok(std::string message);
void hint_aux_ok(std::string message);
std::string tcp_helper(std::string message, const char *server_ip, const char *server_port);
std::tuple<char *, char *> parse_args(int argc, char *argv[]);

std::tuple<char *, char *> parse_args(int argc, char *argv[])
{
    char *server_ip, *server_port;

    /* Parse arguments in the following way
    First, if there are no arguments, use the default values
    Second, if there is only one argument, it can be either the server ip or the server port
    Server ip is preceded by -n and server port is preceded by -p
    Third, if there are two arguments, the order is irrelevant
    If there are more than two arguments, print an error message and exit
    */

    if (argc == 1)
    {
        std::cerr << "argc == 1" << std::endl;
        server_ip = (char *)GSIP;
        server_port = (char *)GSPORT;
    }
    else if (argc == 3)
    {
        std::cerr << "argc == 3" << std::endl;
        if (strcmp(argv[1], "-n") == 0)
        {
            server_ip = (char *)argv[2];
            server_port = (char *)GSPORT;
        }
        else if (strcmp(argv[1], "-p") == 0)
        {
            server_ip = (char *)GSIP;
            server_port = (char *)argv[2];
        }
        else
        {
            std::cout << "Error: Invalid arguments.\n";
            exit(1);
        }
    }
    else if (argc == 5)
    {
        std::cerr << "argc == 5" << std::endl;
        if (strcmp(argv[1], "-n") == 0)
        {
            if (strcmp(argv[3], "-p") == 0)
            {
                server_ip = (char *)argv[2];
                server_port = (char *)argv[4];
            }
            else
            {
                std::cout << "Error: Invalid arguments.\n";
                exit(1);
            }
        }
        else if (strcmp(argv[1], "-p") == 0)
        {
            if (strcmp(argv[3], "-n") == 0)
            {
                server_ip = (char *)argv[4];
                server_port = (char *)argv[2];
            }
            else
            {
                std::cout << "Error: Invalid arguments.\n";
                exit(1);
            }
        }
        else
        {
            std::cout << "Error: Invalid arguments.\n";
            exit(1);
        }
    }
    else
    {
        std::cout << "Error: Invalid arguments.\n";
        exit(1);
    }

    int server_port_int;
    try {
        server_port_int = std::stoi(server_port);
    } catch (std::exception const &e) {
        std::cout << "Error: Invalid port number.\n";
        exit(1);
    }

    if (server_port_int < 1024 || server_port_int > 65535)
    {
        std::cout << "Error: Invalid port number.\n";
        exit(1);
    }

    return std::make_tuple(server_ip, server_port);
}

int main(int argc, char *argv[])
{
    int fd, errorcode;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;

    // ! Comment out this to disable stderr output
    std::cerr.rdbuf(nullptr);

    // check if the number of arguments is correct
    auto ip_port_tuple = parse_args(argc, argv);
    char *server_ip = std::get<0>(ip_port_tuple);
    char *server_port = std::get<1>(ip_port_tuple);

    std::cerr << "Server IP: " << server_ip << std::endl;
    std::cerr << "Server Port: " << server_port << std::endl;

    // create a socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
    {
        std::cout << "Error socket: " << strerror(errno) << std::endl;
        exit(1);
    }

    // set the server's address
    if (memset(&hints, 0, sizeof(hints)) == NULL)
    {
        std::cout << "Error memset: " << strerror(errno) << std::endl;
        exit(1);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
    }

    errorcode = getaddrinfo(server_ip, server_port, &hints, &res);
    if (errorcode != 0)
    {
        std::cout << "Error getaddrinfo: " << gai_strerror(errorcode) << std::endl;
        exit(1);
        freeaddrinfo(res);
        close(fd);
    }

    /* main program code */
    while (1)
    {
        std::string input, command, message;
        std::getline(std::cin, input);
        std::stringstream ss(input);

        ss >> command;
        ss >> message;

        // poll for timeout UDP (must review)
        struct timeval tv;
        tv.tv_sec = SOCKET_TIMEOUT;
        tv.tv_usec = 0;
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv) == -1)
        {
            std::cout << "Error " << strerror(errno) << std::endl;
            exit(1);
        }

        if (command == START || command == SG)
        {
            if (message.empty())
            {
                std::cout << "Input Error: Invalid command.\n";
                continue;
            }
            start_new_game(message, fd, res, addr);
        }
        else if (command == PLAY || command == PL)
        {
            if (message.empty())
            {
                std::cout << "Input Error: Invalid command.\n";
                continue;
            }
            play(message, fd, res, addr);
        }
        else if (command == "quit")
        {
            exit_game(message, fd, res, addr);
        }
        else if (command == "exit")
        {
            exit_game(message, fd, res, addr);
            break;
        }
        else if (command == GUESS || command == GW)
        {
            if (message.empty())
            {
                std::cout << "Input Error: Invalid command.\n";
                continue;
            }
            guess(message, fd, res, addr);
        }
        else if (command == SCOREBOARD || command == SB)
        {
            scoreboard(server_ip, server_port);
        }
        else if (command == "rev")
        {
            reveal_word(fd, res, addr);
        }
        else if (command == "state" || command == "st")
        {
            status(server_ip, server_port);
        }
        else if (command == "hint" || command == "h")
        {
            hint(server_ip, server_port);
        }
        else
        {
            std::cout << "Input Error: Invalid command.\n";
        }
    }

    freeaddrinfo(res);
    close(fd);
}

std::string format_word(std::string word_to_format)
{
    auto len = word_to_format.length();

    std::string formatted_word;
    for (size_t i = 0; i < len; i++)
    {
        formatted_word.push_back((char)toupper(word_to_format[i]));
        formatted_word.push_back(' ');
    }
    formatted_word.pop_back();

    return formatted_word;
}

std::string receive_message(int fd, struct sockaddr_in addr, size_t buf_size)
{
    socklen_t addrlen = sizeof(addr);
    char buffer[buf_size] = {0};
    if (recvfrom(fd, buffer, buf_size, 0, (struct sockaddr *)&addr, &addrlen) == -1)
    {
        return NULL;
    }

    std::cerr << "Received: " << buffer;

    return buffer;
}

void send_message(int fd, const char *message, size_t buf_size, struct addrinfo *res)
{
    if (sendto(fd, message, buf_size, 0, res->ai_addr, res->ai_addrlen) == -1)
    {
        std::cout << "Error send_message : " << strerror(errno) << std::endl;
        exit(1);
    }
}

void reveal_word(int fd, struct addrinfo *res, struct sockaddr_in addr)
{
    if (player_id.empty())
    {
        std::cout << "Error: You are not playing a game.\n";
        return;
    }

    std::string message = REV + player_id + "\n";
    send_message(fd, message.c_str(), message.length(), res);
    std::string response;

    try
    {
        response = receive_message(fd, addr, BLOCK_SIZE);
    }
    catch (std::exception &e)
    {
        std::cout << "Error: Timeout.\n";
        return;
    }

    // remove all characters after \n
    response = response.substr(0, response.find('\n') + 1);

    std::string status = get_status(response);
    if (status == "OK")
    {
        // ?! IS THIS CORRECT
        exit(1);
    }
    else if (status == "ERR")
    {
        std::cout << "There is no ongoing game.\n";
        exit(1);
    }
    else
    {
        std::cout << "The word is " << status << "\n";
    }
}

// TODO CLEAN UP THIS WHOLE FUNCTION
void start_new_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr)
{
    std::string message = SNG + id + "\n";
    std::string response;

    send_message(fd, message.c_str(), message.length(), res);

    try
    {
        response = receive_message(fd, addr, BLOCK_SIZE);
    }
    catch (std::exception &e)
    {
        std::cout << "Error: Timeout.\n";
        return;
    }

    // remove \n from response
    response.pop_back();
    // TODO fix input splitting
    std::string response_command = response.substr(0, response.find(' '));

    // TODO fix input splitting
    std::string status = get_status(response);
    if (status != OK)
    {
        std::cout << "YOU CAN'T DO THAT!! THERE'S A GAME GOING ON!\n";
        return;
    }

    player_id = id;
    move_number = 1;
    word = "";

    std::string n_letters = response.substr(response.find(' ', response.find(' ') + 1) + 1, response.find(' ', response.find(' ', response.find(' ') + 1) + 1));
    std::string max_errors = response.substr(response.find(' ', response.find(' ', response.find(' ') + 1) + 1) + 1, response.length());

    // convert number of letters into a number of underscores to print

    int n_letters_int;

    try
    {
        n_letters_int = std::stoi(n_letters);
    }
    catch (std::exception &e)
    {
        std::cout << "Error: Invalid message. You have commited an error.\n";
        exit(1);
    }
    for (int i = 0; i < n_letters_int; i++)
    {
        word += "_";
    }

    std::string formatted_word = format_word();
    std::cout << "New game started (max " << max_errors << " errors): " << formatted_word << "\n";
}

std::string get_status(std::string message)
{
    size_t i = 4; // Skip reply signature (RLG) and space

    if (message.length() < 4)
    {
        printf("Error: Invalid message. You have commited an error.\n");
        exit(1);
    }

    std::string status;
    // take off \n if it exists
    if (message[message.length() - 1] == '\n')
    {
        message.pop_back();
    }
    // split message from i = 4 to the first space
    while (message[i] != ' ' && i < message.length())
    {
        status.push_back(message[i]);
        i++;
    }

    return status;
}

// function to return the positions of underscores in a word
auto get_underscore_positions(std::string message)
{
    std::vector<size_t> positions;
    size_t i = 0;
    for (char c : message)
    {
        if (c == '_')
        {
            positions.push_back(i);
        }
        i++;
    }

    return positions;
}

std::string play_win_ok(std::string letter)
{
    auto positions = get_underscore_positions(word);

    // if there are no underscores, there is a bug somewhere
    if (positions.size() == 0)
    {
        printf("Error: No underscores in word.\n");
        exit(1);
    }

    // replace all underscores with the letter
    for (size_t i : positions)
    {
        word[i] = letter[0];
    }

    return word;
}

std::string play_aux_ok(std::string word_pos, std::string letter)
{
    // TODO fix input splitting
    word_pos.pop_back();
    word_pos = word_pos.substr(word_pos.find(' ', word_pos.find(' ', word_pos.find(' ') + 1) + 1) + 1, word_pos.length());

    std::cerr << "word_pos: " << word_pos << "\n";

    // split word_pos into two strings by the first space
    // TODO fix input splitting
    std::string positions = word_pos.substr(word_pos.find(' ') + 1, word_pos.length());

    // transform word_pos into a vector of ints
    if (positions != "")
    {
        std::vector<size_t> word_pos_int;
        std::string word_pos_int_str;
        for (size_t i = 0; i < positions.length(); i++)
        {
            if (positions[i] == ' ')
            {
                try
                {
                    word_pos_int.push_back((size_t)std::stoi(word_pos_int_str));
                }
                catch (std::exception &e)
                {
                    std::cout << "Error: Invalid message. You have commited an error.\n";
                }
                word_pos_int_str.clear();
            }
            else
            {
                word_pos_int_str.push_back(positions[i]);
            }
        }
        try
        {
            word_pos_int.push_back((size_t)std::stoi(word_pos_int_str));
        }
        catch (std::exception &e)
        {
            std::cout << "Error: Invalid message. You have commited an error.\n";
            exit(1);
        }

        // update word
        for (size_t i = 0; i < word_pos_int.size(); i++)
        {
            word.at(word_pos_int.at(i) - 1) = letter[0];
        }
    }
    return word;
}

void guess(std::string guess_word, int fd, struct addrinfo *res, struct sockaddr_in addr)
{
    std::string response;
    if (player_id.empty())
    {
        std::cout << "Error: You are not playing a game.\n";
        return;
    }

    while (response == "")
    {
        // Send message
        std::string message = PWG + player_id + ' ' + guess_word + ' ' + std::to_string(move_number) + '\n';
        send_message(fd, message.c_str(), message.length(), res);
        try
        {
            response = receive_message(fd, addr, BLOCK_SIZE);
        }
        catch (std::exception &e)
        {
            std::cout << "Error: Timeout.\n";
            return;
        }

        // remove all characters _after \n
        response = response.substr(0, response.find('\n') + 1);
    }

    std::string status = get_status(response);
    move_number++;
    if (status.compare(WIN) == 0)
    {
        printf("YOU ARE A GENIUS!!! THE WORD WAS %s\n",
               guess_word.c_str());
    }
    else if (status == "DUP")
    {
        printf("YOU ALREADY TRIED THIS WORD!!!\n");
    }
    else if (status.compare(NOK) == 0)
    {
        printf("YOU ARE WRONG!!!\n");
    }
    else if (status.compare(OVR) == 0)
    {
        printf("YOU LOST!!! YOU ARE DUMB!!!\n");
    }
    else if (status.compare(INV) == 0)
    {
        printf("MESSAGE LOST IN TRAFFIC!!!\n");
    }
    else
    {
        printf("ERROR!!!\n");
        move_number--;
    }
    return;
}

void play(std::string letter, int fd, struct addrinfo *res, struct sockaddr_in addr)
{
    std::string response = "";

    if (player_id.empty())
    {
        std::cout << "Error: You are not playing a game.\n";
        return;
    }

    if (letter.length() != 1 || !isalpha(letter[0]))
    {
        printf("Error (play): The letter must be a single character.\n");
        return;
    }

    while (response == "")
    {
        std::string message = PLG + player_id + ' ' + letter + ' ' + std::to_string(move_number) + '\n';
        send_message(fd, message.c_str(), message.length(), res);
        try
        {
            response = receive_message(fd, addr, BLOCK_SIZE);
        }
        catch (std::exception &e)
        {
            std::cout << "Error: Timeout.\n";
            return;
        }

        // remove all characters after the first \n
        response = response.substr(0, response.find('\n') + 1);

        std::cerr << "response: " << response << "\n";
    }

    std::string status = get_status(response);
    move_number++;
    if (status.compare(OK) == 0)
    {
        word = play_aux_ok(response, letter);
        printf("YOU ARE RIGHT!!! THE WORD NOW IS %s\n", format_word().c_str());
    }
    else if (status.compare(WIN) == 0)
    {
        word = play_win_ok(letter);
        printf("YOU ARE A GENIUS!!! THE WORD WAS %s\n", word.c_str());
    }
    else if (status.compare(DUP) == 0)
    {
        printf("YOU ALREADY TRIED THIS LETTER!!!\n");
        move_number--;
    }
    else if (status.compare(NOK) == 0)
    {
        printf("YOU ARE WRONG!!!\n");
    }
    else if (status.compare(OVR) == 0)
    {
        printf("YOU LOST!!! YOU ARE DUMB!!!\n");
    }
    else if (status.compare(INV) == 0)
    {
        printf("MESSAGE LOST IN TRAFFIC!!!\n");
    }
    else
    {
        printf("ERROR!!!\n");
        move_number--;
    }
    return;
}

void scoreboard_aux_ok(std::string scoreboard)
{
    // split the string into two strings after the first space
    scoreboard = scoreboard.substr(scoreboard.find(' ', scoreboard.find(' ') + 1) + 1, scoreboard.length());
    // scoreboard until the first space is file name
    std::string file_name = scoreboard.substr(0, scoreboard.find(' '));
    // file_size is between the first space and the first space
    auto useful_info = scoreboard.substr(scoreboard.find(' ') + 1, scoreboard.length());

    // remove newline from file
    useful_info = useful_info.substr(useful_info.find(' ') + 1, useful_info.length());

    // create new file named file_name with file_size bytes and write useful_info into it
    std::ofstream file(file_name);
    file << useful_info;
    file.close();

    // store the file_size of file_name into length
    std::ifstream file2(file_name, std::ios::binary | std::ios::ate);
    auto file_size = file2.tellg();
    file2.close();

    std::cout << "Scoreboard was saved to " << file_name << " with " << file_size << " bytes.\n";
    std::cout << useful_info;
}

void scoreboard(const char *server_ip, const char *server_port)
{
    std::string message = "GSB\n";
    std::string response = "";

    try
    {
        response = tcp_helper(message, server_ip, server_port);
    }
    catch (std::exception &e)
    {
        std::cout << "Error: Timeout.\n";
        return;
    }

    std::string status = get_status(response);

    if (status.compare(OK) == 0)
    {
        scoreboard_aux_ok(response);
    }
    else
    {
        printf("The scoreboard is empty!\n");
    }
}

void hint_aux_ok(std::string status)
{
    // file_name is between the second space and the third space
    auto second_space = status.find(' ', status.find(' ') + 1) + 1;
    auto third_space = status.find(' ', second_space) + 1;
    auto fourth_space = status.find(' ', third_space) + 1;
    auto length = third_space - second_space;
    auto file_name = status.substr(second_space, length - 1);

    // everything after the fourth space is the useful info
    auto useful_info = status.substr(fourth_space);
    // remove newline from file
    useful_info.pop_back();

    // create a new image file with the name file_name and write useful_info into it
    std::ofstream file(file_name);
    file << useful_info;
    file.close();

    // store the file_size of file_name into length
    std::ifstream file2(file_name, std::ios::binary | std::ios::ate);
    auto file_size = file2.tellg();
    file2.close();

    std::cout << "The hint is in the file " << file_name << " and it is " << file_size << " bytes\n";
}

void hint(const char *server_ip, const char *server_port)
{
    if (player_id.empty())
    {
        std::cout << "Error: You are not playing a game.\n";
        return;
    }

    std::string message = "GHL " + player_id + "\n";
    std::string response = "";

    try
    {
        response = tcp_helper(message, server_ip, server_port);
    }
    catch (std::exception &e)
    {
        std::cout << "Error: Timeout.\n";
        return;
    }

    std::string status = get_status(response);
    if (status.compare(OK) == 0)
    {
        hint_aux_ok(response);
    }
    else
    {
        printf("ERROR!!!\n");
    }
}

std::string tcp_helper(std::string message, const char *server_ip, const char *server_port)
{
    int fd, errorcode;
    ssize_t n;
    struct addrinfo hints, *res;
    std::stringstream buffer;
    char byte[1] = {0};
    char file_data[BLOCK_SIZE];
    ssize_t digits = 0;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        close(fd);
        exit(1);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    errorcode = getaddrinfo(server_ip, server_port, &hints, &res);
    if (errorcode != 0)
    {
        freeaddrinfo(res);
        close(fd);
        exit(1);
    }

    struct timeval tv;
    tv.tv_sec = SOCKET_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv) == -1)
    {
        std::cout << "Error " << strerror(errno) << std::endl;
        exit(1);
    }

    n = connect(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1)
    {
        freeaddrinfo(res);
        close(fd);
        return NULL;
    }

    // TODO byte by byte writes
    n = write(fd, message.c_str(), message.length());
    if (n == -1)
    {
        freeaddrinfo(res);
        close(fd);
        return NULL;
    }

    // read BLOCK_SIZE blocks until \n
    while (byte[0] != '\n')
    {
        n = read(fd, byte, 1);
        if (n == -1)
        {
            freeaddrinfo(res);
            close(fd);
            return NULL;
        }
        buffer << byte[0];
    }

    auto response = buffer.str();
    // number of digits is after the third space and the next \n

    try
    {
        digits = (ssize_t)stoi(response.substr(response.find(' ', response.find(' ', response.find(' ') + 1) + 1) + 1, response.length()));
    }
    catch (std::exception &e)
    {
        freeaddrinfo(res);
        close(fd);
        return buffer.str();
    }

    // read digits bytes into file_data vector in increments of BLOCK_SIZE
    while (digits > 0)
    {
        size_t to_read = (size_t)(digits > BLOCK_SIZE - 1 ? BLOCK_SIZE - 1 : digits);
        memset(file_data, 0, BLOCK_SIZE);
        n = read(fd, file_data, to_read);

        if (n == -1)
        {
            freeaddrinfo(res);
            close(fd);
            return NULL;
        }

        if ((size_t)n < to_read)
        {
            to_read = (size_t)n;
        }

        digits -= n;
        buffer.write(file_data, (ssize_t)to_read);

        if (n == 0)
        {
            break;
        }
    }

    freeaddrinfo(res);
    close(fd);

    std::cerr << "Response: " << buffer.str() << std::endl;

    return buffer.str();
}

void status(const char *server_ip, const char *server_port)
{
    if (player_id.empty())
    {
        std::cout << "Error: You are not playing a game.\n";
        return;
    }

    std::string message = "STA " + player_id + "\n";
    std::string response = "";

    try
    {
        response = tcp_helper(message, server_ip, server_port);
    }
    catch (std::exception &e)
    {
        std::cout << "Error: Timeout.\n";
        return;
    }

    std::string status = get_status(response);
    if (status.compare("ACT") == 0)
    {
        status_aux_ok(response);
    }
    else if (status.compare("NOK") == 0)
    {
        std::cout << "No games found.\n";
    }
    else
    {
        status_aux_ok(response);
    }
}

void status_aux_ok(std::string status)
{

    // split the string into two strings after the first tab

    // file_name is between the second space and the third space
    auto second_space = status.find(' ', status.find(' ') + 1) + 1;
    auto third_space = status.find(' ', second_space) + 1;
    auto length = third_space - second_space;
    auto file_name = status.substr(second_space, length - 1);

    // status is after the fourth space
    auto fourth_space = status.find(' ', third_space) + 1;
    status = status.substr(fourth_space, status.length());

    std::cout << "Status was saved to " << file_name << " with " << length << " bytes.\n";
    std::cout << status;
    std::ofstream file(file_name);
    file << status;
    file.close();
}

void exit_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr)
{
    auto inner_id = id;
    if (player_id.empty() && id.empty())
    {
        std::cout << "You haven't started a game yet!!!\n";
        return;
    }

    if (id.empty())
    {
        inner_id = player_id;
    }

    for (auto i : id)
    {
        if (!isdigit(i))
        {
            inner_id = player_id;
            break;
        }
    }

    std::string message = "QUT " + inner_id + '\n';
    send_message(fd, message.c_str(), message.length(), res);
    std::string response;

    try
    {
        response = receive_message(fd, addr, BLOCK_SIZE);
    }
    catch (std::exception &e)
    {
        std::cout << "Error: Timeout.\n";
        return;
    }

    // remove all characters after the first \n
    response = response.substr(0, response.find('\n') + 1);

    std::string status = get_status(response);
    if (status == "OK")
    {
        printf("GOODBYE :3\n");
    }
    else if (status == "NOK")
    {
        printf("Player doesn't have an ongoing game.\n");
    }
    else
    {
        printf("The connection wasn't properly closed.\n");
    }

    return;
}
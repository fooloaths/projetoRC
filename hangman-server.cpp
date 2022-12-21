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

#include "hangman_server.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <time.h>
#include <unordered_map>
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/types.h>

// TODO don't add \n from hint to file
// TODO check if ID is already in use
// TODO if receive same play/guess twice, assume client didn't receive first reply and repeat the move (just don't store on the file)
// TODO check for unused variables

#define NUMBER_THREADS 16
#define BLOCK_SIZE 256
#define PORT "58046"
#define GAME_NAME 15
#define SEED 73
#define NUMBER_OF_WORDS 26       // TODO algumas destas constantes foram tiradas do rabo
#define WORD_LINE_SIZE 25
#define ARG_PORT "-p"


void udp_server(struct addrinfo hints, struct addrinfo *res, int fd, int errorcode, ssize_t n,
                struct sockaddr_in addr, char buffer[]);
void tcp_server(struct addrinfo hints, struct addrinfo *res, int fd, int errorcode, ssize_t n,
                struct sockaddr_in addr, char buffer[]);
int send_message(int fd, const char* message, size_t buf_size, struct sockaddr_in addr, socklen_t addrlen);
struct request* process_input(char buffer[]);
void create_game_session(std::string word, char moves, std::string PLID, std::string hint);
std::string get_random_line_from_file();
void create_active_game_file(std::string game, struct request *req);
int valid_PLID(std::string PLID);
std::string get_word(std::string line);
std::string get_hint(std::string line);
std::string max_tries(std::string word);
void report_error(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
int treat_request(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
void treat_tcp_request(int fd, struct request *req);
void treat_start(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
void treat_guess(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
void treat_quit(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
void treat_play(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
void treat_state(struct request *req, int fd);
void treat_hint(struct request *req, int fd);
std::string positions(char letter, std::string word);
void record_move_for_active_game(struct request *req);
int compare_plays(struct request *req, std::string line);
int get_move_number(struct request *req);
char* get_word_and_hint(std::string file_path);
int check_for_active_game(struct request *req);
std::string get_current_date_and_time(std::string directory);
void update_game(struct request *req);
void move_to_SCORES(struct request *req, char code);
int won_game(struct request *req);
int FindLastGame(char *PLID, char *fname);
std::string get_word_knowledge(struct request *req);
void increment_game_error(struct request *req);
void increment_game_trials(struct request *req);
void increment_game_success(struct request *req);
void decrement_game_trials(struct request *req);
std::string get_game_errors(struct request *req);
std::string get_game_trials(struct request *req);
int file_exists(std::string name);
void create_directories();
std::string compute_score(struct request *req);
size_t create_temporary_state_file(struct request *req, int game_found, const char *fname);
size_t write_to_temp_file(FILE *file, std::vector<std::string> trials, std::string word, std::string hint, struct request *req, int active_game, const char *file_name);
void treat_scoreboard(struct request *req, int fd);
std::string create_scoreboard_file();
void tcp_write(int fd, std::string message);
// // int FindTopScores(SCORELIST ∗list);

struct request {
    std::string op_code;
    std::string PLID;
    std::string letter_word;
    std::string trial;
    int error;
};

struct game {
    std::string PLID;
    std::string word;
    std::string max_errors;
    std::string move_number;
    std::string hint_file;  // TODO meter o hint file aqui durante o treat request
    std::string word_knowledge;
    int errors;
    int n_succs;
    int int_move_number;
};


std::unordered_map<std::string, struct game*> active_games;
std::string word_file = "word_eng.txt";
std::string port = PORT;
int file_line = 0;
FILE* fp_word_file;

int main(int argc, char **argv) {

    int fd, errorcode = 0;
    ssize_t n = 0;
    // socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    struct sigaction act;
    char buffer[BLOCK_SIZE];
    memset(buffer, '\0', BLOCK_SIZE);

    if (argc < 2) {
        printf("Input Error (main): Expected, at least, 1 argument (word_file), but none was given.\n");
        printf("\nTry running the program in the following format: %s <word_file> [-p GSport] [-v]\n\n", argv[0]);
        return 1;
    }
    if (argc > 2) {
        std::string arg_1 = argv[2];
        std::string arg_2 = argv[3];
        if (arg_1 == ARG_PORT) {
            port = arg_2;
        }
    }

    word_file = argv[1];

    create_directories();

    memset(&act, 0,sizeof(act));
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &act, NULL) == -1) {
        exit(1);
    }

    

    // clone while sharing cout and cerr
    int pid = fork();
    if (pid != 0) { /* Parent process */
        /* UDP server */
          fd = socket(AF_INET, SOCK_DGRAM, 0); //UDP socket
          if (fd == -1) {
              printf("Error (main): An error occured while attempting to create an UDP socket\n");
              exit(1);
          }
          fp_word_file = fopen(word_file.c_str(), "r");
          if (fp_word_file == NULL) {
              printf("Error (main): Couldn't open word file.\n");
              if (errno == EACCES) {
                  printf("    EACCES: Not enough permissions to open file\n");
              }
             /* Close socket and free resources */
              freeaddrinfo(res);
              close(fd);

             return -1;
          }
         srand(SEED); /* Set seed for random num generator */
         udp_server(hints, res, fd, errorcode, n, addr, buffer);
    }
    else {
        /* TCP server */
        fd = socket(AF_INET, SOCK_STREAM, 0); //TCP socket
        if (fd == -1) {
            printf("Error (main): An error occured while attempting to create an TCP socket\n");
            exit(1);
        }
        tcp_server(hints, res, fd, errorcode, n, addr, buffer);
    }

    

    return 0;
}


void udp_server(struct addrinfo hints, struct addrinfo *res, int fd, int errorcode, ssize_t n,
                struct sockaddr_in addr, char buffer[]) {
    
    socklen_t addrlen;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    hints.ai_flags = AI_PASSIVE;

    printf("olá\n");
    errorcode = getaddrinfo(NULL, port.c_str(), &hints, &res);
    if (errorcode != 0) {
        printf("Error (udp_server): An error occured while getting an addrinfo structure\n");
        exit(1);
    }

    n = bind(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1) {
        printf("Error (udp_server): An error occured while binding the address to the UDP socket file descriptor\n");
        exit(1);
    }

    /* Main loop */
    while (1) {
        /* Read requests from UDP socket */
        addrlen = sizeof(addr);
        printf("reeeading\n");
        n = recvfrom(fd,buffer, BLOCK_SIZE,0, (struct sockaddr*) &addr, &addrlen);
        if (n == -1) {
            printf("Error (udp_server): An error occured while trying to receive a message from the UDP socket\n");
            exit(1);
        }

        printf("aaaaaaaaa\n");
        printf("buffer = %s", buffer);
        struct request *req = process_input(buffer);
        if (req == NULL) {
            printf("Error (udp_server): Couldn't allocate memory to process request\n");
        }
        treat_request(fd, addr, addrlen, req);
        free(req);

        /* Reset buffer */
        memset(buffer, '\0', BLOCK_SIZE);
    }

    freeaddrinfo(res);
    close(fd);
}


void tcp_server(struct addrinfo hints, struct addrinfo *res, int fd, int errorcode, ssize_t n,
                struct sockaddr_in addr, char buffer[]) {
    socklen_t addrlen;
    int newfd, ret = 0;
    pid_t pid;
    char byte[1];
    memset(byte, '\0', sizeof byte);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_STREAM; //TCP socket
    hints.ai_flags = AI_PASSIVE;

    errorcode = getaddrinfo(NULL, port.c_str(), &hints, &res);
    if (errorcode != 0) {
        printf("Error (tcp_server): An error occured while getting an addrinfo structure\n");
        exit(1);
    }

    n = bind(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1) {
        printf("Error (tcp_server): An error occured while binding the address to the TCP socket file descriptor\n"); 
        exit(1);
    }

    if (listen(fd, 5) == -1) {
        printf("Error (tcp_server): An error occured while marking the socket as a passive socket for listening\n"); 
        exit(1);
    }

    std::string input_message;
    while (1) {
        addrlen = sizeof(addr);

        /* Accept connection request */
        do {
            newfd = accept(fd, (struct sockaddr*)&addr, &addrlen);
        } while (newfd == -1 && errno == EINTR);
        if (newfd == -1) {
            printf("Error (tcp_server): An error occured while accepting a connection request\n"); 
            exit(1);
        }

        /* Fork a new process to process request */
        if ((pid = fork()) == -1) {
            printf("Error (tcp_server): Fork failed\n");
        }
        else if (pid == 0) { /* Child Process */
            close(fd);

            /* Read from Socket */
            while (byte[0] != '\n') {
                // read BLOCK_SIZE blocks until \n
                n = read(newfd, byte, 1);
                if (n == -1) {
                    freeaddrinfo(res);
                    close(fd);
                    exit(1);
                }
                input_message.push_back(byte[0]);
            }
            printf("tcp_server: The message is %s", input_message.c_str());
            struct request *req = process_input(&(input_message.front()));
            if (req == NULL) {
                printf("Error (tcp_server): Couldn't allocate memory to process request\n");
            }

            /* Treat request */
            treat_tcp_request(newfd, req);
            free(req);
            close(newfd);
            exit(0);
        }
        /* Close connection */
        do {
            ret = close(newfd);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1) {
            printf("Error (tcp_server): Coudln't close file descriptor\n");
            exit(1);
        }

        /* Reset buffers */
        memset(buffer, '\0', BLOCK_SIZE);
        input_message.clear();
        memset(byte, '\0', sizeof byte);
    }

    freeaddrinfo(res);
    close(fd);
}

/* Send Message:

    Sends message through a socket */
int send_message(int fd, const char* message, size_t buf_size, struct sockaddr_in addr, socklen_t addrlen) {
    ssize_t n = sendto(fd, message, buf_size, 0, (struct sockaddr*) &addr, addrlen);

    if(n == -1) {
        /* Error */
        printf("Error (send_message): An error occured while sending a message.\n");
        printf("    error message was received from ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)\n");
        exit(1);
    }

    return 0;
}


/* Process Input:
    Receives a buffer containing what was read from the socket
    
    Parses information stored in buffer onto a struct request and returns
    a pointer to that struct */
struct request* process_input(char buffer[]) {
    printf("\nprocess_input: Starting function\n");

    printf("process_input: Allocating memory for request structure\n");
    struct request *req = (struct request *) malloc(sizeof(struct request));
    
    /* Check if memory for the request was correctly allocated */
    if (req == NULL) {
        printf("Error (process_input): Ran out of memory while allocating space for the request\n");
        return NULL;
    }

    /* Initialize struct */
    printf("process_input: Initializing struct req memory\n");
    req->error = FALSE;
    memset(&(req->op_code), 0, sizeof(req->op_code));
    memset(&(req->letter_word), 0, sizeof(req->letter_word));
    memset(&(req->trial), 0, sizeof(req->trial));
    memset(&(req->PLID), 0, sizeof(req->PLID));

    int i = 0;
    /* Retrieve op code defining the requested functionality from the server */
    printf("process_input: Starting OP CODE loop\n");
    while (buffer[i] != ' ' && i < BLOCK_SIZE && buffer[i] != '\n') {

        (req->op_code).push_back(buffer[i]);

        i++;

        if (i == BLOCK_SIZE) {
            /* Should have never gotten this big

                pelo menos alguma coisa é big UwU
            */
            req->error = TRUE;
            return req;
        }
    }
    if (req->op_code == GSB) {
        /* Nothing more to parse */
        req->letter_word = "NULL"; req->trial = "NULL"; req->PLID = "NULL";
        return req;
    }
    i++;

    /* Retrieve the player ID */
    printf("process_input: Starting PLID loop\n");
    while ((buffer[i] != ' ' && buffer[i] != '\0' && buffer[i] != '\n')  && i < BLOCK_SIZE) {
        //printf("\ni = %d , buffer[i] = %c\n", i, buffer[i]);

        (req->PLID).push_back(buffer[i]);
        i++;
    
        if (i == BLOCK_SIZE) {
            /* Should have never gotten this big */
            req->error = TRUE;
            return req;
        }
    }
    i++;

    if (valid_PLID(req->PLID) == -1) {
        /* Invalid PLID */
        req->error = TRUE;
                    // TODO maybe use req->error instead of valid_plid in other parts of code
    }

    printf("process_input: Checking op code to see if we should continue\n");
    if (req->op_code == QUT || req->op_code == REV || req->op_code == SNG || req->op_code == GSB ||
        req->op_code == STA || req->op_code == GHL) {
        /* Nothing more to parse */
        printf("process_input: No need to continue\n");
        req->letter_word = "NULL"; req->trial = "NULL";
        return req;
    }

    /* Retrieve guessed letter or word */
    while (buffer[i] != ' ' && i < BLOCK_SIZE) {
        if (!isalpha(buffer[i])) {
            /* Invalid Syntax */
            req->error = TRUE;
        }

        (req->letter_word).push_back(buffer[i]);
        i++;

        if (i == BLOCK_SIZE) {
            /* Should have never gotten this big */
            req->error = TRUE;
            return req;
        }
    }
    i++;

    /* Retrieve current number of attempts */
    while ((buffer[i] != '\n' && buffer[i] != '\0') && i < BLOCK_SIZE) {

        if (!isdigit(buffer[i])) {
            /* Invalid syntax: All requests with this many parameters end with move number */
            req->error = TRUE;
        }
        (req->trial).push_back(buffer[i]);
        i++;

        if (i == BLOCK_SIZE) {
            /* Should have never gotten this big */
            req->error = TRUE;
            return req;
        }
    }

    return req;
}

/* Valid PLID:

    Checks if a string corresponds to a valid player ID. Returns 0 if it is valid,
    otherwise returns -1
    
    A string is a valid player ID if and only if:
     - It has size 6 
     - All six of the characters are digits 
     - The first (leftmost) digit is 0 or 1 */
int valid_PLID(std::string PLID) {

    /* Check string size */
    if (PLID.length() != 6) {
        return -1;
    }

    /* Check that all characters in PLID are digits */
    size_t i = 0;
    while (PLID[i] != '\0') {

        if (PLID[i] < '0' || PLID[i] > '9') {
            return -1;
        }
        i++;
    }

    if (PLID[0] < '0' || PLID[0] > '1') {
        /* First digit must be 0 or 1 */
        return -1;
    }

    return 0;
}


/* Get Random Line From File:
    
    Picks a random line from the word file used and returns that line on a
    dynamically allocated buffer. The line contains a word and the name of
    the corresponding hint file
    
    Pseudo-randomness is implemented using rand() and computing the modulus
    by NUMBER_OF_WORDS
    
    NUMBER_OF_WORDS is the number of lines on the word file */
std::string get_random_line_from_file() {
    // int line = rand() % NUMBER_OF_WORDS;
    file_line = file_line % NUMBER_OF_WORDS;

    int line_num = 0;
    char *buffer = (char *) malloc(WORD_LINE_SIZE * sizeof(char));
    if (buffer == NULL) {
        printf("Error (get_random_line_from_file): Ran out of memory\n");
    }

    size_t buf_size = WORD_LINE_SIZE;

    /* Fetch a random line from the file */
    while (getline(&buffer, &buf_size, fp_word_file)) {
        if (line_num == file_line) {
            file_line++;
            break;
        }
        line_num++;
        memset(buffer, '\0', WORD_LINE_SIZE);
    }

    /* Reset file offset */
    fseek(fp_word_file, 0, SEEK_SET);

    std::string output = buffer;
    free(buffer);
    return output;
}


/* Treat Request:
    Receives file descriptor for UDP socket, an sockaddr_in structure and a play
    request.
    
    Checks the op_code associated with the request and calls the sub-routine
    that implements that functionality */
int treat_request(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req) {

    if (req->error == TRUE) {
        /* Something in the given request is invalid */
        report_error(fd, addr, addrlen, req);
        return 0;
    }

    if (req->op_code == SNG) {
        /* Start New Game */
        treat_start(fd, addr, addrlen, req);
    }
    else if (req->op_code == PLG) {
        /* Play Letter */
        treat_play(fd, addr, addrlen, req);
    }
    else if (req->op_code == PWG) {
        /* Guess Word */
        treat_guess(fd, addr, addrlen, req);
    }
    else if (req->op_code == QUT) {
        /* Close game session, if one exists */
        treat_quit(fd, addr, addrlen, req);
    }
    else {
        /* Invalid protocol message */
        std::string message = ERR + '\n'; // TODO see if other parts of req are invalid (Acho que já está feio ?)
        send_message(fd, message.c_str(), message.length(), addr, addrlen);
    }

    return 0;
}

/* Treat TCP Request:
    Receives file descriptor for TCP socket, an sockaddr_in structure and a play
    request.
    
    Checks the op_code associated with the request and calls the sub-routine
    that implements that functionality */
void treat_tcp_request(int fd, struct request *req) {

    if (req->error == TRUE) {
        /* Something in the given request is invalid */
        //report_error(fd, addr, addrlen, req);
        return;
    }
    if (req->op_code == STA) {
        /* Send game state */
        treat_state(req, fd);
    }
    else if (req->op_code == GHL) {
        /* Send hint */
        treat_hint(req, fd);    
    }
    else if (req->op_code == GSB) {
        /* Scoreboard */
        treat_scoreboard(req, fd);
    }
    else {
        /* Invalid protocol message */
        std::string message = ERR + '\n'; // TODO see if other parts of req are invalid (Acho que já está feio ?)
        // send_message(fd, message.c_str(), message.length(), addr, addrlen);
    }
}

std::string create_scoreboard() {
    // iterate through all the files in the SCORES folder, and store the file_name in a vector
    std::cerr << "create_scoreboard: Starting function\n";
    std::vector<std::string> scoreboard;

    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir ("SCORES")) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            if (ent->d_name[0] != '.') {
                std::string file_name = ent->d_name;
                auto score_sep = file_name.find('_');
                std::string scoreboard_line = "";

                auto score = stoi(file_name.substr(0, score_sep));

                scoreboard_line.append(file_name.substr(0, score_sep));
                scoreboard_line.push_back(' ');
                // find the string from score_sep to the next underscore
                auto pl_id = file_name.substr(score_sep + 1, file_name.find('_', score_sep + 1) - score_sep - 1);
                scoreboard_line.append(pl_id);

                // open the file and store the first line into a string named word
                std::ifstream file("SCORES/" + file_name);
                std::string word;
                std::getline(file, word);   
                word = word.substr(0, word.find(' '));
                scoreboard_line.push_back(' ');
                scoreboard_line.append(word);
                scoreboard_line.push_back(' ');

                // the nubmer of lines in the file is the number of guesses
                int guesses = 0;
                int bad_guesses = 0;
                while (std::getline(file, word)) {
                    guesses++;

                    // bad guess is the contents of the last line of the file
                    if (file.peek() == std::ifstream::traits_type::eof()) {
                        bad_guesses = stoi(word);
                    }
                }   
            
                auto good_guesses = guesses - bad_guesses;

                scoreboard_line.append("Good guesses: " + std::to_string(good_guesses));
                scoreboard_line.push_back(' ');
                scoreboard_line.append("Total trials: " + std::to_string(guesses));
                scoreboard_line.push_back('\n');

                file.close();
                // if the line alraedy exists in the vector, don't add it
                if (std::find(scoreboard.begin(), scoreboard.end(), scoreboard_line) == scoreboard.end()) {
                    scoreboard.push_back(scoreboard_line);
                }
                scoreboard_line.clear();
            }
        }
        closedir (dir);
    } else {
        /* could not open directory */
        perror ("");
        return "";
    }

    // sort the vector scoreboard based on the score which is the first number in the string in descending order
    std::sort(scoreboard.begin(), scoreboard.end(), [](std::string a, std::string b) {
        auto score_a = stoi(a.substr(0, a.find(' ')));
        auto score_b = stoi(b.substr(0, b.find(' ')));
        return score_a > score_b;
    });

    scoreboard.insert(scoreboard.begin(), "TOP 10 SCORES:\n");

    // convert scoreboard to a string
    std::string scoreboard_string = "";
    int i = 0;
    for (auto line : scoreboard) {
        if (i == 11) {
            break;
        }
        scoreboard_string.append(line);
        i++;
    }


    if (scoreboard_string == "TOP 10 SCORES:\n") {
        return "EMPTY";
    }
    

    std::cout << "create_scoreboard: scoreboard = " << scoreboard_string << std::endl;
    return scoreboard_string;
    
}

/* Treat scoreboard

    Written by the great and powerful footvaalvica, the mighty and powerful.

    This function is called when the client sends a scoreboard request.
    It will send the scoreboard to the client.
*/
void treat_scoreboard(struct request *req, int fd) {
    auto message = create_scoreboard();
    //get current unix time in seconds
    auto time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::string file_name = "SCOREBOARD_" + std::to_string(time) + ".txt ";
    std::string file_length = std::to_string(message.size() + 1) + " ";

    if (message == "") {
        message = ERR + '\n';
    }
    else if (message == "EMPTY") {
        message = "RSB EMPTY\n";
    }
    else {
        message = "RSB OK " + file_name + file_length + message + '\n';
    }

    std::cout << "treat_scoreboard: message = " << message << std::endl;

    tcp_write(fd, message);
    // ssize_t n = write(fd, message.c_str(), message.length());
    // if (n == -1) {
        
    //     /*freeaddrinfo(res);
    //     close(fd);*/
    //     exit(1);
    // }

    return;
}


void treat_state(struct request *req, int fd) {

    std::string status; 
    std::string message = RST;
    int found = check_for_active_game(req);
    char game_path[FINISHEd_GAME_PATH_SIZE]; // TODO check this constant
    memset(game_path, '\0', FINISHEd_GAME_PATH_SIZE);
    if (found == -1) {
        /* No active game */
        status = FIN;

        found = FindLastGame(&(req->PLID).front(), game_path);
        if (found == 0) {
            /* No finished games found either */
            status = NOK;

            /* Reply to client */
            message = message + " " + status + "\n";
            tcp_write(fd, message);
        }
        found = -1;
    }
    else {
        // printf("treat_state: A game was found!!!\n");
        std::string path = ACTIVE_GAME_PATH + req->PLID + TXT;
        sprintf(game_path, "%s", path.c_str());
        status = ACT;
    }

    /* Create temporary state file */
    size_t file_size = create_temporary_state_file(req, found, game_path);

    /* Send temp file to client */
    std::string file_name = STATE + req->PLID + TXT;

    message = message + " " + status + " " + file_name + " " + std::to_string(file_size) + "     ";

    std::string content;
    std::ifstream file;
    file.open(file_name.c_str());

    while (getline (file, content)) {
        content.push_back('\n');
        message = message + content;
    }
    file.close();


    /* Send reply */
    tcp_write(fd, message);

    /* Delete temp file */
    std::string delete_command = "rm ";
    delete_command = delete_command + file_name;
    system(delete_command.c_str());
}

/* Create Temporary State File:
    Receives a pointer to a struct request req and the name of the last active
    (or currently active) game file of the player who sent the request
    
    Creates a temporary file with the game data */
size_t create_temporary_state_file(struct request *req, int game_found, const char *fname) {
    FILE *game_file, *state_file;
    std::string state_file_name = STATE + req->PLID + TXT;

    /* Create file and open for writing */
    state_file = fopen(state_file_name.c_str(), "w");
    if (state_file == NULL) {
        printf("Error (create_temporary_state_file): State file couldn't be created\n");
        return 0;
    }

    /* Open last active game file for reading */
    game_file = fopen(fname, "r");
    if (game_file == NULL) {
        printf("Error (create_temporary_state_file): Couldn't open game file for reading\n");
        return 0;
    }

    /* Exctract word and hint from game file */ // TODO esta parte ainda só funciona se for um active game file
    char *word_and_hint = get_word_and_hint(fname);
    std::string word = get_word(word_and_hint);
    std::string hint = get_hint(word_and_hint);

    /* Read */
    char *line = (char *) malloc(BLOCK_SIZE * sizeof(char)); // TODO isto está feito à bruta. o malloc está big boy
    // size_t line_num = 1;
    size_t buf_size = BLOCK_SIZE;
    std::vector<std::string> trials; /* Vector with all trial lines to be appended to file */

    /* Iterate over trials and guesses */
    getline(&line, &buf_size, game_file); // Skip first line
    while (getline(&line, &buf_size, game_file) != -1) {
        std::string formatted_line;

        if (line[0] == T[0]) {
            /* If it was a letter guess */
            formatted_line = LETTER_TRIAL;
        }
        else if (line[0] == G[0]) {
            /* If it was a word guess */
            formatted_line = WORD_GUESS;
        }

        int i = 2;
        while (line[i] != '\n' && line[i] != EOF) {
            /* Append the word/letter */
            formatted_line.push_back(line[i]);
            i++;
        }
        formatted_line.push_back('\n');
        trials.push_back(formatted_line);
    }

    size_t n = write_to_temp_file(state_file, trials, word, hint, req, game_found, fname);

    fclose(game_file);
    fclose(state_file);
    free(word_and_hint);

    // TODO se for um jogo ativo, ir buscar word knowledge

    return n;
}

/* Write to Temp File:

    Returns file size */
size_t write_to_temp_file(FILE *file, std::vector<std::string> trials, std::string word,
        std::string hint, struct request *req, int game_found, const char *file_name) {

    size_t n = 0; // TODO caguei nos error checks nesta função por agora

    /* Write last game/game found for PLID */
    std::string line;
    if (game_found == -1) {
        line = LAST_GAME_MSG + req->PLID + "\n";
    }
    else {
        line = ACTIVE_GAME_FOUND + req->PLID + "\n";
    }
    n = n + fwrite(line.c_str(), sizeof(char), line.length(), file);

    /* Write word and hint (if not an active game) */
    if (game_found == -1) {
        line = "Word: " + word + "; Hint file: " + hint + '\n';
        n = n + fwrite(line.c_str(), sizeof(char), line.length(), file);
    }

    /* Write nº of transactions */
    line = TRANSACTIONS_MSG + std::to_string(trials.size()) + " ---" + '\n';
    n = n + fwrite(line.c_str(), sizeof(char), line.length(), file);

    /* Write trials/guesses */
    for (auto line_it: trials) {
        n = n + fwrite(line_it.c_str(), sizeof(char), line_it.length(), file);
    }

    // /* Write WIN/QUIT/ETC (if not an active game) */
    // if (game_found == -1) {
    //     line = TERMINATION + file_name[15]; // index 15 of a finished game is always the termination code
    //     line.push_back('\n');
    // }
    // else {
    //     line = CURRENTLY_SOLVED + get_word_knowledge(req) + "\n";
    //     printf("Write_to_temp_file: Writing current knowledge of word\n%s", line.c_str());
    // }
    // n = n + fwrite(line.c_str(), sizeof(char), line.length(), file);

    return n;
}

/* Treat Start:
    Receives file descriptor for UDP socket, an addrinfo structure and a play
    request.
    
    If the client has no active game, a new one is started. 
    Otherwise a NOK (Not OK) status message is sent back to the client */
void treat_start(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req) {

    std::string status;
    std::string message = RSG;
    std::string suffix = " ";
    message.push_back(' ');

    std::string game_path = ACTIVE_GAME_PATH + req->PLID + ".txt";

    int found = file_exists(game_path);
    if (found == -1) {
        /* No active game found for player PLID */

        status = OK;
        std::string line = get_random_line_from_file();
        std::string word = get_word(line);
        printf("word = %s\n", word.c_str());
        std::string hint = get_hint(line);
        suffix = suffix + std::to_string(word.length());
        std::string moves = max_tries(word);
        suffix.push_back(' '); suffix.push_back(moves.front());
        create_game_session(word, moves.front(), req->PLID, hint);
        create_active_game_file(line, req);
    }
    else {
        status = NOK;
    }
    message = message + status;
    if (found == -1) {
        message = message + suffix;
    }
    message = message + "\n";

    printf("treat_start: The message being sent is\n%s", message.c_str());
    send_message(fd, message.c_str(), message.length(), addr, addrlen);
}

/* Max Tries:

    Receives a word and returns the maximum number of errors that can b made 
    before losing the game. */
std::string max_tries(std::string word) {
    std::string moves;
        if (word.length() <= 6) {
            moves = '7';
        }
        else if (word.length() <= 10) {
            moves = '8';
        }
        else {
            moves = '9';
        }
    return moves;
}

/* Get First Line From Game File:
    Receives a pointer to a struct request
    
    Opens the active game for the player who sent the request (req->PLID)
    and returns the first line from the file.
    
    The first line on an active game file is of the type
    <word> <hint_file>
     */
std::string get_first_line_from_game_file(struct request *req) { // TODO check if this function is being used

    FILE *fp;

    /* Allocate memory for string containing path to active game */
    char* fname = (char *) malloc(GAME_NAME * sizeof(char));
    if (fname == NULL) {
        printf("Error (get_first_line_from_game_file): Ran out of memory\n");
    }

    /* Store path to file on fname */
    sprintf(fname, "GAMES/GAME_%s.txt", (req->PLID).c_str());
    
    /* Allocate memory for string that will store first line from file */
    char *buffer = (char *) malloc(WORD_LINE_SIZE * sizeof(char));
    if (buffer == NULL) {
        printf("Error (get_first_line_from_game_file): Ran out of memory\n");
    }
    size_t buf_size = WORD_LINE_SIZE;

    /* Open game file */
    fp = fopen(fname, "r");

    /* Get first line from file */
    getline(&buffer, &buf_size, fp);
    
    // std::string output = buffer;
    // free(buffer);
    // free(fname);

    return buffer; // TODO dar free algures
}

/* Get Word:
    Receives a string and returns the first word (all characters up to the
    first space) */
std::string get_word(std::string line) {
    size_t i = 0;

    std::string output;
    while (line[i] != ' ') {
        output.push_back(line[i]);
        i++;
    }
    return output;
}

/* Get Hint:
    Receives a string and returns the second word */
std::string get_hint(std::string line) {
    size_t i = 0;

    std::string output;
    /* Skip word */
    while (line[i] != ' ') {i++;}
    i++;

    while (line[i] != '\0' && line[i] != '\n') {
        output.push_back(line[i]);
        i++;
    }

    return output;
}

/* Treat Guess:
    Receives file descriptor for UDP socket, an addrinfo structure and a play
    request.
    
    If the client has an active game and a valid play request was given, 
    checks if the guessed word matches the game word, updates the game 
    accordingly and sends a reply:

      - If player won: Close server side state of active game and move game
      file to SCORES directory 
      
      - If play was unsuccessful with no more attempts:
      End server side active game struct and move game file to corresponding
      directories 
      
      - If play was unsuccessful with some attempts remaining:
        Update game file */
void treat_guess(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req) {
    std::string message = RWG;
    message.push_back(' ');

    /* Look for active games with req->PLID */
    if (valid_PLID(req->PLID) == -1 || check_for_active_game(req) == -1) {
        /* Send error message */
        printf("Reporting error???\n");
        report_error(fd, addr, addrlen, req);
        return;
    }

    /* Compare number of moves to req->trials */
    std::string move_number = get_game_trials(req);
    std::string moves = " " + move_number + "\n";
    if (req->trial != move_number) {
        /* Send message to client saying something went wrong */
        // TODO if duplicate guess, also reply INV
        printf("treat_guess: req->trial = %s , move_number = %s\n", (req->trial).c_str(), move_number.c_str());
        printf("treat_guess: Going to send INV\n");
        message = message + INV + moves;
        printf("Treat_guess: Sending the following\n%s", message.c_str());
        send_message(fd, message.c_str(), message.length(), addr, addrlen);
        return;
    }

    /* Compare res->word to the word */
    std::string file_path = ACTIVE_GAME_PATH + req->PLID + ".txt";
    char *line = get_word_and_hint(file_path);
    std::string word = get_word(line);
    free(line);

    /* Write play to game file */
    record_move_for_active_game(req);
    increment_game_trials(req);

    if (word == req->letter_word) { // Won the game
        /* Prepare reply for client */
        message = message + WIN + moves;
        move_to_SCORES(req, W);
    }
    else {
        increment_game_error(req);
        if (std::stoi(get_game_errors(req)) > std::stoi(max_tries(word))) { // No more errors
                /* Prepare reply for client */
                message = message + OVR + moves;
                move_to_SCORES(req, F);
            }
        else { // Incorrect guess, but with remaining attempts
            /* Prepare reply for client */
            message = message + NOK + moves;
        }
    }

    /* Send reply to client */
    printf("treat_guess: Sending reply\n%s\n", message.c_str());
    send_message(fd, message.c_str(), message.length(), addr, addrlen);

}

int FindLastGame(char *PLID, char *fname) {
    struct dirent **filelist;
    int nentries, found;
    char dirname[20];
    
    sprintf(dirname, "GAMES/%s/" , PLID);

    nentries = scandir(dirname, &filelist, 0, alphasort);
    found = 0;
    if (nentries <= 0) {
        return (0);
    }
    else {
        while (nentries--) {
            if (filelist[nentries]->d_name[0] != '.') {
                sprintf(fname, "GAMES/%s/%s", PLID, filelist[nentries]->d_name);
                found = 1;
            }
            free(filelist[nentries]);
            if (found) {
                break;
            }
        }
        free(filelist);
    }

    return (found);
}


/*int FindTopScores(SCORELIST ∗list) {
    struct dirent **filelist;
    int nentries, ifile;
    char fname[50];
    FILE *fp;

    nentries = scandir("SCORES/", &filelist, 0, alphasort);
    ifile = 0;
    if (nentries < 0) {
        return (0);
    }
    else {
        while (nentries--) {
            if (filelist[nentries]->d_name[0] != ".") {
                sprintf(fname, "SCORES/%s", filelist[nentries]->d_name);
                fp = fopen(fname, "r");
                if (fp != NULL) {
                    fscanf(fp, "%d %s %s %d %d" ,
                    &list->score[ifile], list->PLID[ifile], list->word[ifile], &list->nsucc[ifile],
                    &list->ntot[ifile]);

                    fclose(fp);
                    ++ifile;
                }
            }
            free(filelist[nentries]);
            if (ifile == 10) {
                break;
            }
        }
        free(filelist);
    }
    list->nscores = ifile;
    return (ifile);
}*/

/* Create Active Game File: 
    Receives the first line "game" for a new game file and a pointer to a struct request 
    
    First line: <word> <hint_file> 
    
    Creates and opens a new file with "game" as it's first line */
void create_active_game_file(std::string game, struct request *req) {
    std::string file_path = ACTIVE_GAME_PATH + req->PLID + ".txt";

    /* Create and open file */
    std::ofstream file(file_path);

    /* Write first line to file */
    file << game;

    /* Close file */
    file.close();
}

/* Returns 0 if file exists, -1 otherwise */
int check_for_active_game(struct request *req) {
    std::string filepath = ACTIVE_GAME_PATH + req->PLID + ".txt";

    /* Check if file exists */
    if (access(filepath.c_str(), F_OK) == 0) {
        /* It exists */
        return 0;
    }
    else {
        /* It doesn't exist. Very sad */
        return -1;
    }
}

/* Returns 0 if dir is a valid directory, -1 otherwise */
int is_directory(std::string dir) {

    if (dir.length() < 1) {
        /* Invalid Input */
        printf("Error (is_directory): Received empty string, instead of a directory\n");
        return -1;
    }

    DIR *directory = opendir(dir.c_str());

    if (errno == ENOENT || errno == ENOTDIR) {
        /* No such directory */
        return -1;
    }
    else if (errno == EACCES) {
        printf("Error (is_directory): Directory exists, but permission to open was not granted\n");
        return -1;
    }
    else if (directory != NULL) {
        /* Directory exists */
        closedir(directory);

        return 0;
    }
    else {
        printf("Error (is_directory): Something went wrong while opening the directory\n");
        return -1;
    }

}

/* Record Move For Active Game:
    Receives a pointer to a struct request and appends a line describing the 
    requested play to an active game file
    
    Besides the first line, all other lines are of the type
    code play
    
    With code being either T (if the player sent a Play request) or G (if the
    player sent a Guess request) and play being the letter or word guessed */
void record_move_for_active_game(struct request *req) {
    std::ofstream file;

    /* Open file */
    std::string file_path = ACTIVE_GAME_PATH + req->PLID + ".txt";
    file.open(file_path, std::ios_base::app);

    /* Check for errors */
    if (file.fail()) {
        printf("Error (record_move_for_active_game): Failed to open game file.\n");
    }

    /* Append new line */
    std::string line;
    if (req->op_code == "PLG") {
        line = T;
    }
    else {
        line = G;
    }
    line = line + " " + req->letter_word + "\n";
    file << line;

    /* Close file */
    file.close();
}

/* Get Move Number:
    
    Opens an active game file for player req->PLID and counts the number of 
    moves made (plays and guesses). The number of moves corresponds to the
    number of lines in the file, excluding the first line */
int get_move_number(struct request *req) {
    std::string file_path = ACTIVE_GAME_PATH + req->PLID + ".txt";

    /* Open file for reading */
    FILE *file;
    file = fopen(file_path.c_str(), "r");
    if (file == NULL) {
        printf("Error (get_move_number): Failed to open file.\n");
        if (errno == EACCES) {
            printf("    EACCES: Not enough permissions to open file\n");
        }
        else if (errno == EFAULT) {
            printf("    EFAULT: Outside accessible adress space\n");
        }
        else if (errno == ENOENT) {
            printf("    ENOENT: A directory component in pathname does not exist or is a dangling symbolic link.\n");
        }
        return -1;
    }


    /* Read */
    char *line = (char *) malloc(LINE_SIZE * sizeof(char));
    int line_num = 1;
    size_t buf_size = LINE_SIZE;

    getline(&line, &buf_size, file); // Skip first line
    while (getline(&line, &buf_size, file) != -1) {
        line_num++;
    }

    fclose(file);
    free(line);

    return line_num;
}

/* Returns 0 for duplicate moves and -1 otherwise */
int check_for_duplicate_move(struct request *req) {
    std::string file_path = ACTIVE_GAME_PATH + req->PLID + ".txt";

    /* Open file for reading */
    FILE *file;

    file = fopen(file_path.c_str(), "r");
    if (file == NULL) {
        printf("Error (check_for_duplicate_move): Couldn't open file.\n");
        if (errno == EACCES) {
            printf("    EACCES: Not enough permissions to open file\n");
        }
        else if (errno == EFAULT) {
            printf("    EFAULT: Outside accessible adress space\n");
        }
        else if (errno == ENOENT) {
            printf("    ENOENT: A directory component in pathname does not exist or is a dangling symbolic link.\n");
        }
        return -1;
    }

    /* Read */
    char *line = (char *) malloc(LINE_SIZE * sizeof(char));
    memset(line, '\0', LINE_SIZE);
    size_t buf_size = LINE_SIZE;
    while (getline(&line, &buf_size, file) != -1) {
        if (compare_plays(req, line) == 0) {
            return 0;
        }
        memset(line, '\0', LINE_SIZE);
    }

    fclose(file);
    free(line);

    return -1;
}

/* Returns 0 for identical plays and -1 for different plays */
int compare_plays(struct request *req, std::string line) {
    
    if ((req->op_code == "PLG" && line.front() == 'G') || 
        (req->op_code == "PWG" && line.front() == 'T')) {
            /* If type of move is different */
            return -1;
    }
    
    size_t i = 0;
    while (line[i] != ' ') {i++;} // Skip code
    i++; // Skip space

    std::string word;
    while (line[i] != '\0' && line[i] != '\n') {
        word.push_back(line[i]);
        i++;
    }

    if (word == req->letter_word) {
        return 0;
    }
    
    return -1;
}


/* Get word and hint:
    Receives a pointer to a struct request
    
    Opens an active game file for that player and reads the first line (the
    line containing the word to be guessed and the name of the hint file).
    Returns that line */
char* get_word_and_hint(std::string file_path) {

    /* Open file for reading */
    FILE *file;

    file = fopen(file_path.c_str(), "r");
    if (file == NULL) {
        printf("Error (get_word_and_hint): Couldn't open file.\n");
        if (errno == EACCES) {
            printf("    EACCES: Not enough permissions to open file\n");
        }
        else if (errno == EFAULT) {
            printf("    EFAULT: Outside accessible adress space\n");
        }
        else if (errno == ENOENT) {
            printf("    ENOENT: A directory component in pathname does not exist or is a dangling symbolic link.\n");
        }
        return NULL;
    }

    /* Read */
    char *line = (char *) malloc(LINE_SIZE * sizeof(char));
    memset(line, '\0', LINE_SIZE);
    size_t buf_size = LINE_SIZE;
    getline(&line, &buf_size, file);

    return line; // TODO lembrar de dar free algures
}

/* Move to Scores:

    Renames active game file for player req->PLID to the format
    YYYYMMDD_HHMMSS_code.txt where code is either W (win), F (Fail) or
    Q (Quit) and moves it to the SCORES/ directory 
    
    Additionally, removes the pointer to the struct game corresponding to req->PLID
    from the unordered map "active_games" and frees the memory allocated for that
    pointer to struct game */
void move_to_SCORES(struct request *req, char code) {
    /* Code should be Q, F or W */
    int err = 0;

    /* Get game struct */
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    if (it == active_games.end()) {
        /* Wasn't found */
        return;
    }
    struct game *g = it->second;

    /* Check if a directory exists for req->PLID's completed games, otherwise
        create it */
    std::string completed_games = GAMES_DIR + req->PLID; // TODO acabar
    DIR *directory = opendir(completed_games.c_str());

    if (directory == NULL) {
        /* Doesn't Exist. Create it */
        if (mkdir(completed_games.c_str(), 0777) == -1) {
            /* Failed to create directory */
            printf("Error (move_to_SCORES): Failed to create directory");
            std::cerr << "Error :  " << strerror(errno) << std::endl;
        }
    }
    else {
        closedir(directory);
    }

    /* Copy game file's content to GAMES/PLID/ */

    std::string file_name = completed_games + "/" + get_current_date_and_time(GAMES_DIR) + "_" + code + ".txt";
    std::string current_path = ACTIVE_GAME_PATH + req->PLID + ".txt";
    std::string shell_command = COPY + current_path + " " + file_name;
    err = system(shell_command.c_str());
    if (err == -1) {
        exit(1);
    }

    /* Move game file to SCORES/ */
    file_name = SCORES + compute_score(req) + "_" + req->PLID + "_" + get_current_date_and_time(SCORES) + ".txt";
    shell_command = MOVE + current_path + " " + file_name;
    err = system(shell_command.c_str());
    if (err == -1) {
        exit(1);
    }

    /* Add number of sucessful moves at the end of the first line of the score file */
    FILE *fd = fopen(file_name.c_str(), "a+");
    char c = '\0';
    err = fseek(fd, 0, SEEK_SET);
    if (err == -1) {
        exit(1);
    }
    while ((c = fgetc(fd)) != '\n') {;} // Skip to end of first line

    std::string num_errs = get_game_errors(req) + "\n";
    fwrite(num_errs.c_str(), sizeof(char), num_errs.length(), fd);
    fclose(fd);

    /* Remove game from unordered map and free memory */
    free(g);
    active_games.erase(req->PLID); // Remove game from hash table
}


/* Treat Quit:
    Receives file descriptor for UDP socket, an addrinfo structure and a play
    request.
    
    If the client has an active game and a valid play request was given, 
    any server side information regarding the active game is freed and 
    the game file is moved */
void treat_quit(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req) {
    printf("aaasdfasfasdasd\n");
    std::string message = RQT;
    message.push_back(' ');
    std::string status;
    if (valid_PLID(req->PLID) == -1 || req->error == TRUE) {
        /* No active game or invalid PLID */
        // move_to_SCORES(req, Q);
        status = ERR;
    }
    else if (check_for_active_game(req) == -1) {
        status = NOK;
    }
    else {
        /* Close game and move file to SCORES */
        printf("Game found\n");
        move_to_SCORES(req, Q);
        status = OK;
    }

    /* Complete reply message */
    message = message + status + "\n";

    /* Send reply through UDP socket */
    send_message(fd, message.c_str(), message.length(), addr, addrlen);
}

/* Get Current Date and Time:

    If directory == GAMES: returns current date and time in the format
    YYYYMMDD_HHMMSS 
    
    If directory == SCORES: returns current date and time in the format
    DDMMYYYY_HHMMSS */
std::string get_current_date_and_time(std::string directory) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    std::string output;
    if (directory == SCORES) {
        output = std::to_string(tm.tm_year) + std::to_string(tm.tm_mon) + std::to_string(tm.tm_mday) + "_";
    }
    else {
        output = std::to_string(tm.tm_mday) + std::to_string(tm.tm_mon) + std::to_string(tm.tm_year) + "_";
    }

    output = output + std::to_string(tm.tm_hour) + std::to_string(tm.tm_min) + std::to_string(tm.tm_sec);

    return output;
}


/* Treat Play:
    Receives file descriptor for UDP socket, an addrinfo structure and a play
    request.
    
    If the client has an active game and a valid play request was given, 
    performs the given play,updates the game accordingly and sends a reply:

      - If player won: Close server side state of active game and move game
      file to SCORES directory 
      
      - If play was successful: Update game file 
      
      - If play was unsuccessful with no more attempts:
      End server side active game struct and move game file to corresponding
      directories 
      
      - If play was unsuccessful with some attempts remaining:
        Update game file */
void treat_play(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req) {
    std::string message = RLG;
    message.push_back(' ');

    /* Look for active games with req->PLID */
    if (valid_PLID(req->PLID) == -1 || check_for_active_game(req) == -1) {
        /* Send error message */
        report_error(fd, addr, addrlen, req);

        return;
    }

    /* Compare number of moves to req->trials */
    std::string move_number = get_game_trials(req);
    std::string moves = " " + move_number + "\n";
    printf("O req->trial = %s e o move_number = %s\n", (req->trial).c_str(), move_number.c_str());
    if (req->trial != move_number) {
        /* Send message to client saying something went wrong */
        message = message + INV + moves;
        send_message(fd, message.c_str(), message.length(), addr, addrlen);
        return;

        // TODO analisar se aqui é para registar no ficheiro / incrementar erros
    }
    if (check_for_duplicate_move(req) == 0) {
        /* If this move had already been played */
        message = message + DUP + moves;
        send_message(fd, message.c_str(), message.length(), addr, addrlen);
        return;
    }

    moves.pop_back(); /* Remove \n */

    /* Compare res->word to the word */
    std::string file_path = ACTIVE_GAME_PATH + req->PLID + ".txt";
    char *line = get_word_and_hint(file_path);
    std::string word = get_word(line);
    free(line);

    /* Write to play to file */
    record_move_for_active_game(req);

    // TODO falta o DUP!!!!

    if (!(positions((req->letter_word).front(), word).empty())) { // Some positions were found
        /* Prepare reply for client and update game file */
        update_game(req);
        increment_game_success(req);
        if (won_game(req)) {
            /* Won the game */
            increment_game_trials(req);
            message = message + WIN + moves + "\n";
            move_to_SCORES(req, W);

            // TODO falta mudar para completed GAMES subdir
        }
        else {
            /* Simply guessed a correct letter*/
            message = message + OK + moves + " ";
            message = message + positions((req->letter_word).front(), word) + "\n";
        }
    }
    else { /* Wrong guess */
        increment_game_error(req);
        increment_game_trials(req);
        if (std::stoi(get_game_errors(req)) > std::stoi(max_tries(word))) { // No more errors
            /* Prepare reply for client */
            message = message + OVR + moves + "\n";
            move_to_SCORES(req, F);
        }
        else { // Incorrect guess, but with remaining attempts
            /* Prepare reply for client */
            message = message + NOK + moves + "\n";
        }
    }
    printf("treat_play: The message being sent is\n%s\n", message.c_str());
    send_message(fd, message.c_str(), message.length(), addr, addrlen);

}

/* Positions:
    Returns a string with the number of occurences of "letter" in "word",
    followed by the numerical positions in "word" where the character "letter" 
    appears */
std::string positions(char letter, std::string word) {

    std::string output;
    int pos = 1;
    size_t length = word.length();
    int number_occ = 0;
    for (size_t i = 0; i < length; i++) {
        if (word[i] == letter) {
            output = output + std::to_string(pos) + " ";
            number_occ++;
        }
        pos++;
    }
    if (number_occ == 0) {
        return output;
    }

    /* Remove last space */
    output.pop_back();

    /* Add number of matches to the start of the string */
    output = std::to_string(number_occ) + " " + output;

    //printf("O vetor positions é (%s)\n", output.c_str());

    return output;
}


/* Create Game Session:
    Creates and populates a new struct game, adds it to the
    active_games unordered_map (hash map) and returns a pointer to it */
void create_game_session(std::string word, char moves, std::string PLID, std::string hint) {
    struct game *new_game = (struct game *) malloc(sizeof(struct game));
    if (new_game == NULL) {
        printf("Error (create_game_session): Ran out of memory\n");
    }

    /* Initialize game struct to stop Valgrind's cries of despair */
    memset(&(new_game->word), 0, sizeof(new_game->word));
    memset(&(new_game->PLID), 0, sizeof(new_game->PLID));
    memset(&(new_game->max_errors), 0, sizeof(new_game->max_errors));
    memset(&(new_game->move_number), 0, sizeof(new_game->move_number));
    memset(&(new_game->hint_file), 0, sizeof(new_game->hint_file));
    memset(&(new_game->word_knowledge), 0, sizeof(new_game->word_knowledge));


    /* Populate it */
    new_game->word = word;
    new_game->PLID = PLID;
    // new_game->max_errors = moves;
    new_game->max_errors = moves;
    new_game->n_succs = 0;
    new_game->errors = 0;
    new_game->int_move_number = 1;
    new_game->move_number = "1";
    new_game->hint_file = hint;
    
    for (auto it = new_game->word.begin(); it != new_game->word.end(); it++) {
        new_game->word_knowledge.push_back('_');
    }
    
    /* Add it to active games hashmap */
    active_games.insert({PLID, new_game}); // TODO lembrar de dar pop quando o jogo acaba
}

/* Update Game:
    Updates a player's corresponding active game struct */
void update_game(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    /* Update move number */
    increment_game_trials(req);

    size_t i = 0;
    for (auto c: g->word) {
        /* Iterate over word. If char i in word matches the guessed letter,
            update current knowledge of the word */
        if (c == req->letter_word.front()) {
            g->word_knowledge[i] = req->letter_word.front();
        }
        i++;
    }
}


/* Increment Game Error 

    Increments the attribute errors for the struct game indexed by req->PLID */
void increment_game_error(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    g->errors++;
}

/* Get Game Errors 

    Getter function for the "errors" attribute of the struct game indexed by
    req->PLID  */
std::string get_game_errors(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    return std::to_string(g->errors);
}

/* Get Game Trials 

    Getter function for the "move_number" attribute of the struct game indexed by
    req->PLID  */
std::string get_game_trials(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    return g->move_number;
}


/* Get Word Knowledge 

    Getter function for the "word_knowledge" attribute of the struct game indexed by
    req->PLID  */
std::string get_word_knowledge(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    return g->word_knowledge;
}


/* Increment game trials 

    Increment the attribute move_number for the struct game indexed by req->PLID */
void increment_game_trials(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    /* Update move number */
    int moves = std::stoi(req->trial) + 1;
    if (req->trial.length() < (std::to_string(moves)).length()) {
        g->move_number.resize((std::to_string(moves)).length());
    }
    g->move_number = std::to_string(moves);
    g->int_move_number++;
}


/* Decrement game trials 

    Decrement the attribute move_number for the struct game indexed by req->PLID */
void decrement_game_trials(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    /* Update move number */
    int moves = std::stoi(req->trial) - 1;
    g->move_number = std::to_string(moves);
    g->int_move_number--;
}

/* Won Game:
    Checks if the player (req->PLID) has won the game */
int won_game(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;


    return g->word == g->word_knowledge;
}

/* Report Error:
    Sends an error message to the client

    If the struct request pointed to by req has PWG or RLG as an op_code, the
    reply format is:
        op_code ERR\n
    
    Otherwise, it is:
        ERR\n*/
void report_error(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req) {
    std::string message;

    if (req->op_code == PWG || req->op_code == PLG) {
        /* Invalid guess or play request */
        message = req->op_code + " ";
    }
    
    std::string error = ERR;
    error.push_back('\n');
    if (message.empty()) {
        /* Not one of the aforementioned requests */
        message = error;
    }
    else {
        message = message + error;
    }

    send_message(fd, message.c_str(), message.length(), addr, addrlen);
}

/* File exists:

    Returns 0 if file exists, otherwise returns -1*/
int file_exists(std::string name) {
    if (FILE *file = fopen(name.c_str(), "r")) {
        fclose(file);
        return 0;
    }
    else {
        return -1;
    }   
}

/* Treat Hint:
    
    If there is no active game for req->PLID or there is something wrong with
    the request, replies RHL NOK
    
    Otherwise, sends the contents of the hint file, with the the reply message
    formatted as 
    RHL status [Fname Fsize Fdata]*/
void treat_hint(struct request *req, int fd) {
    std::string message = RHL;
    message.push_back(' ');
    // printf("treat_hint: Starting function. The message is currently\n%s\n", message.c_str());

    /* Look for active games with req->PLID */
    if (valid_PLID(req->PLID) == -1 || check_for_active_game(req) == -1) {
        /* Send NOK message */
        
        message = message + NOK + "\n";
        // printf("treat_hint: Sending message: %s", message.c_str());
        tcp_write(fd, message);
        // ssize_t n = write(fd, message.c_str(), message.length());
        // if (n == -1) {
        //     exit(1);
        // }
        return;
    }
    message = message + OK + " ";
    // printf("treat_hint: Game found. The message is currently\n%s\n", message.c_str());

    /* Get name of hint file */
    std::string game_path = ACTIVE_GAME_PATH + req->PLID + TXT;
    // std::string line = get_word_and_hint(game_path);
    char *line = get_word_and_hint(game_path);
    std::string hint = get_hint(line);
    // printf("treat_hint: The hint is\n%s\n", hint.c_str());

    message = message + hint + " ";
    // printf("treat_hint: The message is now\n%s\n", message.c_str());

    /* Open hint file */
    std::string hint_path = PICTURES + hint;
    // printf("treat_hint: Opening file (read binary) with path\n%s\n", hint_path.c_str());
    FILE *file = fopen(hint_path.c_str(), "rb");

    /* Get file size */
    fseek(file, 0, SEEK_END); // Place offset at the end of file
    ssize_t size = ftell(file); // Get offset value
    fseek(file, 0, SEEK_SET); // Set offset back to the start of file
    // printf("treat_hint: File size is %ld bytes\n", size);

    message = message + std::to_string(size) + " ";
    // printf("treat_hint: The message is now\n%s\n", message.c_str());
    fclose(file);
    
    /* Read file content */
    std::ifstream hint_file(hint_path);
    std::string content;


    while (getline (hint_file, content)) {
        content.push_back('\n');
        message = message + content;
    }
    hint_file.close();


    message = message + "\n";
    // printf("treat_hint: Message being sent is\n%s", message.c_str());
    /* Send reply */
    tcp_write(fd, message);
    // ssize_t n = write(fd, message.c_str(), message.length());
    // if (n == -1) {
    //     exit(1);
    // }
    free(line);
}

/* Increment game success 

    Increment the attribute n_succs for the struct game indexed by req->PLID */
void increment_game_success(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    /* Update move number */
    g->n_succs++;
}


std::string compute_score(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    /* Update move number */
    int score = (float) (g->n_succs * 100 / g->int_move_number);

    return std::to_string(score);
}

void create_directories() {
    std::string games = GAMES_DIR;
    if (is_directory(GAMES_DIR) == -1) {
        mkdir(GAMES_DIR, 0777);
    }
    if (is_directory(SCORES) == -1) {
        mkdir(SCORES, 0777);
    }
    if (is_directory(PICTURES) == -1) {
        mkdir(PICTURES, 0777);
    }
}

void tcp_write(int fd, std::string message) {
    ssize_t bytes = message.length();
    ssize_t n;
    const char *ptr = message.c_str();

    printf("tcp_write: We want to send message = %s", ptr);
    while (bytes > 0) {
        printf("\nyo\n");
        n = write(fd, ptr, message.length());
        if (n <= 0) {
            printf("Error (tcp_write): Something went wrong writing to the file descriptor\n");
            exit(1);
        }
        bytes = bytes - n;
        ptr = ptr + n;
    }
}

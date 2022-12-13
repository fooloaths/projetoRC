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
#define PORT "58011"
#define GAME_NAME 15
#define SEED 73
#define NUMBER_OF_WORDS 6       // TODO algumas destas constantes foram tiradas do rabo
#define WORD_LINE_SIZE 25


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
void treat_start(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
void treat_guess(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
void treat_quit(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
void treat_play(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req);
std::string positions(char letter, std::string word);
void record_move_for_active_game(struct request *req);
int compare_plays(struct request *req, std::string line);
int get_move_number(struct request *req);
char* get_word_and_hint(struct request *req);
int check_for_active_game(struct request *req);
std::string get_current_date_and_time(std::string directory);
void update_game(struct request *req);
void move_to_SCORES(struct request *req, char code);
int won_game(struct request *req);
int FindLastGame(char *PLID, char *fname);
void increment_game_error(struct request *req);
void increment_game_trials(struct request *req);
void decrement_game_trials(struct request *req);
std::string get_game_errors(struct request *req);
std::string get_game_trials(struct request *req);
int file_exists(std::string name);
/*int FindTopScores(SCORELIST ∗list)*/

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
};


std::unordered_map<std::string, struct game*> active_games;
std::string word_file = "word_eng.txt";
FILE* fp_word_file;


int main(int argc, char **argv) {

    int fd, errorcode;
    ssize_t n;
    socklen_t addrlen;
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

    word_file = argv[1];


    memset(&act, 0,sizeof(act));
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &act, NULL) == -1) {
        exit(1);
    }

    int pid = fork();
    if (pid != 0) { /* Parent process */
        /* UDP server */
        fd = socket(AF_INET,SOCK_DGRAM,0); //UDP socket
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
    }
    else {
        /* TCP server */
        exit(1); // TODO exit while it hasn't been implemented
    }


    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    hints.ai_flags = AI_PASSIVE;

    errorcode = getaddrinfo(NULL,PORT,&hints,&res);
    if (errorcode != 0) {
        printf("Error (main): An error occured while getting an addrinfo structure\n");
        exit(1);
    }

    n = bind(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1) {
        printf("Error (main): An error occured while binding the address to the UDP socket file descriptor\n");
        exit(1);
    }

    /* Main loop */
    while (1) {
        /* Read requests from UDP socket */
        addrlen = sizeof(addr);
        n = recvfrom(fd,buffer, BLOCK_SIZE,0, (struct sockaddr*) &addr, &addrlen);
        if (n == -1) {
            printf("Error (main): An error occured while trying to receive a message from the UDP socket\n");
            exit(1);
        }

        struct request *req = process_input(buffer);
        if (req == NULL) {
            printf("Error (main): Couldn't allocate memory to process request\n");
        }
        treat_request(fd, addr, addrlen, req);
        free(req);

        /* Reset buffer */
        memset(buffer, '\0', BLOCK_SIZE);

        /*n = sendto(fd,buffer,n,0, (struct sockaddr*)&addr,addrlen);
        if (n == -1) {
            printf("Error (main): An error occured while sending a reply\n");
            exit(1);
        }*/
    }

    freeaddrinfo(res);
    close(fd);

    return 0;
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

    struct request *req = (struct request *) malloc(sizeof(struct request));
    
    /* Check if memory for the request was correctly allocated */
    if (req == NULL) {
        printf("Error (process_input): Ran out of memory while allocating space for the request\n");
        return NULL;
    }

    /* Initialize struct */
    req->error = FALSE;
    memset(&(req->op_code), 0, sizeof(req->op_code));
    memset(&(req->letter_word), 0, sizeof(req->letter_word));
    memset(&(req->trial), 0, sizeof(req->trial));
    memset(&(req->PLID), 0, sizeof(req->PLID));

    int i = 0;
    /* Retrieve op code defining the requested functionality from the server */
    while (buffer[i] != ' ' && i < BLOCK_SIZE) {

        (req->op_code).push_back(buffer[i]);

        i++;

        if (i == BLOCK_SIZE) {
            /* Should have never gotten this big */
            req->error = TRUE;
            return req;
        }
    }
    i++;

    /* Retrieve the player ID */

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

    if (req->op_code == QUT || req->op_code == REV || req->op_code == SNG) {
        /* Nothing more to parse */
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
    int i = 0;
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
    int line = rand() % NUMBER_OF_WORDS;

    int line_num = 0;
    char *buffer = (char *) malloc(WORD_LINE_SIZE * sizeof(char));
    if (buffer == NULL) {
        printf("Error (get_random_line_from_file): Ran out of memory\n");
    }

    size_t buf_size = WORD_LINE_SIZE;

    /* Fetch a random line from the file */
    while (getline(&buffer, &buf_size, fp_word_file)) {
        if (line_num == line) {
            break;
        }
        line_num++;
        memset(buffer, '\0', WORD_LINE_SIZE);
    }
    return buffer;
}


/* Treat Request:
    Receives file descriptor for UDP socket, an addrinfo structure and a play
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

/* Treat Start:
    Receives file descriptor for UDP socket, an addrinfo structure and a play
    request.
    
    If the client has not active game, a new one is started. 
    Otherwise a NOK (Not OK) status message is send back to the client */
void treat_start(int fd, struct sockaddr_in addr, socklen_t addrlen, struct request *req) {

    //char* fname = (char *) malloc(GAME_NAME * sizeof(char));
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

    return buffer; // TODO dar free algures
}

/* Get Word:
    Receives a string and returns the first word (all characters up to the
    first space) */
std::string get_word(std::string line) {
    int i = 0;

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
    int i = 0;

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
        printf("treat_guess: req->trial = %s , move_number = %s\n", req->trial, move_number);
        printf("treat_guess: Going to send INV\n");
        message = message + INV + moves;
        printf("Treat_guess: Sending the following\n%s", message.c_str());
        send_message(fd, message.c_str(), message.length(), addr, addrlen);
        return;
    }

    /* Compare res->word to the word */
    char *line = get_word_and_hint(req);
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
        if (get_game_errors(req) == max_tries(word)) { // No more errors
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
    
    int i = 0;
    while (line[i] != ' ') {i++;} // Skip code
    i++; // Skip space

    std::string word;
    while (line[i] != '\0' || line[i] != '\n') {
        word.push_back(line[i]);
        i++;
    }

    if (word == req->letter_word) {
        return 0;
    }
    
    return -1;
}

char* get_word_and_hint(struct request *req) {
    std::string file_path = ACTIVE_GAME_PATH + req->PLID + ".txt";

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
    system(shell_command.c_str());

    /* Move game file to SCORES/ */
    file_name = SCORES + g->move_number + "_" + req->PLID + "_" + get_current_date_and_time(SCORES) + ".txt";
    shell_command = MOVE + current_path + " " + file_name;
    system(shell_command.c_str());


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

    std::string message = RQT;
    message.push_back(' ');
    std::string status;
    if (valid_PLID(req->PLID) == -1 || check_for_active_game(req) == -1) {
        /* No active game or invalid PLID */
        status = ERR;
    }
    else {
        /* Close game and move file to SCORES */
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
    std::string message = RWG;
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
    if (req->trial != move_number) {
        /* Send message to client saying something went wrong */
        message = message + INV + moves;
        send_message(fd, message.c_str(), message.length(), addr, addrlen);
        return;

        // TODO analisar se aqui é para registar no ficheiro / incrementar erros
    }
    moves.pop_back(); /* Remove \n */

    /* Compare res->word to the word */
    char *line = get_word_and_hint(req);
    std::string word = get_word(line);
    free(line);

    /* Write to play to file */
    record_move_for_active_game(req);

    // TODO falta o DUP!!!!

    if (!(positions((req->letter_word).front(), word).empty())) { // Some positions were found
        /* Prepare reply for client and update game file */
        update_game(req);
        if (won_game(req)) {
            /* Won the game */
            message = message + WIN + "\n";
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
        if (get_game_errors(req) == max_tries(word)) { // No more errors
            /* Prepare reply for client */
            message = message + OVR + moves;
            move_to_SCORES(req, F);
        }
        else { // Incorrect guess, but with remaining attempts
            /* Prepare reply for client */
            message = message + NOK + moves;
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
    int length = word.length();
    int number_occ = 0;
    for (int i = 0; i < length; i++) {
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
    new_game->max_errors = moves;
    new_game->errors = 0;
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

    int i = 0;
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


/* Increment game trials 

    Increment the attribute move_number for the struct game indexed by req->PLID */
void increment_game_trials(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    /* Update move number */
    int moves = std::stoi(req->trial) + 1;
    g->move_number = std::to_string(moves);
}


/* Decrement game trials 

    Decrement the attribute move_number for the struct game indexed by req->PLID */
void decrement_game_trials(struct request *req) {
    auto it = active_games.find(req->PLID); // Gets iterator pointing to game
    struct game *g = it->second;

    /* Update move number */
    int moves = std::stoi(req->trial) - 1;
    g->move_number = std::to_string(moves);
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

    if (req->op_code == PWG || req->op_code == RLG) {
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

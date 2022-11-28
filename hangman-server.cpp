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
#include <unordered_map>

// TODO don't add \n from hint to file
// TODO check if ID is already in use
// TODO 10 mágico na FindTopScore???
// TODO quando recebemos certos comandos, temos de ver se há um jogo ativo para esse PLID
// TODO maybe delete server init

#define NUMBER_THREADS 16
#define BLOCK_SIZE 256
#define PORT "58000"
#define GAME_NAME 15
#define SEED 73
#define NUMBER_OF_WORDS 6
#define WORLD_LINE_SIZE 64

int server_init();
int send_message(int fd, const char* message, size_t buf_size, struct addrinfo *res);
struct request* process_input(char buffer[]);
std::string get_random_word_from_file();
int treat_request(int fd, struct addrinfo *res, struct request *req);
void treat_start(int fd, struct addrinfo *res, struct request *req);
void treat_guess(int fd, struct addrinfo *res, struct request *req);
int FindLastGame(char *PLID, char *fname);
/*int FindTopScores(SCORELIST ∗list)*/

struct request {
    std::string op_code;
    std::string PLID;
    std::string letter_word;
    std::string trial;
};

struct game {
    std::string PLID;
    std::string word;
    std::string max_errors;
    std::string move_number;
};

std::vector<int> session_ids;
static pthread_mutex_t id_table_mutex;
std::string word_file = "word_file";
FILE* fp_word_file;
std::unordered_map<std::string, struct game*> active_games; // Key should be a PLID


int main(int argc, char **argv) {
    FILE *fserv;

    int fd, errorcode;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;    // TODO custom name for structs
    struct sockaddr_in addr;
    struct sigaction act;
    char buffer[BLOCK_SIZE];


    if (argc < 2) {
        printf("Input Error (main): Expected, at least, 1 argument (word_file), but none was given.\n");
        printf("\nTry running the program in the following format: %s <word_file> [-p GSport] [-v]\n\n", argv[0]);
        return 1;
    }

    printf("Starting hangman game server\n");

    if (server_init() == -1) {
        printf("Error (main): An error occured while initializing the server\n");
        return -1;
    }

    memset(&act, 0,sizeof(act));
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &act, NULL) == -1) {
        exit(1);
    }

    int pid = fork();
    if (pid == 0) { /* Child Process*/
        /* UDP server */
        fd = socket(AF_INET,SOCK_DGRAM,0); //UDP socket
        if (fd == -1) {
            printf("Error (main): An error occured while attempting to create an UDP socket\n");
            exit(1);
        }
        fp_word_file = fopen("word_file", "r");
        srand(SEED); /* Set seed for random num generator */
    }
    else {
        /* TCP server */
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

    n = bind(fd,res->ai_addr, res->ai_addrlen);
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

        write(1,"received: ",10); write(1,buffer,n); // TODO remover isto mais tarde. Vai servir de teste por agora

        struct request *req = process_input(buffer);
        if (req == NULL) {
            printf("Error (main): Couldn't allocate memory to process request\n");
        }
        treat_request(fd, res, req);



        n = sendto(fd,buffer,n,0, (struct sockaddr*)&addr,addrlen);
        if (n == -1) {
            printf("Error (main): An error occured while sending a reply\n");
            exit(1);
        }
    }

    freeaddrinfo(res);
    close(fd);

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


struct request* process_input(char buffer[]) {
    struct request *req = (struct request *) malloc(sizeof(struct request));
    
    if (req == NULL) {
        printf("Error (process_input): Ran out of memory while allocating space for the request\n");
        return NULL;
    }

    int i = 0;
    while (buffer[i] != ' ') {
        req->op_code.push_back(buffer[i]);
        i++;
    }
    i++;

    while (buffer[i] != ' ' || buffer[i] != '\0') {
        req->PLID.push_back(buffer[i]);
        i++;
    } 
    i++;

    if (req->op_code.compare(QUT) || req->op_code.compare(REV)) {
        /* Nothing more to parse */
        req->letter_word = "NULL"; req->trial = "NULL";
        return req;
    }

    while (buffer[i] != ' ') {
        req->letter_word.push_back(buffer[i]);
        i++;
    }
    i++;

    while (buffer[i] != '\0') {
        req->trial.push_back(buffer[i]);
        i++;
    }

    return req;
}

std::string get_random_word_from_file() {
    int line = rand() % NUMBER_OF_WORDS;

    int line_num = 1;
    char *buffer = (char *) malloc(WORLD_LINE_SIZE * sizeof(char));
    if (buffer == NULL) {
        printf("Error (get_word_from_file): Ran out of memory\n");
    }

    size_t buf_size = WORLD_LINE_SIZE;

    /* Fetch a random line from the file */
    while (getline(&buffer, &buf_size, fp_word_file)) {
        if (line_num == line) {
            break;
        }
        memset(buffer, '\0', WORLD_LINE_SIZE);
    }

    /* Extract word from file */
    int i = 0;
    std::string output;
    while (buffer[i] != ' ') {
        output.push_back(buffer[i]);
        i++;
    }

    return output;
}

int treat_request(int fd, struct addrinfo *res, struct request *req) {
    if (req->op_code == SNG) {
        /* Start New Game */
        treat_start(fd, res, req);
    }
    else if (req->op_code == PLG) {
        /* Play Letter */
    }
    else if (req->op_code == PWG) {
        /* Guess Word */
        treat_guess(fd, res, req);
    }

    return 0;
}

void create_game_session(std::string word, char moves, std::string PLID) {
    struct game *new_game = (struct game *) malloc(sizeof(struct game));
    if (new_game == NULL) {
        printf("Error (create_game_session): Ran out of memory\n");
    }

    /* Initialize game struct */
    new_game->word = word;
    new_game->PLID = PLID;
    new_game->max_errors = moves;
    new_game->move_number = "1";
    
    /* Add it to active games hashmap */
    active_games[PLID] = new_game;
}

void treat_start(int fd, struct addrinfo *res, struct request *req) {
    char* fname = (char *) malloc(GAME_NAME * sizeof(char));
    int found = FindLastGame(&req->PLID[0], fname);
    std::string status;
    std::string message = RSG;
    std::string suffix = " ";
    message.push_back(' ');

    if (found == 0) {
        /* No active game found for player PLID */
        status = OK;
        std::string word = get_random_word_from_file();
        char moves;
        suffix = suffix + std::to_string(word.length());
        if (word.length() <= 6) {
            moves = '7';
        }
        else if (word.length() <= 10) {
            moves = '8';
        }
        else {
            moves = '9';
        }
        suffix.push_back(' '); suffix.push_back(moves);
    }
    else {
        status = NOK;
    }
    message = message + status;
    if (found == 0) {
        message = message + suffix;
    }

    send_message(fd, message.c_str(), message.length(), res);
}

std::string get_first_line_from_game_file(struct request *req) {

    FILE *fp;

    /* Allocate memory for string containing path to active game */
    char* fname = (char *) malloc(GAME_NAME * sizeof(char));
    if (fname == NULL) {
        printf("Error (get_first_line_from_game_file): Ran out of memory\n");
    }

    /* Store path to file on fname */
    sprintf(fname, "GAMES/GAME_%s.txt", req->PLID);
    
    /* Allocate memory for string that will store first line from file */
    char *buffer = (char *) malloc(WORLD_LINE_SIZE * sizeof(char));
    if (buffer == NULL) {
        printf("Error (get_first_line_from_game_file): Ran out of memory\n");
    }
    size_t buf_size = WORLD_LINE_SIZE;

    /* Open game file */
    fp = fopen(fname, "r");

    /* Get first line from file */
    getline(&buffer, &buf_size, fp);

    return buffer;
}

std::string get_word(struct request *req) {
    std::string line = get_first_line_from_game_file(req);

    return strtok(&line[0], " ");
}

std::string get_hint_file_name(struct request *req) {
    std::string line = get_first_line_from_game_file(req);


    std::string token = strtok(&line[0], " ");
    token = strtok(NULL, " ");

    return token;
}


void treat_guess(int fd, struct addrinfo *res, struct request *req) {
    /* Look for active games with req->PLID */

    /* Get word and number of moves */
    std::string real_word = get_word(req);

    /* Compare number of moves to req->trials */

    /* Compare res->word to the word */

    /* Reply to client */
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


int server_init() {

    if (pthread_mutex_init(&id_table_mutex, NULL) != 0) {
        printf("Error (server_init): Failed to create mutex for id table.\n");
        return -1;
    }

    return 0;
}

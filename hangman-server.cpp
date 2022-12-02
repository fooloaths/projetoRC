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

// TODO don't add \n from hint to file
// TODO check if ID is already in use
// TODO 10 magico na FindTopScore???
// TODO quando recebemos certos comandos, temos de ver se ha um jogo ativo para esse PLID
// TODO maybe delete server init
// TODO if receive same play/guess twice, assume client didn't receive first reply and repeat the move (just don't store on the file)

#define NUMBER_THREADS 16
#define BLOCK_SIZE 256
#define PORT "58000"
#define GAME_NAME 15
#define SEED 73
#define NUMBER_OF_WORDS 6       // TODO algumas destas constantes foram tiradas do rabo
#define WORLD_LINE_SIZE 64

int server_init();
int send_message(int fd, const char* message, size_t buf_size, struct addrinfo *res);
struct request* process_input(char buffer[]);
struct game* get_active_game(std::string PLID);
void create_game_session(std::string word, char moves, std::string PLID, std::string hint);
std::string get_random_line_from_file();
void create_active_game_file(std::string game, struct request *req);
std::string get_word(std::string line);
std::string get_hint(std::string line);
std::string max_tries(std::string word);
int treat_request(int fd, struct addrinfo *res, struct request *req);
void treat_start(int fd, struct addrinfo *res, struct request *req);
void treat_guess(int fd, struct addrinfo *res, struct request *req);
void treat_quit(int fd, struct addrinfo *res, struct request *req);
void record_move_for_active_game(struct request *req);
int compare_plays(struct request *req, std::string line);
int get_move_number(struct request *req);
char* get_word_and_hint(struct request *req);
int check_for_active_game(struct request *req);
std::string get_current_date_and_time();
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
    std::string hint_file;  // TODO meter o hint file aqui durante o treat request
};

std::vector<int> session_ids;
static pthread_mutex_t id_table_mutex;
std::string word_file = "word_file";
FILE* fp_word_file;


int main(int argc, char **argv) {

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
        if (fp_word_file == NULL) {
            printf("Error (main): Couldn't open word file.\n");
            if (errno == EACCES) {
                printf("    EACCES: Not enough permissions to open file\n");
            }
            return -1; // TODO close socket
        }
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

int valid_PLID(std::string PLID) {
    if (PLID.length() != 6) {
        return -1;
    }

    int i = 0;
    while (PLID[i] != '\0' || PLID[i] != '\n') {
        if (PLID[i] < '0' || PLID[i] > '9') {
            return -1;
        }
        i++;
    }

    if (PLID[0] > '1') {
        /* First digit must be 0 or 1 */
        return -1;
    }

    return 0;
}

std::string get_random_line_from_file() {
    int line = rand() % NUMBER_OF_WORDS;

    int line_num = 1;
    char *buffer = (char *) malloc(WORLD_LINE_SIZE * sizeof(char));
    if (buffer == NULL) {
        printf("Error (get_random_line_from_file): Ran out of memory\n");
    }

    size_t buf_size = WORLD_LINE_SIZE;

    /* Fetch a random line from the file */
    while (getline(&buffer, &buf_size, fp_word_file)) {
        if (line_num == line) {
            break;
        }
        memset(buffer, '\0', WORLD_LINE_SIZE);
    }
    return buffer;
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
    else if (req->op_code == QUT) {
        /* Close game session, if one exists */
        treat_quit(fd, res, req);
    }
    else {
        /* Invalid protocol message */
        std::string message = ERR + '\n'; // TODO see if other parts of req are invalid
        send_message(fd, message.c_str(), message.length(), res);
    }

    return 0;
}

//void create_game_session(std::string word, char moves, std::string PLID, std::string hint) {
//    struct game *new_game = (struct game *) malloc(sizeof(struct game));
//   if (new_game == NULL) {
//        printf("Error (create_game_session): Ran out of memory\n");
//    }
//
//    /* Initialize game struct */
//    new_game->word = word;
//    new_game->PLID = PLID;
//    new_game->max_errors = moves;
//    new_game->move_number = "1";
//    new_game->hint_file = hint;
//    
//    /* Add it to active games hashmap */
//    active_games[PLID] = new_game;
//}

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
        std::string line = get_random_line_from_file();
        std::string word = get_word(line);
        std::string hint = get_hint(line);
        //char moves;
        suffix = suffix + std::to_string(word.length());
        std::string moves = max_tries(moves);
        suffix.push_back(' '); suffix.push_back(moves.front());
        create_active_game_file(line, req);
        //create_game_session(word, moves, req->PLID, hint);
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

std::string get_first_line_from_game_file(struct request *req) {

    FILE *fp;

    /* Allocate memory for string containing path to active game */
    char* fname = (char *) malloc(GAME_NAME * sizeof(char));
    if (fname == NULL) {
        printf("Error (get_first_line_from_game_file): Ran out of memory\n");
    }

    /* Store path to file on fname */
    sprintf(fname, "GAMES/GAME_%s.txt", (req->PLID).c_str());
    
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

    return buffer; // TODO dar free algures
}

std::string get_word(std::string line) {
    int i = 0;

    std::string output;
    while (line[i] != ' ') {
        output.push_back(line[i]);
        i++;
    }
    return output;
}

std::string get_hint(std::string line) {
    int i = 0;

    std::string output;
    /* Skip word */
    while (line[i] != ' ') {i++;}
    i++;

    while (line[i] != '\0') {
        output.push_back(line[i]);
        i++;
    }

    return output;
}

void treat_guess(int fd, struct addrinfo *res, struct request *req) {
    std::string message = RLG;
    message.push_back(' ');

    /* Look for active games with req->PLID */
    if (valid_PLID(req->PLID) == -1 || check_for_active_game(req) == -1) { // TODO also need to send ERR in some other cases
        /* No active game for req->PLID */
        message = message + ERR + "\n";
        send_message(fd, message.c_str(), message.length(), res);
    }

    /* Compare number of moves to req->trials */
    int move_number = get_move_number(req);
    std::string moves = std::to_string(' ') + std::to_string(move_number) + "\n";
    if (std::stoi(req->trial) != move_number) {
        /* Send message to client saying something went wrong */
        // TODO if duplicate guess, also reply INV
        message = message + INV + moves;
        send_message(fd, message.c_str(), message.length(), res);
    }

    /* Compare res->word to the word */
    char *line = get_word_and_hint(req);
    std::string word = get_word(line);
    free(line);

    if (word == req->letter_word) { // Won the game
        /* Send reply to client */
        message = message + WIN + moves;
        send_message(fd, message.c_str(), message.length(), res);
    }
    else if (req->trial == max_tries(word)) { // No more errors
        /* Send reply to client */
        message = message + OVR + moves;
        send_message(fd, message.c_str(), message.length(), res);
    }
    else { // Incorrect guess, but with remaining attempts
        /* Send reply to client */
        message = message + NOK + moves;
        send_message(fd, message.c_str(), message.length(), res);
    }

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
        return 0;
    }
    else {
        printf("Error (is_directory): Something went wrong while opening the directory\n");
        return -1;
    }

}

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
    line = line + " " + req->letter_word;
    file << line;

    /* Close file */
    file.close();
}

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


void treat_quit(int fd, struct addrinfo *res, struct request *req) {

    std::string message = RQT + std::to_string(' ');
    std::string status;
    if (valid_PLID(req->PLID) == -1 || check_for_active_game(req) == -1) {
        /* No active game or invalid PLID */
        status = ERR;
    }
    else {
        /* Close game and move file to SCORES */

        std::string file_name = SCORES + get_current_date_and_time() + "_" + Q + ".txt";
        std::string current_path = ACTIVE_GAME_PATH + req->PLID + ".txt";
        std::string shell_command = "mv " + current_path + " " + file_name;
        system(shell_command.c_str());

        status = OK;
    }

    /* Complete reply message */
    message = message + status + "\n";

    /* Send reply through UDP socket */
    send_message(fd, message.c_str(), message.length(), res);
}

std::string get_current_date_and_time() {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    std::string output = std::to_string(tm.tm_year) + std::to_string(tm.tm_mon) + std::to_string(tm.tm_mday) + "_";
    output = output + std::to_string(tm.tm_hour) + std::to_string(tm.tm_min) + std::to_string(tm.tm_sec);

    return output;
}
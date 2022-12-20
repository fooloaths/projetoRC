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
#define SNG "SNG "
#define QUT "QUT "
#define PLG "PLG "
#define RLG "RLG"
#define PWG "PWG "
#define OK "OK"
#define WIN "WIN"
#define DUP "DUP"
#define NOK "NOK"
#define OVR "OVR"
#define INV "INV"
#define ERR "ERR"
#define REV "REV "
#define rev "rev "
#define VICTORY_MESSAGE "WELL DONE! You guessed: "

void start_new_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr);
std::string receive_message(int fd, sockaddr_in addr, size_t buf_size);
void send_message(int fd, char message[], size_t buf_size, struct addrinfo *res);
void exit_game(std::string id, int fd, struct addrinfo *res, struct sockaddr_in addr);
std::string get_status(std::string message);
std::string play_aux_ok(std::string message, std::string letter);
void play(std::string letter, int fd, struct addrinfo *res, struct sockaddr_in add);
std::string format_word(std::string word_to_format = word);
void scoreboard(const char* server_ip, const char* server_port);
void guess(std::string word, int fd, struct addrinfo *res, struct sockaddr_in addr);
void reveal_word(int fd, struct addrinfo *res, struct sockaddr_in addr);
void status(const char* server_ip, const char* server_port);
void hint(const char* server_ip, const char* server_port);
void status_aux_ok(std::string message);
void hint_aux_ok(std::string message);
std::string tcp_helper(std::string message, const char* server_ip, const char* server_port);
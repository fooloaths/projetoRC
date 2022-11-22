CC=gcc
CFLAGS=-I. -Wall -Wextra -Werror -fsanitize=address -fsanitize=undefined -g 

hangman_client.out: hangman_client.cpp
	$(CC) $(CFLAGS) -o hangman_client.out hangman_client.cpp

clean:
	rm *.out

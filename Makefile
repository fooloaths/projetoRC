CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -fsanitize=address -fsanitize=undefined -g 

hangman_client.out: hangman_client.cpp
	$(CXX) $(CXXFLAGS) -o hangman_client.out hangman_client.cpp

clean:
	rm *.out
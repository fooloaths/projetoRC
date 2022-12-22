CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -g #-fsanitize=address -fsanitize=undefined

all: server client

client:
	$(CXX) $(CXXFLAGS) -o player hangman_client.cpp

server:
	$(CXX) $(CXXFLAGS) -o GS hangman_server.cpp
	
clean:
	rm GS
	rm player
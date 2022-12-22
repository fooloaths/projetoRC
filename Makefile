CXX ?= g++

all: server client

client:
	$(CXX) -o player hangman_client.cpp

server:
	$(CXX) -o GS hangman_server.cpp
	
clean:
	rm GS
	rm player
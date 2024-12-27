FLAGS=-Wall -Werror -Wshadow -std=c++17
SSL=-lssl -lcrypto
PTHREAD=-lpthread
SDL2=-lSDL2

all:
	g++ $(FLAGS) server.cpp -o server $(SSL) $(PTHREAD) $(SDL2)

.PHONY: clean
clean:
	rm -f server
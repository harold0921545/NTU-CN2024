FLAGS=-Wall -Werror -Wshadow -std=c++17
SSL=-lssl -lcrypto
SDL2=-lSDL2

all:
	g++ $(FLAGS) client.cpp -o client $(SSL) $(SDL2)

.PHONY: clean
clean:
	rm -f client
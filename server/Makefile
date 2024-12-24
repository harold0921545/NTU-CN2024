FLAGS=-Wall -Werror -Wshadow -std=c++17
SSL=-lssl -lcrypto
CV=`pkg-config --cflags --libs opencv4`
PTHREAD=-lpthread

all:
	g++ $(FLAGS) server.cpp -o server $(SSL) $(CV) $(PTHREAD)

.PHONY: clean
clean:
	rm -f server
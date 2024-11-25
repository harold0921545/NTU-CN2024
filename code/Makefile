GTKMM=`pkg-config gtkmm-3.0 --libs --cflags`
FLAGS=-Wall -Werror -Wshadow -std=c++17
SSL=-lssl -lcrypto
CV=`pkg-config --cflags --libs opencv4`
PTHREAD=-lpthread

all:
	g++ $(FLAGS) server.cpp -o server $(SSL) $(CV) $(PTHREAD)
	g++ $(FLAGS) client.cpp $(GTKMM) -o client $(SSL) $(CV)

.PHONY: clean
clean:
	rm -f server client
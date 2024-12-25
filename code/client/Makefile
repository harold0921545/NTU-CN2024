FLAGS=-Wall -Werror -Wshadow -std=c++17
SSL=-lssl -lcrypto
CV=`pkg-config --cflags --libs opencv4`

all:
	g++ $(FLAGS) client.cpp -o client $(SSL) $(CV)

.PHONY: clean
clean:
	rm -f client
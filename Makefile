build:
	g++ -Wall -std=c++11 client.cpp -o client
	g++ -Wall -std=c++11 server.cpp -o server

clean: 
	rm -rf *.o *~  client server
CPPFLAGS=-g -lprotobuf -pthread
LDFLAGS=-g

server: server_main.cpp mensaje.pb.cc Server.cpp
	 g++ -g $(CPPFLAGS) -o server server_main.cpp mensaje.pb.cc Server.cpp $(LDFLAGS)

client: client_main.cpp mensaje.pb.cc Client.cpp
	 g++ -g $(CPPFLAGS) -o client client_main.cpp mensaje.pb.cc Client.cpp $(LDFLAGS)
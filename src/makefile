IDIR=../include
CC=g++
CPPFLAGS=-I$(IDIR) -lprotobuf -pthread -std=c++11

ODIR=obj
LDIR=../lib

SERVERCPP= server_runner.cpp Server.cpp
CLIENTCPP= client_runner.cpp Client.cpp
MSGCC= mensaje.pb.cc

ODIR=../runner

PROTOCPPOUT=../lib

server: server_runner.cpp Server.cpp
	 $(CC) $(CPPFLAGS) -o server $(SERVERCPP) $(IDIR)/$(MSGCC)

client: client_runner.cpp Client.cpp
	 $(CC) $(CPPFLAGS) -o client  $(CLIENTCPP) $(IDIR)/$(MSGCC)

message: $(IDIR)/mensaje.proto
	protoc -I=$(IDIR) --cpp_out=$(IDIR) mensaje.proto

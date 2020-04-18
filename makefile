IDIR=./include
CC=g++
CPPFLAGS=-I$(IDIR) -lprotobuf -pthread -std=c++11
SRCDIR=./src
CHATDIR=$(SRCDIR)/Chat
RUNNERDIR=$(SRCDIR)/runners

SERVERCPP= $(RUNNERDIR)/server_runner.cpp $(CHATDIR)/Server.cpp
CLIENTCPP= $(RUNNERDIR)/client_runner.cpp $(CHATDIR)/Client.cpp

PROTOCPPOUT=../lib
PROTOCFLAGS=-I=$(IDIR) --cpp_out=$(IDIR)
PROTOCC=protoc
MSGCC= mensaje.pb.cc

server: $(SERVERCPP)
	 $(CC) $(CPPFLAGS) -o server $(SERVERCPP) $(IDIR)/$(MSGCC)

client: $(CLIENTCPP)
	 $(CC) $(CPPFLAGS) -o client  $(CLIENTCPP) $(IDIR)/$(MSGCC)

message: $(IDIR)/mensaje.proto
	$(PROTOCC) $(PROTOCFLAGS) mensaje.proto

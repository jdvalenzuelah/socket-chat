# Linux Socket Chat

Simple linux chat using sockets

### Prerequisites
- Google's Protobuf
- make

### Installing

Clone repository

```
git clone https://github.com/val171001/socket-chat.git
```

Install make and protobuf (with fedora as follows)

```
sudo dnf install make
```
```
sudo dnf install protobuf
```

If you encountered any issues after installing protobuf follow the instructions on [official repository](https://github.com/protocolbuffers/protobuf) 

Compiling protobuf messages
```
make message
```

Compiling client

```
make client
```

Compiling server

```
make server
```

### Running

Server

```
./server 8080
```

Client

```
./client noobmaster69 127.0.0.1 8080
```

By default a command line text interface is shown, if you would like to use a graphical user interface.

## Authors

* **Josue Valenzuela** - *Initial work* - [Github](https://github.com/val171001)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

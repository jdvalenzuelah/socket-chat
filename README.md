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

First cd in to src before compiling anything

```
cd src/
```

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

## Authors

* **Josue Valenzuela** - *Initial work* - [Github](https://github.com/val171001)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

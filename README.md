# Linux Socket Chat

Simple linux chat using sockets

### Prerequisites
- Google's Protobuf
- make

```
Give examples
```

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
sudo dnf install protobufke
```

Compiling protobuf messages

```
make messages
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
./server noobmaster69 127.0.0.1 8080
```

## Authors

* **Josue Valenzuela** - *Initial work* - [Github](https://github.com/val171001)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

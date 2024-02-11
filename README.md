# Simple-FTP
This Program is Simple FTP program. When the server shares the directory, the client can list up the content of directory, get the file in the directory, and put the file into the server. This project service unit testing. Unit testing is implemented with Gtest. You can use the FTP service and check out gtest. 

## Build
- CMake
- Gtest
- Gmock

## How to Use
```
Server Usage: ./fshared -p <port-number> -d <directory-to-be-shared>
Client Usage: ./fshare <host-ip:port-number> <command> <filepath (if necessary)>
```

## Difficult Point
Gtest and Gmock is the program targeting C++ program. The developing process was not easy because this program is the pure C program. Because of this point, Direct Gtest usage is complex. To help the programmer use Gtest in C, I would like to share my experience. If you want to use Gtest in C, You should use extern "C"(You can check it in the Code). It makes possible for C++ compiler to compile this area into C style. Finally, You can use Gtest in C.
Gmock is more complicated than Gtest. The method which I chose is linking-time Interposition using Dynamic linking. You need to make virtual function with Wrapper Class and mock this function. And then, In this Code, you declare this function and set the return value into virtual function. You can check this method in the code. 

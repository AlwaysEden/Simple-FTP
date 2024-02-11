#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
	#include "fshare.h"
}

class Wrapper {
	public:
    		virtual ~Wrapper() {}
    		virtual ssize_t send(int sockfd, const void *buf, size_t len, int flags) = 0;
    		virtual int lstat(const char *pathname, struct stat *statbuf) = 0;
		virtual ssize_t recv(int sockfd, void *buf, size_t len, int flags) = 0;
};

class Mocking :public Wrapper{
public:
	using Wrapper::send;
	using Wrapper::lstat;
	using Wrapper::recv;

    	MOCK_METHOD(ssize_t, send, (int sockfd, const void *buf, size_t len, int flags), (override));
    	MOCK_METHOD(int, lstat, (const char *pathname, struct stat *statbuf), (override));
	MOCK_METHOD(ssize_t, recv, (int sockfd, void *buf, size_t len, int flags), (override));
};


Mocking *sendMock;
ssize_t send(int sockfd, const void *buf, size_t len, int flags){
	return sendMock->send(sockfd, buf, len, flags);
}

Mocking *lstatMock;
int lstat(const char *pathname, struct stat *statbuf){
	return lstatMock->lstat(pathname, statbuf);
}

Mocking *recvMock;
ssize_t recv(int sockfd, void *buf, size_t len, int flags){
	return recvMock->recv(sockfd, buf, len, flags);
}


class fshare_F: public ::testing::Test {
protected:
	void SetUp() override {
	char *argv[] = {"./fshare", "127.0.0.1:9090", "list"};
	get_option(3, argv);

	using ::testing::_;
        using ::testing::Return;
        sendMock = new Mocking();
        EXPECT_CALL(*sendMock, send(
                                ::testing::An<int>(),
                                ::testing::An<const void*>(),
                                ::testing::An<size_t>(),
                                ::testing::An<int>())).WillRepeatedly(testing::ReturnArg<2>());
	request(1);
    }

    void TearDown() override {
	    delete sendMock;
    }
};


TEST(fshare, getOption) {	
	char *argv6[] = {"./fshare", "o"};
	EXPECT_EQ(get_option(2, argv6), 1);
	char *argv1[] = {"./fshare", "127.0.0.1:9090", "list"};
	EXPECT_EQ(get_option(3, argv1), 0);
	char *argv2[] = {"./fshare", "127.0.0.1:9090", "get", "fshared/filename", "./"};
	EXPECT_EQ(get_option(5, argv2), 0);
	char *argv3[] = {"./fshare", "127.0.0.1:9090", "put", "./filename", "fshared/"};
	EXPECT_EQ(get_option(5, argv3), 0);
	char *argv4[] = {"./fshare", "-h"};
	EXPECT_EQ(get_option(2, argv4), 1);
	char *argv5[] = {"./fshare", "-o"};
	EXPECT_EQ(get_option(2, argv5), 1);
}


TEST(fshare, request) {
	using ::testing::_;	
	using ::testing::Return;
	sendMock = new Mocking();	
	EXPECT_CALL(*sendMock, send( 
				::testing::An<int>(), 
				::testing::An<const void*>(), 
				::testing::An<size_t>(), 
				::testing::An<int>())).WillRepeatedly(testing::ReturnArg<2>());
	lstatMock = new Mocking();	
	EXPECT_CALL(*lstatMock, lstat( ::testing::An<const char*>(), ::testing::An<struct stat *>() )).WillRepeatedly(Return(0));

	char *argv1[] = {"./fshare", "127.0.0.1:9090", "list"};
	get_option(3, argv1);
	EXPECT_EQ(request(1), 0);
	
	char *argv2[] = {"./fshare", "127.0.0.1:9090", "get", "fshared/filename", "./"};
	get_option(5, argv2);
	EXPECT_EQ(request(1), 0);
	
	char filename[255] = "test.txt";
	int fd = open(filename, O_WRONLY | O_CREAT, 0644);
	char buf[10] = "123456789";
	write(fd, buf, strlen(buf)); 
	close(fd);

	char *argv3[] = {"./fshare", "127.0.0.1:9090", "put", filename, "fshared/"};
	get_option(5, argv3);
	EXPECT_EQ(request(1), 0);
	
	remove(filename);
	delete lstatMock;
	delete sendMock;
}


TEST_F(fshare_F, receiveListResponse){
	using ::testing::_;	
	using ::testing::Return;
	recvMock = new Mocking();	
	EXPECT_CALL(*recvMock, recv( 
				::testing::An<int>(), 
				::testing::An<void*>(), 
				::testing::An<size_t>(), 
				::testing::An<int>())).WillRepeatedly(::testing::Invoke([](int sockfd, void *buf, size_t len, int flags){
								size_t readed = read(sockfd, buf, len);
								return readed;
							}));
	
	char test1[] = "test1.txt";
	char test2[] = "test2.txt";

	server_header sh;
	sh.is_error = 0;
	sh.payload_size = 10;
	int fd = open(test1, O_WRONLY | O_CREAT, 0664);
	write(fd, &sh, sizeof(sh));
	char buf1[sh.payload_size] = "0123456789";
	write(fd, buf1, sh.payload_size);
	close(fd);

	fd = open(test1, O_RDONLY);
	EXPECT_EQ(receive_list_response(fd), 0);
	close(fd);

	sh.is_error = 0;
	sh.payload_size = 1024;
	fd = open(test2, O_WRONLY | O_CREAT, 0664);
	write(fd, &sh, sizeof(sh));
	char buf2[sh.payload_size] = "0123456789";
	write(fd, buf2, sh.payload_size);
	close(fd);

	fd = open(test2, O_RDONLY);
	EXPECT_EQ(receive_list_response(fd), 0);
	close(fd);
	
	remove(test1);
	remove(test2);
	delete recvMock;
}


TEST(fshare, makeDirectory){
	using ::testing::_;
	using ::testing::Return;

	EXPECT_EQ(make_directory("fshare/"), 0);
	
	system("rm -r fshare/");
}



TEST(fshare, receiveGetResponse){

	using ::testing::_;
        using ::testing::Return;
        recvMock = new Mocking();
        EXPECT_CALL(*recvMock, recv(
                                ::testing::An<int>(),
                                ::testing::An<void*>(),
                                ::testing::An<size_t>(),
                                ::testing::An<int>())).WillRepeatedly(::testing::Invoke([](int sockfd, void *buf, size_t len, int flags){
                                                                size_t readed = read(sockfd, buf, len);
                                                                return readed;
                                                        }));
	
	char *argv[] = {"./fshare", "127.0.0.1:9090", "get", "fshared/filename", "./"};
        get_option(5, argv);

	char test1[] = "test1.txt";
	int fd = open(test1, O_WRONLY | O_CREAT, 0664);
	server_header sh;
        sh.is_error = 0;
        sh.payload_size = 10;
        write(fd, &sh, sizeof(sh));
        
	char buf1[sh.payload_size] = "0123456789";
        write(fd, buf1, sh.payload_size);
        close(fd);
	
	fd = open(test1, O_RDONLY);
	EXPECT_EQ(receive_get_response(fd), 0);
	system("rm -r fshared");
	
	delete recvMock;
}

TEST(fshare, receivePutResponse){
	using ::testing::_;
        using ::testing::Return;
        recvMock = new Mocking();
        EXPECT_CALL(*recvMock, recv(
                                ::testing::An<int>(),
                                ::testing::An<void*>(),
                                ::testing::An<size_t>(),
                                ::testing::An<int>())).WillOnce(Return(sizeof(sh)));
	char *argv[] = {"./fshare", "127.0.0.1:9090", "put", "./filename", "fshared/"};
        get_option(5, argv);

	EXPECT_EQ(receive_put_response(1), 0);
	free(recvMock);

	
}

/*
TEST(fshare, listFlow){
	char *argv1[] = {"./fshare", "127.0.0.1:9090", "list"};
        EXPECT_EQ(get_option(3, argv1), 0);	
	EXPECT_EQ(request(1), 0);

	char test1[] = "test1.txt";
        char test2[] = "test2.txt";

        server_header sh;
        sh.is_error = 0;
        sh.payload_size = 10;
        int fd = open(test1, O_WRONLY | O_CREAT, 0664);
        write(fd, &sh, sizeof(sh));
        char buf1[sh.payload_size] = "0123456789";
        write(fd, buf1, sh.payload_size);
        close(fd);

        fd = open(test1, O_RDONLY);
        EXPECT_EQ(receive_list_response(fd), 0);
        close(fd);

        sh.is_error = 0;
        sh.payload_size = 1024;
        fd = open(test2, O_WRONLY | O_CREAT, 0664);
        write(fd, &sh, sizeof(sh));
        char buf2[sh.payload_size] = "0123456789";
        write(fd, buf2, sh.payload_size);
        close(fd);

        fd = open(test2, O_RDONLY);
        EXPECT_EQ(receive_list_response(fd), 0);
        close(fd);

        remove(test1);

}*/

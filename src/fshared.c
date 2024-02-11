/*  Server : receive request from the client and response back
    $ ./fshared -p 8080 -d files
    
    Client 로부터 다음과 같은 명령어를 request 받았을 때 :
        - list : send back 오류 없음 + files 안의 리스트 size + files 안의 리스트 내용
        - get hello.txt :
            - files 안의 hello.txt 가 있으면 > send back 오류 없음 + hello.txt 의 size + hello.txt 의 내용
            - files 안의 hello.txt 가 없으면 > send back 오류 있음 + 0 + 0
        - put hi.txt : 
            - files 안에 hi.txt 를 씀 > send back 오류 없음 + hi.txt 의 size + hi.txt 의 내용
            - files 안에 hi.txt 를 쓰지 못함 (생성 불가 or 다 못씀 등) > send back 오류 있음 + 0 + 0
*/    

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include "fshared.h"

char * cmd_str[N_cmd] = {
	"list",
    "get",
    "put"
} ;
/*
typedef enum {
    list,
    get,
    put,
    N_cmd
} cmd ;


typedef struct {
    cmd command ;
    int src_path_len ;
    int des_path_len ;
    int payload_size ;
} client_header ;

typedef struct {
    int is_error ; // on success 0, on error 1
    int payload_size ;
} server_header ;
*/
char * recv_payload = 0x0 ;
char * send_payload = 0x0 ;

client_header ch ;
server_header sh ;

int port_num = -1 ;
char * server_dir = 0x0 ;

const int buf_size = 512 ;

/* 
   send_bytes
        return 0 if all given bytes are successfully sent
        return 1 otherwise
*/

int 
send_bytes(int fd, char * buf, size_t len)
{
    char * p = buf ;
    size_t acc = 0 ;

    while (acc < len)
    {
        size_t sent ;
        sent = send(fd, p, len - acc, 0) ;
        if (sent == -1)
            return 1 ;
        p += sent ;
        acc += sent ;
    }
    return 0 ;
}

int
directory_check(char * filepath)
{
    DIR * dir = opendir(filepath) ;
    if (dir == NULL) {
        fprintf(stderr, "Directory does not exist!\n") ;
        return EXIT_FAILURE ;
    }
    closedir(dir) ;

    return EXIT_SUCCESS ;
}

void 
print_usage() 
{
    printf("Usage: ./fshared -p <port-number> -d <directory-to-be-shared>\n");
}

void
get_option(int argc, char * argv[])
{
    // -p : port number
    // -d : directory to share with the client

    int option ;
    while ((option = getopt(argc, argv, "p:d:")) != -1) { // Fix the condition here
        switch (option) {
            case 'p':
                port_num = atoi(optarg) ;
                if(port_num < 1023 || port_num > 49151) {
                    fprintf(stderr, "Range of port number should be [1024, 49150]\n") ;
                    return ;
                }
                break ;
            case 'd':
                server_dir = optarg ;
                if ((directory_check(server_dir))) {
                    fprintf(stderr, "Given directory does not exist.\n") ;
                    return ;
                }
                break ;
            case '?':
                if (optopt == 'p') {
                    fprintf(stderr, "Option '-p' requires a port number.\n") ;
                } else if (optopt == 'd') {
                    fprintf(stderr, "Option '-d' requires a directory name.\n") ;
                } else {
                    fprintf(stderr, "Unknown option -%c.\n", optopt) ;
                }
                print_usage() ;
                return ;
        }
    }

    // Check if both options were provided
    if (port_num == -1 || server_dir == NULL) {
        print_usage() ;
        return ;
    }

    return ;
}

void
list_response(char * filepath, const int conn) 
{
    DIR * dir = opendir(filepath) ;
    if (dir == NULL) { 
        fprintf(stderr, "Failed to open a directory %s!\n", filepath) ;
        return ;
    }

    struct dirent * entry ;
    while ((entry = readdir(dir)) != NULL) {
        int len = strlen(filepath) + 1 + strlen(entry->d_name) + 1 ;
        char * local_filepath = (char *) malloc(len) ;
        if (local_filepath == NULL) {
            fprintf(stderr, "Failed to allocate a memory!\n") ;
            closedir(dir) ;
            return ;
        }
        snprintf(local_filepath, len, "%s/%s", filepath, entry->d_name) ;
        // local_filepath[len] = '\0' ;

        struct stat filestat ;
        if (lstat(local_filepath, &filestat) == -1) {
            fprintf(stderr, "Failed to get a file status of %s!\n", local_filepath) ;
            free(local_filepath) ;
            closedir(dir) ;
            return ;
        }

        int sent = 0 ;
        if (S_ISREG(filestat.st_mode)) {
            sh.is_error = 0 ;
            sh.payload_size = len ;
            if ((sent = send(conn, &sh, sizeof(sh), 0)) != sizeof(sh)) { // send header
                perror("send error : ") ;
                free(local_filepath) ;
                closedir(dir) ;
                return ;
            }
            if ((sent = send(conn, local_filepath, len, 0)) != len) { // send payload
                perror("send error : ") ;
                free(local_filepath) ;
                closedir(dir) ;
                return ;
            }
        } else if (S_ISDIR(filestat.st_mode)) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                list_response(local_filepath, conn) ;
            }
        } 

        free(local_filepath) ;
    }

    closedir(dir) ;
}

char *
parse_directory(char * toparse) {
    char * parsed_dir = dirname(toparse) ;
    return parsed_dir ;
}

void
get_response(int conn) 
{
    recv_payload = (char *) malloc(ch.payload_size) ;
    int received ;
    if ((received = recv(conn, recv_payload, ch.payload_size, 0)) != ch.payload_size) {
        perror("receive error : ") ;
        return ;
    }    
    
    struct stat filestat ;
    if (lstat(recv_payload, &filestat) == -1) {
        fprintf(stderr, "Failed to get a file status of %s!\n", recv_payload) ;
        free(recv_payload) ;
        return ;
    }

    sh.is_error = 0 ;
    sh.payload_size = filestat.st_size ;

    int sent ;
    if ((sent = send(conn, &sh, sizeof(sh), 0)) != sizeof(sh)) { // send server header
        perror("send error 1 : ") ;
        return ;
    }

    // 파일 열어서 읽고, 그 내용을 send
    FILE * fp = fopen(recv_payload, "rb") ;
    if (fp == NULL) {
        fprintf(stderr, "Failed to open a file %s!\n", recv_payload) ;
        free(recv_payload) ;
        return ;
    }
    free(recv_payload) ;

    char buf[buf_size] ;
    int read_size ;
    while ((read_size = fread(buf, 1, buf_size, fp)) > 0) { // send payload
        if ((sent = send(conn, buf, read_size, 0)) != read_size) {
            perror("send error 2 : ") ;
            fclose(fp) ;
            return ;
        }
    }

    fclose(fp) ;
}

void
make_directory(char * towrite) 
{
    char * temp = (char *) malloc(strlen(towrite) + 1) ;

    for (int i = 0; i <= strlen(towrite); i++) {
        if (towrite[i] == '/') {
            strncpy(temp, towrite, i) ;
            temp[i] = '\0' ;

            DIR * dir = opendir(temp) ;
            if (dir) { // if a directory exists
                closedir(dir) ;
            } else if (errno == ENOENT) { // if a directory does not exist
                if (mkdir(temp, 0776) == -1) {
                    fprintf(stderr, "Failed to make a new directory %s!\n", temp) ;
                    free(temp) ;
                    return ;
                }
            }
        }
    }
    free(temp) ;
}

void
put_response(int conn)
{
    // source path from the payload
    recv_payload = (char *) malloc(ch.src_path_len) ;
    int received ;
    if ((received = recv(conn, recv_payload, ch.src_path_len, 0)) != ch.src_path_len) {
        perror("receive error source : ") ;
        free(recv_payload) ;
        return ;
    }
    char * file_name = (char *) malloc(strlen(basename(recv_payload)) + 1) ;
    strcpy(file_name, basename(recv_payload)) ;
    free(recv_payload) ;

    // destination path from the payload
    recv_payload = (char *) malloc(ch.des_path_len) ;
    if ((received = recv(conn, recv_payload, ch.des_path_len, 0)) != ch.des_path_len) {
        perror("receive error destination : ") ;
        free(file_name) ;
        free(recv_payload) ;
        return ;
    }
    
    int file_len = ch.des_path_len + 1 + strlen(file_name) + 1 ;
    char * file_towrite = (char *) malloc(file_len) ;
    snprintf(file_towrite, file_len, "%s/%s", recv_payload, file_name) ;
    free(file_name) ;
    free(recv_payload) ;

    printf("checking : put destination %s\n", file_towrite) ;

    make_directory(file_towrite) ;

    FILE * fp = fopen(file_towrite, "wb") ;
    if (fp == NULL) {
        fprintf(stderr, "Failed to open a file %s!\n", file_towrite) ;
        free(file_towrite) ;
        return ;
    }

    char buf[buf_size] ;
    while ((received = recv(conn, buf, buf_size, 0)) > 0) { // receive payload
        if (fwrite(buf, 1, received, fp) < 0) {
            fprintf(stderr, "Failed to write a file %s\n", file_towrite) ;
            free(file_towrite) ;
            fclose(fp) ;
            return ;
        }
    }

    free(file_towrite) ;
    fclose(fp) ;

    sh.is_error = 0 ;
    sh.payload_size = 0 ;

    int sent ;
    if ((sent = send(conn, &sh, sizeof(sh), 0)) != sizeof(sh)) { // send server header
        perror("send error : ") ;
        return ;
    }
}

void *
go_thread(void * arg) 
{
    int conn = *((int *) arg) ;
    free(arg) ;

    /* Receive message from the client */

    int received ;
    if ((received = recv(conn, &ch, sizeof(ch), 0)) != sizeof(ch)) { // receive client header
        perror("receive error : ") ;
        return NULL ;
    } 

    char buf[buf_size] ;
    int len = 0 ;
    if (ch.command == list) {
        list_response(server_dir, conn) ;
        printf(">> List response completed!\n") ;
    } else if (ch.command == get) {
        get_response(conn) ;
        printf(">> Get response completed!\n") ;
    } else if (ch.command == put) {
        put_response(conn) ;
        printf(">> Put response completed!\n") ;
    }

    shutdown(conn, SHUT_WR) ;

    return NULL;
}

int
main(int argc, char * argv[])
{
    get_option(argc, argv) ;

    /* Connect socket */

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0) ;
    if (listen_fd == 0) {
        perror("socket failed : ") ;
        exit(EXIT_FAILURE) ;
    }

    struct sockaddr_in address ;
    memset(&address, '0', sizeof(address)) ;
    address.sin_family = AF_INET ;
    address.sin_addr.s_addr = INADDR_ANY ; // localhost
    address.sin_port = htons(port_num) ;
    if (bind(listen_fd, (struct sockaddr *) &address, sizeof(address)) < 0) { 
        perror("bind failed : ") ;
        exit(EXIT_FAILURE) ;
    }

    while (1) {
        if (listen(listen_fd, 16) < 0) {
            perror("listen failed : ") ;
            exit(EXIT_FAILURE) ; 
        }

        int * new_socket = (int *) malloc(sizeof(int)) ;
        int address_len = sizeof(address) ;
        *new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t *) &address_len) ;
        if (*new_socket < 0) {
            perror("accept failed : ") ;
            exit(EXIT_FAILURE) ;
        }

        pthread_t working_thread ;
        if (pthread_create(&working_thread, NULL, go_thread, new_socket) == 0) {
            // data receiving is done in working thread
        } else {
            close(*new_socket) ;
        }
    }

    return 0 ;
}

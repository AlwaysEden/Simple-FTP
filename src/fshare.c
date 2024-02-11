/*  Client : request to the server and receive the response message
    $ ./fshare 192.168.0.1:8080 list

    Server 로 request :
        - list          : send "list" + 0 + NULL
        - get hello.txt : send "get" + strlen("hello.txt") + NULL
        - put hi.txt    : send "put" + sizeof("hi.txt") + content of "hi.txt"

    Server 로부터 다음과 같은 response message 를 받음 :
        - list : receives files 의 directory 안의 내용 > print
        - get hello.txt
            - receives 오류 없음 > 끝
            - receives 오류 있음 > 오류 메시지 print
        - put hi.txt
            - receives 오류 없음 > 끝
            - receives 오류 있음 > 오류 메시지 print
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include <pthread.h>
#include "fshare.h"

char * cmd_str[N_cmd] = {
	"list",
	"get",
	"put"
} ;

char * recv_payload = 0x0 ;
char * send_payload = 0x0 ;

client_header ch ;
server_header sh ;

char * hostip = 0x0 ;
int port_num = -1 ;
char * src_path = 0x0 ;
char * dest_dir = 0x0 ;

const int buf_size = 512 ;


cmd
get_cmd_code (char * s)
{
	for (int i = 0 ; i < N_cmd ; i++) {
	 	if (strcmp(s, cmd_str[i]) == 0) {
			return i ;
		}
	}
	return N_cmd ;
}

void 
print_usage() 
{
    printf("Usage: ./fshare <host-ip:port-number> <command> <filepath (if necessary)>\n");
}

int 
get_option(int argc, char * argv[])
{
    // parse the command-line argument and assign values

    // fshare <host-ip:port-num> <command> <file_path> <directory-name>
    // fshare 192.168.0.1:8080 list
    // fshare 192.168.0.1:8080 get hello.txt ./dest
    // fshare 192.168.0.1:8080 put hi.txt


    int option ;
    while ((option = getopt(argc, argv, "h")) != -1) {
        switch (option) {
            case 'h':
                print_usage() ;
		return 1;
            case '?':
                fprintf(stderr, "Unknown option -%c.\n", optopt) ;
                return 1;
        }
    }
    
    if (argc < 3) { // at least two argument is required : <host-ip:port-number> <command>
        print_usage();
        return 1;
    }

    // extract IP address and port from <host-ip:port-number>
    char * host_port = strdup(argv[1]) ; 
    
    char * colon_ptr = strchr(host_port, ':') ; // find the position of the colon
    if (colon_ptr == NULL) {
        fprintf(stderr, "Invalid format for <host-ip:port-number>.\n") ;
        print_usage() ;
        return 1;
    }
    *colon_ptr = '\0'; // replace the colon with null-terminator to separate IP and port strings

    // get host IP
    hostip = strdup(host_port) ;//IP is checked when connecting. Therefore, There is no handling.
   
    
    // get port number
    port_num = atoi(colon_ptr + 1);
    if (port_num < 1023 || port_num > 49151) {
        fprintf(stderr, "Range of port number should be [1024, 49150]\n") ;
        return 1;
    }
    
    
    // get and set user-command
    //char * user_command = argv[optind + 1] ; 
    char *user_command = argv[2];
    if ((ch.command = get_cmd_code(user_command)) == N_cmd) { // set command on the header
        fprintf(stderr, "Wrong command.\n") ;
        return 1;
    }
    if( (ch.command == get || ch.command == put) && argc < 5){
	print_usage();
	return 1;
    }
    // set file path and destination directory 
    if (ch.command == get || ch.command == put) {
        src_path = (char *) malloc(strlen(argv[3]) + 1) ;
        if (src_path == NULL) {
            fprintf(stderr, "Failed to allocate a memory...\n") ;
            return 1;
        }
        strcpy(src_path, argv[3]) ;
        src_path[strlen(src_path)] = '\0' ;
        
        dest_dir = (char *) malloc(strlen(argv[4]) + 1) ;
        if (dest_dir == NULL) {
            fprintf(stderr, "Failed to allocate a memory...\n") ;
            return 1;
        }
        strcpy(dest_dir, argv[4]) ;
        dest_dir[strlen(dest_dir)] = '\0' ;
    }

    // check if options were provided
    if (hostip == NULL || port_num == -1 || user_command == NULL) {
        print_usage() ;
        return 1;
    }

    if (ch.command == get || ch.command == put) {
        if (src_path == NULL || dest_dir == NULL) {
            print_usage() ;
            return 1;
        }
    }

    return 0;
}

char *
parse_directory(char * toparse) 
{
    char * parsed_dir = dirname(toparse) ;
    return parsed_dir ;
}

int
request(const int sock_fd)
{
    /*  TASK : header + payload 설정하고 server 에 send
        - list                : send "list" + 0 + 0 + NULL
        - get a/hello.txt b/c : send "get" + strlen("a/hello.txt") + strlen(b/c) + a/hello.txt
        - put x/hi.txt y/z    : send "put" + sizeof("x/hi.txt") + content of "x/hi.txt"
    */
    int sent ;
    int sent_check = sizeof(ch) ;
    char * chp = (char *) &ch ;
    char * ptr ;
    if (ch.command == list) {
        ch.payload_size = 0 ;
        ch.src_path_len = 0 ;
        ch.des_path_len = 0 ;
        while (sent_check > 0 && (sent = send(sock_fd, chp, sent_check, 0)) > 0) { // send header
            chp += sent ;
            sent_check -= sent ;
        }
       	if(sent < 0 ){
		fprintf(stderr,"ERROR: send()\n");
		return 1;
	}	
        printf(">> List request completed!\n") ;
    } else if (ch.command == get) {
        ch.src_path_len = strlen(src_path) ;
        ch.des_path_len = 0 ;
        ch.payload_size = ch.src_path_len + ch.des_path_len ;
        while (sent_check > 0 && (sent = send(sock_fd, chp, sent_check, 0)) > 0) { // send header
            chp += sent ;
            sent_check -= sent ;
        }
        sent_check = ch.payload_size ;
        ptr = src_path ;
        while (sent_check > 0 && (sent = send(sock_fd, ptr, sent_check, 0)) > 0) { // send payload
            ptr += sent ;
            sent_check -= sent ;
        }
        printf(">> Get request completed!\n") ;
    } else { // ch.command == put
        ch.src_path_len = strlen(src_path) ;
        ch.des_path_len = strlen(dest_dir) ;

        struct stat filestat ;
        if (lstat(src_path, &filestat) == -1) {
            fprintf(stderr, "Failed to get a file status of %s\n", src_path) ;
            return 1;
        }
        ch.payload_size = ch.src_path_len + ch.des_path_len + filestat.st_size ;
        
        FILE * fp = fopen(src_path, "rb") ;
        if (fp == NULL) {
            fprintf(stderr, "Failed to open a file %s\n", src_path) ;
            return 1;
        }
        
        while (sent_check > 0 && 0 < (sent = send(sock_fd, chp, sent_check, 0))) { // send header
            chp += sent ;
            sent_check -= sent ;
        } 

        send_payload = (char *) malloc(ch.src_path_len + ch.des_path_len + 1) ;
        strcpy(send_payload, src_path) ;
        strcat(send_payload, dest_dir) ;

        ptr = send_payload ;
        sent_check = ch.src_path_len + ch.src_path_len ;
        while (sent_check > 0 && 0 < (sent = send(sock_fd, ptr, sent_check, 0))) {
            ptr += sent ;
            sent_check -= sent ;
        }

        char buf[buf_size] ;
        int read_size = 0;
        while ((read_size = fread(buf, 1, buf_size, fp)) > 0) {
            if ((sent = send(sock_fd, buf, read_size, 0)) < 0) { // send payload
                perror("send error file data : ") ;
                fclose(fp) ;
                return 1;
            }
        }

        free(send_payload) ;
        fclose(fp) ;
        printf(">> Put request completed!\n") ;
    }
    return 0;
}


int
receive_list_response(int sock_fd) 
{
    // receiving can be done multiple times
    int received ;
    do {
        if ((received = recv(sock_fd, &sh, sizeof(sh), 0)) != sizeof(sh)) { // receive server header
            if (received == 0) {
                continue ;
            }
            perror("receive error : ") ;
            return 1;
        }
        if (sh.is_error != 0) {
            perror("response error : ") ;
            return 1;
        }
        
        char buf[buf_size] ;
        int len = 0 ;

	fprintf(stderr, "payload: %d\n",sh.payload_size);
        if (sh.payload_size <= buf_size) {
            if ((received = recv(sock_fd, buf, sh.payload_size, 0)) != sh.payload_size) {
                perror("receive error 2 : ") ;
                return 1;
            }
            buf[sh.payload_size] = '\0' ;
            printf("> %s\n", buf) ;
        } else {
            while ((received = recv(sock_fd, buf, buf_size, 0)) > 0) {
                if (recv_payload == 0x0) {
                    recv_payload = (char *) malloc(received) ;
                    memcpy(recv_payload, buf, received) ;
                    len = received ;
                } else {
                    recv_payload = realloc(recv_payload, len + received) ;
                    memcpy(recv_payload + len, buf, received) ;
                    len += received ;
                }
            }
            buf[ch.payload_size] = '\0' ;
            printf("> %s\n", recv_payload) ;
            free(recv_payload) ;
        }
    } while (received > 0) ;
    return 0;
}

int
make_directory(char * towrite) 
{
    char * temp = (char *) malloc(strlen(towrite) + 1) ;

    for (int i = 0; i <= strlen(towrite); i++) {
        if (towrite[i] == '/') {
            strncpy(temp, towrite, i) ;
            temp[i] = '\0' ;
            printf("dir : %s\n", temp) ; 

            DIR * dir = opendir(temp) ;
            if (dir) { // if a directory exists
                closedir(dir) ;
            } else if (errno == ENOENT) { // if a directory does not exist
                if (mkdir(temp, 0776) == -1) {
                    fprintf(stderr, "Failed to make a new directory %s!\n", temp) ;
                    free(temp) ;
                    return 1;
                }
            }
        }
    }
    free(temp) ;
    return 0;
}

int
receive_get_response(int sock_fd)
{
    int received ;
    if ((received = recv(sock_fd, &sh, sizeof(sh), 0)) != sizeof(sh)) { // receive server header
        perror("receive server header error : ") ;
        return 1;
    } 
    if (sh.is_error != 0) {
        perror("response error : ") ;
        return 1;
    }
    
    int file_len = strlen(dest_dir) + 1 + strlen(basename(src_path)) + 1 ;
    char * file_towrite = (char *) malloc(file_len) ;
    snprintf(file_towrite, file_len, "%s/%s", dest_dir, basename(src_path)) ;

    make_directory(file_towrite) ;

    FILE * fp = fopen(file_towrite, "wb") ;
    if (fp == NULL) {
        fprintf(stderr, "Failed to open a file %s!\n", file_towrite) ;
        free(file_towrite) ;
        return 1;
    }

    char buf[buf_size] ;
    while ((received = recv(sock_fd, buf, buf_size, 0)) > 0) {
        if (fwrite(buf, 1, received, fp) < 0) {
            fprintf(stderr, "Failed to write a file %s!\n", file_towrite) ;
            free(file_towrite) ;
            fclose(fp) ;
            return 1;
        }
    }

    printf(">> Writing %s succesfully completed!\n", file_towrite) ;

    free(file_towrite) ;
    fclose(fp) ;
    return 0;
}

int
receive_put_response(int sock_fd) 
{
    int received ;
    if ((received = recv(sock_fd, &sh, sizeof(sh), 0)) != sizeof(sh)) {
        perror("receive error : ") ;
        return 1;
    }
    if (sh.is_error != 0) {
        perror("response error : ") ;
        return 1;
    }
    printf(">> Writing %s on server succesfully completed!\n", basename(src_path)) ;
    return 0;
}

void
receive_response(int sock_fd)
{
    if (ch.command == list)
        receive_list_response(sock_fd) ;
    else if (ch.command == get) 
        receive_get_response(sock_fd) ;
    else // ch.command == put
        receive_put_response(sock_fd) ;
}


#ifndef UNITTEST 
int
main(int argc, char * argv[])
{
    if(get_option(argc, argv)){
	fprintf(stderr, "ERROR: get_option()\n");
	return 1;
    }
    
    /* Connect socket */

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0) ;
    if (sock_fd <= 0) {
        perror("socket failed : ") ;
        exit(EXIT_FAILURE) ;
    }

    struct sockaddr_in serv_addr ;
    memset(&serv_addr, '\0', sizeof(serv_addr)) ;
    serv_addr.sin_family = AF_INET ;
    serv_addr.sin_port = htons(port_num) ;
    if (inet_pton(AF_INET, hostip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton failed : ") ;
        exit(EXIT_FAILURE) ;
    }

    if (connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect failed : ") ;
        exit(EXIT_FAILURE) ;
    }

    
    /* Send the request to the server */
    
    request(sock_fd) ;
    shutdown(sock_fd, SHUT_WR) ;


    /* Receive response from the server */

    receive_response(sock_fd) ;

    // 이렇게 설정하니 receive_get_response 받기 전에 server 에서 sending 할 때 broken pipe error 뜸
    // if (ch.command == list) 
    //     receive_list_response(sock_fd) ;
    // else if (ch.command == put) 
    //     receive_get_response(sock_fd) ; 
    
    return 0 ;
}
#endif

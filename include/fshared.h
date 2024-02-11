
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


int send_bytes(int, char*, size_t);
int directory_check(char*);
void get_option(int, char**);
void list_response(char *, const int);
char *parse_directory(char *);
void get_response(int);
void make_directory(char *);
void put_response(int);
void *go_thread(void *);

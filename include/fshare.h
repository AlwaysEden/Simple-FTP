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

extern char * recv_payload ;
extern char * send_payload ;

extern client_header ch ;
extern server_header sh ;

extern char * hostip ;
extern int port_num ;
extern char * src_path ;
extern char * dest_dir ;

extern const int buf_size ;


int get_option(int, char**);
cmd get_cmd_code(char *);
void print_usage();
char *parse_directory(char *);
int request(const int);
int receive_list_response(int);
int make_directory(char *);
int receive_get_response(int);
int receive_put_response(int);
void receive_response(int);


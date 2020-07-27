#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <dirent.h>
// #include <stdint.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>


struct packet{
  unsigned char protocol[5];
  unsigned char type;
  unsigned int length;
} __attribute__((packed));

struct config_details{
  unsigned int n;
  unsigned int k;
  unsigned int blk_size;
} __attribute__((packed));


typedef unsigned char u8;



int send_file_message(int target_sd, char *file_name, int file_size);
int recieve_file_message(int target_sd, char*file_name, int file_size);
struct packet construct_header(unsigned char type, int length);
int my_sendfile(int target_sd, int file_fd, off_t *offset, int file_size);
int my_sendfile_encode(int target_sd[], char *message, int n, int k, int len);
int send_endcode_file(int target_sd[], char *file_name, int file_size, int n, int k, int len);
int recieve_from_servers(int target_sd[], int is_connected[], char*file_name, int n, int k, int len);
int stripe_handler(int target_sd[], int is_connected[],u8 **data_block, int k, int n, int len);
static int gf_gen_decode_matrix_simple(u8 * encode_matrix, u8 * decode_matrix, u8 * invert_matrix, u8 * temp_matrix, u8 * decode_index, u8 * frag_err_list, int nerrs, int k, int m);

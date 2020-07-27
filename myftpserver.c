#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "myftp.h"

#define MAXTHREAD 10

struct file_title{
  char file_name[30];
  int file_title_len;
  int file_size;
};
struct file_data{
  struct file_title *list_ptr[100];
  int file_num;
};

int no_of_connection = 0;
pthread_mutex_t mutex;
int n, k, server_id, block_size, port_number;

void *pthread_connection(void *pthread_package);
char *protocol_to_string(void *protocol, int size);
char *readdir_to_string(char *dir);
char *log_to_string(struct file_data *file_arr);

int main(int argc, char *argv[]){
  //variable declaration
  
  
  pthread_mutex_init(&mutex, NULL);
  
  
  //error when no port argument
    if (argc != 2){
        if (argc < 2)
            printf("Error: Too few arguments\n");
        if (argc > 2)
            printf("Error: Too many arguments\n");
        printf("The command-line interface is: ./myftpserver serverconfig.txt\n");
        exit(1);
    }
    if (strcmp("serverconfig.txt", argv[1]) != 0){
	    printf("Error: Wrong config file name!\n");
	    exit(1);
    }
  //error when invalid port number
  /*if((PORT = atoi(argv[1])) == 0){
    printf("bad port '%s'\n",argv[1]);
    exit(1);
  }*/
    char *config_name = argv[1];
    char buff[10];
    FILE *fp = fopen(config_name, "r");
    n = atoi(fgets(buff, 10, fp));      //number of coded blocks
    k = atoi(fgets(buff, 10, fp));      //number of fixed-size blocks
    server_id = atoi(fgets(buff, 10, fp));
    block_size = atoi(fgets(buff, 10, fp));
    port_number = atoi(fgets(buff, 10, fp));
    printf("server is runing with configuration:\n");
    printf("n = %d\nk = %d\nserver_id = %d\nBlock size = %d\nPort number = %d\n", n, k, server_id, block_size, port_number);
    printf("server start ...\n");
    fclose(fp);

  int server_sd; //server(here) socket_descriptor
  int client_sd; //client socket descriptor
  int *client_socket_ptr;
  int addr_len;
  long val = 1;

  //sockaddr_in conatin short sin_family, short sin_port, string sin_zero, struct sin_addr
  struct sockaddr_in server_addr;
  struct sockaddr_in client_addr;
  

  server_sd = socket(AF_INET, SOCK_STREAM, 0);
  //Enable reusable server port from tutor3
  if (setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1){
      perror("setsockopt");
      exit(1);
  }
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  //htonl/htons = (host to network)long/short || ntohl/ntohs = (network to host)long/short
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port_number);
  // if cannot bind the server_addr information, print error message and exit
  if (bind(server_sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
    printf("bind error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }

  if (listen(server_sd, 10) < 0){
    printf("listen error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }
  //pthread loop
  addr_len = sizeof(struct sockaddr_in);
  while (1){
    client_sd = accept(server_sd, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);
    // printf("clienet_sd = %d\n", client_sd);
    pthread_t current_thread;
    pthread_mutex_lock(&mutex);
    no_of_connection++;
    pthread_mutex_unlock(&mutex);
    if (no_of_connection <= 10)
        printf("Number of clients is: %d\n", no_of_connection);
    
    char clientname[BUFSIZ];
    fprintf(stderr, "client %s is connected\n", inet_ntop(AF_INET, &client_addr.sin_addr, clientname, sizeof(clientname)));
    pthread_create(&current_thread, NULL, pthread_connection, (void *)(int*)&client_sd);
  }
  return 0;
}

//pthread_package currently contain an integer size client socket descriptor
void *pthread_connection(void *pthread_package){
  //variable declarition
  int client_sd = *(int *)pthread_package;
  int new_log = 1;
  //int k = 3, n = 5, len = 1024;
  unsigned int package_length;

  FILE * list_fd;
  //recieve request packet from client

  char client_request[100];
  char file_name[50] = "./data/";
  struct packet client_packet;
  struct file_data file_arr;
  
  //Handle maximum connection error
  if (no_of_connection > MAXTHREAD){
      printf("Maximum connections is reached.\n");
      close(client_sd);
      pthread_mutex_lock(&mutex);
      no_of_connection--;
      pthread_mutex_unlock(&mutex);
      pthread_exit(NULL);
  }

  //verify config_details
  struct config_details client_config;
  package_length = recv(client_sd, &client_config, sizeof(client_config), 0);
  if(client_config.blk_size == block_size &&
     client_config.n == n &&
     client_config.k == k){
       send(client_sd, "OK", 2, 0);
  }
  else{
    send(client_sd, "NOK", 3, 0);
    close(client_sd);
    pthread_mutex_lock(&mutex);
    no_of_connection--;
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
  }
  
  package_length = recv(client_sd, client_request, sizeof(client_request), 0);
    
  memcpy(&client_packet, client_request, sizeof(client_packet));
  memcpy(file_name + strlen(file_name), client_request + sizeof(client_packet), sizeof(file_name));
  // printf("length = %ld\n", client_packet.length);
  client_packet.length = ntohl(client_packet.length);
  // check is it a new file_list.log

  file_arr.file_num = 0;
  //if the log file is exist
  //new_log to 0 = do not need to create new file
  //read the metadate of file
  if (access("./data/file_list.log", F_OK) != -1){
      
    new_log = 0;
    list_fd = fopen("./data/file_list.log", "r");
    // printf(" I am in ?\n");
    if (fscanf(list_fd,"%d", &file_arr.file_num) != EOF){

      // printf(" I am in ?\n");
      for (int i = 0; i < file_arr.file_num; i++){
        file_arr.list_ptr[i] = malloc(sizeof(struct file_title));
        fscanf(list_fd, "%s", file_arr.list_ptr[i]->file_name);
        fscanf(list_fd, "%d", &file_arr.list_ptr[i]->file_title_len);
        fscanf(list_fd, "%d", &file_arr.list_ptr[i]->file_size);

        // printf("%s %d %d\n", file_arr.list_ptr[i]->file_name,
        //                   file_arr.list_ptr[i]->file_title_len,
        //                   file_arr.list_ptr[i]->file_size);
      }      
    }
    fclose(list_fd);
  }
  


  // printf("file_arr.file_num = %d\n",file_arr.file_num);
  //check the the protocol type
  //case "myftp"
  
  // if (strcmp(protocol_to_string(client_packet.protocol, sizeof(client_packet.protocol)), "myftp") == 0){
    //printf("the protocol is myftp\n");
    //LIST request
    if (client_packet.type == 0xA1){
      //send the file_list to client
      char * file_list = log_to_string(&file_arr);
      struct packet respone = construct_header(0xA1, sizeof(struct packet) + strlen(file_list) + 1);
      if ((send(client_sd, (char*) &respone, sizeof(respone), 0)) < 0){
        printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
        pthread_mutex_lock(&mutex);
        no_of_connection--;
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
      }
    
      if ((send(client_sd, file_list, strlen(file_list) + 1, 0)) < 0){
        printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
        pthread_mutex_lock(&mutex);
        no_of_connection--;
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
      }
      free(file_list);
    }
    //GET request
    else if (client_packet.type == 0xB1){
      int access = 0;
      int file_id = -1;
      char confirm[3];
      for (int i = 0; i < file_arr.file_num; i++){
        // printf("i\n");
        // printf("file name = %s\n", file_name + 7);
        // printf("compare = %d\n", memcmp(file_arr.list_ptr[i]->file_name, (file_name + 7),11));
        if(!memcmp(file_arr.list_ptr[i]->file_name, (file_name + 7),file_arr.list_ptr[i]->file_title_len)){
          access = 1;
          file_id = i;
          break;
        }
      }
      //file name not in log file
      if (!access){
        // printf("cant access\n");
        struct packet respone = construct_header(0xB3, sizeof(struct packet));
        if ((send(client_sd, (char*) &respone, sizeof(respone), 0)) < 0){
          printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
          pthread_mutex_lock(&mutex);
          no_of_connection--;
          pthread_mutex_unlock(&mutex);
          pthread_exit(NULL);
        }        
      }
      else{
        int stripe_num;
        int file_size = file_arr.list_ptr[file_id]->file_size;
        //0xB2 header
        struct packet respone = construct_header(0xB2, sizeof(struct packet));
        if ((send(client_sd, (char*) &respone, sizeof(respone), 0)) < 0){
          printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
          pthread_mutex_lock(&mutex);
          no_of_connection--;
          pthread_mutex_unlock(&mutex);
          pthread_exit(NULL);
        }   

        //cal number of stripe
        stripe_num = file_size / (k * block_size);
        if ((k * block_size) - (file_size % (k * block_size)) > 0){
          stripe_num += 1;
        }
        int send_bytes;
        /*PROBLEAM !!!*/
        struct packet file_header = construct_header(0xFF, sizeof(struct packet) + file_size);
        if ((send(client_sd, (char*) &file_header, sizeof(file_header), 0)) < 0){
          printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
          pthread_mutex_lock(&mutex);
          no_of_connection--;
          pthread_mutex_unlock(&mutex);
          pthread_exit(NULL);
        }
        //check client is ready for stripe
        if(recv(client_sd, &confirm, 2, 0) > 0){
          // printf("arrive?\n");
          if(strcmp(confirm, "OK") == 0){
            int curr_i = 0;
            int recv_i;
            for (; curr_i < stripe_num;){
              char stripe_name[50];
              strcpy(stripe_name, file_name);
              sprintf(stripe_name + strlen(stripe_name), ".stripe_%d", curr_i + 1);
              // printf("%s\n", stripe_name);
              send_bytes = send_file_message(client_sd, stripe_name, block_size);
              recv(client_sd, &recv_i, sizeof(int), 0);
              // if(strcmp(confirm, "OK") == 0){
              //   printf("stripe_id = %d finished\n", i);
              // }
              if(recv_i == curr_i){
                // printf("stripe_id = %d finished\n", curr_i);
                curr_i += 1;
              }
            }
          }

        }
        // if ((send_bytes = send_file_message(client_sd, file_name, file.st_size)) > 0);
      }
    }
    //PUT request
    else if (client_packet.type == 0xC1){

      struct packet respone = construct_header(0xC2, sizeof(struct packet));
      if ((send(client_sd, (char*) &respone, sizeof(respone), 0)) < 0){
        printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
        pthread_mutex_lock(&mutex);
        no_of_connection--;
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
      }
      else{

        //receive 0xFF header
        int file_size;
        int receive_size;
        int stripe_num;
        struct packet file_header;
        if(recv(client_sd, (char*) &file_header, sizeof(struct packet), 0) <= 0){
          printf("server put reply error \n");
          pthread_mutex_lock(&mutex);
          no_of_connection--;
          pthread_mutex_unlock(&mutex);
          pthread_exit(NULL);
        }
        // printf("\n did I recv?\n");
        //cal the number of stripe
        file_header.length = ntohl(file_header.length);
        file_size = file_header.length - sizeof(file_header);
        stripe_num = file_size / (k * block_size);
        if ((k * block_size) - (file_size % (k * block_size)) > 0){
          stripe_num += 1;
        }
        // printf("stripe_num = %d\n",stripe_num);
        int recv_i;
        int curr_i = 0;
        for (; curr_i < stripe_num;){
          char stripe_name[50];
          // char confirm[3];
          strcpy(stripe_name, file_name);
          sprintf(stripe_name + strlen(stripe_name), ".stripe_%d", curr_i + 1);
          // printf("%s\n", stripe_name);
          receive_size = recieve_file_message(client_sd, stripe_name, block_size);
          recv(client_sd, &recv_i, sizeof(int), 0);
          // if(strcmp(confirm, "OK") == 0){
          //     printf("stripe_id = %d finished\n", i);
          // }
          if(recv_i == curr_i){
            // printf("stripe_id = %d finished\n", curr_i);
            curr_i += 1;
          }
        }
        //check if this file is new or update
        int new_file = 1;
        for (int i = 0; i < file_arr.file_num; i++){
          if (!memcmp(file_arr.list_ptr[i]->file_name, (file_name + 7),file_arr.list_ptr[i]->file_title_len)){
            
            // printf("here?\n");
            new_file = 0;
            file_arr.list_ptr[i]->file_size = file_size;
          }
        }
        // if it is new file
        if(new_file){
          
          //add new file_title to list 
          //file_arr.file_num add one new file title
          file_arr.list_ptr[file_arr.file_num] = malloc(sizeof(struct file_title));
          strcpy(file_arr.list_ptr[file_arr.file_num]->file_name, file_name + 7);
          file_arr.list_ptr[file_arr.file_num]->file_size = file_size;
          file_arr.list_ptr[file_arr.file_num]->file_title_len = strlen(file_name + 7);
          file_arr.file_num += 1;
        }
        
      }
      //log file file_list.log
      /*
      The format is 
      file name 30bytes + file name size 5bytes, + file size + /r/n
      */
       //create file discriptor for file log
      //list_fd = open("./data/file_list.log", O_CREAT | O_APPEND | O_RDWR, 0766);
      
      //if already have previous data, read
      if (new_log){
        list_fd = fopen("./data/file_list.log", "a");
      }
      else{
        list_fd = fopen("./data/file_list.log", "r+");
      }
      if (file_arr.file_num > 0){
        fseek(list_fd, 0, SEEK_SET);
        fprintf(list_fd, "%d\n", file_arr.file_num);
        for( int i = 0; i < file_arr.file_num ; i++){
          char log_title[100];
          sprintf(log_title, "%s",file_arr.list_ptr[i]->file_name);
          memset(log_title + strlen(log_title), ' ', (30 - strlen(log_title)));
          sprintf(log_title + 30, "%3d",file_arr.list_ptr[i]->file_title_len);
          memset(log_title + 33, ' ', 2);
          sprintf(log_title + 35, "%d", file_arr.list_ptr[i]->file_size);
          fprintf(list_fd,"%s\n", log_title);
        }

      }

      fclose(list_fd);
    }
  sleep(1);
  pthread_mutex_lock(&mutex);
  no_of_connection--;
  pthread_mutex_unlock(&mutex);
  close(client_sd);
  pthread_exit(NULL);
}

char *protocol_to_string(void *protocol, int size){
  int* convert = (int*) protocol;
  char *protocal_str = malloc(sizeof(char) * 6);
  memcpy(protocal_str, convert, size);
  protocal_str[5] = '\0';
  printf("protocol = %x\n", convert[2]);
  printf("size = %d\n", size);

  return protocal_str;
}

//take a director and return the file name inside delimited by the newline character ‘\n’.
char *readdir_to_string(char *dir){
  char *dir_list = malloc(sizeof(char) * BUFSIZ);
  int buff_size = 0;
  int skip = 0;
  DIR *dir_ptr = opendir(dir);
  struct dirent *dirent_ptr;
  if (dir_ptr != NULL){
    //combine all file name with '\n'
    while ((dirent_ptr = readdir(dir_ptr)) != NULL){
      if (dirent_ptr->d_name[0] != '.'){
        sprintf(dir_list + buff_size, "%s\n", dirent_ptr->d_name);
        buff_size += (strlen(dirent_ptr->d_name) + 1);
      }
      skip += 1;
    }
    //remove the last '\n' character
    dir_list[strlen(dir_list) - 1] = '\0';
    return dir_list;
  }
  else{
    return NULL;
  }
}

//char ptr function takes the metadata struct and return the file name list
char *log_to_string(struct file_data *file_arr){
  char *dir_list = malloc(sizeof(char) * 2048);
  int offset = 0;
  memset(dir_list, 0, 2048);
  for(int i = 0; i < file_arr->file_num; i++){
    sprintf(dir_list + offset, "%s\n", file_arr->list_ptr[i]->file_name);
    offset += (strlen(file_arr->list_ptr[i]->file_name) + 1);
  }
  dir_list[strlen(dir_list) - 1] = '\0';
  return dir_list;

}


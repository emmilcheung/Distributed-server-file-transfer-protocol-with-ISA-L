#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "myftp.h"

typedef struct server {
    char ip_address[30];
    int port;
} Server;

int main(int argc, char* argv[]){

    //variable declaration
    int PORT; //
    int length;
    struct packet request; //packet header 
    struct sockaddr_in server_addr[5];
    int sd[5]; //socket_discriptor for server
    //int k = 3, n = 5, len = 1024;
    //int p = n - k;
    char file_name[50] = ""; //file name of get / put request from command line
    fd_set allset;
    
    
    memcpy(&request.protocol, "myftp",5);
    /*Handle command-line interface errors*/
    if (argc < 3){
        printf("Error: Too few arguments\n");
        printf("The command-line interface is:\n");
        printf("./myftpclient clientconfig.txt <list|get|put> <file>\n");
        exit(1);
    }
    if (strcmp("list", argv[2]) == 0){
        request.type = 0xA1;
        if (argc > 3){
            printf("Error: Too many arguments\n");
            printf("The <file> argument only exists for the get and put commands\n");
            exit(1);
        }
    }
    else if (strcmp("get", argv[2]) == 0 || strcmp("put", argv[2]) == 0){
        if (argc != 4){
            if (argc < 4)
                printf("Error: Too few arguments\n");
            if (argc > 4)
                printf("Error: Too many arguments\n");
            printf("The command-line interface is:\n");
            printf("./myftpclient clientconfig.txt <list|get|put> <file>\n");
            exit(1);
        }
        
        if (strcmp("get", argv[2]) == 0){
            request.type = 0xB1;
            strcat(file_name, argv[3]);
        }
        if (strcmp("put", argv[2]) == 0){
            request.type = 0xC1;
            //verify the access of file
            // strcpy(file_name, "./");
            strcat(file_name, argv[3]);
            if (access(file_name, F_OK) == -1){
                printf("Non-existing file!\n");
                exit(0);
            }

        }
        // printf("str = %s\n", file_name);
    }
    else {
        printf("Error: Unknown command!\n");
        printf("The command-line interface is:\n");
        printf("./myftpclient clientconfig.txt <list|get|put> <file>\n");
        exit(1);
    }
    if (strcmp("clientconfig.txt", argv[1]) != 0){
	    printf("Error: Wrong config file name!\n");
	    exit(1);
    }
    /*if((PORT = atoi(argv[2])) == 0){
        printf("bad port '%s'\n",argv[1]);
        exit(1);
    }*/
    int n, k, block_size;
    char* config_name = argv[1];
    char buff[30];
    FILE *fp = fopen(config_name, "r");
    n = atoi(fgets(buff, 30, fp));      //number of coded blocks
    k = atoi(fgets(buff, 30, fp));      //number of fixed-size blocks
    int p = n - k;
    block_size = atoi(fgets(buff, 30, fp));
    // printf("client is runing with configuration:\n");
    // printf("n = %d\nk = %d\nBlock size = %d\n", n, k, block_size);
    // printf("clilent start..\n");

    Server server[n];
    for (int i=0; i<n; i++){
        fgets(buff, 30, fp);
        char *token = strtok(buff, ":");
        strcpy(server[i].ip_address, token);
        token = strtok(NULL, ":");
        server[i].port = atoi(token);
        // printf("Server[%d]:\tIP address = %s\t", i, server[i].ip_address);
        // printf("Port number = %d\n", server[i].port);
    }
    fclose(fp);
    
    //socket configuration from tutor 1
    int server_num = n;
    int is_connected[n];
    for (int i = 0; i < n; i++)
        is_connected[i] = 1;
    int all_connected = 1;
    // printf("servers = {");
    for (int i = 0; i < server_num; i++){
        sd[i]=socket(AF_INET,SOCK_STREAM,0);
        memset(&server_addr[i],0,sizeof(server_addr[i]));
        server_addr[i].sin_family=AF_INET;
        server_addr[i].sin_addr.s_addr=inet_addr(server[i].ip_address);
        server_addr[i].sin_port=htons(server[i].port);
        if(connect(sd[i],(struct sockaddr *)&server_addr[i],sizeof(server_addr[i]))<0){
            is_connected[i] = 0;
            all_connected = 0;
        }
        // if(is_connected[i])
        //    printf("%d ", i+1);
    }
    //printf("} is connected\n");
    
    /////////////////////////////////////////
    // sd[1]=socket(AF_INET,SOCK_STREAM,0);
    // memset(&server_addr[1],0,sizeof(server_addr[1]));
    // server_addr[1].sin_family=AF_INET;
    // server_addr[1].sin_addr.s_addr=inet_addr(argv[1]);
    // server_addr[1].sin_port=htons(2234);
    // if(connect(sd[1],(struct sockaddr *)&server_addr[1],sizeof(server_addr[1]))<0){
    //     is_connected[1] = 0;
    //     all_connected = 0;
    // }
    // server_num = 2;
    /////////////////////////////////////////


    char temp[100], list_respone[BUFSIZ];
  

    //verify config file for server and client
    struct config_details client_config;
    client_config.n = n;
    client_config.k = k;
    client_config.blk_size = block_size;
    for (int i = 0; i < server_num; i++){
        if(is_connected[i] && (length=send(sd[i],&client_config, sizeof(client_config),0))<0){
            printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);// exit(0);
        }
    }
    
    for(int i = 0; i < server_num; i++){
        memset(temp, 0, sizeof(temp));
        if (is_connected[i] ){
            if(recv(sd[i], (char *) &temp, sizeof(temp), 0) < 0){
                printf("myftp: configuration details of client and servers are not match\n");                exit(0);
            }

            if(strcmp(temp, "OK") != 0){
                printf("myftp: configuration details of client and servers are not match\n");
                exit(0);
            }
        }
    }
    
    memset(temp, 0, sizeof(temp));
    memset(list_respone, 0, sizeof(list_respone));
    request.length = sizeof(request) + strlen(file_name);
    request.length = htonl(request.length);
    memcpy(temp, &request, sizeof(request));
    memcpy(temp + sizeof(request), file_name, sizeof(file_name));
        
    if (request.type == 0xA1){
        //suppose each server have same metadata, 
        //recv from every server
        for (int i = 0; i < server_num; i++){       
            if(is_connected[i]){     
                if((length=send(sd[i],temp, sizeof(temp),0))<0){
                    printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
                    exit(0);
                }

                struct packet list_header;
                if(recv(sd[i], (char *) &list_header, sizeof(struct packet), 0) <= 0){
                    printf("myftp: connection to host %s port %d: Resource temporarily unavailable\n", argv[1], PORT);
                    exit(0);
                }
                list_header.length = ntohl(list_header.length);
                
                // printf("length = %d\n",list_header.length);
                int list_size = list_header.length - sizeof(list_header);
                if(recv(sd[i], list_respone, list_size , 0) > 0){
                    printf("%s\n",list_respone);
                }
                break;
            }
        }
        
    }
    /*GET_REQUEST*/
    else if (request.type == 0xB1){

        
        // printf("\nposition1\n");
        //send the protocol to each is_connected server
        for (int i = 0; i < server_num; i++){
            if(is_connected[i] && (length=send(sd[i],temp, sizeof(temp),0))<0){
                printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);// exit(0);
            }
        }
        // printf("\nposition 2\n");
        struct packet get_reply;
        for(int i = 0; i < server_num; i++){
            //get do not require all connection
            //mark which server is not working 
            //if less then k, reject
            if(is_connected[i] && (recv(sd[i], (char *) &get_reply, sizeof(struct packet), 0) < 0)){
                is_connected[i] = 0;
            }
            //if server log have not such file, report and exit
            if (get_reply.type == 0xB3){
                printf("No such file\n");
                exit(0);
            }


        }

        
        //count enough server for GET
        int is_On = 0;
        for(int i = 0; i < server_num; i++)
            if(is_connected[i])
                is_On += 1;

        if (is_On < k){
            printf("No enough server \n");
            exit(0);
        }
        int file_size;

        // printf("\n position 3\n");
        //recive file from server
       if((file_size = recieve_from_servers(sd, is_connected, file_name, n, k, block_size)) > 0){
            printf("%s\t\t\t\t\t\t\t100%%\t\t%d\n",file_name, file_size);

       }
    }
    /*PUT_REQUEST*/
    else if (request.type == 0xC1){
        //if any one server cannot connect, stop put
        if (!all_connected){
            printf("Servers are not ready\n");
            exit(0);
        }
        //send file name and protocol to server
        for (int i = 0; i < server_num; i++){
            if((length=send(sd[i],temp, sizeof(temp),0))<0){
                printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);// exit(0);
            }
        }
        //receive from each server
        struct packet put_reply;
        for(int i = 0; i < server_num; i++){
            if(recv(sd[i], (char *) &put_reply, sizeof(struct packet), 0) < 0){
                printf("myftp: connection to host %s port %d: Resource temporarily unavailable\n", argv[1], PORT);
                exit(0);
            }
            /*Handle non-existing file case*/
            put_reply.length = ntohl(put_reply.length); 
            if(put_reply.type != 0xC2){
                printf("myftp: no such file;\n");
                exit(0);
            }
        }

        //send 0xFF header
        int send_bytes;
        struct stat file;
        stat(file_name, &file);

        struct packet file_header = construct_header(0xFF, sizeof(struct packet) + file.st_size);

        for (int i = 0; i < server_num; i++){
            if ((send(sd[i], (char*) &file_header, sizeof(file_header), 0)) < 0){
                printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                exit(0);
            }
        }
        //0xFF payload
        if((send_bytes = send_endcode_file(sd, file_name, file.st_size, n, k, block_size)) > 0 ){
            printf("%s\t\t\t\t\t\t\t100%%\t\t%d\n",file_name, send_bytes);
        }

    }
		
	return 0;
}



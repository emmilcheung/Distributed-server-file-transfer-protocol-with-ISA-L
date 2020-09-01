#include "myftp.h"
#include <getopt.h>
#include <isa-l.h>

#define MMAX 255
#define KMAX 255

//call by server to send stripe by stripe name 
int send_file_message(int target_sd, char *file_name, int file_size){
  int fd;
  int bytes_sent;
  off_t offset = 0;
  fd = open(file_name, O_RDONLY);
  //system call the file size from file path
  int remain_bytes = file_size;
  while (((bytes_sent = my_sendfile(target_sd, fd, &offset, file_size)) > 0) && (remain_bytes > 0)){
    remain_bytes -= bytes_sent;
  }
  // printf("leaved\n");
  close(fd);
  return file_size;
}

//call by send_file_message()
int my_sendfile(int target_sd, int file_fd, off_t *offset, int file_size){
  char buff[file_size];
  int bytes;
  int bytes_sent;
  int total_bytes = 0;
  memset(&buff, 0, 0);
  while ((bytes = read(file_fd, &buff, file_size)) > 0){
    *offset = lseek(file_fd, *offset + file_size, SEEK_SET);
    bytes_sent = send(target_sd, buff, bytes, 0);
    //if send fail, send it again
    while(bytes_sent < 0){
      return -1;
    }
    total_bytes += bytes_sent;
  }
  if (total_bytes != file_size)
    return -1;
    
  return total_bytes;
}

//call by server to recieve stripe by stripe name
int recieve_file_message(int target_sd, char*file_name, int file_size){
  
    char * file_data = malloc(file_size);
    int fd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, 0766);
    int bytes_recieved;
    int remain_byte = file_size;
    send(target_sd, "NG", 2, 0);
    while((remain_byte > 0) && ((bytes_recieved = recv(target_sd, file_data, file_size, 0)) > 0)){
        write(fd, file_data, bytes_recieved);
        remain_byte -= bytes_recieved;
    }
    send(target_sd, "OK", 2 ,0 );
    close(fd);
    free(file_data);
  return file_size;
}

struct packet construct_header(unsigned char type, int length){
      struct packet respone;
      memcpy(&respone.protocol, "myftp", 5);
      respone.type = type;
      respone.length = htonl(length);

      return respone;
}

//call by client to send whole file to n servers
int send_endcode_file(int target_sd[], char *file_name, int file_size, int n, int k, int len){
  int fd;
  //int k = 3, n = 5, len = 1024;
  int p = n - k;
  int zero_blk_size = 0;
  int bytes;
  int bytes_sent;
  int stripe_num;
  int remain_bytes = file_size;
  char buff[len * n];
  off_t offset = 0;

  


  fd = open(file_name, O_RDONLY);
  //system call the file size from file path
  stripe_num = file_size / (k * len);
  if ((zero_blk_size = (k * len) - (file_size % (k * len))) > 0){
    stripe_num += 1;
  }
  // printf("stripe_num = %d, zero_blk_size = %d\n", stripe_num, zero_blk_size);
  for(int i = 0; i < stripe_num; i++){
    memset(buff, 0, n * len);
    bytes = read(fd, &buff, k * len);
    offset = lseek(fd, offset + k * len, SEEK_SET);
    // printf("bytes = %d\n", bytes);
    // printf("%ld",strlen(buff));
    //fill zero block
    // if (i == stripe_num -1){
    //   printf("buff = %c\n",buff[n * len - 2]);
    //   for (int j = 0; j < zero_blk_size; j++){
    //     buff[n *len - j - 1] = 0;
    //   }
    //   // printf(" have zero block\n");
    // }

    // printf("\n====================================================================================================\n");
    my_sendfile_encode(target_sd, buff, n, k, len);
    remain_bytes -= bytes_sent;
    for(int j = 0; j < n; j++)
      // send(target_sd[j], "OK", 2 ,0 );
      //control curr_i
      send(target_sd[j], &i, sizeof(int), 0);
  }
  return file_size;
  close(fd);
}

//call by send_endcode_file() to encode per stripe and send to n servers
int my_sendfile_encode(int target_sd[], char *message, int n, int k, int len){
  int bytes;
  int total_bytes = 0;
  int maxfd = 0;
  int done = 0; //for check all server get the 

  fd_set allset;
  

  //new try
  //int k = 3, n = 5, len = 1024;
  int p = n - k;
  int server_num = n;

  u8 *encode_matrix;
  u8 *error_matrix;
  u8 *invert_matrix;
  u8 *table;

  u8 *data_block[255];

  // Allocate coding matrices
  encode_matrix = malloc(n * k);
  table = malloc(32 * k  * p);
  // printf("can in \n");

  for(int i = 0; i < n; i++){
    data_block[i] = &message[i * len];
    // printf("%s\n",data_block[i]);
  }
  
  // printf("can pass \n");
  gf_gen_rs_matrix(encode_matrix, n, k);
  ec_init_tables(k, p, &encode_matrix[k * k], table);
  ec_encode_data(len, k, p, table, data_block, &data_block[k]);

  // printf("\ncan encode \n");
  
  //set maxfd
  for (int i = 0; i < server_num; i++)
    maxfd = (target_sd[i] > maxfd )? target_sd[i] : maxfd;

  //multiplexing
  while(true){
    char confirm[3];
    //empty allset
    FD_ZERO(&allset);
    //set each server socket to multiplexer
    for (int i = 0; i < server_num; i++)
      FD_SET(target_sd[i], &allset);
    
    select(maxfd + 1, &allset, NULL, NULL, NULL);
    // for each socket
    for (int i = 0; i < server_num; i++){
      //if it is triggered
      if(FD_ISSET(target_sd[i], &allset)){

        // printf("sd = %d\n", target_sd[i]);
        //get the confirm message first
        recv(target_sd[i], &confirm, 2, 0);
        confirm[2] = '\0';
        //if it is "NG", server is ready to recieve stripe_block
        if(strcmp(confirm, "NG") == 0){
          total_bytes += send(target_sd[i], data_block[i], len, 0);
          // printf("%s\n",data_block[i]);
        }
        //if it is "OK", server is received the stripe_block and count this server is done
        else if(strcmp(confirm, "OK") == 0)
          done += 1;
        
      }
    }
    //all server retrun "OK" message for receive, this stripe is done
    if(done >= server_num)
      break;
  }
  
  //if send fail
  while(total_bytes < 0){
    return -1;
  }
      
  return total_bytes;
}

//call by client to recieve stripes from server and write to local
int recieve_from_servers(int target_sd[], int is_connected[], char*file_name, int n, int k, int len){
  
  //int k = 3, n = 5, len = 1024;
  int p = n - k;
  int file_size;
  int maxfd = 0;
  int stripe_num;
  int active_server = 0;
  int zero_blk_size;
  struct packet file_header;
  fd_set allset;
  u8 *data_block[MMAX];
  //receive 0xFF header
  //set maxfd
  for (int i = 0; i < n; i++){
    if(is_connected[i]){
      maxfd = (target_sd[i] > maxfd )? target_sd[i] : maxfd;
      active_server += 1;
    }
  }
  while(true){    
    FD_ZERO(&allset);
    //set each server socket to multiplexer
    for (int i = 0; i < n; i++)
      if(is_connected[i])
        FD_SET(target_sd[i], &allset);
    
    select(maxfd + 1, &allset, NULL, NULL, NULL);
    /*PROBLEM !!!*/

    for (int i = 0; i < n; i++){
      //if it is triggered
      if(is_connected[i] && FD_ISSET(target_sd[i], &allset)){
        // printf("sd = %d\n", target_sd[i]);
        recv(target_sd[i], (char*) &file_header, sizeof(struct packet), 0);
        active_server -= 1;
      }
    }
    if(active_server <= 0) 
      break;
  }
  for (int i = 0; i < n; i++){
    if(is_connected[i]){
      send(target_sd[i], "OK", 2, 0);
    }
  }
  
  // cal the number of stripe
  file_header.length = ntohl(file_header.length);
  file_size = file_header.length - sizeof(file_header);
  stripe_num = file_size / (k * len);
  if ((zero_blk_size = (k * len) - (file_size % (k * len))) > 0){
    stripe_num += 1;
  }
  // printf("stripe_num = %d, zero_blk_size = %d\n", stripe_num, zero_blk_size);

  // char * file_data = malloc(file_size);
  int fd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, 0766);
  int remain_bytes = file_size;
  int wrote_bytes = 0;

  for(int i = 0; i < stripe_num; i++){
      // printf("\nstripe_num = %d\n",stripe_num);

    //0 normal , -1 error
    stripe_handler(target_sd, is_connected, data_block, k, n, len);
    if (i < stripe_num - 1){
      for (int j = 0; j < k; j++){
        // printf("i = %d\n",i);
        wrote_bytes += write(fd,data_block[j], len);
        // printf("%s",data_block[j]);
      }
    }
    else if (zero_blk_size > 0){
      remain_bytes -= wrote_bytes;
      // printf("offset = %d",file_size % (k * len));
      // printf("\nremain_bytes = %d\n",remain_bytes);
      int j = 0;
      while(remain_bytes > 0 && j < k){
        int size = (len < remain_bytes)? len: remain_bytes;
        wrote_bytes += write(fd,data_block[j], size);
        // printf("%s",data_block[j]);
        remain_bytes -= size;
        j += 1;

      }
    }
    for (int j = 0; j < n; j++){
      if(is_connected[j]){
        // send(target_sd[i], "OK", 2, 0);
        //control curr_i
        send(target_sd[j], &i, sizeof(int), 0);
      }
      
    }
    // printf("\n================================================================\n");

  }
  //printf("wrote bytes = %d\n",wrote_bytes);

  close(fd);
  // free(file_data);
  // printf("file_size %d\n", file_size);
  return wrote_bytes;
}

//call by recieve_from_servers() to recv block from n servers and decode
int stripe_handler(int target_sd[], 
                    int is_connected[], 
                    u8 **data_block, 
                    int k, 
                    int n, 
                    int len){
  int p = n - k, ret;
  int done = 0;
  int nerrs = 0;
  int maxfd;
  int active_server = 0;
  u8 *recover_srcs[KMAX];
	u8 *recover_outp[KMAX];
	u8 frag_err_list[MMAX];

  u8 *encode_matrix, *decode_matrix;
	u8 *invert_matrix, *temp_matrix;
	u8 *table;
	u8 decode_index[MMAX];

  fd_set allset;

  //construct error list fromm is_connected
  // printf("\nerror list = {");
  for (int i = 0; i < n; i++){
    if(is_connected[i]){
      active_server +=1;
    }
    else{
      frag_err_list[nerrs++] = i;
      // printf("%d ", frag_err_list[nerrs-1]);

    }
  }
  // printf("}\n");
  encode_matrix = malloc(n * k);
	decode_matrix = malloc(k * k);
	invert_matrix = malloc(k * k);
	temp_matrix = malloc(k * k);
	table = malloc(k * p * 32);

  gf_gen_rs_matrix(encode_matrix, n, k);

  //set maxfd
  for (int i = 0; i < n; i++){
    if(is_connected[i]){
      maxfd = (target_sd[i] > maxfd )? target_sd[i] : maxfd;
    }
    data_block[i] = malloc(len);
  }
  while (true){
    FD_ZERO(&allset);
    
    //set each server socket to multiplexer
    for (int i = 0; i < n; i++)
      if(is_connected[i])
        FD_SET(target_sd[i], &allset);
    
    select(maxfd + 1, &allset, NULL, NULL, NULL);

    for (int i = 0; i < n; i++){
      //this sd is active and be triggered, receive data from socket and 
      if(is_connected[i] && FD_ISSET(target_sd[i], &allset)){
        // printf("\t\trecv sd = %d\n", target_sd[i]);        

		/*BEFORE CHANGE*/
        // recv(target_sd[i], data_block[i], len, 0);

		/*AFTER CHANGE*/
		int bytes_received;
		int offset = 0;
		while((offset < len) && ((bytes_received = recv(target_sd[i], data_block[i]+offset, len, 0)) > 0)){
			// write(fd, file_data, bytes_received);
			offset += bytes_received;
		}
		
        // printf("\nblock no : %d\n%s",i,data_block[i]);
        done += 1;
        break;      
      }
    }
        // printf("\n================================================================\n");

    // printf("done = %d\n", done);
    if(done >= active_server)
      break;
  }
  for (int i = 0; i < p; i++) {
    if (NULL == (recover_outp[i] = malloc(len))) {
      printf("alloc error: Fail\n");
      return -1;
    }
  }
  for (int i = 0; i < p; i++)

  // printf("\n\t\tposition 6\n");
  // reference from tutorial 4
  ret = gf_gen_decode_matrix_simple(encode_matrix, decode_matrix,
                  invert_matrix, temp_matrix, decode_index,
                  frag_err_list, nerrs, k, n);
  // printf("\n\t\tposition 7\n");
  if (ret != 0) {
    printf("Fail on generate decode matrix\n");
    return -1;
  }
  for (int i = 0; i < k; i++)
    recover_srcs[i] = data_block[decode_index[i]];
  // printf("\n\t\tposition 8\n");
  ec_init_tables(k, nerrs, decode_matrix, table);
  ec_encode_data(len, k, nerrs, table, recover_srcs, recover_outp);
  // printf("\n\t\tposition 9\nnerrs = %d\n ", nerrs);
  for (int i = 0; i < nerrs; i++){
      if(frag_err_list[i] < k )
        memcpy(data_block[frag_err_list[i]], recover_outp[i], len);
        // printf("%s",data_block[frag_err_list[i]]);
  }
  // printf("\n\t\tposition 10\n");

  return 0;

}
// reference from tutorial 4
static int gf_gen_decode_matrix_simple(u8 * encode_matrix,
				       u8 * decode_matrix,
				       u8 * invert_matrix,
				       u8 * temp_matrix,
				       u8 * decode_index, u8 * frag_err_list, int nerrs, int k,
				       int m)
{
	int i, j, p, r;
	int nsrcerrs = 0;
	u8 s, *b = temp_matrix;
	u8 frag_in_err[MMAX];

	memset(frag_in_err, 0, sizeof(frag_in_err));

	// Order the fragments in erasure for easier sorting
	for (i = 0; i < nerrs; i++) {
		if (frag_err_list[i] < k)
			nsrcerrs++;
		frag_in_err[frag_err_list[i]] = 1;
	}

	// Construct b (matrix that encoded remaining frags) by removing erased rows
	for (i = 0, r = 0; i < k; i++, r++) {
		while (frag_in_err[r])
			r++;
		for (j = 0; j < k; j++)
			b[k * i + j] = encode_matrix[k * r + j];
		decode_index[i] = r;
	}

	// Invert matrix to get recovery matrix
	if (gf_invert_matrix(b, invert_matrix, k) < 0)
		return -1;

	// Get decode matrix with only wanted recovery rows
	for (i = 0; i < nerrs; i++) {
		if (frag_err_list[i] < k)	// A src err
			for (j = 0; j < k; j++)
				decode_matrix[k * i + j] =
				    invert_matrix[k * frag_err_list[i] + j];
	}

	// For non-src (parity) erasures need to multiply encode matrix * invert
	for (p = 0; p < nerrs; p++) {
		if (frag_err_list[p] >= k) {	// A parity err
			for (i = 0; i < k; i++) {
				s = 0;
				for (j = 0; j < k; j++)
					s ^= gf_mul(invert_matrix[j * k + i],
						    encode_matrix[k * frag_err_list[p] + j]);
				decode_matrix[k * p + i] = s;
			}
		}
	}
	return 0;
}

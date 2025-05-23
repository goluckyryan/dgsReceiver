#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "192.168.203.211"
#define SERVER_PORT 9001

#define SERVER_SUMMARY 4
#define INSUFF_DATA 5

#define ACQ_STOP -1

int debug = 0;
int netSocket = -1;

typedef struct {
   int type;		/* usually SERVER_SUMMARY */
   int recordSize;	
   int status;		/* 0 for success */
   int numRecord;	
} ServerReply;

int32_t data[10000];

int GetData(){
  int replyType;
  ServerReply reply;
  do{    
    int request = htonl(1);
    if( send(netSocket, &request, sizeof(request), 0) < 0 ){
      printf("fail to send request.\n");
    }else{
      printf("Request sent.\n");
    }
    
    int bytes_received = 0;
    do{
      bytes_received = recv(netSocket, ((char *) &reply), sizeof(ServerReply), 0);
      if (bytes_received > 0) {
        if( debug > 0){
          printf("Byte received : %d \n", bytes_received);
          printf("received data = \n");
          printf("          Type : %d\n", ntohl(reply.type) );
          printf("   Record size : %d\n", ntohl(reply.recordSize) );
          printf("        Status : %d\n", ntohl(reply.status) );
          printf("   Num. Record : %d\n", ntohl(reply.numRecord) );
        }
      } else {
        printf("No response or connection closed. retry in 1 sec.\n");
        sleep(1);
      }
    }while(bytes_received <= 0);

    replyType = ntohl(reply.type);
  
    if( replyType == SERVER_SUMMARY){
      printf("Server summary \n");
    }else if ( replyType == INSUFF_DATA){
      printf("Received Insufficient data, request again... \n");
    }else{
      printf("No data\n");
      return ACQ_STOP;
    }
    usleep(500000);  // 500,000 microseconds = 0.5 seconds
  }while( replyType != 4 );

  int recordByte = ntohl(reply.recordSize);
  int numRecord = ntohl(reply.numRecord);

  int byte_received = recv(netSocket, data, recordByte * numRecord, 0);
  int word_received = byte_received/4;
  printf("Received %d bytes = %d words\n", byte_received, word_received);

  return byte_received;
}

int WriteData(int bytes_received){
  if( bytes_received <= 0 ) return -1 ;

  FILE* file = fopen("output.bin", "ab");
  if (!file) {
    printf("Failed to open file (output.bin) for writing.\n");
    return 1;
  }

  size_t items_written = fwrite(data, 1, bytes_received, file);
  fclose(file);

  printf("Wrote %zu bytes to output.bin | received %d bytes\n", items_written, bytes_received);
  
  return 0;
}

//###############################################################
int main(int argc, char **argv) {

  // if( argc != 2){

  //   return -1;
  // }

  const int waitSec = 5;

  struct sockaddr_in server_addr;

  //============ Setup server address struct
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
    printf("Invalid address/Address not supported\n");
    return 1;
  }

  //============ attemp to estabish connection
  do{
    // Create netSocket
    netSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (netSocket < 0) {
      printf("netSocket creation failed!\n");
      return 1;
    }

    // Attempt to connect
    if (connect(netSocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
      printf("Connected to server.\n");
      break;
    } else {
      printf("Connection failed, retrying in %d seconds...\n", waitSec);
      close(netSocket);
      sleep(waitSec);
      netSocket = -1;
    }
  }while(netSocket < 0 );

  printf("netSocket = %d \n", netSocket);

  int bytes_received  = 0;
  do{
    //============ Send request and get data 
    int bytes_received = GetData();

    // for( int i = 0; i < word_received; i++){
    //   printf("%-4d | 0x%08X\n", i, data[i]);
    // }

    //============ Write data to file
    WriteData(bytes_received);

  }while(bytes_received != ACQ_STOP);


  // Close netSocket
  close(netSocket);
  return 0;
}

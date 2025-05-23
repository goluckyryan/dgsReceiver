#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

#define SERVER_IP "192.168.203.211"
#define SERVER_PORT 9001

#define SERVER_SUMMARY 4
#define INSUFF_DATA 5

#define TRIG_DATA_SIZE 16 // words

#define ACQ_STOP -1

int debug = 0;
int netSocket = -1;

struct gebData{
	int32_t type;										 /* type of data following */
	int32_t length;
	uint64_t timestamp;
};
typedef struct gebData GEBDATA;

uint32_t data[10000];
std::string runName;

#define MAX_NUM_BOARD 0xFFF
#define MAX_NUM_CHANNEL 100
FILE * outFile[MAX_NUM_BOARD][MAX_NUM_CHANNEL]; // save each channel

int GetData(){
  int replyType = 0;
  int reply[4]; // 0 = Type, 1 = Record Size in Byte, 2 = Status, 3 = num. of record
  do{    
    int request = htonl(1);
    if( send(netSocket, &request, sizeof(request), 0) < 0 ){
      printf("fail to send request. retry after 2 sec.\n");
      sleep(2);
      continue;
    }else{
      if( debug > 0 ) printf("Request sent. ");
    }
    
    int bytes_received = 0;
    do{
      bytes_received = recv(netSocket, ((char *) &reply), sizeof(reply), 0);
      if (bytes_received > 0) {
        if( debug > 1){
          printf("\n");
          printf("Byte received : %d \n", bytes_received);
          printf("received data = \n");
          printf("          Type : %d\n", ntohl(reply[0]) );
          printf("   Record size : %d Byte\n", ntohl(reply[1]) );
          printf("        Status : %d\n", ntohl(reply[2]) );
          printf("   Num. Record : %d\n", ntohl(reply[3]) );
        }
      } else {
        printf("No response or connection closed. retry in 1 sec.\n");
        sleep(1);
      }
    }while(bytes_received <= 0);

    replyType = ntohl(reply[0]);
  
    if( debug > 0 ) {
      if( replyType == SERVER_SUMMARY){
        printf("Server summary.\n");
      }else if ( replyType == INSUFF_DATA){
        printf("Received Insufficient data, request again... \n");
      }else{
        printf("No data\n");
        return ACQ_STOP;
      }
    }
    usleep(500000);  // 500,000 microseconds = 0.5 seconds
  }while( replyType != SERVER_SUMMARY );

  int recordByte = ntohl(reply[1]);
  int numRecord = ntohl(reply[3]);

  int bytes_received = recv(netSocket, data, recordByte * numRecord, 0);
  int word_received = bytes_received/4;
  printf("Received %d bytes = %d words\n", bytes_received, word_received);

  return bytes_received;
}

int DumpData(int bytes_received){
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

int WriteData(int bytes_received){

  const int words_received = bytes_received / 4;
  int index = 0;

  do{

    if(data[index] == 0xAAAAAAAA){ //==== DIG data

      int header[3];
      for( int i = 0; i < 3; i++) header[i] = ntohl(data[index+i+1]);

      int ch_id					          = (header[0] & 0x0000000F) >> 0;	// Word 1: 3..0
      int event_type				      = (header[2] & 0x03800000) >> 23;	// Word 3: 25..23

      if( ch_id >= 10 ) {
        std::string msg = "unknown";
        switch (ch_id){
          case 0xD : msg = "Run is done"; break;
          case 0xE : msg = "Empty"; break;
          case 0xF : {
            msg = "FIFO issue";
            if( event_type == 1) msg += " - overflow";
            if( event_type == 2) msg += " - underflow";
          }break;
        }
        printf("\033[34mType %X data encountered (%s). skip.\033[0m\n", ch_id, msg.c_str());
        if( debug > 0 ) for( int i = 0 ; i < 4; i++) printf("%d | 0x%08X\n", index + i, ntohl(data[index+i]));
        index += 4; // Type F data is always 4 words.
        continue;
      }

      int board_id 			          = (header[0] & 0x0000FFF0) >> 4;	// Word 1: 15..4
      int packet_length_in_words	= (header[0] & 0x07FF0000) >> 16;	// Word 1: 26..16
      int timestamp_lower 		    = (header[1] & 0xFFFFFFFF) >> 0;	// Word 2: 31..0
      int timestamp_upper 		    = (header[2] & 0x0000FFFF) >> 0;	// Word 3: 15..0
      // int header_type				      = (header[2] & 0x000F0000) >> 16;	// Word 3: 19..16

      int packet_length_in_bytes	= packet_length_in_words * 4;

      gebData GEB_data;
      GEB_data.type = 0;
      GEB_data.length = packet_length_in_bytes;
      GEB_data.timestamp  = ((uint64_t)(timestamp_upper)) << 32;
      GEB_data.timestamp |=  (uint64_t)(timestamp_lower);

      if (packet_length_in_words + index  > words_received){
        printf("\033[31m ERROR. DIG Data. Word received < packet length. \033[0m\n");
        return -2;
      }

      index += packet_length_in_words + 1; //? not sure

      
      if ( outFile[board_id][ch_id] == NULL){
        char outFileName[1000];
        sprintf (outFileName, "%s_trig_%4.4i_%01X", runName.c_str() , board_id, ch_id);
        outFile[board_id][ch_id] = fopen(outFileName, "ab");
        if (!outFile[board_id][ch_id]) {
          printf("\033[31m Failed to open file (%s) for writing. \033[0m\n", outFileName);
          return 1;
        }else{
          printf("\033[34m Opened %s \033[0m \n", outFileName);
        }
      }

      size_t items_written = fwrite(&GEB_data, 1, sizeof(gebData), outFile[board_id][ch_id]);
      items_written += fwrite(&data[index], 1, packet_length_in_bytes, outFile[board_id][ch_id]); //todo not sure
    
    }else if(data[index] == 0xAAAA0000){ //==== TRIG data

      int header[TRIG_DATA_SIZE];
      for( int i = 1; i < TRIG_DATA_SIZE; i++) header[i] = ntohl(data[index+i]);

      int ch_id					    = 0x0;
      int board_id	        = 0xF;
      int header_type       = 0xE;
      
      int packet_length_in_words  = 10;
      int packet_length_in_bytes	= packet_length_in_words * 4;

      int payload[11];

      payload[0] = 0xAAAAAAAA;
      
      payload[1]  = ch_id;
      payload[1] |= board_id << 4;
      payload[1] |= packet_length_in_words << 16;  // always 8 words payload

      payload[2]  = header[4]   ;
      payload[2] |= header[3]  << 16;
      
      payload[3]  = header[2]   ;
      payload[3] |= header_type  << 16; // header_type
      //payload[3] |= 0x0  << 23; // event_type
      payload[3] |= 3 << 26;

      payload[4] = (header[ 1] << 16) + header[ 5];
      payload[5] = (header[ 6] << 16) + header[ 7];
      payload[6] = (header[ 8] << 16) + header[ 9];
      payload[7] = (header[10] << 16) + header[11];
      payload[8] = (header[12] << 16) + header[13];
      payload[9] = (header[14] << 16) + header[15];

      gebData GEB_data;
      GEB_data.type = 0;
      GEB_data.length = packet_length_in_bytes;
      GEB_data.timestamp  = ((uint64_t)(header[2])) << 32;
      GEB_data.timestamp |= ((uint64_t)(header[3])) << 16;
      GEB_data.timestamp |=  (uint64_t)(header[4]);

      if (TRIG_DATA_SIZE + index  > words_received){
        printf("\033[31m ERROR. TRIG DATA. Word received < packet length. \033[0m\n");
        return -2;
      }

      index += TRIG_DATA_SIZE;

      if ( outFile[board_id][ch_id] == NULL){
        char outFileName[1000];
        sprintf (outFileName, "%s_trig_%4.4i_%01X", runName.c_str() , board_id, ch_id);
        outFile[board_id][ch_id] = fopen(outFileName, "ab");
        if (!outFile[board_id][ch_id]) {
          printf("\033[31m Failed to open file (%s) for writing. \033[0m\n", outFileName);
          return 1;
        }else{
          printf("\033[34m Opened %s \033[0m \n", outFileName);
        }
      }

      size_t items_written = fwrite(&GEB_data, 1, sizeof(gebData), outFile[board_id][ch_id]);
      items_written += fwrite(payload, 1, sizeof(payload), outFile[board_id][ch_id]);

      // fclose(outFile);
      // printf("Wrote %zu bytes to output.bin\n", items_written);

    }else{

      printf("\033[31m ERROR. unknown data type. dump data. index : %d \033[0m\n", index);

      // do{
      //   printf("%-4d | 0x%08X\n", index, data[index]);
      //   index ++;
      // }while(index < words_received);

      DumpData(bytes_received);
      return -99;
    }

    // printf("index : %d | %d\n", index, words_received);

  }while(index < words_received);

  //TODO how many byte written to file

  return 0;
}

//###############################################################
int main(int argc, char **argv) {

  if( argc != 2){
    printf("need a run name.\n");
    return -1;
  }

  runName = argv[1];

  for( int i = 0; i < MAX_NUM_BOARD; i++){
    for( int j = 0; j < MAX_NUM_CHANNEL; j++){
      outFile[i][j] = NULL;
    }
  }

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

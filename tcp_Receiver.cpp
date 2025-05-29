#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <time.h>

int debug = 1;

#define MAX_FILE_SIZE_BYTE 1024LL*1024*1024*2

#define TRIG_DATA_SIZE 16 // words

// #define ENABLE_GEB_HEADER

enum ReplyType{
  INSUFF_DATA = 5,
  SERVER_SUMMARY = 4
};

enum ReplyStatus{
  Insufficent_data = -1,
  Acq_stopped = -2,
  No_respone = -3
};

enum DataStatus{
  Good,
  TypeD_RunIsDone,
  DIG_inComplete,
  DIG_fileOpenError,
  TRIG_inComplete,
  TRIG_fileOpenError,
  UnknownDataType
};

int netSocket = -1;

#ifdef ENABLE_GEB_HEADER
  struct gebData{
    int32_t type;										 /* type of data following */
    int32_t length;
    uint64_t timestamp;
  };
  typedef struct gebData GEBDATA;
#endif

uint32_t data[10000];

std::string serverIP = "192.168.203.211";
int serverPort = 9001;
std::string runName;

#define MAX_NUM_BOARD 0xFFF
#define MAX_NUM_CHANNEL 100
class OutFile{
public:
  FILE * file;
  int count;
  long long fileSize;
  size_t writeByte;

  OutFile(){
    file = NULL;
    count = 0;
    fileSize = 0;
  }
  ~OutFile(){
    fclose(file);
  }

  int NewFile(std::string extraFileName, int board_id, int ch_id){
    char outFileName[1000];
    sprintf (outFileName, "%s_%03i_%4.4i_%01X%s", runName.c_str(), count, board_id, ch_id, extraFileName.c_str());
    file = fopen(outFileName, "ab");
    if (!file) {
      printf("\033[31m Failed to open file (%s) for writing. \033[0m\n", outFileName);
      return -1;
    }else{
      printf("\033[34m Opened %s \033[0m \n", outFileName);
      return 1;
    }
  }

  int OpenFile(std::string extraFileName, int board_id, int ch_id){
    if( file == NULL){
      return NewFile(extraFileName, board_id, ch_id);
    }else{
      if( fileSize > MAX_FILE_SIZE_BYTE){
        fclose(file);
        count ++;
        fileSize = 0;
        return NewFile(extraFileName, board_id, ch_id);
      }
    }
    return 0;
  }

  bool Write(const void* data, size_t size, size_t countToWrite = 1) {
    if (!file) return false;
    size_t written = fwrite(data, size, countToWrite, file);
    writeByte += written * size;
    fileSize += written * size;
    // if(written != countToWrite ) printf("write eeror\n.");
    fflush(file);
    return written == countToWrite;
  }

  size_t GetWrittenByte(){
    size_t haha = writeByte;
    writeByte = 0;
    return haha;
  }

};

OutFile outFile[MAX_NUM_BOARD][MAX_NUM_CHANNEL]; // save each channel
uint64_t totalFileSize = 0 ; //byte 

void SetUpConnection(){
  const int waitSec = 5;
  struct sockaddr_in server_addr;

  //============ Setup server address struct
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(serverPort);
  if (inet_pton(AF_INET, serverIP.c_str(), &server_addr.sin_addr) <= 0) {
    printf("Invalid address/Address not supported\n");
    return;
  }

  //============ attemp to estabish connection
  do{
    // Create netSocket
    netSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (netSocket < 0) {
      printf("netSocket creation failed!\n");
      return;
    }

    // Attempt to connect
    if (connect(netSocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
      printf("Connected to server. %s\n", serverIP.c_str());
      break;
    } else {
      printf("Connection failed %s, retrying in %d seconds...\n", serverIP.c_str(), waitSec);
      close(netSocket);
      sleep(waitSec);
      netSocket = -1;
    }
  }while(netSocket < 0 );

  // printf("netSocket = %d \n", netSocket);

}

int GetData(){ //return bytes_received.
  int replyType = 0;
  int reply[4]; // 0 = Type, 1 = Record Size in Byte, 2 = Status, 3 = num. of record

  int request = htonl(1);
  if( send(netSocket, &request, sizeof(request), 0) < 0 ){
    printf("fail to send request. \n");
    // printf("fail to send request. retry after 2 sec.\n");
    // sleep(2);
    // continue;
  }else{
    if( debug > 3 ) printf("Request sent. ");
  }
  
  int bytes_received  = recv(netSocket, ((char *) &reply), sizeof(reply), 0);
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

    replyType = ntohl(reply[0]);

    if( replyType == SERVER_SUMMARY){
      if(debug > 3 ) printf("Server summary.\n");
    }else if ( replyType == INSUFF_DATA){
      if(debug > 3 ) printf("Received Insufficient data. \n");
      return Insufficent_data;
    }else{
      printf("No data\n");
      return Acq_stopped;
    }    

    int recordByte = ntohl(reply[1]);
    int numRecord = ntohl(reply[3]);

    int total_bytes_received = 0;

    do{
      int bytes_received = recv(netSocket, &data[total_bytes_received/4], recordByte * numRecord, 0);
      int word_received = bytes_received/4;
      total_bytes_received += bytes_received;
      if( debug > 0 ) printf("Received %d bytes = %d words | total received %d Bytes, Record size %d bytes\n", bytes_received, word_received, total_bytes_received, recordByte*numRecord);
    }while(total_bytes_received < recordByte * numRecord);

    return total_bytes_received;

  } else {
    printf("No response or connection closed.\n");
    return No_respone;
  }
  
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

void PrintData(int startIndex, int endIndex){
  printf("------ data:\n");
  for( int i = startIndex; i <= endIndex ; i++){
    printf("%-6d | 0x%08X\n", i, data[i]);
  }
}

DataStatus WriteData(int bytes_received){

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

        if( ch_id == 0xD ) return TypeD_RunIsDone;

        continue;
      }

      int board_id 			          = (header[0] & 0x0000FFF0) >> 4;	// Word 1: 15..4
      int packet_length_in_words	= (header[0] & 0x07FF0000) >> 16;	// Word 1: 26..16
      
      #ifdef ENABLE_GEB_HEADER
        int timestamp_lower 		    = (header[1] & 0xFFFFFFFF) >> 0;	// Word 2: 31..0
        int timestamp_upper 		    = (header[2] & 0x0000FFFF) >> 0;	// Word 3: 15..0
        // int header_type				      = (header[2] & 0x000F0000) >> 16;	// Word 3: 19..16
        int packet_length_in_bytes	= packet_length_in_words * 4;

        gebData GEB_data;
        GEB_data.type = 0;
        GEB_data.length = packet_length_in_bytes;
        GEB_data.timestamp  = ((uint64_t)(timestamp_upper)) << 32;
        GEB_data.timestamp |=  (uint64_t)(timestamp_lower);
      #endif

      if (packet_length_in_words + index  > words_received){
        printf("\033[31m ERROR. DIG Data. Word received < packet length. \033[0m\n");
        PrintData(index, words_received-1);
        return DIG_inComplete;
      }
      
      index += packet_length_in_words + 1; //? not sure
      int packet_length_in_bytes	= packet_length_in_words * 4; 

      outFile[board_id][ch_id].OpenFile("", board_id, ch_id);
      #ifdef ENABLE_GEB_HEADER
        outFile[board_id][ch_id].Write(&GEB_data, sizeof(gebData));
      #endif
      outFile[board_id][ch_id].Write(&data[index], packet_length_in_bytes);
      totalFileSize += outFile[board_id][ch_id].GetWrittenByte();
    
    }else if(data[index] == 0xAAAA0000){ //==== TRIG data

      int header[TRIG_DATA_SIZE];
      for( int i = 1; i < TRIG_DATA_SIZE; i++) header[i] = ntohl(data[index+i]);

      int ch_id					    = 0x0;
      int board_id	        =  99;
      int header_type       = 0xE;
      
      int packet_length_in_words  = 10;
      
      int payload[10];

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
      
      #ifdef ENABLE_GEB_HEADER
        gebData GEB_data;
        GEB_data.type = 0;
        GEB_data.length = packet_length_in_bytes;
        GEB_data.timestamp  = ((uint64_t)(header[2])) << 32;
        GEB_data.timestamp |= ((uint64_t)(header[3])) << 16;
        GEB_data.timestamp |=  (uint64_t)(header[4]);
      #endif

      if (TRIG_DATA_SIZE + index  > words_received){
        printf("\033[31m ERROR. TRIG DATA. Word received < packet length. \033[0m\n");
        PrintData(index, words_received-1);
        return TRIG_inComplete;
      }

      index += TRIG_DATA_SIZE;

      outFile[board_id][ch_id].OpenFile("_trig", board_id, ch_id);
      #ifdef ENABLE_GEB_HEADER
        outFile[board_id][ch_id].Write(&GEB_data, sizeof(gebData));
      #endif
      outFile[board_id][ch_id].Write(payload, sizeof(payload));
      totalFileSize += outFile[board_id][ch_id].GetWrittenByte();

    }else{

      printf("\033[31m ERROR. unknown data type. dump data. index : %d \033[0m\n", index);

      // do{
      //   printf("%-4d | 0x%08X\n", index, data[index]);
      //   index ++;
      // }while(index < words_received);

      DumpData(bytes_received);
      return UnknownDataType;
    }

    // printf("index : %d | %d\n", index, words_received);

  }while(index < words_received);

  return Good;
}

//###############################################################
int main(int argc, char **argv) {

  if( argc != 4){
    printf("usage:\n");
    printf("%s [IP] [Port] [file_prefix]", argv[0]);
    return -1;
  }

  serverIP = argv[1];
  serverPort = atoi(argv[2]);
  runName = argv[3];

  SetUpConnection();

  time_t startTime = time(NULL);
  time_t lastPrint = startTime;
  int displayTimeIntevral = 2; //sec

  DataStatus status = Good;
  do{
    //============ Send request and get data 
    int bytes_received = GetData();

    //============ Write data to file
    if( bytes_received >= 0 ) status = WriteData(bytes_received); // it decodes the data, and save the data for each channel.
    
    //============ Status
    time_t now = time(NULL);
    if (now - lastPrint >= displayTimeIntevral) { 
      time_t elapsed = now - startTime;
      printf("Elapsed: %ld sec | totalFileSize = %llu bytes\n", elapsed, totalFileSize);
      fflush(stdout);  // Make sure it prints immediately
      lastPrint = now;
    }

    // usleep(100*1000);
  }while(status != TypeD_RunIsDone);

  printf("program ended.\n");
  
  // Close netSocket
  close(netSocket);
  return 0;
}

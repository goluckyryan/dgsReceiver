#ifndef READER_H
#define READER_H

#include <stdio.h> /// for FILE
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>
#include <time.h> // time in nano-sec
#include <stdint.h>


class Hit{
public:
  uint8_t  board;
  uint8_t  channel;
  uint16_t energy;
  uint64_t timestamp;

  //TAC-II data
  uint16_t trigType;
  uint16_t wheel;
  uint16_t multiplicity;
  uint16_t userRegister;
  uint16_t coarseTS;
  uint16_t triggerBitMask;
  uint16_t tdcOffset[4];
  uint16_t vernierAB;
  uint16_t vernierCD;

  uint8_t prePhase[4] = {3,0, 1, 2};


  void Print(){
    printf("Board : %u, Channel : %u, Energy : %u, Time : %lu\n", board, channel, energy, timestamp);
  }

  void FillTAC(uint32_t * data, bool debug = false){
    board = 99;
    channel = 0;
    timestamp = (((uint64_t) data[2] & 0xFFFF) << 16) + data[1];

    trigType     = data[3] >> 16; wheel          = data[3] & 0xFFFF;
    multiplicity = data[4] >> 16; userRegister   = data[4] & 0xFFFF;
    coarseTS     = data[5] >> 16; triggerBitMask = data[5] & 0xFFFF; 
    tdcOffset[0] = data[6] >> 16; tdcOffset[1]   = data[6] & 0xFFFF; 
    tdcOffset[2] = data[7] >> 16; tdcOffset[3]   = data[7] & 0xFFFF; 
    vernierAB    = data[8] >> 16; vernierCD      = data[8] & 0xFFFF; 

    if( debug){
      printf("--------------------------  TAC-II data --------------------------\n");
      printf("    timestamp : 0x%012lX = %lu x 10 ns\n", timestamp, timestamp );
      
      uint64_t timestamp2 = (timestamp & 0xFFFFFFFF0000) + coarseTS;
      
      printf("    Coarse TS : 0x%04X = %d x 10 ns\n", coarseTS, coarseTS);
      printf("   timestamp2 : 0x%012lX = %lu x 10 ns\n", timestamp2, timestamp2 );

      uint64_t timeDiff = 0;
      if( timestamp2 < timestamp ) {
        timeDiff = timestamp - timestamp2;
      }else{
        timeDiff = timestamp2 - timestamp;
      }

      printf("abs time diff : %ld ns\n", timeDiff * 10);
      printf("     trigType : 0x%04X\n", trigType);
      printf("        wheel : 0x%04X\n", wheel);
      printf("         User : 0x%04X\n", userRegister);
      printf("      trigger : 0x%04X\n", triggerBitMask);
      printf(" TDC offset 0 : 0x%04X\n", tdcOffset[0]);
      printf(" TDC offset 1 : 0x%04X\n", tdcOffset[1]);
      printf(" TDC offset 2 : 0x%04X\n", tdcOffset[2]);
      printf(" TDC offset 3 : 0x%04X\n", tdcOffset[3]);
      printf("   Vernier AB : 0x%04X\n", vernierAB);
      printf("   Vernier CD : 0x%04X\n", vernierCD);
      printf("==================================================================\n");
    }

  }
  
  double CalTAC(bool debug = false){
    
    int tdcRef = (coarseTS * 10) % 65536;

    uint16_t validMask = 0;
    for( int i = 0 ; i < 4; i++){
      int check =  tdcRef - ( tdcOffset[i] * 4) % 65536;
      printf("check-%d | %d \n", i, check);
      if( 0 < check && check < 350 ) validMask += (1 << i); 
    }

    if( debug) printf("Valid Mask %X \n", validMask);

    int vernier[4];
    double offset[4];

    uint8_t validBit = (vernierAB >> 12) & 0xF;
    vernier[0] =  vernierAB & 0x3F;
    vernier[1] = (vernierAB >> 6) & 0x3F;
    vernier[2] =  vernierCD & 0x3F;
    vernier[3] = (vernierCD >> 6) & 0x3F;

    if( debug) printf("VernierAB : 0x%04X => Valid 0x%X \n", vernierAB, validBit);
    int validCount = 0;
    double avgOffset = 0;
    for( int i = 0; i < 4; i++){
      if( debug) printf("Vernier-%d : 0x%02X = %3d | offset : 0x%X = %d |", i, vernier[i], vernier[i], tdcOffset[i], tdcOffset[i]);
      if( ((validBit >> i) & 0x1 ) && ((validMask >> i) & 0x1 )  ){
        offset[i] = tdcOffset[i] * 4 + prePhase[i] - 0.05 * vernier[i] ;
        validCount ++;
        avgOffset += offset[i];
        if( debug) printf("\n");
      }else{
        offset[i] = 0;
        if( debug) printf(" XXX offset %d  is not valid\n", i);
      }
    }

    // average of the offset
    avgOffset /= validCount;
    if( debug) printf("average offset : %.3f ns\n", avgOffset); 

    return avgOffset; // ns

  }

};


class Reader{
private:
  FILE * inFile;
  unsigned int inFileSize;
  unsigned int filePos;
  unsigned int totNumBlock;

  unsigned int iBlock;
  std::vector<unsigned int> blockPos;
  bool isEndOfFile;

  void init();

public:

  Reader();
  Reader(std::string fileName);
  ~Reader();

  void openFile(std::string fileName);

  int  ReadNextBlock(bool fastRead = false, bool debug = false);

  void ScanNumBlock();
  int  ReadBlock(unsigned int index, bool verbose = false);

  Hit * hit;

};


void Reader::init(){
  inFileSize = 0;
  filePos = 0;
  totNumBlock = 0;
  
  iBlock = 0;
  blockPos.clear();
  isEndOfFile = false;

  hit = new Hit();

}

Reader::Reader(){
  init();
}

Reader::Reader(std::string fileName){ 
  init();
  openFile(fileName);
}

Reader::~Reader(){
  if( inFile ) fclose(inFile);
  delete hit;
}

inline void Reader::openFile(std::string fileName){
  inFile = fopen(fileName.c_str(), "r");
  if( inFile == NULL ){
    printf("Cannot open file : %s \n", fileName.c_str());
  }else{
    fseek(inFile, 0L, SEEK_END);
    inFileSize = ftell(inFile);
    rewind(inFile);
    isEndOfFile = false;
  }
}

inline int Reader::ReadNextBlock(bool fastRead, bool debug){
  if( !inFile ) return -1;
  if( isEndOfFile ) return -10;

  uint32_t word;

  int dummy = fread(&word, sizeof(word), 1, inFile);

  if( word != 0xAAAAAAAA ) {
    printf("header problem. stop. %08X, filePos : %ld, word-index : %ld\n", word, ftell(inFile), ftell(inFile)/4);
    return -1;
  }

  if( word == 0xAAAAAAAA ){
    if( fastRead ){
      
      fseek( inFile, 9*sizeof(uint32_t), SEEK_CUR);
      if( debug ) printf("Skip.\n");

    }else{

      uint32_t data[9];

      dummy = fread(data, sizeof(uint32_t), 9, inFile);

      if( debug ) for( int i = 0; i < 9; i++) printf("%d | 0x%08X\n", i, data[i]);

      hit->FillTAC(data, debug);
      hit->CalTAC(debug);

    }
  }

  filePos = ftell(inFile);
  isEndOfFile = false;
  if( filePos >= inFileSize ) isEndOfFile = true;
  iBlock ++;
 
  return 0;
}

inline int Reader::ReadBlock(unsigned int index, bool verbose){
  if( totNumBlock == 0 ) return -1;
  if( index >= totNumBlock ) return -1;
  if( isEndOfFile ) return -10;

  fseek(inFile, 0L, SEEK_SET);

  fseek(inFile, blockPos[index], SEEK_CUR);
  iBlock = index;
  return ReadNextBlock(0, verbose);
}

inline void Reader::ScanNumBlock(){
  if( !inFile ) return;

  fseek(inFile, 0L, SEEK_CUR);
  totNumBlock = 0;
  blockPos.clear();

  filePos = 0;
  blockPos.push_back(filePos);

  while(ReadNextBlock(1, 0) == 0){
    blockPos.push_back(filePos);
    totNumBlock ++;    
  }

  printf("\nScan complete: number of data Block : %u\n", totNumBlock);
  printf("\n");

  fseek(inFile, 0L, SEEK_CUR);
  isEndOfFile = false;

}

#endif
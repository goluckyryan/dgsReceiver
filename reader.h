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
  uint64_t timestampTrig;
  uint64_t timestampTDC; // for TDC timestamp

  //TAC-II data
  uint16_t trigType;
  uint16_t wheel;
  uint16_t multiplicity;
  uint16_t userRegister;
  uint16_t coarseTS;
  uint16_t triggerBitMask;
  uint16_t offset[4];
  uint16_t vernierAB;
  uint16_t vernierCD;

  uint8_t validBit;
  int vernier[4];
  int vernierOrder;
  bool isVernierGoodOrder;
  double phaseTime[4]; // in ns
  
  bool valid[4];
  double avgPhaseTimestamp;

  uint8_t phaseOffet[4] = {3,0, 1, 2};
  
  void PrintAsIfRaw(){
    printf("-------------------- raw data -------------------\n");
    printf(" 0 | 0xAAAA\n");
    printf(" 1 | 0x%04X\n", trigType);
    printf(" 2 | 0x%04X\n", (uint16_t)(timestampTrig/10 >> 32));
    printf(" 3 | 0x%04X\n", (uint16_t)((timestampTrig/10 & 0xFFFFFFFF) >> 16));
    printf(" 4 | 0x%04X\n", (uint16_t)(timestampTrig/10 & 0xFFFF));
    printf(" 5 | 0x%04X\n", wheel);
    printf(" 6 | 0x%04X\n", multiplicity);
    printf(" 7 | 0x%04X\n", userRegister);
    printf(" 8 | 0x%04X\n", coarseTS);
    printf(" 9 | 0x%04X\n", triggerBitMask);
    printf("10 | 0x%04X\n", offset[0]);
    printf("11 | 0x%04X\n", offset[1]);
    printf("12 | 0x%04X\n", offset[2]);
    printf("13 | 0x%04X\n", offset[3]);
    printf("14 | 0x%04X\n", vernierAB);
    printf("15 | 0x%04X\n", vernierCD);
    printf("-------------------------------------------------\n");
  }

  void Print(){
    
    printf("--------------------------  TAC-II data --------------------------\n");
    // printf("Board : %u, Channel : %u\n", board, channel);
    printf("MTRGtimestamp : 0x%012lX x 10 = %lu ns\n", timestampTrig/10, timestampTrig );
    printf("    Coarse TS : 0x%012X \n", coarseTS);
    printf(" TDCtimestamp : 0x%012lX x 10 = %lu ns\n", timestampTDC/10, timestampTDC );

    uint64_t timeDiff = timestampTDC - timestampTrig;
 
    printf("abs time diff : %ld ns\n", timeDiff);
    printf("     trigType : 0x%04X\n", trigType);
    printf("        wheel : 0x%04X\n", wheel);
    printf("         User : 0x%04X\n", userRegister);
    printf("      trigger : 0x%04X\n", triggerBitMask);
    printf(" TDC offset 0 : 0x%04X x 4 ns = %d\n", offset[0], offset[0] * 4);
    printf(" TDC offset 1 : 0x%04X x 4 ns = %d\n", offset[1], offset[1] * 4);
    printf(" TDC offset 2 : 0x%04X x 4 ns = %d\n", offset[2], offset[2] * 4);
    printf(" TDC offset 3 : 0x%04X x 4 ns = %d\n", offset[3], offset[3] * 4);
    printf("   Vernier AB : 0x%04X\n", vernierAB);
    printf("   Vernier CD : 0x%04X\n", vernierCD);
    printf("--------------------\n");
    printf("Vernier-0 : 0x%02X = %u\n", vernier[0], vernier[0]);
    printf("Vernier-1 : 0x%02X = %u\n", vernier[1], vernier[1]);
    printf("Vernier-2 : 0x%02X = %u\n", vernier[2], vernier[2]);
    printf("Vernier-3 : 0x%02X = %u\n", vernier[3], vernier[3]);

    printf("==================================================================\n");

  }

  void FillTDC(uint32_t * data, bool debug = false){
    board = 99;
    channel = 0;
    timestampTrig = (((uint64_t) data[2] & 0xFFFF) << 16) + data[1];

    trigType     = data[3] >> 16; wheel          = data[3] & 0xFFFF;
    multiplicity = data[4] >> 16; userRegister   = data[4] & 0xFFFF;
    coarseTS     = data[5] >> 16; triggerBitMask = data[5] & 0xFFFF; 
    offset[0]    = data[6] >> 16; offset[1]      = data[6] & 0xFFFF; 
    offset[2]    = data[7] >> 16; offset[3]      = data[7] & 0xFFFF; 
    vernierAB    = data[8] >> 16; vernierCD      = data[8] & 0xFFFF; 

    //==== check coarseTS 
    timestampTDC = (timestampTrig & 0xFFFFFFFF0000) + coarseTS;
    if( timestampTDC < timestampTrig ) timestampTDC += 0x10000;

    timestampTrig *= 10; // convert to 10 ns
    timestampTDC *= 10; // convert to 10 ns

    validBit = (vernierAB >> 12) & 0xF;
    vernier[1] = 64 -  vernierAB & 0x3F;
    vernier[0] = 64 - (vernierAB >> 6) & 0x3F;
    vernier[3] = 64 -  vernierCD & 0x3F;
    vernier[2] = 64 - (vernierCD >> 6) & 0x3F;

    if( debug) {
      PrintAsIfRaw();
      Print();
    }

  }
  
  double CalTAC(bool debug = false){
  
    
    double haha = timestampTDC - timestampTDC % 262144;
    for( int i = 0 ; i < 4; i++){
      double kaka = offset[i] * 4; // convert to ns
      phaseTime[i] = haha + kaka; 
      valid[i] = false; // reset valid
      if( debug ) printf("phase time-%d | %.0f + %.0f = %.0f ns \n", i, haha, kaka, phaseTime[i]);
    }

    short validMask = 0x0;
    for( int i = 0; i < 4; i++){
      if( debug ) printf("----------- %d \n", i);
      short sign = 1;
      double diff = timestampTDC - phaseTime[i];
      if( debug) printf("Diff %.0f \n", timestampTDC - phaseTime[i]); 
      if ( 200 < abs(diff) && abs(diff) < 300  ) {
        validMask |= (1 << i);
        if( debug ) printf("| phase time-%d is valid.\n", i);
        valid[i] = true;
      }else{
        if( diff < 0 ) sign = -1;
        if( diff > 0 ) sign = 1;
        do{        
          phaseTime[i] += sign * 262144; 
          diff = timestampTDC - phaseTime[i];
          if( debug) printf("Diff %.0f \n", timestampTDC - phaseTime[i]); 
          if ( 200 < abs(diff) && abs(diff) < 300  ) {
            validMask |= (1 << i);
            if( debug ) printf("| phase time-%d is valid after roll-over correction.\n", i);
            valid[i] = true;
            break;
          }
        }while(abs(diff) > 262144);
        
        if( debug ) printf("| phase time-%d is NOT valid after roll-over correction.\n", i);

      }
    }

    isVernierGoodOrder = true;
    std::vector<std::pair<int, int>> vernierPair;
    for( int i = 0; i < 4; i++){
      vernierPair.push_back(std::make_pair(i, vernier[i]));
    }
    std::sort(vernierPair.begin(), vernierPair.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
      return a.second < b.second;
    });
    vernierOrder = 0;
    for( int i = 0; i < 4; i++) vernierOrder += vernierPair[i].first * pow(10, 3-i);

    if( !(vernierOrder == 123 || vernierOrder == 1230 || vernierOrder ==2301 || vernierOrder == 3012) ) isVernierGoodOrder = false;
    if( isVernierGoodOrder  && debug ) printf(" the Vernier is in good order : %d\n", vernierOrder);

    int tolerance = 4; // step
    std::vector<std::pair<int, int>> outlinerPair;
    for( int i = 0; i < 4; i ++){
      for( int j = i+1; j < 4; j++){
        int diff = vernierPair[j].second - vernierPair[i].second;
        int dist = 20 * (j-i);
        int tor = tolerance * (j-i);
        if( debug) printf("%d - %d | diff : %d | %d - %d ", j,  i, diff, dist - tor , dist + tor);

        if( abs(dist - diff) > tor) {
          outlinerPair.push_back(std::make_pair(vernierPair[i].first, vernierPair[j].first));
          if( debug) printf(" | X \n");
        }else{
          if( debug) printf(" | O \n");
        }
      }
    }


    if( debug) printf("VernierAB : 0x%04X => Valid 0x%X \n", vernierAB, validBit);
    int validCount = 0;
    avgPhaseTimestamp = 0;
    for( int i = 0; i < 4; i++){
      if( debug) printf("Vernier-%d : 0x%02X = %02d * 50 ps = %.3f ns | offset : 0x%X  * 4 = %d", i, vernier[i], vernier[i], vernier[i] * 0.05, offset[i], offset[i] *4);
      if( (validBit & (1 << i)) && (validMask & (1 << i)) ){
        phaseTime[i] +=  phaseOffet[i] - 0.05 * vernier[i] ;
        validCount ++;
        avgPhaseTimestamp += phaseTime[i];
        if( debug) printf("| OOO\n");
      }else{
        valid[i] = false;
        if( debug) printf("| XXX \n");
      }
    }

    // average of the offset
    avgPhaseTimestamp /= validCount;

    if( debug) printf("average offset : %.3f ns | diff to MTRG : %f\n", avgPhaseTimestamp, timestampTrig - avgPhaseTimestamp); 
    
    return avgPhaseTimestamp; // ns

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

  int ScanNumBlock();
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

      hit->FillTDC(data, debug);
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

inline int Reader::ScanNumBlock(){
  if( !inFile ) return 0 ;

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

  fseek(inFile, 0L, SEEK_SET);
  isEndOfFile = false;

  return totNumBlock;

}

#endif
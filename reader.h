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

  void Print(){
    printf("Board : %u, Channel : %u, Energy : %u, Time : %lu\n", board, channel, energy, timestamp);
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
  }
}

inline int ReadNextBlock(bool fastRead, bool debug){
  

}



#endif
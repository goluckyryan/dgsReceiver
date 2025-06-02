#include "reader.h"

void script(){

  //================= example 
  // Hit hit;

  // hit.timestamp  = ((uint64_t)0x003D << 32);;
  // hit.timestamp += ((uint64_t)0xDA5D << 16);
  // hit.timestamp += 0xEA84;

  // hit.trigType = 20735;
  // hit.wheel = 890;
  // hit.multiplicity = 1;
  // hit.userRegister = 4660;
  // hit.coarseTS = 60052;
  // hit.triggerBitMask = 1;
  // hit.tdcOffset[0] = 45252;
  // hit.tdcOffset[1] = 45253;
  // hit.tdcOffset[2] = 51749;
  // hit.tdcOffset[3] = 51749;
  // hit.vernierAB = 53914;
  // hit.vernierCD = 2973;


  // printf(" 1 | 0x%04X\n", hit.trigType);
  // printf(" 2 | 0x%04X\n", (uint16_t)(hit.timestamp >> 32) & 0xFFFF);
  // printf(" 3 | 0x%04X\n", (uint16_t)(hit.timestamp >> 16) & 0xFFFF);
  // printf(" 4 | 0x%04X\n", (uint16_t)hit.timestamp & 0xFFFF);
  // printf(" 5 | 0x%04X\n", hit.wheel);
  // printf(" 6 | 0x%04X\n", hit.userRegister);
  // printf(" 7 | 0x%04X\n", hit.coarseTS);
  // printf(" 8 | 0x%04X\n", hit.triggerBitMask);
  // printf(" 9 | 0x%04X\n", hit.tdcOffset[0]);
  // printf("10 | 0x%04X\n", hit.tdcOffset[1]);
  // printf("11 | 0x%04X\n", hit.tdcOffset[2]);
  // printf("12 | 0x%04X\n", hit.tdcOffset[3]);
  // printf("13 | 0x%04X\n", hit.vernierAB);
  // printf("14 | 0x%04X\n", hit.vernierCD);
  
  // hit.CalTAC();

  //================= Reader
  Reader reader("haha_000_0099_0_trig");

  // reader.ReadNextBlock(1, 1);
  // reader.ReadNextBlock(1, 1);
  // reader.ReadNextBlock(1, 1);
  // reader.ReadNextBlock(0, 1);

  reader.ScanNumBlock();

  reader.ReadBlock(0, 1);

}
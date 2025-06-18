#include "reader.h"

#include "TH1F.h"
#include "TStyle.h"
#include "TMath.h"
#include "TCanvas.h"
#include "TString.h"

uint32_t * packData(uint32_t * data){
  uint32_t * payload = new uint32_t[10];

  payload[0] = 0xAAAAAAAA;
      
  payload[1]  = 0;
  payload[1] |= 99 << 4;
  payload[1] |= 10 << 16;  // always 8 words payload
  
  payload[2]  = data[4]   ;
  payload[2] |= data[3]  << 16;  // timestamp 31:0
  
  payload[3]  = data[2]   ;  // timestamp 47:32
  payload[3] |= 0xE  << 16; // header_type
  //payload[3] |= 0x0  << 23; // event_type
  payload[3] |= 3 << 26;
  
  payload[4] = (data[ 1] << 16) + data[ 5];  // trigType, wheel
  payload[5] = (data[ 6] << 16) + data[ 7];  // multiplicity, userRegister
  payload[6] = (data[ 8] << 16) + data[ 9];  // coarseTS, triggerBitMask
  payload[7] = (data[10] << 16) + data[11];  // tdcOffset[0], tdcOffset[1]
  payload[8] = (data[12] << 16) + data[13];  // tdcOffset[2], tdcOffset[3]
  payload[9] = (data[14] << 16) + data[15];  // vernierAB, vernierCD

  return payload;
}

void script(){

  //================= example 
  // Hit hit;

  // uint32_t data[16] = {
  //   0xAAAA, //0 header
  //   0x50FF, //1 trigtype
  //   0x0000, //2 timestamp 47:32
  //   0x0B2B, //3 timestamp 31:16
  //   0x3B32, //4 timestamp 15:0
  //   0xA051, //5 wheel
  //   0x0000, //6 multiplicity
  //   0x7234, //7 userRegister
  //   0x1494, //8 coarseTS
  //   0x0001, //9 triggerBitMask
  //   0xB322, //10 tdcOffset[0]
  //   0xB323, //11 tdcOffset[1]
  //   0x517B, //12 tdcOffset[2]
  //   0x517B, //13 tdcOffset[3]
  //   0xFEA6, //14 vernierAB
  //   0x0543  //15 vernierCD
  // };

  // for( int i = 0; i < 16; i++ ) printf("%2d| 0x%04X\n", i, data[i]);


  // uint32_t * packedData = packData(data);
  // // for( int i = 0; i < 10; i++ ) printf("%2d| 0x%08X\n", i, packedData[i]);
  // hit.FillTDC(&packedData[1], true);
  // hit.CalTAC(true);

  //================= Reader


  
  
  Reader reader("haha_Ctest_2_1500Trig_000_0099_0_trig");
  
  int totBlock = reader.ScanNumBlock();
  
  reader.ReadNextBlock(0, 1);
  printf("##################################\n");
  reader.ReadNextBlock(0, 1);
  printf("##################################\n");
  reader.ReadNextBlock(0, 1);
  
  //return;
  
  int displayNANCount = 0;
  
  int plotRange = 50; // ns
  int plotRangeStart = 250;
  int plotRangeStart2 = 100;

  TH1F * h0 = new TH1F("h0", "timestampTDC - timestampTRIG; [ns]", 1000, 0, 600);
  TH1F * h1 = new TH1F("h1", "timestampTDC - phaseTime; [ns]; count / 10 ps", 100 * plotRange  ,  plotRangeStart, plotRangeStart + plotRange);
  TH1F * h4 = new TH1F("h4", "timestampTRIG - phaseTime; [ns]; count / 10 ps", 100 * plotRange  , plotRangeStart2, plotRangeStart2 + plotRange);
  TH1F * h2 = new TH1F("h2", "valid ID", 4, 0, 4);
  TH1F * h3 = new TH1F("h3", "valid count", 5, 0, 5);

  TH1F * hp[4];
  for( int i = 0 ; i < 4; i++){
//    hp[i] = new TH1F(Form("hp%d",i), Form("timestampTRIG - phaseTime[%d]; [ns]; count/ 10 ps", i), 100 * plotRange,plotRangeStart2, plotRangeStart2 + plotRange);
    hp[i] = new TH1F(Form("hp%d",i), Form("vernier[%d] ; [ns]; count/ 10 ps", i), 66 , -1, 65 );
  }
  for( int i = 0; i < totBlock; i++){
    reader.hit->Clear();
    int haha = reader.ReadNextBlock();
    if( haha < 0 ) {
      printf("Error reading block %d, code: %d\n", i, haha);
      break;
    }
    double diff = reader.hit->timestampTDC - reader.hit->avgPhaseTimestamp;
    // printf("Block %d, TDC timestamp: %ld, avg phase timestamp: %f, diff: %f\n", i, reader.hit->timestampTrig, reader.hit->avgPhaseTimestamp, diff);
    h0->Fill(reader.hit->timestampTDC - reader.hit->timestampTrig);
    h1->Fill(diff); 
    h4->Fill(reader.hit->timestampTrig - reader.hit->avgPhaseTimestamp); 
    int totalValidCount = 0;
    for ( int j = 0; j < 4; j++){
      if( reader.hit->valid[j] ) {
        h2->Fill(j);
//        hp[j]->Fill(reader.hit->timestampTrig - reader.hit->phaseTime[j]);
        hp[j]->Fill(reader.hit->vernier[j] );
        totalValidCount++;
      }
      if( totalValidCount == 0 ) {
        if( (TMath::IsNaN(diff) || !reader.hit->isVernierGoodOrder) &&  displayNANCount < 10 ) {
          reader.hit->PrintAsIfRaw();
          reader.hit->Print();
          reader.hit->CalTAC(true);
          displayNANCount++;
        }
      }
    }
    h3->Fill(totalValidCount);
  }

  TCanvas * c1 = new TCanvas("c1", "c1", 1600, 600);
  c1->Divide(3, 3);

  gStyle->SetOptStat(0xFFFF);
  c1->cd(1); c1->cd(1)->SetLogy(1); h0->Draw();
  c1->cd(2); c1->cd(2)->SetLogy(0); h1->Draw();
  c1->cd(3); c1->cd(3)->SetLogy(0); h4->Draw();
  
  c1->cd(4); hp[0]->Draw();
  c1->cd(5); hp[1]->Draw();
  c1->cd(6); h2->Draw();
  
  c1->cd(7); hp[2]->Draw();
  c1->cd(8); hp[3]->Draw();
  c1->cd(9); h3->Draw();

}

//-----------------------------------------------------------------------------
// Company:		Argonne National Laboratory
// Division:	Physics
// Project:		DGS Receiver
// Author:		Michael Oberling
// File:		dgsReceiver.cpp
// Description: Log file functions.
//-----------------------------------------------------------------------------

// Use any of the following to compile:
// Windows: CodeBlocks, add ws2_32 to the list of link libraries.
//    Project->build options->Linker settings->Link Libraries->Add->ws2_32
// For Linux, any of the following:
//g++ dgsReceiver.cpp -O3 -o dgsReceiver -lstdc++ -pedantic -Wall -Wextra -Wa,-adhln -g > dgsReceiver.s
//gcc dgsReceiver.cpp -O3 -o dgsReceiver -lstdc++ -pedantic -Wall -Wextra -Wa,-adhln -g > dgsReceiver.s
//g++ dgsReceiver.cpp -O3 -o dgsReceiver -lstdc++ -pedantic -Wall -Wextra
//gcc dgsReceiver.cpp -O3 -o dgsReceiver -lstdc++ -pedantic -Wall -Wextra

#define VERSION "6.57"
//  V6.57: Added option (DEBUG_OUTPUT_FILE) that will generate an ASCII debug file for received trigger data.
//         Also renamed the output files for triggers to help identify them in the data set more easily.
//  V6.56: Untested version for receiving and storing trigger data to file.
//  V6.56b: Work in progress.  Adding trigger readout support. Removed BIG_FILE_BUFFER. (Always on.)
//  V6.55: SAFE_DATA_PARSE option removed. (Always on.)
//  V6.54: Added error message if type F header is reported with a channel id of 9 or less.
//  V6.53: Fixed multi-chunk run files only having the last chunk's files set to read only.
//  V6.52: Significant cleanup.  No functionality changes.
//  V6.51: Added option to dump unknown data to a diagnostic output file for
//         initial testing of trigger FIFO readout.


/* This code was proudly stoled from Carl Lionberger at LBL */
/* and is meant to be an independent GRETINA data receiver */
/* for use at digital Gammasphere at ANL */


//============= START OF BUILD CONFIGURATION SWITCHES ==================//

/* WRITEGTFORMAT:   When defined writes data in GEB format.  When defined,
 * 					this will replace the SOE word with the Geb header.
 * 					When not defined the program will take one less argument
 * 					from the user as there is no need to pass a Geb ID. In this
 * 					mode the raw data, including the 0xAAAAAAAA word, is written
 * 					to disk, without any Geb headers.
*/
#define WRITEGTFORMAT
#define FOLDER_PER_RUN      // MBO 20220721: When defined, will create a separate subdirectory for each run.

const bool SAVE_TYPE_F = false; // Ryan 20250521, save type F headers, //TODO need to implement

#define DUMP_UNKNOWN_DATA_TO_DISK// MBO 20220801:  Write all unknown data to a diagnostic output file.  (Write trigger data hack enable switch.)
#define DEBUG_OUTPUT_FILE

//============= END OF BUILD CONFIGURATION SWITCHES ==================//

// #define statements are being moved here, rather than the haphazard way
// they've been added below.  Work in progress as of 12/9/2021

// BUILD PARAMETERS:
// SUMMARY_OUTPUT_INTERVAL: The delay between summary update.
#define SUMMARY_OUTPUT_INTERVAL 5
// MAXNS: Controls the minimum frequently receiver checks for data during low,
//  or no event rate.
#define MAXNS 10000			// MBO 20200615: changed from 100000 to 10000


// OTHER PARAMETERS THAT ARE EXPECTED TO RARLEY IF EVER CHANGE:
// SOE: The start of event header dataword.
#define DIG_SOE         0xAAAAAAAA
#define DIG_SOE_MASK    0xFFFFFFFF
#define TRIG_SOE        0xAAAA0000
#define TRIG_SOE_MASK   0xFFFF0000
#define ANY_SOE         0xAAAA0000
#define ANY_SOE_MASK    0xFFFF0000


// MAXBOARDID: The maximum value of the user package data.
#define MAXBOARDID 4095
// MAXCHID: The maximum channel ID value + 1.
#define MAXCHID 16
// DIG_MIN_HEADER_LENGTH_UINT32: The smallest digitizer header than can be processed, in
//	terms of 32-bit words.
#define DIG_MIN_HEADER_LENGTH_UINT32 3
// DIG_MIN_HEADER_LENGTH_BYTES: The smallest digitizer header than can be processed, in
//	terms of bytes.
#define DIG_MIN_HEADER_LENGTH_BYTES DIG_MIN_HEADER_LENGTH_UINT32 * 4
// TRIG_MIN_HEADER_LENGTH_UINT32: The smallest trigger header than can be processed, in
//	terms of 32-bit words.
#define TRIG_MIN_HEADER_LENGTH_UINT32 16
// TRIG_MIN_HEADER_LENGTH_BYTES: The smallest trigger header than can be processed, in
//	terms of bytes.
#define TRIG_MIN_HEADER_LENGTH_BYTES TRIG_MIN_HEADER_LENGTH_UINT32 * 4
#define REFORMATTED_HEADER_LENGTH_UINT32 12
#define REFORMATTED_HEADER_LENGTH_BYTES REFORMATTED_HEADER_LENGTH_UINT32 * 4

#define DIG_BOARD_ID_MASK 0xFFF

#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include <signal.h>
#include "dgsReceiver.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define CLK_TCK ((__clock_t) __sysconf (2))

#include "psNet.h"


/*
Current list of GEB types from torben as of 20211207.
Provided here for reference only.  This is provided by the user.
#define GEB_TYPE_DECOMP         1
#define GEB_TYPE_RAW            2
#define GEB_TYPE_TRACK          3
#define GEB_TYPE_BGS            4
#define GEB_TYPE_S800_RAW       5
#define GEB_TYPE_NSCLnonevent   6
#define GEB_TYPE_GT_SCALER      7
#define GEB_TYPE_GT_MOD29       8
#define GEB_TYPE_S800PHYSDATA   9
#define GEB_TYPE_NSCLNONEVTS   10
#define GEB_TYPE_G4SIM         11
#define GEB_TYPE_CHICO         12
#define GEB_TYPE_DGS           14
#define GEB_TYPE_DGSTRIG       15
#define GEB_TYPE_DFMA          16
#define GEB_TYPE_PHOSWICH      17
#define GEB_TYPE_PHOSWICHAUX   18
#define GEB_TYPE_GODDESS       19
#define GEB_TYPE_LABR          20
#define GEB_TYPE_LENDA         21
#define GEB_TYPE_GODDESSAUX    22
#define GEB_TYPE_XA            23

#define MAX_GEB_TYPE           24
*/

#define FILE_BUF_SIZE_KB 512
#define FILE_BUF_SIZE FILE_BUF_SIZE_KB*1024+8
static uint32_t min_board_id;
static uint32_t max_board_id;

static char* file_buffer[MAXBOARDID][MAXCHID];

#ifdef DEBUG_OUTPUT_FILE
  static char* diag_file_buffer[MAXBOARDID][MAXCHID];
#endif // DEBUG_OUTPUT_FILE

static int64_t max_file_size;

struct gebData{
	int32_t type;										 /* type of data following */
	int32_t length;
	uint64_t timestamp;
};
typedef struct gebData GEBDATA;

/* globals */

int32_t fp;
char fn[512];
int64_t totbytes = 0;
int8_t has_connected = 0;

int32_t debug = 1;

#ifdef WRITEGTFORMAT
	int32_t GEB_TYPE_DGS = 0;
#endif	//WRITEGTFORMAT

struct rcvrInstance{
	struct sockaddr_in adr_srvr;	/* AF_INET */
	int32_t len_inet;								 /* length	*/
	int32_t recSock;
	int32_t packetsreceived;
	int32_t packetssent;
	int32_t seqerrs;
	int32_t bytesrec;
};



//get 10M of mem
// for DGS we just store to a great big chunk then write to disk
#define DATA_MEM_SIZE 10000000
int8_t datamem[DATA_MEM_SIZE];


//deprecated for DGS
int32_t recLenGDig;

int64_t bytes_written_to_file[MAXBOARDID][MAXCHID]; // MBO 20200619:  changed to array

int64_t totbytesInLargestFile; // MBO 20200619:  new
#ifdef DUMP_UNKNOWN_DATA_TO_DISK
  FILE* diag_unknown_ofile;
#endif //DUMP_UNKNOWN_DATA_TO_DISK

#define FILE_OPEN_CHECK(file) (file != 0)
FILE* ofile[MAXBOARDID][MAXCHID];	// MBO 20200617:  Normal mode writes changed from POSIX to  ANSI C file iO

#ifdef DEBUG_OUTPUT_FILE
  #define FILE_OPEN_CHECK(file) (file != 0)
  FILE* diag_ofile[MAXBOARDID][MAXCHID];	// MBO 20200617:  Normal mode writes changed from POSIX to  ANSI C file iO
#endif // DEBUG_OUTPUT_FILE

int32_t chunck = 0;

/*----------------------------------------------------------------------*/

char * initReceiver (char *srvr_addr){

	printf("%s\n", __func__);

	struct rcvrInstance *retval;
	int32_t port;

	if (debug > 0) printf ("initReceiver\n");

	port = SERVER_PORT;

	/* create the receiver instance */
	/* which is also the return value */
	/* of this function */

	retval = (struct rcvrInstance *) calloc (sizeof (struct rcvrInstance), 1);
	if (!retval) return (0); /* error return */

	/* create server addressed socket */

	memset (&retval->adr_srvr, 0, sizeof retval->adr_srvr);
	retval->adr_srvr.sin_family = AF_INET;
	retval->adr_srvr.sin_port = htons (port);
	retval->adr_srvr.sin_addr.s_addr = inet_addr (srvr_addr);

	if (retval->adr_srvr.sin_addr.s_addr == INADDR_NONE){
		printf ("bad server address %s port %d\n", srvr_addr, port);
		return (0);
	};

	retval->len_inet = sizeof (retval->adr_srvr);
	retval->recSock = -1;

	printf ("initReceiver: will use Server addr %s and port: %d\n", srvr_addr, port);
	fflush (stdout);

	return (char *) retval;
}

/*-----------------------------------------------------------------------------*/

int32_t stopReceiver (char *instancechar){

	printf("%s\n", __func__);

	struct rcvrInstance *instance;

	if (debug > 0) printf ("stopReceiver\n");

	instance = (struct rcvrInstance *) instancechar;
	if (!instance){
		printf ("Null receiver instance in stopReceiver\n");
		return -1;
	}
	if (instance->recSock == -1) return 0;

	close (instance->recSock);
	instance->recSock = -1;
	return 0;
}


//MBO 20200617: New Function. Lets try to be generous with the socket options
void setsocketoption(int32_t sock){
	int32_t rcvbuf = 65536;
	int32_t sndbuf = 65536;
//	int32_t nodelay = 1;
	if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)(&rcvbuf), sizeof(rcvbuf))) printf("could not set SO_RCVBUF");
	if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)(&sndbuf), sizeof(sndbuf))) printf("could not set SO_SNDBUF");
//	if(setsockopt(sock, SOL_TCP, TCP_NODELAY, (char*)(&nodelay), sizeof(nodelay)))
//		printf("could not set TCP_NODELAY");
	return;
}


/*----------------------------------------------------------------------*/

int32_t getReceiverData2 (char *instancechar, int8_t **retptr, int32_t *readsize) {

  // Get data from network socket
  // there are few types of return data

  debug = 0;

	struct rcvrInstance *instance;
	struct reqPacket request;
	evtServerRetStruct firstreply;
	int32_t bytesret = 0;
	int32_t temptype;
	int32_t recsize;
	int32_t numrecs = 0;
	int32_t numret = 0;
	int32_t numbytesleft;
	if (debug > 0)	printf ("##### %s\n", __func__);

	instance = (struct rcvrInstance *) instancechar;
	if (!instance){
		printf ("Null receiver instance in getReceiverData\n");
		return -1;
	}

	if (instance->recSock == -1){

		// try again
		instance->recSock = socket (AF_INET, SOCK_STREAM, 0);
		if (instance->recSock == -1){
			printf ("Unable to open socket.\n");
			return -1;
		}

		setsocketoption(instance->recSock);

    // establish a connection to a remote server, retunr 0 for success
		if (connect (instance->recSock, (struct sockaddr *) &instance->adr_srvr, instance->len_inet) < 0){
			if (has_connected == 1) printf ("connect failed %s\n", strerror (errno));
			close (instance->recSock);
			instance->recSock = -1;
			return -1;
		}

    if (debug > 0) printf ("have socket ?  %d \n", instance->recSock);

		// MBO 20200616: Let's try queueing up 6 requests:
		/*
		for( int i = 0; i < 5; i++){
			request.type = htonl (CLIENT_REQUEST_EVENTS);
			if (send (instance->recSock, (char*)&request, sizeof (struct reqPacket), 0) < 0){
				printf ("request send failed\n");
				close (instance->recSock);
				instance->recSock = -1;
				return -1;
			}			
		}*/

		request.type = htonl (CLIENT_REQUEST_EVENTS);
    // send data to the netwrok socket
		if (send (instance->recSock, (char*)&request, sizeof (struct reqPacket), 0) < 0){
			printf ("request send failed\n");
			close (instance->recSock);
			instance->recSock = -1;
			return -1;
		}else{
			if (debug > 0) {
				printf (" sent request data=\n");
				for(unsigned int i=0; i < (sizeof (struct reqPacket)); i++){
					printf ("\t 0x%08X \n", ((char *) &request)[i]);
				}
			}
		}
	}   
  
	while (bytesret < (int32_t)(sizeof (evtServerRetStruct))){
    // receive data over network socket, return number of byte received, 0 = closed by peer, -1 error
    numret = recv (instance->recSock, ((char *) &firstreply.type) + bytesret, sizeof (evtServerRetStruct) - bytesret, 0);
		if (numret <= 0){
			if (debug > 0) printf ("Error, no more data: need: %d total: %d\n", (int32_t)(sizeof (evtServerRetStruct)) - (int32_t)(bytesret), (uint32_t)(sizeof (evtServerRetStruct)));
			break;
		}else{
			bytesret += numret;
			if (debug > 0){
				printf ("received bytes=%d, bytesret=%d | %d\n", numret, bytesret, (int32_t)(sizeof (evtServerRetStruct)));
				printf ("received data=\n");
				for(unsigned int i=0;i < (sizeof (struct reqPacket)); i++){
					printf ("%08X \n", (((char*)((void *)(&firstreply.type))) + bytesret)[i]);
				}
			}
		}
	}// end of while-loop

	if (numret <= 0){
		printf ("read returned %d\n", numret);
		return -1;
	}
	temptype = ntohl (firstreply.type) & 0x000000FF;

  if (debug > 0 ) printf( "firstreply | 0x%08X -> 0x%08X, temptype %d\n", firstreply.type, ntohl(firstreply.type), temptype);
	
	if (temptype == SERVER_SUMMARY){
    // for dgs- this is total size of data to get. not size of each indiv. record.
		recsize = ntohl (firstreply.recLen);
		numrecs = ntohl (firstreply.recs);
    
		if (debug > 0) {
      printf ("SERVER_SUMMARY \n");
		  printf ("recsize =%d numrecs =%d\n", recsize, numrecs);
    }

		/* ask for the next data */
		request.type = htonl (CLIENT_REQUEST_EVENTS);

		if (send (instance->recSock, (char*)&request, sizeof (struct reqPacket), 0) < 0){
			printf ("request send failed\n");
			close (instance->recSock);
			instance->recSock = -1;
			return -1;
		}else{
			if (debug > 0) printf ("requested CLIENT_REQUEST_EVENTS \n");
		}

	}else if (temptype == INSUFF_DATA){
    if (debug > 0) printf ("received INSUFF_DATA\n");

    /* go ahead and ask again */
    request.type = htonl (CLIENT_REQUEST_EVENTS);
    if (send (instance->recSock, (char*)&request, sizeof (struct reqPacket), 0) < 0){
      printf ("request send failed\n");
      close (instance->recSock);
      instance->recSock = -1;
      return -1;
    }else{
      if (debug > 0) printf ("sent CLIENT_REQUEST_EVENTS\n");
    }
    return -1;
  }else{
    /* No point in asking for more; we arecsize =16re bailing out */
    if (temptype == SERVER_SENDER_OFF){
      if (debug > 0) printf ("temptype == SERVER_SENDER_OFF\n");
      //printf("Sender not enabled\n");
    }else{
      printf ("Illegal first packet type %d\n", temptype);
    }

    if (debug > 0) printf ("to close socket\n");

    close (instance->recSock);
    instance->recSock = -1;
    return -1;
  }


	/* if you got here, there is data to be read */
	//recsperbuf = INBUFSIZE / (recsize * 4);

	//deg recsize is total bytes to read.. not size of one rec.
	numbytesleft = recsize;

	if (debug > 0) printf ("there is data to be read-numbytesleft =%d \n", numbytesleft);

	bytesret = 0;
	while (bytesret < numbytesleft){

    //if (debug > 0) printf ("to read data \n");
		numret = recv (instance->recSock, (char*)(datamem + bytesret), numbytesleft - bytesret, 0);

		if (debug > 0) printf ("got %d bytes	\n", numret);

		if (numret <= 0){
			break;
		}else{
			bytesret += numret;
			instance->bytesrec += bytesret;
			instance->packetsreceived++;
		}
	} // end of while

	if (numret == 0){
		printf (" End of file! \n");
		close (instance->recSock);
		instance->recSock = -1;
		return -1;
	}

	if (numret < 0){
		printf ("read returned %d\n", numret);
		return -1;
	}

	*retptr = datamem;
	*readsize = recsize;
	return 0;
}
/*----------------------------------------------------------------------*/

int32_t printPackets (char *instancechar){

	printf("#### %s\n", __func__);

	struct rcvrInstance *instance;

	instance = (struct rcvrInstance *) instancechar;

	printf ("Packets received, sent, diff, seqerrs, bytesrec	= %d %d %d %d %d\n",
					instance->packetsreceived, instance->packetssent,
					instance->packetsreceived - instance->packetssent, instance->seqerrs, instance->bytesrec);
	return 0;
}

/*----------------------------------------------------------------------*/

int32_t print_info (int64_t totbytes){

	printf("#### %s\n", __func__);
	/* declarations */

	static int64_t last_totbytes = 0;
	static int64_t tnow, tthen, tstart;
	static int32_t firsttime = 1;
	double r1, deltaTime, deltaBytes;
	int32_t i1, i;
	time_t ticks;

  int32_t j;

	if (firsttime){
		firsttime = 0;
		tstart = time (NULL);
	};

	tnow = time (NULL);

	deltaTime = tnow - tthen;
	deltaBytes = totbytes - last_totbytes;

	// printf ("totbytes=%lli\n", totbytes);
	// printf ("last_totbytes=%lli\n", last_totbytes);
	// printf ("t_now =%lli\n", tnow);
	// printf ("t_then=%lli\n", tthen);
	// printf ("deltaTime=%f\n", (float) deltaTime);
	// printf ("deltaBytes=%f\n", (float) deltaBytes);

	last_totbytes = totbytes;
	tthen = tnow;

	r1 = (double) totbytes / 1024 / 1024;
	printf ("%10.3f Mbytes; ", (float) r1);
	r1 = deltaBytes / deltaTime;
	r1 /= 1024;
	printf ("%7.0f KB/s; ", (float) r1);
	r1 = (double) (totbytes) / (double) (1024) / (double) (tnow - tstart);
	printf ("AVG: %7.0f KB/s; ", (float) r1);

  for (i = 0; i < MAXBOARDID; i++)
    for (j = 0; j < MAXCHID; j++)
      if (FILE_OPEN_CHECK(ofile[i][j])) printf ("%i-%i ",i,j);

  #ifdef DEBUG_OUTPUT_FILE
    for (i = 0; i < MAXBOARDID; i++)
        for (j = 0; j < MAXCHID; j++)
            if (FILE_OPEN_CHECK(diag_ofile[i][j])) printf ("%i-%i ",i,j);
  #endif // DEBUG_OUTPUT_FILE

	/* prime for next */


	/* time and runtime */

	ticks = time (NULL);
	printf (" %.24s; ", ctime (&ticks));
	r1 = tnow - tstart;
	i1 = r1 / 3600;
	r1 = r1 - i1 * 3600;
	r1 /= 60;
	if (r1 < 10)
		printf ("runtime: %ih %4.2fm\n", i1, (float) r1);
	else
		printf ("runtime: %ih %4.1fm\n", i1, (float) r1);

	/* done */

	fflush (stdout);
	return (0);

}

/*----------------------------------------------------------------------*/

void set_readonly(){

	printf("#### %s\n", __func__);
	/* declarations */
	int32_t data_fd;
	char str[550];

  int32_t i, j;

	/* specify readonly for everyone */
  for (i = 0; i < MAXBOARDID; i++)
    for (j = 0; j < MAXCHID; j++)
      if (FILE_OPEN_CHECK(ofile[i][j])){
                    sprintf (str, "%s_%4.4i_%1.1i", fn, i, j);
                    data_fd = open (str, O_RDWR);
                    close (data_fd);
                    ofile[i][j] = 0;
                    printf ("%s is now readonly\n", str);
                };

	#ifdef DEBUG_OUTPUT_FILE
        for (i = 0; i < MAXBOARDID; i++)
            for (j = 0; j < MAXCHID; j++)
                if (FILE_OPEN_CHECK(diag_ofile[i][j])){
                    sprintf (str, "diag_%s_%4.4i_%1.1i", fn, i, j);
                    data_fd = open (str, O_RDWR);
                    close (data_fd);
                    diag_ofile[i][j] = 0;
                    printf ("diag_%s is now readonly\n", str);
                };
  #endif // DEBUG_OUTPUT_FILE
	/* done */

	return;

}

/*----------------------------------------------------------------------*/

void set_board_readonly (int32_t board_num){

	printf("#### %s\n", __func__);
	/* declarations */

	int32_t data_fd;
	char str[550];

  int32_t j;

	/* specify readonly for everyone */

    for (j = 0; j < MAXCHID; j++)
      if (FILE_OPEN_CHECK(ofile[board_num][j]))
        {
          sprintf (str, "%s_%4.4i_%1.1i", fn, board_num, j);
          data_fd = open (str, O_RDWR);
          close (data_fd);
          ofile[board_num][j] = 0;
          printf ("%s is now readonly\n", str);
        };

	#ifdef DEBUG_OUTPUT_FILE
        for (j = 0; j < MAXCHID; j++)
            if (FILE_OPEN_CHECK(diag_ofile[board_num][j]))
                {
                    sprintf (str, "diag_%s_%4.4i_%1.1i", fn, board_num, j);
                    data_fd = open (str, O_RDWR);
                    close (data_fd);
                    diag_ofile[board_num][j] = 0;
                    printf ("%s is now readonly\n", str);
                };

  #endif // DEBUG_OUTPUT_FILE
	/* done */

	return;

}

/*----------------------------------------------------------------------*/
void close_all (void){

	printf("#### %s\n", __func__);
	time_t ticks;

  int32_t i, j;

	printf ("\n\nClosing all files at ");
	ticks = time (NULL);
	printf ("%.24s\n", ctime (&ticks));
	fflush (stdout);

    for (i = 0; i < MAXBOARDID; i++)
      for (j = 0; j < MAXCHID; j++)
        if (FILE_OPEN_CHECK(ofile[i][j]))
          {
            #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
              close (ofile[i][j]);
            #else
              fclose (ofile[i][j]);
              free(file_buffer[i][j]);
            #endif
//						ofile[j][j] = 0;
            bytes_written_to_file[i][j] = 0;
            printf ("close board file %i-%i\n", i, j);
          };


	#ifdef DEBUG_OUTPUT_FILE
        for (i = 0; i < MAXBOARDID; i++)
            for (j = 0; j < MAXCHID; j++)
                if (FILE_OPEN_CHECK(diag_ofile[i][j]))
                    {
                        #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                            close (diag_ofile[i][j]);
                        #else
                            fclose (diag_ofile[i][j]);
                            free(diag_file_buffer[i][j]);
                        #endif
//						diag_ofile[j][j] = 0;
                        bytes_written_to_file[i][j] = 0;
                        printf ("close board file %i-%i\n", i, j);
                    };
	#endif // DEBUG_OUTPUT_FILE

  totbytesInLargestFile = 0;
	set_readonly ();
	return;
}

void stop_receiver (void){

	printf("#### %s\n", __func__);

	printf ("last statistics:\n");
	print_info (totbytes);
	printf ("\nall done/quit\n\n");
	printf ("$Id: gtReceiver6.c,v %s 2021/11/23 19:51:40 tl Exp $\n", VERSION);
	exit (0);
}


void forced_stop (void){

	printf("#### %s\n", __func__);

	time_t ticks;

	printf ("\n\nForced stop at ");
	ticks = time (NULL);
	printf ("%.24s\n", ctime (&ticks));
	fflush (stdout);

	close_all();
	stop_receiver();
	return;
}

void signal_catcher (int32_t sigval){

	printf("#### %s\n", __func__);

	time_t ticks;

	printf ("\n\nreceived signal <%i> at ", sigval);
	ticks = time (NULL);
	printf ("%.24s\n", ctime (&ticks));
	fflush (stdout);

	forced_stop();
	return;
}


void exit_if_all_files_closed (void){

	printf("#### %s\n", __func__);

  int32_t i, j;

    for (i = 0; i < MAXBOARDID; i++)
      for (j = 0; j < MAXCHID; j++)
        if (FILE_OPEN_CHECK(ofile[i][j]))
          return;

	#ifdef DEBUG_OUTPUT_FILE
        for (i = 0; i < MAXBOARDID; i++)
            for (j = 0; j < MAXCHID; j++)
                if (FILE_OPEN_CHECK(diag_ofile[i][j]))
                    return;
	#endif // DEBUG_OUTPUT_FILE

	stop_receiver ();
}

void close_board (int32_t board_num){

	printf("#### %s\n", __func__);

	time_t ticks;

  int32_t j;

	printf ("\n\nEnd of data packet received for board #%i at ", board_num);
	ticks = time (NULL);
	printf ("%.24s\n", ctime (&ticks));
	fflush (stdout);


    for (j = 0; j < MAXCHID; j++)
      if (FILE_OPEN_CHECK(ofile[board_num][j]))
        {
          #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
            close (ofile[board_num][j]);
          #else
            fclose (ofile[board_num][j]);
            free(file_buffer[board_num][j]);
          #endif
//					ofile[board_num][j] = 0;
          bytes_written_to_file[board_num][j] = 0;
          printf ("close board file %i-%i\n", board_num, j);
        };

    #ifdef DEBUG_OUTPUT_FILE
          for (j = 0; j < MAXCHID; j++)
              if (FILE_OPEN_CHECK(diag_ofile[board_num][j]))
                  {
                      #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                          close (diag_ofile[board_num][j]);
                      #else
                          fclose (diag_ofile[board_num][j]);
                          free(diag_file_buffer[board_num][j]);
                      #endif
  //					diag_ofile[board_num][j] = 0;
                      printf ("close diag board file %i-%i\n", board_num, j);
                  };
    
    #endif // DEBUG_OUTPUT_FILE

	set_board_readonly (board_num);
	return ;
}

/*----------------------------------------------------------------------*/
/* rl==CryG_CS_RDLen.RVAL, get i as: */
/* caget CryG_CS_RDLen.RVAL */
/* CryG_CS_RDLen.RVAL						 20 */

void setRecLen (int32_t rl) {
	recLenGDig = rl / 2;
}
/*----------------------------------------------------------------------*/
int32_t dumpUnknownDataToDaigFile(int8_t *buffer, int32_t size2write) {

	printf("\033[31m!!!!!!!! %s \033[0m\n", __func__);
	char str[550];
	int32_t wstat = 0;
	uint32_t *buffer_uint32;
	buffer_uint32 = (uint32_t *) buffer;

    #ifndef DUMP_UNKNOWN_DATA_TO_DISK
        printf ("ooops:	event started with %08X instead of 0xAAAAXXXX.  block skipped\n", *buffer_uint32);
        return -2;
    #else
        // MBO 20220801: Quick hack to get trigger data to disk for inital testing.
        // printf ("ooops:	event started with %08X instead of 0xAAAAXXXX.  Dumping mysterious data to diagnostic file\n", *buffer_uint32);
        sprintf (str, "%s_dump_DATA", fn);
        printf( "ooops: some word in buffer is not recongined. Dump the whole buffer to %s\n", str);

        /* open file */

          diag_unknown_ofile = fopen (str, "ab");
          if (!FILE_OPEN_CHECK(diag_unknown_ofile)){
              printf ("Can't open or create diagnostic output file.");
              return -2;
          }
          

          // printf("buffer : \n");
          // uint32_t * bufferTemp = (uint32_t *) buffer;
          // for( unsigned int i = 0; i < size2write/4; i++){
          //   printf("%5d | 0x%08X\n", i, bufferTemp[i]);
          // }


          wstat = fwrite (buffer, size2write, 1, diag_unknown_ofile);
          if (wstat != size2write){
              printf("Aborting write of %d bytes due to unhandled write error.", size2write);
              return -2;
          }
          fclose(diag_unknown_ofile);
        return -2;
    #endif
}

/*----------------------------------------------------------------------*/

int32_t writeEvents2 (int8_t *buffer, int32_t size2write, int32_t *writtenBytes) {

  debug = 0;

	printf("#### %s\n", __func__);

	// struct inbuf *inlist = 0;
	char str[550];
	#ifdef DEBUG_OUTPUT_FILE
    char diag_str[550];
	#endif // DEBUG_OUTPUT_FILE
	int32_t wstat = 0, buffer_size;
	int32_t retval = 0, i;
	int32_t goodctr = 0, badctr = 0;
	uint32_t *buffer_uint32;
	uint32_t hdr[TRIG_MIN_HEADER_LENGTH_UINT32], t1, t2, t3, t4;
	uint32_t reformatted_hdr[REFORMATTED_HEADER_LENGTH_UINT32];
	static int32_t buffer_position = 0; // byte offset within buffer
	static uint32_t ch_id = 0;
	static uint32_t board_id = 0;
	static uint32_t packet_length_in_words = 0;	// length in 32-bit words
	static int32_t packet_length_in_bytes = 0;	// length in bytes
//	static uint32_t geo_addr = 0;
	static uint32_t header_type = 0;
	static uint32_t event_type = 0;
//	static uint32_t header_length = 0;
  static uint32_t timestamp_lower = 0;
  static uint32_t timestamp_upper = 0;



  bool is_digitizer_data = true;
	bool is_trigger_data = true;
	#ifdef WRITEGTFORMAT
		GEBDATA Geb;
	#else
		const uint32_t soe = SOE;
	#endif // WRITEGTFORMAT

	*writtenBytes = 0;

	if (debug > 0){
		printf ("entered writeEvents2, size2write=%i\n", size2write);
		fflush (stdout);
	};

	if (!buffer) return -3;

	if (debug > 0) printf ("buffer non null\n");

	//here evtlen in dgs means len of all events in bytes
	buffer_size = size2write;

	if (debug > 0) printf ("to write %d bytes to ptr %p \n", buffer_size, buffer);

	/* intercept the data and write it out */
	/* in GT GEB/payload format */

	/* read the events in the buffer */

	buffer_position = 0;
	buffer_uint32 = (uint32_t *) buffer;

	while (buffer_position < buffer_size){
    // gtReciever 6 method
    // check first word for proper data alignment
    if (debug > 0) printf("Buffer HEAD : 0x%08X | ", *buffer_uint32);

    if ((*buffer_uint32 & DIG_SOE_MASK) == DIG_SOE){ 
        if (debug > 0) printf(" DIG SOE \n");
        // If the first word is 0xAAAAAAAA (DIG_SOE), then process as digitizer data.
        is_digitizer_data = true;
        is_trigger_data = false;
        
        /* Skip the 0xAAAAAAAA */
        buffer_position += sizeof (uint32_t);
        buffer_uint32++;
        
        if (buffer_position + DIG_MIN_HEADER_LENGTH_BYTES > buffer_size){
          printf ("ooops:	data block has %i extra bytes\n", buffer_size - buffer_position);
          return -2;
        }

        /* extract the timestamp from the header */
        /* after swapping the bytes */
        for (i = 0; i < DIG_MIN_HEADER_LENGTH_UINT32; i++){
          hdr[i] = buffer_uint32[i];

            /* before 4 3 2 1 */
            /*		  | | | | */
            /* after  1 2 3 4 */

            // MBO 20200616: use ntohl here instead?
            t1 = (hdr[i] & 0x000000ff) << 24;
            t2 = (hdr[i] & 0x0000ff00) << 8;
            t3 = (hdr[i] & 0x00ff0000) >> 8;
            t4 = (hdr[i] & 0xff000000) >> 24;
            hdr[i] = t1 + t2 + t3 + t4;
            /*
            hdr[i] = ntohl(hdr[i]);	// MBO 20200616:
            */
        }

        //************ strip out header bits **************/
        //digitizer format
        //wor |  31 | 30 | 29 | 28 | 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 | 19 | 18 | 17 | 16 | 15 | 14 | 13 | 12 | 11 | 10 | 09 | 08 | 07 | 06 | 05 | 04 | 03 | 02 | 01 | 00 |
        //0		|                                                                      FIXED 0xAAAAAAAA                                                                          |
        //1		|     Geo Addr            |                 PACKET LENGTH                        |                    USER PACKET DATA                       |     CHANNEL ID    |
        //2		|                                                          LEADING EDGE DISCRIMINATOR TIMESTAMP[31:0]                                                            |
        //3		|         HEADER LENGTH        |  EVENT TYPE  |  0 | TTS| INT|    HEADER TYPE    |                   LEADING EDGE DISCRIMINATOR TIMESTAMP[47:32]                 |

        ch_id					          = (hdr[0] & 0x0000000F) >> 0;	// Word 1: 3..0
        board_id 			          = (hdr[0] & 0x0000FFF0) >> 4;	// Word 1: 15..4
        packet_length_in_words	= (hdr[0] & 0x07FF0000) >> 16;	// Word 1: 26..16
    //	geo_addr				= (hdr[0] & 0xF8000000) >> 27;	// Word 1: 31..27
        #ifdef WRITEGTFORMAT
            timestamp_lower 		= (hdr[1] & 0xFFFFFFFF) >> 0;	// Word 2: 31..0
            timestamp_upper 		= (hdr[2] & 0x0000FFFF) >> 0;	// Word 3: 15..0
        #endif
        header_type				= (hdr[2] & 0x000F0000) >> 16;	// Word 3: 19..16
        event_type				= (hdr[2] & 0x03800000) >> 23;	// Word 3: 25..23
    //	header_length			= (hdr[2] & 0xFC000000) >> 26;	// Word 3: 31..26

        packet_length_in_bytes	= packet_length_in_words * 4;

        #ifdef WRITEGTFORMAT
            /* create the GEB header */
            Geb.type = GEB_TYPE_DGS;
            Geb.length = packet_length_in_bytes;
            //full 48-bit timestamp stored in 64-bit uint32_t.
            Geb.timestamp = ((uint64_t)(timestamp_upper)) << 32;
            Geb.timestamp |= (uint64_t)(timestamp_lower);
        #endif //WRITEGTFORMAT

        if (buffer_position + packet_length_in_bytes > buffer_size){
          printf ("ooops:	data block has %i extra bytes\n", buffer_size - buffer_position);
          return -2;
        }

        if (packet_length_in_words < DIG_MIN_HEADER_LENGTH_UINT32){
          printf ("ooops:	packet_length: %i (%i bytes) is less than minimum required for header(%i Bytes)!! skip block...\n", packet_length_in_words, packet_length_in_bytes, DIG_MIN_HEADER_LENGTH_BYTES);
          return -2;
        }else if (buffer_position + packet_length_in_bytes < buffer_size){
          if (buffer_uint32[packet_length_in_words] != DIG_SOE){
            printf ("ooops:	Packet length should be %i, but 0xAAAAAAAA not found at boundary! Got %08X instead. skip block...\n", packet_length_in_words, buffer_uint32[packet_length_in_words]);
            return -2;
          }
        }
    }else if ((*buffer_uint32 & TRIG_SOE_MASK) == TRIG_SOE){
      if (debug > 0) printf(" TRIG SOE \n");

      // If the first word is 0xAAAAXXXX (TRIG_SOE), where XXXX is not AAAA, then process as trigger data.
      is_digitizer_data = false;
      is_trigger_data = true;

      if (buffer_position + TRIG_MIN_HEADER_LENGTH_BYTES > buffer_size){
        printf ("ooops:	data block has %i extra bytes\n", buffer_size - buffer_position);
        return -2;
      }

      /* extract the timestamp from the header */
      /* after swapping the bytes */
      /// Ryan 

      // if (debug > 1) printf("----- buffer :\n");
      for (i = 0; i < TRIG_MIN_HEADER_LENGTH_UINT32; i ++){
        hdr[i] = ntohl(buffer_uint32[i]);
        // if (debug > 1) printf("%2d | 0x%08X -> 0x%08X \n",i,  buffer_uint32[i], hdr[i]);        
      }

      //************ reparse into a digitizer like header **************/
      //digitizer format
      //word|  31 | 30 | 29 | 28 | 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 | 19 | 18 | 17 | 16 | 15 | 14 | 13 | 12 | 11 | 10 | 09 | 08 | 07 | 06 | 05 | 04 | 03 | 02 | 01 | 00 |
      //0		|                                                                      FIXED 0xAAAAAAAA                                                                          |
      //1		|     Geo Addr            |                 PACKET LENGTH                        |                    USER PACKET DATA                       |     CHANNEL ID    |
      //2		|                                                          LEADING EDGE DISCRIMINATOR TIMESTAMP[31:0]                                                            |
      //3		|         HEADER LENGTH        |  EVENT TYPE  |  0 | TTS| INT|    HEADER TYPE    |                   LEADING EDGE DISCRIMINATOR TIMESTAMP[47:32]                 |

      ch_id					    = 0x0;
      board_id	        = 0xF; 
      header_type       = 0xE;
      
      // Trim the board id, or "user package data" to the maximum
      // number of bits supported by the digitizer header.
      // board_id = board_id & DIG_BOARD_ID_MASK;
      packet_length_in_words  = 11;
      packet_length_in_bytes	= packet_length_in_words * 4;

      reformatted_hdr[0] = 0xAAAAAAAA;
      
      reformatted_hdr[1]  = ch_id;
      reformatted_hdr[1] |= board_id << 4;
      reformatted_hdr[1] |= packet_length_in_words << 16;  // always 8 words payload

      reformatted_hdr[2]  = hdr[4]   ;
      reformatted_hdr[2] |= hdr[3]  << 16;
      
      reformatted_hdr[3]  = hdr[2]   ;
      reformatted_hdr[3] |= header_type  << 16; // header_type
      //reformatted_hdr[3] |= 0x0  << 23; // event_type
      reformatted_hdr[3] |= 3 << 26;

      reformatted_hdr[ 4] = (hdr[1] << 16) + hdr[2];
      reformatted_hdr[ 5] = (hdr[3] << 16) + hdr[4];
      reformatted_hdr[ 6] = (hdr[5] << 16) + hdr[6];
      reformatted_hdr[ 7] = (hdr[7] << 16) + hdr[8];
      reformatted_hdr[ 8] = (hdr[9] << 16) + hdr[10];
      reformatted_hdr[ 9] = (hdr[11] << 16) + hdr[12];
      reformatted_hdr[10] = (hdr[13] << 16) + hdr[14];
      reformatted_hdr[11] =  hdr[15];


      #ifdef WRITEGTFORMAT
        /* create the GEB header */
        Geb.type = GEB_TYPE_DGS;
        Geb.length = packet_length_in_bytes;
        //full 48-bit timestamp stored in 64-bit uint32_t.
        Geb.timestamp  = ((uint64_t)(hdr[2])) << 32;
        Geb.timestamp |= ((uint64_t)(hdr[3])) << 16;
        Geb.timestamp |=  (uint64_t)(hdr[4]);
      #endif //WRITEGTFORMAT


      if (buffer_position + packet_length_in_bytes > buffer_size){
        printf ("ooops:	data block has %i extra bytes\n", buffer_size - buffer_position);
        return -2;
      }


      // if (packet_length_in_words < TRIG_MIN_HEADER_LENGTH_UINT32){
      //   printf ("ooops:	packet_length: %i (%i bytes) is less than minimum required for header(%i Bytes)!! skip block...\n", 
      //                    packet_length_in_words, packet_length_in_bytes, TRIG_MIN_HEADER_LENGTH_BYTES);
      //   return -2;
      // }else if (buffer_position + packet_length_in_bytes < buffer_size){
      //     if (buffer_uint32[packet_length_in_words] != TRIG_SOE){
      //       printf ("ooops:	Packet length should be %i, but 0xAAAAXXXX not found at boundary! Got %08X instead. skip block...\n", 
      //                       packet_length_in_words, buffer_uint32[packet_length_in_words]);
      //       return -2;
      //     }
      // }
    }else{
      if( debug > 0 ) printf(" Other SOE \n");
      printf("header : 0x%08X\n", *buffer_uint32);
      return dumpUnknownDataToDaigFile(buffer, size2write);
    }


    /* see if the proper file is open */
    /* or open it */

    // report an error if the header type is 0xF and the channel number is not > 9
    if ((header_type == 0xF) && (ch_id <= 9)){
      printf ("Error: Type F header reported as channel 9 or less. ch_id = %d", ch_id);
    }else{

      if (!FILE_OPEN_CHECK(ofile[board_id][ch_id])){

        //==== set file name
        if (is_trigger_data){
          sprintf (str, "%s_trig_%4.4i_%01X", fn, board_id, ch_id);
          #ifdef DEBUG_OUTPUT_FILE
            sprintf (diag_str, "%s_diag_trig_%4.4i_%01X.txt", fn, board_id, ch_id);
          #endif // DEBUG_OUTPUT_FILE

        }else{
          sprintf (str, "%s_%4.4i_%01X", fn, board_id, ch_id);
        }

        /* make sure it does not exist already */
        fp = open (str, O_RDONLY, 0);

        if (fp != -1){
          printf ("\n");
          printf ("----------------------------------------------------\n");
          printf ("ERROR: file \"%s\" already exists!!! QUIT!\n", str);
          printf ("			 delete file first if you want to overwrite it\n");
          printf ("----------------------------------------------------\n");
          printf ("\n");
          printf ("\n");
          close(fp);
          exit (1);
        }

        /* open file */
        printf("opening file : %s \n", str);
        ofile[board_id][ch_id] = fopen (str, "wb");
        file_buffer[board_id][ch_id] = (char*)malloc(FILE_BUF_SIZE);
        setvbuf(ofile[board_id][ch_id], file_buffer[board_id][ch_id], _IOFBF, FILE_BUF_SIZE);
        
        if (is_trigger_data) {
          #ifdef DEBUG_OUTPUT_FILE
            printf("opening diag file : %s \n", diag_str);
            diag_ofile[board_id][ch_id] = fopen (diag_str, "wb");
            diag_file_buffer[board_id][ch_id] = (char*)malloc(FILE_BUF_SIZE);
            setvbuf(diag_ofile[board_id][ch_id], diag_file_buffer[board_id][ch_id], _IOFBF, FILE_BUF_SIZE);
          #endif // DEBUG_OUTPUT_FILE
        }

        printf("First event received from:  BOARD_ID: %3.3i, CH_ID: %01X ", board_id, ch_id);

        if (FILE_OPEN_CHECK(ofile[board_id][ch_id])){
          if (min_board_id > board_id)  min_board_id = board_id;
          if (max_board_id < board_id)  max_board_id = board_id;
          printf("Opened new file %s\n", str);
        }else{
          printf("ERROR\nERROR: failed to open file %s, quit\n", str);
          forced_stop();
        };
      };

      //======= * write GEB header out */
      #ifdef WRITEGTFORMAT
        wstat = fwrite ((char *) &Geb, sizeof (GEBDATA), 1, ofile[board_id][ch_id]);
        if (wstat != 1) {
            printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
            forced_stop();
            return -4;
        }
        bytes_written_to_file[board_id][ch_id] += sizeof (GEBDATA);

        *writtenBytes += sizeof (GEBDATA);
      #else // if not defined WRITEGTFORMAT
      
          wstat = fwrite ((char *) &(soe), sizeof (soe), 1, ofile[board_id][ch_id]);
          if (wstat != 1){
              printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
              forced_stop();
              return -4;
          }

          bytes_written_to_file[board_id][ch_id] += sizeof (soe);

          *writtenBytes += sizeof (soe);
      #endif // WRITEGTFORMAT

      /* write payload out */
      if (is_digitizer_data){
        wstat = fwrite ((char *) (buffer + buffer_position), packet_length_in_bytes, 1, ofile[board_id][ch_id]);
        if (wstat != 1){
          printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
          forced_stop();
          return -4;
        }
      }
    
      if (is_trigger_data){
        #ifdef DEBUG_OUTPUT_FILE
          for( unsigned int i = 0; i < 12; i++){
            fprintf(diag_ofile[board_id][ch_id], "%08X\n", reformatted_hdr[i]);
          }
        #endif // DEBUG_OUTPUT_FILE

        wstat = fwrite ((char *)(&(reformatted_hdr[1])), packet_length_in_bytes, 1, ofile[board_id][ch_id]);
        if (wstat != 1){
            printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
            forced_stop();
            return -4;
        }
      }
      bytes_written_to_file[board_id][ch_id] += packet_length_in_bytes;
      *writtenBytes += packet_length_in_bytes;
      
    }
        
    if (is_trigger_data){
        buffer_position += TRIG_MIN_HEADER_LENGTH_BYTES;
    }else{
        buffer_position += packet_length_in_bytes;
    }
    buffer_uint32 = (uint32_t *) (buffer + buffer_position);

    if ((header_type == 0xF) && (event_type == 0x0) && (ch_id == 0xD)){
        close_board(board_id);
        exit_if_all_files_closed();
    }

  };

	if (badctr) printf ("%d write failures out of %d packets\n", badctr, badctr + goodctr);

	return retval;
}

/*##########################################################################*/
/*##########################################################################*/
int main (int32_t argc, char **argv){

	/* declarations */

	int32_t st, ns, nwritten;
	char *Receiver;
	int64_t tnow = 0, tthen = 0;
	char hostIP[INET_ADDRSTRLEN];
	struct hostent *hp;
	int8_t *input1;
	int8_t *input2;
	int32_t num_bytes_read = 0;
	//int64_t bytes_written_to_file = 0; // MBO 20200619:  changed to array

  uint32_t i, j;

	// Show version and all build switches"
	printf ("dgsReceiver.cpp V%s \n", VERSION);
	printf ("\n");
	// Now list build parameteres:
	printf ("Compiled with the following options:\n");
	//======================= Write Data Format
  #ifdef WRITEGTFORMAT
      printf ("Data Format: GEB\n");
  #else
      printf ("Data Format: RAW\n");
  #endif
	//======================= Operation mode
  printf ("Operating Mode: Continuous\n");
	//======================= Disk IO
  printf ("Disk IO Library: ANSI C\n");
	//======================= Network Library
  printf ("Network Library: GNU\n");
	//======================= Filter Type F Message
  //TODO 
	//======================= Network Library 
  printf ("Data Organization: File per Channel\n");
	//======================= Folder Organ
  #ifdef FOLDER_PER_RUN
      printf ("Folder Organization: Folder per Run\n");
  #else
      printf ("Folder Organization: Common Folder\n")
  #endif
	//======================= Unknown Data Handling
	#ifndef DUMP_UNKNOWN_DATA_TO_DISK
        printf ("Unknown Data Handling Mode: Stop Run\n");
	#else
        printf ("Unknown Data Handling Mode: Write to Diagnostic File\n");
	#endif
	//======================= Diagonstic ASCII File Output
	#ifndef DEBUG_OUTPUT_FILE
	    printf ("Diagnostic ASCII File Output: Disabled\n");
	#else
	    printf ("Diagnostic ASCII File Output: Enabled\n");
	#endif
	printf ("Summary output Interval: %d seconds\n", SUMMARY_OUTPUT_INTERVAL);
	printf ("\n");
	printf ("\n");

  min_board_id = 0xFFFF;
  max_board_id = 0x0000;
  for (i = 0; i < MAXBOARDID; i++){
    for (j = 0; j < MAXCHID; j++) {
      bytes_written_to_file[i][j] = 0;
      ofile[i][j] = 0;
    }
  }
	/* help */

	#ifdef WRITEGTFORMAT
	if (argc == 7) debug = 1;
	#else
	if (argc == 6) debug = 1;
	#endif

	#ifdef WRITEGTFORMAT
	if (argc < 5)
	#else
	if (argc < 4)
	#endif
  {
		printf ("\n");
		printf ("argc=%i\n",argc);
		printf ("\n");
		#ifdef WRITEGTFORMAT
			printf ("use: dgsReceiver <1:server> <2:filename> <3:extension_prefix> <4:maxfilesize> <5:GEBID> \n");
			printf ("                  1        2        3      4       5       \n");
			printf ("e.g: dgsReceiver ioc1 data_run_001 gtd 2000000000 14		\n");
		#else
			printf ("use: dgsReceiver <server> <filename> <extension_prefix> <maxfilesize> \n");
			printf ("                    1         2     3      4      \n");
			printf ("e.g: dgsReceiver ioc1 data_run_001 gtd 2000000000 \n");
		#endif //WRITEGTFORMAT
		printf ("\n");
		printf ("<2:filename> specifies the base file name.\n");
		printf ("<3:extension_prefix> specifies the start of the second part of the file name.\n");
    printf ("                     The actual file name will be <filename>.<extension_prefix>_<chunk number>_<board_id>_<ch_id>\n");
    printf ("                     e.g. data_run_001.gtd_001_1234_3 = Chunk:1, Board_ID:1234, Ch_ID:3\n");
		printf ("\n");
		printf ("<4:maxfilesize> specifies the max file size in bytes. If a file\n");
		printf ("                runs over the limit a new file will be opened with a new\n");
		printf ("                counter number at the end. A value of 2000000000 or less \n");
		printf ("                ensures the files are less than 2GB and can be read by\n");
		printf ("                all operating systems.\n");
		printf ("\n");
		printf ("                When any file in a chunk reaches the chunk files size limit,\n");
		printf ("                all files in that chunk are closed and new data will be stored in\n");
		printf ("                the next chunk.\n");
		printf ("\n");
		printf ("\n");
		#ifdef WRITEGTFORMAT
			printf ("<5:GEBID> has no effect on the operation of the receiver.  This number\n");
			printf ("          is simply passed to the geb headers in the output data files.\n");
			printf ("\n");
			printf ("          GEBID=14 is DGS data\n");
			printf ("          GEBID=15 is DGSTRIG data\n");
			printf ("          GEBID=16 is DFMA data\n");
			printf ("\n");
			printf ("NOTE: use 'ctrl-c' to stop this receiver program.  It is designed\n");
			printf ("      to be killed either with 'ctrl-c' or when it receives the\n");
			printf ("      appropriate type 'F' message from the IOC, and it will close\n");
			printf ("      out any open files cleanly and properly shutdown for either scenario.\n");
			printf ("\n");
		#endif // WRITEGTFORMAT
		exit (0);
	};

	/* set the record length */
	/* should eventually fetch this value from EPICS database */

	// caget_reclen ("CryG_CS_RDLen.RVAL", &i1);
//deprecated
	setRecLen (0);
	//printf ("using record length %i\n", i1);

	/* get/hide output file name in global string */

	#ifdef FOLDER_PER_RUN
    sprintf (fn, "%s/%s.%s_%3.3i", argv[2], argv[2], argv[3], chunck);
  #else
    sprintf (fn, "%s.%s_%3.3i", argv[2], argv[3], chunck);
  #endif // FOLDER_PER_RUN

  #ifdef FOLDER_PER_RUN
      int32_t mkdir_ret;
      // Need to check if folder already exists
      // and make the folder if it does not.
      mkdir_ret = mkdir(argv[2], 0777);
  #endif // FOLDER_PER_RUN

	/* catch contrl-c so we can clean up properly */

	signal (SIGINT, signal_catcher);

	/* find the servers IP address and check */

	hp = gethostbyname (argv[1]);

	if (hp == NULL){
		printf ("cannot find IP for host \"%s\", gethostbyname returns %p\n", argv[1], (void*)(hp));
		fflush (stdout);
		exit (1);
	};

	if (hp->h_addrtype != AF_INET){
		printf (" hp->h_addrtype != AF_INET for \"%s\"\n", argv[1]);
		fflush (stdout);
		exit (1);
	};

  sprintf (hostIP, "%s", inet_ntop (hp->h_addrtype, hp->h_addr, hostIP, sizeof (hostIP)));

	Receiver = (char *) calloc (sizeof (struct rcvrInstance), 1);
	Receiver = initReceiver (hostIP);

	#ifdef WRITEGTFORMAT
		/* get the GEBID */
		GEB_TYPE_DGS = atoi (argv[5]);
	#endif // WRITEGTFORMAT

	/* request and receive max_file_size data buffers */
	/* ns and usleep is used to slow down */
	/* requests if there is no data or not */
	/* enough data */

	max_file_size = atoi (argv[4]);

	ns = 1;
	while (1){
		/* get a data buffer */
		st = getReceiverData2 (Receiver, &input1, &num_bytes_read);

		/* if we failed x.1_000delay, else */
		/* write buffer to disk */

		if (st != 0){ // has connection problem
			usleep (ns);
			ns = (ns << 1);
			if (ns > MAXNS) ns = MAXNS;

			/* keep user informed even if we have no counts */

			if (ns >= MAXNS){
				tnow = time (NULL);
				if ((tnow - tthen) >= SUMMARY_OUTPUT_INTERVAL){
					if (has_connected == 1){
						print_info (totbytes);
					}else{
						puts("waiting for connection...\n");
						tthen = tnow;
					}
				};
			};

		}else{
			has_connected  = 1;
			if (ns != 1) ns = (ns >> 1);	// MBO 20200615: added line.
        input2 = input1;
			do{
				/* if we get here we have data to dump to disk */
        if ((totbytesInLargestFile + num_bytes_read) > max_file_size){
          /* set the new file name */
          printf("file size reached %" PRId64 " of %" PRId64 " limit\n", totbytesInLargestFile, max_file_size);												

          /* properly close the old files */
          
          close_all();

          chunck++;
          #ifdef FOLDER_PER_RUN
              sprintf (fn, "%s/%s.%s_%3.3i", argv[2], argv[2], argv[3], chunck);
          #else
              sprintf (fn, "%s.%s_%3.3i", argv[2], argv[3], chunck);
          #endif // FOLDER_PER_RUN

          printf ("Starting new data chunk: #%3.3i\n", chunck);
          fflush (stdout);

          //print_info (totbytes);
        }

        st = writeEvents2 (input2, num_bytes_read, &nwritten);

        if (st == 0){
            // ok
        }else if(st <= -3){
              printf ("failed to write data to disk\n");
        }else if(st == -2){
            #ifndef DUMP_UNKNOWN_DATA_TO_DISK
              printf ("skipping data block\n");
            #else
              // Carry on.
              st = 0;
            #endif
        }else if(st == -1){
            printf ("unknown fault\n");
        }else{
            input2 += st;
            num_bytes_read -= st;
        }


        /* keep user informed */
        totbytes += nwritten;
        printf ("nwritten=%i, totbytes=%lli\n", nwritten, totbytes);
        tnow = time (NULL);
        if ((tnow - tthen) >=  SUMMARY_OUTPUT_INTERVAL){
          print_info (totbytes);
          tthen = tnow;
        };

        /* new files */

        /* NOTE: we close all files at the same time	*/
        /* so that they all have the same time stamp	*/
        /* range because that makes it much easier	*/
        /* to merger the data later on */

        if (min_board_id <= max_board_id){
          for (i = min_board_id; i <= max_board_id; i++){
            for (j = 0; j < MAXCHID; j++){
              if (totbytesInLargestFile < bytes_written_to_file[i][j])
                  totbytesInLargestFile = bytes_written_to_file[i][j];
            }
          }
        }

      } while (st > 0);

    };

  }// end of while loop

	/* done (we should never really get here) */

	exit (0);

}

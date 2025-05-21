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

/* SINGLESHOT:  When defined, the receiver will exit once the output file(s) reach
 * 				the specified size.  See FULL_FILE_MODE, for more details.
*/
//#define SINGLESHOT

#ifdef SINGLESHOT
/* FULL_FILE_MODE:  When defined, the receiver will only exit once all open files
 * 					reach the specified size provided by the user. Otherwise,
 * 					when not defined, will close when the first file reaches the
 * 					specified file size.
 *
 * 					In other words, if this is set with file-per-channel mode configured
 *					all opened channel files will have the same number of events as set
 *					by the specified file size provided when running the program.
 *
 *					Note that the receiver has no knowledge of the number of enabled channels.
 * 					A file is only opened once an event has been received to open that file.
 * 					If the rate difference between channels is extreme, and the requested size
 *					limit is set small, then it is possible that the above condition
 * 					("all opened channel files" reaching the specified size limit) could be
 *					satisfied before any receiving any events from very slow or low rate channels.
 *
 * 					(At least one file must be opened before the program will close itself.)
*/
#define FULL_FILE_MODE
#endif // SINGLESHOT

//#define  NO_SAVE			// MBO 20200617: Receive only, no saving or processing.
//#define  NO_SAVE_BUT_STILL_PROCESS   // MBO 20200722: Receive and process the data, but don't save to disk.
//#define USE_POSIX_FILE_LIB // MBO 20200616: When defined file IO used POSIX non-blocking library calls,
							// MBO 20200617: When not defined, data write will use the ANSI C file IO (also non-blocking...)
#define FILE_PER_CHANNEL	// MBO 20200616: When defined, will write one file per channel
//#define SINGLE_FILE		// MBO 20200624: Overrides FILE_PER_CHANNEL, saves one file per IOC.  auto shut down does not work properly in this mode yet.
							// MBO 20200626: When neither FILE_PER_CHANNEL nor SINGLE_FILE is defined, will save one file per Digitizer
#define FOLDER_PER_RUN      // MBO 20220721: When defined, will create a separate subdirectory for each run.
//#define FILTER_TYPE_F		// MBO 20200626: When defined, will remove all type F headers from output.
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
#define REFORMATTED_HEADER_LENGTH_UINT32 10
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

#ifndef USE_POSIX_FILE_LIB
    #define FILE_BUF_SIZE_KB 512
    #define FILE_BUF_SIZE FILE_BUF_SIZE_KB*1024+8
    #ifdef SINGLE_FILE
        static char* file_buffer;
    #else // not SINGLE_FILE
        static uint32_t min_board_id;
        static uint32_t max_board_id;
        #ifdef FILE_PER_CHANNEL
            #if defined(SINGLESHOT) && defined(FULL_FILE_MODE)
                static uint16_t write_inhibit[MAXBOARDID][MAXCHID];
            #endif	//defined
            static char* file_buffer[MAXBOARDID][MAXCHID];
        #else // not FILE_PER_CHANNEL
            #if defined(SINGLESHOT) && defined(FULL_FILE_MODE)
                static uint16_t write_inhibit[MAXBOARDID];
            #endif //defined
            static char* file_buffer[MAXBOARDID];
        #endif // not FILE_PER_CHANNEL
    #endif // not SINGLE_FILE
#endif // not USE_POSIX_FILE_LIB

#ifdef DEBUG_OUTPUT_FILE
    #ifndef USE_POSIX_FILE_LIB
        #ifdef SINGLE_FILE
            static char* diag_file_buffer;
        #else // not SINGLE_FILE
            #ifdef FILE_PER_CHANNEL
                static char* diag_file_buffer[MAXBOARDID][MAXCHID];
            #else // not FILE_PER_CHANNEL
                static char* diag_file_buffer[MAXBOARDID];
            #endif // not FILE_PER_CHANNEL
        #endif // not SINGLE_FILE
    #endif // not USE_POSIX_FILE_LIB
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


#ifdef SINGLE_FILE
	int64_t bytes_written_to_file;
#else
	#ifdef FILE_PER_CHANNEL	// MBO 20200616:
		int64_t bytes_written_to_file[MAXBOARDID][MAXCHID]; // MBO 20200619:  changed to array
	#else
		int64_t bytes_written_to_file[MAXBOARDID]; // MBO 20200619:  changed to array
	#endif
#endif

#ifndef SINGLESHOT
int64_t totbytesInLargestFile; // MBO 20200619:  new
#endif // SINGLESHOT
#ifdef DUMP_UNKNOWN_DATA_TO_DISK
    #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
        int32_t diag_ofile;
    #else
        FILE* diag_unknown_ofile;
    #endif
#endif //DUMP_UNKNOWN_DATA_TO_DISK

#ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
	#define FILE_OPEN_CHECK(file) (file > 0)
	#ifdef SINGLE_FILE
			int32_t ofile;
	#else
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			int32_t ofile[MAXBOARDID][MAXCHID];
		#else
			int32_t ofile[MAXBOARDID];
		#endif
	#endif
#else
	#define FILE_OPEN_CHECK(file) (file != 0)
	#ifdef SINGLE_FILE
			FILE* ofile;
	#else
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			FILE* ofile[MAXBOARDID][MAXCHID];	// MBO 20200617:  Normal mode writes changed from POSIX to  ANSI C file iO
		#else
			FILE* ofile[MAXBOARDID];			// MBO 20200617:  Normal mode writes changed from POSIX to  ANSI C file iO
		#endif
	#endif
#endif

#ifdef DEBUG_OUTPUT_FILE
    #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
        #define FILE_OPEN_CHECK(file) (file > 0)
        #ifdef SINGLE_FILE
                int32_t diag_ofile;
        #else
            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                int32_t diag_ofile[MAXBOARDID][MAXCHID];
            #else
                int32_t diag_ofile[MAXBOARDID];
            #endif
        #endif
    #else
        #define FILE_OPEN_CHECK(file) (file != 0)
        #ifdef SINGLE_FILE
                FILE* diag_ofile;
        #else
            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                FILE* diag_ofile[MAXBOARDID][MAXCHID];	// MBO 20200617:  Normal mode writes changed from POSIX to  ANSI C file iO
            #else
                FILE* diag_ofile[MAXBOARDID];			// MBO 20200617:  Normal mode writes changed from POSIX to  ANSI C file iO
            #endif
        #endif
    #endif // USE_POSIX_FILE_LIB
#endif // DEBUG_OUTPUT_FILE

int32_t chunck = 0;

#ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
#define FILE_RETRY_LIMIT 50
#define FILE_WRITE_RETRY_DELAY 10000

int32_t nonblocking_file_write(int32_t fildes, const void *buf, size_t nbyte){

	printf("%s\n", __func__);

	int32_t wstat = 0;
	int32_t siz = 0;
	int32_t attempts = 0;
	while (siz != nbyte){
		wstat = write (fildes, buf, nbyte);
		if (wstat == -1){
			attempts++;
			printf(".");
		}else{
			siz += wstat;
		}

		if (siz == nbyte){
			if (attempts > 0 ) printf("\n");
			break;
		}else{
			if (attempts > FILE_RETRY_LIMIT){
				printf("ERROR: cannot write to disk.\n");
				return -1;
			}else{
				usleep(FILE_WRITE_RETRY_DELAY);
			}
		}
	}
	return siz;
}
#endif

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

	#ifdef FILE_PER_CHANNEL	// MBO 20200616:
		int32_t j;
	#endif

	if (firsttime){
		firsttime = 0;
		tstart = time (NULL);
	};

	tnow = time (NULL);

	deltaTime = tnow - tthen;
	deltaBytes = totbytes - last_totbytes;

	printf ("totbytes=%li\n", totbytes);
	printf ("last_totbytes=%li\n", last_totbytes);
	printf ("t_now =%li\n", tnow);
	printf ("t_then=%li\n", tthen);
	printf ("deltaTime=%f\n", (float) deltaTime);
	printf ("deltaBytes=%f\n", (float) deltaBytes);

	last_totbytes = totbytes;
	tthen = tnow;

	r1 = (double) totbytes / 1024 / 1024;
	printf ("%10.3f Mbytes; ", (float) r1);
	r1 = deltaBytes / deltaTime;
	r1 /= 1024;
	printf ("%7.0f KB/s; ", (float) r1);
	r1 = (double) (totbytes) / (double) (1024) / (double) (tnow - tstart);
	printf ("AVG: %7.0f KB/s; ", (float) r1);

	#ifdef SINGLE_FILE
	#else
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			for (i = 0; i < MAXBOARDID; i++)
				for (j = 0; j < MAXCHID; j++)
					if (FILE_OPEN_CHECK(ofile[i][j])) printf ("%i-%i ",i,j);
		#else
			for (i = 0; i < MAXBOARDID; i++)
				if (FILE_OPEN_CHECK(ofile[i])) printf ("%i ",i);
		#endif
	#endif

    #ifdef DEBUG_OUTPUT_FILE
        #ifdef SINGLE_FILE
        #else
            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                for (i = 0; i < MAXBOARDID; i++)
                    for (j = 0; j < MAXCHID; j++)
                        if (FILE_OPEN_CHECK(diag_ofile[i][j])) printf ("%i-%i ",i,j);
            #else
                for (i = 0; i < MAXBOARDID; i++)
                    if (FILE_OPEN_CHECK(diag_ofile[i])) printf ("%i ",i);
            #endif
        #endif
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

	#ifdef SINGLE_FILE
	#else
		int32_t i;
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			int32_t j;
		#endif
	#endif

	/* specify readonly for everyone */

	#ifdef SINGLE_FILE
		sprintf (str, "%s", fn);
		data_fd = open (str, O_RDWR);
		close (data_fd);
		ofile = 0;
		printf ("%s is now readonly\n", str);
	#else
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			for (i = 0; i < MAXBOARDID; i++)
				for (j = 0; j < MAXCHID; j++)
					if (FILE_OPEN_CHECK(ofile[i][j])){
                        sprintf (str, "%s_%4.4i_%1.1i", fn, i, j);
                        data_fd = open (str, O_RDWR);
                        close (data_fd);
                        ofile[i][j] = 0;
                        printf ("%s is now readonly\n", str);
                    };
		#else
			for (i = 0; i < MAXBOARDID; i++)
				if (FILE_OPEN_CHECK(ofile[i])){
                    sprintf (str, "%s_%4.4i", fn, i);
                    data_fd = open (str, O_RDWR);
                    close (data_fd);
                    ofile[i] = 0;
                    printf ("%s is now readonly\n", str);
                };
		#endif // FILE_PER_CHANNEL
	#endif // SINGLE_FILE

	#ifdef DEBUG_OUTPUT_FILE
        #ifdef SINGLE_FILE
            sprintf (str, "diag_%s", fn);
            data_fd = open (str, O_RDWR);
            close (data_fd);
            diag_ofile = 0;
            printf ("diag_%s is now readonly\n", str);
        #else
            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                for (i = 0; i < MAXBOARDID; i++)
                    for (j = 0; j < MAXCHID; j++)
                        if (FILE_OPEN_CHECK(diag_ofile[i][j])){
                            sprintf (str, "diag_%s_%4.4i_%1.1i", fn, i, j);
                            data_fd = open (str, O_RDWR);
                            close (data_fd);
                            diag_ofile[i][j] = 0;
                            printf ("diag_%s is now readonly\n", str);
                        };
            #else
                for (i = 0; i < MAXBOARDID; i++)
                    if (FILE_OPEN_CHECK(diag_ofile[i])){
                       sprintf (str, "diag_%s_%4.4i", fn, i);
                        data_fd = open (str, O_RDWR);
                        close (data_fd);
                        diag_ofile[i] = 0;
                        printf ("diag_%s is now readonly\n", str);
                    };
            #endif // FILE_PER_CHANNEL
        #endif // SINGLE_FILE
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

	#ifdef FILE_PER_CHANNEL	// MBO 20200616:
		int32_t j;
	#endif

	/* specify readonly for everyone */

	#ifdef SINGLE_FILE
		sprintf (str, "%s", fn);
		data_fd = open (str, O_RDWR);
		close (data_fd);
		ofile = 0;
		printf ("%s is now readonly\n", str);
	#else
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			for (j = 0; j < MAXCHID; j++)
				if (FILE_OPEN_CHECK(ofile[board_num][j]))
					{
						sprintf (str, "%s_%4.4i_%1.1i", fn, board_num, j);
						data_fd = open (str, O_RDWR);
						close (data_fd);
						ofile[board_num][j] = 0;
						printf ("%s is now readonly\n", str);
					};
		#else
			if (FILE_OPEN_CHECK(ofile[board_num]))
				{
					sprintf (str, "%s_%4.4i", fn, board_num);
					data_fd = open (str, O_RDWR);
					close (data_fd);
					ofile[board_num] = 0;
					printf ("%s is now readonly\n", str);
				};
		#endif
	#endif

	#ifdef DEBUG_OUTPUT_FILE
        #ifdef SINGLE_FILE
            sprintf (str, "diag_%s", fn);
            data_fd = open (str, O_RDWR);
            close (data_fd);
            diag_ofile = 0;
            printf ("diag_%s is now readonly\n", str);
        #else // not SINGLE_FILE
            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                for (j = 0; j < MAXCHID; j++)
                    if (FILE_OPEN_CHECK(diag_ofile[board_num][j]))
                        {
                            sprintf (str, "diag_%s_%4.4i_%1.1i", fn, board_num, j);
                            data_fd = open (str, O_RDWR);
                            close (data_fd);
                            diag_ofile[board_num][j] = 0;
                            printf ("%s is now readonly\n", str);
                        };
            #else // not FILE_PER_CHANNEL
                if (FILE_OPEN_CHECK(diag_ofile[board_num]))
                    {
                        sprintf (str, "%s_%4.4i", fn, board_num);
                        data_fd = open (str, O_RDWR);
                        close (data_fd);
                        diag_ofile[board_num] = 0;
                        printf ("%s is now readonly\n", str);
                    };
            #endif // not FILE_PER_CHANNE
        #endif // not SINGLE_FILE
    #endif // DEBUG_OUTPUT_FILE
	/* done */

	return;

}

/*----------------------------------------------------------------------*/
void close_all (void){

	printf("#### %s\n", __func__);
	time_t ticks;

	#ifdef SINGLE_FILE
	#else
		int32_t i;
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			int32_t j;
		#endif
	#endif

	printf ("\n\nClosing all files at ");
	ticks = time (NULL);
	printf ("%.24s\n", ctime (&ticks));
	fflush (stdout);

	#ifdef SINGLE_FILE
		if (FILE_OPEN_CHECK(ofile)){
			#ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
				close (ofile);
			#else
				fclose (ofile
				free(file_buffer);
			#endif
	//		ofile = 0;
			bytes_written_to_file = 0;
			printf ("close board file\n");
		}
	#else
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
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
		#else
			for (i = 0; i < MAXBOARDID; i++)
				if (FILE_OPEN_CHECK(ofile[i]))
					{
						#ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
							close (ofile[i]);
						#else
							fclose (ofile[i]);
							free(file_buffer[i]);
						#endif
	//					ofile[i] = 0;
						bytes_written_to_file[i] = 0;
						printf ("close board file %i\n", i);
					};
		#endif
	#endif

	#ifdef DEBUG_OUTPUT_FILE
        #ifdef SINGLE_FILE
            if (FILE_OPEN_CHECK(diag_ofile))
            {
                #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                    close (diag_ofile);
                #else
                    fclose (diag_ofile
                    free(diag_file_buffer);
                #endif
        //		diag_ofile = 0;
                bytes_written_to_file = 0;
                printf ("close board file\n");
            }
        #else
            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
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
            #else
                for (i = 0; i < MAXBOARDID; i++)
                    if (FILE_OPEN_CHECK(diag_ofile[i]))
                        {
                            #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                                close (diag_ofile[i]);
                            #else
                                fclose (diag_ofile[i]);
                                free(diag_file_buffer[i]);
                            #endif
        //					diag_ofile[i] = 0;
                            bytes_written_to_file[i] = 0;
                            printf ("close board file %i\n", i);
                        };
            #endif
        #endif
	#endif // DEBUG_OUTPUT_FILE


    #ifndef SINGLESHOT
        totbytesInLargestFile = 0;
    #endif // SINGLESHOT
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

	#ifdef SINGLE_FILE
	#else
		int32_t i;
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			int32_t j;
		#endif
	#endif

	#ifdef SINGLE_FILE
		if (FILE_OPEN_CHECK(ofile))
			return;
	#else
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			for (i = 0; i < MAXBOARDID; i++)
				for (j = 0; j < MAXCHID; j++)
					if (FILE_OPEN_CHECK(ofile[i][j]))
						return;
		#else
			for (i = 0; i < MAXBOARDID; i++)
				if (FILE_OPEN_CHECK(ofile[i]))
						return;
		#endif
	#endif

	#ifdef DEBUG_OUTPUT_FILE
        #ifdef SINGLE_FILE
            if (FILE_OPEN_CHECK(diag_ofile))
                return;
        #else
            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                for (i = 0; i < MAXBOARDID; i++)
                    for (j = 0; j < MAXCHID; j++)
                        if (FILE_OPEN_CHECK(diag_ofile[i][j]))
                            return;
            #else
                for (i = 0; i < MAXBOARDID; i++)
                    if (FILE_OPEN_CHECK(diag_ofile[i]))
                            return;
            #endif
        #endif
	#endif // DEBUG_OUTPUT_FILE

	stop_receiver ();
}

void close_board (int32_t board_num){

	printf("#### %s\n", __func__);

	time_t ticks;

	#ifdef FILE_PER_CHANNEL	// MBO 20200616:
		int32_t j;
	#endif

	printf ("\n\nEnd of data packet received for board #%i at ", board_num);
	ticks = time (NULL);
	printf ("%.24s\n", ctime (&ticks));
	fflush (stdout);

	#ifdef SINGLE_FILE
		if (FILE_OPEN_CHECK(ofile))
		{
			#ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
				close (ofile);
			#else
				fclose (ofile);
				free(file_buffer);
			#endif
	//		ofile = 0;
			bytes_written_to_file = 0;
			printf ("close board file\n");
		}
	#else
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
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
		#else
			if (FILE_OPEN_CHECK(ofile[board_num]))
				{
					#ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
						close (ofile[board_num]);
					#else
						fclose (ofile[board_num]);
						free(file_buffer[board_num]);
					#endif
	//				ofile[board_num] = 0;
					bytes_written_to_file[board_num] = 0;
					printf ("close board file %i\n", board_num);
				};
		#endif
	#endif

    #ifdef DEBUG_OUTPUT_FILE
        #ifdef SINGLE_FILE
            if (FILE_OPEN_CHECK(diag_ofile))
            {
                #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                    close (diag_ofile);
                #else
                    fclose (diag_ofile);
                    free(diag_file_buffer);
                #endif
        //		diag_ofile = 0;
                printf ("close diag board file\n");
            }
        #else
            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
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
            #else
                if (FILE_OPEN_CHECK(diag_ofile[board_num]))
                    {
                        #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                            close (diag_ofile[board_num]);
                        #else
                            fclose (diag_ofile[board_num]);
                            free(diag_file_buffer[board_num]);
                        #endif
        //				ofile[board_num] = 0;
                        printf ("close diag board file %i\n", board_num);
                    };
            #endif
        #endif

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

	printf("#### %s\n", __func__);

	char str[550];
	int32_t wstat = 0;
	uint32_t *buffer_uint32;
	buffer_uint32 = (uint32_t *) buffer;

    #ifndef DUMP_UNKNOWN_DATA_TO_DISK
        printf ("ooops:	event started with %08X instead of 0xAAAAXXXX.  block skipped\n", *buffer_uint32);
        return -2;
    #else
        // MBO 20220801: Quick hack to get trigger data to disk for inital testing.
        printf ("ooops:	event started with %08X instead of 0xAAAAXXXX.  Dumping mysterious data to diagnostic file\n", *buffer_uint32);
        sprintf (str, "%s_DIAG_DATA", fn);

        /* open file */
        #ifdef USE_POSIX_FILE_LIB
            diag_unknown_ofile = open (str, O_WRONLY | O_CREAT | O_APPEND | O_NONBLOCK, 0644);
            if (!FILE_OPEN_CHECK(diag_unknown_ofile))
            {
                printf ("Can't open or create diagnostic output file.");
                return -2;
            }
            wstat = nonblocking_file_write(diag_unknown_ofile, buffer, size2write);
            if (wstat != size2write)
            {
                printf("Aborting write of %d bytes due to unhandled write error.", size2write);
                return -2;
            }
            close(diag_unknown_ofile);
        #else
            diag_unknown_ofile = fopen (str, "ab");
            if (!FILE_OPEN_CHECK(diag_unknown_ofile))
            {
                printf ("Can't open or create diagnostic output file.");
                return -2;
            }
            wstat = fwrite (buffer, size2write, 1, diag_unknown_ofile);
            if (wstat != size2write)
            {
                printf("Aborting write of %d bytes due to unhandled write error.", size2write);
                return -2;
            }
            fclose(diag_unknown_ofile);
        #endif
        return -2;
    #endif
}

/*----------------------------------------------------------------------*/

int32_t writeEvents2 (int8_t *buffer, int32_t size2write, int32_t *writtenBytes) {

  debug = 1;

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
// additional data fields to decode for triggers:
  static uint32_t trigger_type = 0;
  static uint32_t timestamp_middle = 0;
  static uint32_t wheel = 0;
  static uint32_t aux_data = 0;
  static uint32_t tdc_ts_lo = 0;
  static uint32_t trig_acks = 0;
  static uint32_t offset[4] = {0,0,0,0};
  static uint32_t val_p0_p1 = 0;
  static uint32_t val_p2_p3 = 0;


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

    printf("Buffer HEAD : 0x%08X | ", *buffer_uint32);

    // if ((*buffer_uint32 & ANY_SOE_MASK) != ANY_SOE){
    //   printf(" ANY SOE \n");
    //   return dumpUnknownDataToDaigFile(buffer, size2write);
      
    // }else 
    if ((*buffer_uint32 & DIG_SOE_MASK) == DIG_SOE){ 
      printf(" DIG SOE \n");
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
      //word	|  31 | 30 | 29 | 28 | 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 | 19 | 18 | 17 | 16 | 15 | 14 | 13 | 12 | 11 | 10 | 09 | 08 | 07 | 06 | 05 | 04 | 03 | 02 | 01 | 00
      //0		|                                                                      FIXED 0xAAAAAAAA                                                                          |
      //1		|     Geo Addr            |                 PACKET LENGTH                        |                    USER PACKET DATA                       |     CHANNEL ID    |
      //2		|                                                          LEADING EDGE DISCRIMINATOR TIMESTAMP[31:0]                                                            |
      //3		|         HEADER LENGTH        |  EVENT TYPE  |  0 | TTS| INT|    HEADER TYPE    |                   LEADING EDGE DISCRIMINATOR TIMESTAMP[47:32]                 |

      ch_id					= (hdr[0] & 0x0000000F) >> 0;	// Word 1: 3..0
      board_id 				= (hdr[0] & 0x0000FFF0) >> 4;	// Word 1: 15..4
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
      printf(" TRIG SOE \n");

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

      printf("----- buffer :\n");
      for (i = 0; i < TRIG_MIN_HEADER_LENGTH_UINT32; i ++){
        hdr[i] = ntohl(buffer_uint32[i]);
        printf("%2d | 0x%08X -> 0x%08X \n",i,  buffer_uint32[i], hdr[i]);

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

      //************ reparse into a digitizer like header **************/
      //digitizer format
      //word	|  31 | 30 | 29 | 28 | 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 | 19 | 18 | 17 | 16 | 15 | 14 | 13 | 12 | 11 | 10 | 09 | 08 | 07 | 06 | 05 | 04 | 03 | 02 | 01 | 00
      //0		|                                                                      FIXED 0xAAAAAAAA                                                                          |
      //1		|     Geo Addr            |                 PACKET LENGTH                        |                    USER PACKET DATA                       |     CHANNEL ID    |
      //2		|                                                          LEADING EDGE DISCRIMINATOR TIMESTAMP[31:0]                                                            |
      //3		|         HEADER LENGTH        |  EVENT TYPE  |  0 | TTS| INT|    HEADER TYPE    |                   LEADING EDGE DISCRIMINATOR TIMESTAMP[47:32]                 |

      ch_id					= 0xF;//
  //  AAAA        			= (hdr[0] & 0x0000FFFF) >> 0;   skip the AAAA's
      trigger_type			= (hdr[0] & 0xFFFF0000) >> 16;
      timestamp_lower 		= (hdr[1] & 0x0000FFFF) >> 0;
      timestamp_middle        = (hdr[1] & 0xFFFF0000) >> 16;
      timestamp_upper 		= (hdr[2] & 0x0000FFFF) >> 0;
      wheel	                = (hdr[2] & 0xFFFF0000) >> 16;
      aux_data                = (hdr[3] & 0x0000FFFF) >> 0;
      board_id	            = (hdr[3] & 0xFFFF0000) >> 16; 
      tdc_ts_lo               = (hdr[4] & 0x0000FFFF) >> 0;
      trig_acks               = (hdr[4] & 0xFFFF0000) >> 16;
      offset[0]               = (hdr[5] & 0x0000FFFF) >> 0;
      offset[1]               = (hdr[5] & 0xFFFF0000) >> 16;
      offset[2]               = (hdr[6] & 0x0000FFFF) >> 0;
      offset[3]               = (hdr[6] & 0xFFFF0000) >> 16;
      val_p0_p1               = (hdr[7] & 0x0000FFFF) >> 0;
      val_p2_p3               = (hdr[7] & 0xFFFF0000) >> 16;

      // Trim the board id, or "user package data" to the maximum
      // number of bits supported by the digitizer header.
      board_id = board_id & DIG_BOARD_ID_MASK;
      packet_length_in_bytes	= REFORMATTED_HEADER_LENGTH_BYTES - 4;

      reformatted_hdr[0] = 0xAAAAAAAA;
      reformatted_hdr[1] = ch_id              << 0;
      reformatted_hdr[1] |= board_id          << 4;
      reformatted_hdr[1] |= ((uint32_t)(REFORMATTED_HEADER_LENGTH_UINT32 - 1)) << 16;
      reformatted_hdr[2] = timestamp_lower    << 0;
      reformatted_hdr[2] |= timestamp_middle  << 16;
      reformatted_hdr[3] = timestamp_upper    << 0;
      reformatted_hdr[3] |= 0x7               << 23; // event_type
      reformatted_hdr[3] |= ((uint32_t)(REFORMATTED_HEADER_LENGTH_UINT32 - 1)) << 26;
      reformatted_hdr[4] = tdc_ts_lo            << 0;
      reformatted_hdr[4] |= trigger_type      << 16;
      reformatted_hdr[5] = offset[0]          << 0;
      reformatted_hdr[5] |= offset[1]         << 16;
      reformatted_hdr[6] = offset[2]          << 0;
      reformatted_hdr[6] |= offset[3]         << 16;
      reformatted_hdr[7] = aux_data           << 0;
      reformatted_hdr[7] |= wheel             << 16;
      reformatted_hdr[8] = val_p0_p1          << 0;
      reformatted_hdr[8] |= val_p2_p3         << 16;
      reformatted_hdr[9] = 0x00               << 0;       // unused
      reformatted_hdr[9] |= trig_acks          << 16;

      #ifdef WRITEGTFORMAT
        /* create the GEB header */
        Geb.type = GEB_TYPE_DGS;
        Geb.length = packet_length_in_bytes;
        //full 48-bit timestamp stored in 64-bit uint32_t.
        Geb.timestamp = ((uint64_t)(timestamp_upper)) << 32;
        Geb.timestamp |= ((uint64_t)(timestamp_middle)) << 16;
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
          if (buffer_uint32[packet_length_in_words] != TRIG_SOE){
            printf ("ooops:	Packet length should be %i, but 0xAAAAXXXX not found at boundary! Got %08X instead. skip block...\n", packet_length_in_words, buffer_uint32[packet_length_in_words]);
            return -2;
          }
      }
    }else{
      printf(" Other SOE \n");
      return dumpUnknownDataToDaigFile(buffer, size2write);
    }


    /* see if the proper file is open */
    /* or open it */

  #ifdef FILTER_TYPE_F
    if(header_type != 0xF) {
  #else
    // report an error if the header type is 0xF and the channel number is not > 9
    if ((header_type == 0xF) && (ch_id <= 9)){
      printf ("Error: Type F header reported as channel 9 or less. ch_id = %d", ch_id);
    }else{
  #endif

  #ifdef SINGLE_FILE
    #ifdef SINGLESHOT
      #ifdef WRITEGTFORMAT
        if (bytes_written_to_file + packet_length_in_bytes + sizeof(GEBDATA) > max_file_size)
      #else
        if (bytes_written_to_file + packet_length_in_bytes + sizeof(soe) > max_file_size)
      #endif // WRITEGTFORMAT
        {
          return packet_length_in_bytes + sizeof(soe);
        }
    #endif // SINGLESHOT
    
    if (!FILE_OPEN_CHECK(ofile))
  #else
    #ifdef FILE_PER_CHANNEL	// MBO 20200616:
      #ifdef SINGLESHOT
        #ifdef WRITEGTFORMAT
          if (bytes_written_to_file[board_id][ch_id] + packet_length_in_bytes + sizeof(GEBDATA) > max_file_size)
        #else
          if (bytes_written_to_file[board_id][ch_id] + packet_length_in_bytes + sizeof(soe) > max_file_size)
        #endif // WRITEGTFORMAT
          {
            write_inhibit[board_id][ch_id] = 1;
            return packet_length_in_bytes + sizeof(soe);
          }
      #endif // SINGLESHOT

      if (!FILE_OPEN_CHECK(ofile[board_id][ch_id]))
    #else
      #ifdef SINGLESHOT
        #ifdef WRITEGTFORMAT
          if (bytes_written_to_file[board_id] + packet_length_in_bytes + sizeof(GEBDATA) > max_file_size)
        #else
          if (bytes_written_to_file[board_id] + packet_length_in_bytes + sizeof(soe) > max_file_size)
        #endif // WRITEGTFORMAT
          {
            write_inhibit[board_id] = 1;
            return packet_length_in_bytes + sizeof(soe);
          }
      #endif // SINGLESHOT
        if (!FILE_OPEN_CHECK(ofile[board_id]))
    #endif
  #endif // else of ifdef SINGLE_FILE

      {
        if (is_trigger_data){
          /* filename */
          #ifdef SINGLE_FILE
                  sprintf (str, "trig_%s", fn);
          #else
              #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                  sprintf (str, "trig_%s_%4.4i_%01X", fn, board_id, ch_id);
              #else
                  sprintf (str, "trig_%s_%4.4i", fn, board_id);
              #endif
          #endif

          #ifdef DEBUG_OUTPUT_FILE
              #ifdef SINGLE_FILE
                  sprintf (diag_str, "diag_trig_%s", fn);
              #else
                  #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                      sprintf (diag_str, "diag_trig_%s_%4.4i_%01X", fn, board_id, ch_id);
                  #else
                      sprintf (diag_str, "diag_trig_%s_%4.4i", fn, board_id);
                  #endif
              #endif
          #endif // DEBUG_OUTPUT_FILE

        }else{
            /* filename */
            #ifdef SINGLE_FILE
                    sprintf (str, "%s", fn);
            #else
                #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                    sprintf (str, "%s_%4.4i_%01X", fn, board_id, ch_id);
                #else
                    sprintf (str, "%s_%4.4i", fn, board_id);
                #endif
            #endif
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
        };

        /* open file */
        #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
            #ifdef SINGLE_FILE
                ofile = open (str, O_WRONLY | O_CREAT | O_NONBLOCK, 0644);
            #else
                #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                    // MBO 20200616: Let's try going faster on writes with O_NONBLOCK
                    ofile[board_id][ch_id] = open (str, O_WRONLY | O_CREAT | O_NONBLOCK, 0644);
                #else
                    // MBO 20200616: Let's try going faster on writes with O_NONBLOCK
                    ofile[board_id] = open (str, O_WRONLY | O_CREAT | O_NONBLOCK, 0644);
                #endif
            #endif
        #else
            #ifdef SINGLE_FILE
                ofile = fopen (str, "wb");
                file_buffer = (char*)malloc(FILE_BUF_SIZE);
                setvbuf(ofile, file_buffer, _IOFBF, FILE_BUF_SIZE);
            #else
                #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                    ofile[board_id][ch_id] = fopen (str, "wb");
                    #if defined(SINGLESHOT) && defined(FULL_FILE_MODE)
                        write_inhibit[board_id][ch_id] = 0;
                    #endif
                    file_buffer[board_id][ch_id] = (char*)malloc(FILE_BUF_SIZE);
                    setvbuf(ofile[board_id][ch_id], file_buffer[board_id][ch_id], _IOFBF, FILE_BUF_SIZE);
                #else
                    ofile[board_id] = fopen (str, "wb");
                    #if defined(SINGLESHOT) && defined(FULL_FILE_MODE)
                        write_inhibit[board_id] = 0;
                    #endif
                    file_buffer[board_id] = (char*)malloc(FILE_BUF_SIZE);
                    setvbuf(ofile[board_id], file_buffer[board_id], _IOFBF, FILE_BUF_SIZE);
                #endif
            #endif
        #endif
                
        
        if (is_trigger_data) {
            #ifdef DEBUG_OUTPUT_FILE
                #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                    #ifdef SINGLE_FILE
                        diag_ofile = open (diag_str, O_WRONLY | O_CREAT | O_NONBLOCK, 0644);
                    #else
                        #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                            // MBO 20200616: Let's try going faster on writes with O_NONBLOCK
                            diag_ofile[board_id][ch_id] = open (diag_str, O_WRONLY | O_CREAT | O_NONBLOCK, 0644);
                        #else
                            // MBO 20200616: Let's try going faster on writes with O_NONBLOCK
                            diag_ofile[board_id] = open (diag_str, O_WRONLY | O_CREAT | O_NONBLOCK, 0644);
                        #endif
                    #endif
                #else
                    #ifdef SINGLE_FILE
                        diag_ofile = fopen (diag_str, "wb");
                        diag_file_buffer = (char*)malloc(FILE_BUF_SIZE);
                        setvbuf(diag_ofile, diag_file_buffer, _IOFBF, FILE_BUF_SIZE);
                    #else
                        #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                            diag_ofile[board_id][ch_id] = fopen (diag_str, "wb");
                            diag_file_buffer[board_id][ch_id] = (char*)malloc(FILE_BUF_SIZE);
                            setvbuf(diag_ofile[board_id][ch_id], diag_file_buffer[board_id][ch_id], _IOFBF, FILE_BUF_SIZE);
                        #else
                            diag_ofile[board_id] = fopen (diag_str, "wb");
                            diag_file_buffer[board_id] = (char*)malloc(FILE_BUF_SIZE);
                            setvbuf(diag_ofile[board_id], diag_file_buffer[board_id], _IOFBF, FILE_BUF_SIZE);
                        #endif
                    #endif
                #endif
            #endif // DEBUG_OUTPUT_FILE
        }

        printf("First event received from: ");
#ifdef SINGLE_FILE
#else
  printf("BOARD_ID: %3.3i ", board_id);
  #ifdef FILE_PER_CHANNEL // MBO 20200616:
    printf("CH_ID: %01X ", ch_id);
  #endif
#endif

#ifdef SINGLE_FILE
  if (FILE_OPEN_CHECK(ofile)){
#else
  #ifdef FILE_PER_CHANNEL // MBO 20200616:
    if (FILE_OPEN_CHECK(ofile[board_id][ch_id])){
  #else
      if (FILE_OPEN_CHECK(ofile[board_id])){
  #endif
        if (min_board_id > board_id)  min_board_id = board_id;
        if (max_board_id < board_id)  max_board_id = board_id;
#endif
        printf("Opened new file %s\n", str);
      }else{
        printf("ERROR\nERROR: failed to open file %s, quit\n", str);
        forced_stop();
      };
    };

    /* write GEB header out */
    #ifndef NO_SAVE_BUT_STILL_PROCESS
        #ifdef WRITEGTFORMAT
                #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                    #ifdef SINGLE_FILE
                        wstat = nonblocking_file_write(ofile, (char *) &Geb, sizeof (GEBDATA));
                    #else
                        #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                            wstat = nonblocking_file_write(ofile[board_id][ch_id], (char *) &Geb, sizeof (GEBDATA));
                        #else
                            wstat = nonblocking_file_write(ofile[board_id], (char *) &Geb, sizeof (GEBDATA));
                        #endif
                    #endif
                    if (wstat != sizeof (GEBDATA))
                    {
                        printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
                        forced_stop();
                        return -4;
                    }
                #else
                    #ifdef SINGLE_FILE
                        wstat = fwrite ((char *) &Geb, sizeof (GEBDATA), 1, ofile);
                    #else
                        #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                            wstat = fwrite ((char *) &Geb, sizeof (GEBDATA), 1, ofile[board_id][ch_id]);
                        #else
                            wstat = fwrite ((char *) &Geb, sizeof (GEBDATA), 1, ofile[board_id]);
                        #endif
                    #endif
                    if (wstat != 1)
                    {
                        printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
                        forced_stop();
                        return -4;
                    }
                #endif
                
                #ifdef SINGLE_FILE
                    bytes_written_to_file += sizeof (GEBDATA);
                #else
                    #ifdef FILE_PER_CHANNEL	// MBO 20200620:
                        bytes_written_to_file[board_id][ch_id] += sizeof (GEBDATA);
                    #else
                        bytes_written_to_file[board_id] += sizeof (GEBDATA);
                    #endif
                #endif

                *writtenBytes += sizeof (GEBDATA);
        #else // if not defined WRITEGTFORMAT
                #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                    #ifdef SINGLE_FILE
                        wstat = nonblocking_file_write(ofile, (char *) &(soe), sizeof (soe));
                    #else
                        #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                            wstat = nonblocking_file_write(ofile[board_id][ch_id], (char *) &(soe), sizeof (soe));
                        #else
                            wstat = nonblocking_file_write(ofile[board_id], (char *) &(soe), sizeof (soe));
                        #endif
                    #endif
                    if (wstat != sizeof (soe))
                    {
                        printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
                        forced_stop();
                        return -4;
                    }
                #else
                    #ifdef SINGLE_FILE
                        wstat = fwrite ((char *) &(soe), sizeof (soe), 1, ofile);
                    #else
                        #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                            wstat = fwrite ((char *) &(soe), sizeof (soe), 1, ofile[board_id][ch_id]);
                        #else
                            wstat = fwrite ((char *) &(soe), sizeof (soe), 1, ofile[board_id]);
                        #endif
                    #endif
                    if (wstat != 1)
                    {
                        printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
                        forced_stop();
                        return -4;
                    }
                #endif
                #ifdef SINGLE_FILE
                    bytes_written_to_file += sizeof (soe);
                #else
                    #ifdef FILE_PER_CHANNEL	// MBO 20200620:
                        bytes_written_to_file[board_id][ch_id] += sizeof (soe);
                    #else
                        bytes_written_to_file[board_id] += sizeof (soe);
                    #endif
                #endif

                *writtenBytes += sizeof (soe);
        #endif // WRITEGTFORMAT

        /* write payload out */
        if (is_digitizer_data){
            #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                #ifdef SINGLE_FILE
                    wstat = nonblocking_file_write(ofile, (char *) (buffer + buffer_position), packet_length_in_bytes);
                #else
                    #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                        wstat = nonblocking_file_write(ofile[board_id][ch_id], (char *) (buffer + buffer_position), packet_length_in_bytes);
                    #else
                        wstat = nonblocking_file_write(ofile[board_id], (char *) (buffer + buffer_position), packet_length_in_bytes);
                    #endif
                #endif
                if (wstat != packet_length_in_bytes)
                {
                    printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
                    forced_stop();
                    return -4;
                }
            #else
                #ifdef SINGLE_FILE
                    wstat = fwrite ((char *) (buffer + buffer_position), packet_length_in_bytes, 1, ofile);
                #else
                    #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                        wstat = fwrite ((char *) (buffer + buffer_position), packet_length_in_bytes, 1, ofile[board_id][ch_id]);
                    #else
                        wstat = fwrite ((char *) (buffer + buffer_position), packet_length_in_bytes, 1, ofile[board_id]);
                    #endif
                #endif
                if (wstat != 1)
                {
                    printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
                    forced_stop();
                    return -4;
                }
            #endif
        }
        
        if (is_trigger_data){
                #ifdef DEBUG_OUTPUT_FILE
                    #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                    #else
                        #ifdef SINGLE_FILE
                        #else
                            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[0]);
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[1]);
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[2]);
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[3]);
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[4]);
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[5]);
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[6]);
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[7]);
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[8]);
                                fprintf(diag_ofile[board_id][ch_id], "%.08X\n", reformatted_hdr[9]);
                                fprintf(diag_ofile[board_id][ch_id], "trigger_type: %.08X\n", trigger_type);
                                fprintf(diag_ofile[board_id][ch_id], "timestamp_lower: %.08X\n", timestamp_lower);
                                fprintf(diag_ofile[board_id][ch_id], "timestamp_middle: %.08X\n", timestamp_middle);
                                fprintf(diag_ofile[board_id][ch_id], "timestamp_upper: %.08X\n", timestamp_upper);
                                fprintf(diag_ofile[board_id][ch_id], "wheel: %.08X\n", wheel);
                                fprintf(diag_ofile[board_id][ch_id], "aux_data: %.08X\n", aux_data);
                                fprintf(diag_ofile[board_id][ch_id], "board_id: %.08X\n", board_id);
                                fprintf(diag_ofile[board_id][ch_id], "tdc_ts_lo: %.08X\n", tdc_ts_lo);
                                fprintf(diag_ofile[board_id][ch_id], "trig_acks: %.08X\n", trig_acks);
                                fprintf(diag_ofile[board_id][ch_id], "offset[0]: %.08X\n", offset[0]);
                                fprintf(diag_ofile[board_id][ch_id], "offset[1]: %.08X\n", offset[1]);
                                fprintf(diag_ofile[board_id][ch_id], "offset[2]: %.08X\n", offset[2]);
                                fprintf(diag_ofile[board_id][ch_id], "offset[3]: %.08X\n", offset[3]);
                                fprintf(diag_ofile[board_id][ch_id], "val_p0_p1: %.08X\n", val_p0_p1);
                                fprintf(diag_ofile[board_id][ch_id], "val_p2_p3: %.08X\n", val_p2_p3);
                            #else
                                fprintf(diag_ofile[board_id], "%.08X\n", (char *)(&(reformatted_hdr[0])));
                            #endif
                        #endif
                    #endif
                #endif // DEBUG_OUTPUT_FILE
                #ifdef USE_POSIX_FILE_LIB	// MBO 20200616:
                    #ifdef SINGLE_FILE
                        wstat = nonblocking_file_write(ofile, (char *)(&(reformatted_hdr[1])), packet_length_in_bytes);
                    #else
                        #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                            wstat = nonblocking_file_write(ofile[board_id][ch_id], (char *)(&(reformatted_hdr[1])), packet_length_in_bytes);
                        #else
                            wstat = nonblocking_file_write(ofile[board_id], (char *)(&(reformatted_hdr[1])), packet_length_in_bytes);
                        #endif
                    #endif
                    if (wstat != packet_length_in_bytes)
                    {
                        printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
                        forced_stop();
                        return -4;
                    }
                #else
                    #ifdef SINGLE_FILE
                        wstat = fwrite ((char *)(&(reformatted_hdr[1])), packet_length_in_bytes, 1, ofile);
                    #else
                        #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                            wstat = fwrite ((char *)(&(reformatted_hdr[1])), packet_length_in_bytes, 1, ofile[board_id][ch_id]);
                        #else
                            wstat = fwrite ((char *)(&(reformatted_hdr[1])), packet_length_in_bytes, 1, ofile[board_id]);
                        #endif
                    #endif
                    if (wstat != 1)
                    {
                        printf("FILE WRITE ERROR: BOARD: %i CH: %0X", board_id, ch_id);
                        forced_stop();
                        return -4;
                    }
                #endif
            }
        #endif // NO_SAVE_BUT_STILL_PROCESS
        #ifdef SINGLE_FILE
            bytes_written_to_file += packet_length_in_bytes;
        #else
            #ifdef FILE_PER_CHANNEL	// MBO 20200620:
                bytes_written_to_file[board_id][ch_id] += packet_length_in_bytes;
            #else
                bytes_written_to_file[board_id] += packet_length_in_bytes;
            #endif
        #endif
        *writtenBytes += packet_length_in_bytes;
        #ifdef FILTER_TYPE_F
        }	// end if header_type != 0xF
        #else
        }   // end else
        #endif
        if (is_trigger_data)
        {
            buffer_position += TRIG_MIN_HEADER_LENGTH_BYTES;
        }
        else
        {
            buffer_position += packet_length_in_bytes;
        }
        buffer_uint32 = (uint32_t *) (buffer + buffer_position);

//		if (header_type == 0xF)
//		{
//			printf("\n\ntype F: %08X %08X %08X %X %X \n\n", hdr[0],  hdr[1],  hdr[2], event_type, ch_id);
//		}

        if ((header_type == 0xF) && (event_type == 0x0) && (ch_id == 0xD))
        {
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

	#ifdef SINGLE_FILE
	#else
		#if defined(SINGLESHOT) && defined(FULL_FILE_MODE)
			uint32_t all_inhibited;
		#endif // defined
		uint32_t i;
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			int32_t j;
		#endif // FILE_PER_CHANNEL
	#endif // SINGLE_FILE


	// Show version and all build switches"
	printf ("dgsReceiver.cpp V%s \n", VERSION);
	printf ("\n");
	// Now list build parameteres:
	printf ("Compiled with the following options:\n");
	//======================= Write Data Format
    #if defined(NO_SAVE) || defined(NO_SAVE_BUT_STILL_PROCESS)
        for (int32_t sim = 0; sim < 20; sim++) printf ("!!! SIMULATION MODE !!!  NO DATA WILL BE SAVED !!!\n");
    #else
        #ifdef WRITEGTFORMAT
            printf ("Data Format: GEB\n");
        #else
            printf ("Data Format: RAW\n");
        #endif
    #endif 
	//======================= Operation mode
    #ifdef SINGLESHOT
        printf ("Operating Mode: Single Shot\n");
		#ifdef FULL_FILE_MODE
			printf ("Stop Mode: All Files Full\n");
		#else
			printf ("Stop Mode: First File Full\n");
		#endif 
	#else
	    printf ("Operating Mode: Continuous\n");
    #endif 
	//======================= Disk IO
    #ifdef USE_POSIX_FILE_LIB
        printf ("Disk IO Library: POSIX\n");
    #else
        printf ("Disk IO Library: ANSI C\n");
    #endif
	//======================= Network Library
    #ifdef __WIN32__
        printf ("Network Library: Winsock2\n");
    #else
        printf ("Network Library: GNU\n");
    #endif
	//======================= Filter Type F Message
    #ifdef FILTER_TYPE_F
        printf ("Type F Message Filter: Enabled\n");
    #else
        printf ("Type F Message Filter: Disabled\n");
    #endif
	//======================= Network Library 
    #ifdef SINGLE_FILE
        printf ("Data Organization: File per IOC\n");
    #else
        #ifdef FILE_PER_CHANNEL
            printf ("Data Organization: File per Channel\n");
        #else
            printf ("Data Organization: File per Digitizer\n");
        #endif 
    #endif 
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

	#ifdef __WIN32__
		// Declare and initialize variables
		WSADATA wsaData;
		int32_t iResult;
		// Initialize Winsock
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			printf("WSAStartup failed: %d\n", iResult);
			return 1;
		}
	#endif // __WIN32__

	#ifdef SINGLE_FILE
		bytes_written_to_file = 0;
		ofile = 0;
	#else
		min_board_id = 0xFFFF;
		max_board_id = 0x0000;
		#ifdef FILE_PER_CHANNEL	// MBO 20200616:
			for (i = 0; i < MAXBOARDID; i++){
				for (j = 0; j < MAXCHID; j++) {
					bytes_written_to_file[i][j] = 0;
					ofile[i][j] = 0;
				#if defined(SINGLESHOT) && defined(FULL_FILE_MODE)
					write_inhibit[i][j] = 2;
				#endif // FULL_FILE_MODE
				}
			}
		#else
			for (i = 0; i < MAXBOARDID; i++) {
				bytes_written_to_file[i] = 0;
				ofile[i] = 0;
				#if defined(SINGLESHOT) && defined(FULL_FILE_MODE)
					write_inhibit[i] = 2;
				#endif // FULL_FILE_MODE
			}
		#endif // FILE_PER_CHANNEL
	#endif // SINGLE_FILE

	/* help */

	#ifdef WRITEGTFORMAT
	if (argc == 7) debug = 1;
	#else
	if (argc == 6) debug = 1;
	#endif

	#ifdef WRITEGTFORMAT
	if (argc < 5){
	#else
	if (argc < 4){
	#endif
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
        #ifdef SINGLE_FILE
		printf ("                     The actual file name will be <filename>_<chunk number>\n");
		printf ("                     e.g. data_run_001.gtd_001\n");
        #else
            #ifdef FILE_PER_CHANNEL
        printf ("                     The actual file name will be <filename>.<extension_prefix>_<chunk number>_<board_id>_<ch_id>\n");
        printf ("                     e.g. data_run_001.gtd_001_1234_3 = Chunk:1, Board_ID:1234, Ch_ID:3\n");
            #else
        printf ("                     The actual file name will be <filename>.<extension_prefix>_<chunk number>_<board_id>\n");
        printf ("                     e.g. data_run_001.gtd_001_1234 = Chunk:1, Board_ID:1234\n");
            #endif // FILE_PER_CHANNEL
        #endif // SINGLE_FILE
		printf ("\n");
		printf ("<3:maxfilesize> specifies the max file size in bytes. If a file\n");
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
			printf ("<4:GEBID> has no effect on the operation of the receiver.  This number\n");
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
		#ifdef __WIN32__
			sprintf (fn, "%s\\%s.%s_%3.3i", argv[2], argv[2], argv[3], chunck);
		#else
			sprintf (fn, "%s/%s.%s_%3.3i", argv[2], argv[2], argv[3], chunck);
		#endif // __WIN32__
    #else
        sprintf (fn, "%s.%s_%3.3i", argv[2], argv[3], chunck);
    #endif // FOLDER_PER_RUN

    #ifdef FOLDER_PER_RUN
        int32_t mkdir_ret;
        // Need to check if folder already exists
        // and make the folder if it does not.
        #ifdef __WIN32__
            mkdir_ret = _mkdir(argv[2]);
        #else
            mkdir_ret = mkdir(argv[2], 0777);
        #endif // __WIN32__
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
	#ifdef __WIN32__
		sprintf (hostIP, "%s", inet_ntoa (*(in_addr*)(hp->h_addr)));
	#else
		sprintf (hostIP, "%s", inet_ntop (hp->h_addrtype, hp->h_addr, hostIP, sizeof (hostIP)));
	#endif

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
							#ifndef NO_SAVE_BUT_STILL_PROCESS
								#ifdef SINGLESHOT
                                    #if defined(FULL_FILE_MODE) && (!defined(SINGLE_FILE))
                                        if (min_board_id <= max_board_id){
                                            all_inhibited = 1;
                                            #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                                                for (i = min_board_id; i <= max_board_id; i++){
                                                    for (j = 0; j < MAXCHID; j++){
                                                        if (write_inhibit[i][j] == 0)all_inhibited = 0;
													}
												}
                                            #else
                                                for (i = min_board_id; i <= max_board_id; i++){
                                                    if (write_inhibit[i] == 0) all_inhibited = 0;
												}
                                            #endif // FILE_PER_CHANNEL
                                            if (all_inhibited == 1) forced_stop();
                                        }
									#endif // defined
								#else
									if ((totbytesInLargestFile + num_bytes_read) > max_file_size){
										/* set the new file name */
										#ifdef __WIN32__
											printf ("file size reached %I64d of %I64d limit\n", totbytesInLargestFile, max_file_size);
										#else
											printf("file size reached %" PRId64 " of %" PRId64 " limit\n", totbytesInLargestFile, max_file_size);												
										#endif 

										/* properly close the old files */
										#ifdef SINGLESHOT
											forced_stop();
										#else
											close_all();
										#endif 

										chunck++;
                                        #ifdef FOLDER_PER_RUN
											#ifdef __WIN32__
												sprintf (fn, "%s\\%s.%s_%3.3i", argv[2], argv[2], argv[3], chunck);
											#else
												sprintf (fn, "%s/%s.%s_%3.3i", argv[2], argv[2], argv[3], chunck);
											#endif 
                                        #else
                                            sprintf (fn, "%s.%s_%3.3i", argv[2], argv[3], chunck);
                                        #endif // FOLDER_PER_RUN

										printf ("Starting new data chunk: #%3.3i\n", chunck);
										fflush (stdout);

										//print_info (totbytes);
									}
								#endif // defined
							#endif // not NO_SAVE_BUT_STILL_PROCESS


							#ifndef NO_SAVE
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
								#if defined(SINGLESHOT) && ((!defined(FULL_FILE_MODE)) || (defined(SINGLE_FILE)))
                                    /* properly close the old files */
                                    forced_stop();
                                #endif // defined
                                }
							#else
								nwritten = num_bytes_read;
							#endif

							/* keep user informed */

							totbytes += nwritten;
#if(0)
							printf ("nwritten=%i, totbytes=%lli\n", nwritten, totbytes);
#endif
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

							#ifndef SINGLESHOT
                                #ifdef SINGLE_FILE
                                    if (totbytesInLargestFile < bytes_written_to_file)
                                        totbytesInLargestFile = bytes_written_to_file;
                                #else
                                    if (min_board_id <= max_board_id){
                                    #ifdef FILE_PER_CHANNEL	// MBO 20200616:
                                        for (i = min_board_id; i <= max_board_id; i++){
                                            for (j = 0; j < MAXCHID; j++){
                                                if (totbytesInLargestFile < bytes_written_to_file[i][j])
                                                    totbytesInLargestFile = bytes_written_to_file[i][j];
											}
										}
                                    #else
                                        for (i = min_board_id; i <= max_board_id; i++){
                                            if (totbytesInLargestFile < bytes_written_to_file[i])
                                                totbytesInLargestFile = bytes_written_to_file[i];
										}
                                    #endif // FILE_PER_CHANNEL
                                    }
                                #endif // SINGLE_FILE
                            #endif // SINGLESHOT
						} while (st > 0);


				};

		}


	/* done (we should never really get here) */

	exit (0);


}

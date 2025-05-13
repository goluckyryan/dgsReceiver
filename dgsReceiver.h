//--------------------------------------------------------------------------------
// Company:		Argonne National Laboratory
// Division:	Physics
// Project:		DGS Receiver
// Author:		Michael Oberling
// File:		dgsReceiver.h
// Description: Receiver data type definitions.
//--------------------------------------------------------------------------------

#ifndef _DGS_RECEIVER_H
#define _DGS_RECEIVER_H

#define SERVER_PORT 9001       /* where to send requests */

#define AOK 0
#define ERROR -1
/* connect to server. parameter is dotted decimal address */
char *initReceiver(char *server);

#define SOMEDATAMISSING -2
#define NODATA -3
#define TIMEOUT -6
/* request the data.  len is in seconds and generally in range 0.1 to 1.0.
 * This does not return until data is in.  Data will be contiguous and in 
 * order.
 */
int getReceiverData(char *instance, float len, char **dat);

#define OUTOFEVENTS -4
#define BUFFERSMALL -5
/* Get a single event from the pool received by getReceiverData(). Return 
 * 0 if OK, -4 if out of events, -5 if buffer too small.  If out of events,
 * do another call to getReceiverData(), and expect a long time gap between the
 * last event and the next.
 */
int getEvent(char *instance, unsigned int **buffer);

/* close socket, etc */
int stopReceiver(char *instance);

#define INBUFSIZE (64 * 1024)
#define EVENT_MARKER 0xaaaaaaaa

extern int packetsreceived;
extern int packetssent;
extern int seqerrs;
extern int bytesrec;

#ifdef SOLARIS
#define INADDR_NONE -1
#endif

struct inbuf {
   struct inbuf *next;
   unsigned int type;
   unsigned int evtlen;
   unsigned int recs;
   unsigned int *pdata;
};

extern struct inbuf *datRoot;
extern struct inbuf *datEnd;

int printPackets(char *instance);


#endif // _DGS_RECEIVER_H

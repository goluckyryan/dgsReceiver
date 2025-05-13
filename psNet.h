/* this is the server-side API for the network application layer.  The thought
 * is that the network application layer is encapsulated here so we could have
 * different files with different implementsations.
 */

/* 
 * a couple of structs used both by sender and API 
 */
typedef struct {
   int type;		/* usually SERVER_SUMMARY */
   int recLen;		/* 32 bit words/record -DEPRECATED FOR DGS */
   int status;		/* 0 for success */
   int recs;		/* number of records */
 //  int tot_datasize;	/*total data for all records- for DGS*/
} evtServerRetStruct;

struct reqPacket {
   int type;
};

struct incoming {
   struct sockaddr_in client_addr;
   int addrlen;
   int clnt;	/* client socket for tcp */	
   int len;
   struct eNode *outlist;
   /* the request buffer is called on to hold either a struct reqPacket or 
      an evtServerRetStruct at diffent times in its life...
   */
   char request[28];
};


/* udp sender */
#define SENDERPACKSIZE 1200
#define SENDER_PORT 1101	

/* main data server */
#define SERVER_PORT 9001       /* where to send requests */


/* function prototypes */

/* initialize sender network interface
 */
int initSenderNet();

/* meant to be run as a task, this opens a socket and waits for requests. 
   puts requests in reqQ.  Spawned by senderInit().  should be restartable */
int requestGrabber();

/* this actually does the data sending */
int sendToNet(struct eNode *node, struct incoming *req, int last);
/* This indicates that the sending of this data pool is complete.  Req is the
   summary packet.
 */
int  sendRetStruct(struct incoming *req);


/* some things used by both sender and receiver */


#define CLIENT_REQUEST_EVENTS 1
#define SERVER_NORMAL_RETURN 2
#define SERVER_SENDER_OFF 3
#define SERVER_SUMMARY 4
#define INSUFF_DATA 5
#define TARGSIZE (7*1024)

CC=g++
CFLAG= -O3 -lstdc++  -Wall -Wextra

all: dgsReceiver_Ryan dgsReceiver tcp_Receiver

dgsReceiver_Ryan: dgsReceiver_Ryan.cpp 
	$(CC) $(CFLAG) dgsReceiver_Ryan.cpp -o dgsReceiver_Ryan 

dgsReceiver: dgsReceiver.cpp 
	$(CC) $(CFLAG) dgsReceiver.cpp -o dgsReceiver 

tcp_Receiver: tcp_Receiver.cpp 
	$(CC) $(CFLAG) tcp_Receiver.cpp -o tcp_Receiver 

clean:
	-rm dgsReceiver_Ryan dgsReceiver tcp_Receiver

CC=g++
CFLAG= -O3 -lstdc++  -Wall -Wextra

all: Ryan dgsReceiver

Ryan: dgsReceiver_Ryan.cpp 
	$(CC) $(CFLAG) dgsReceiver_Ryan.cpp -o dgsReceiver_Ryan 

dgsReceiver: dgsReceiver.cpp 
	$(CC) $(CFLAG) dgsReceiver.cpp -o dgsReceiver 

clean:
	-rm dgsReceiver_Ryan dgsReceiver

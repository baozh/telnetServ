LIB_NET_COMM_PATH = ../lib/
LIB_NET_COMM = -L$(LIB_NET_COMM_PATH) -lnet 

LIB_BASE_COMM_PATH = ../lib/
LIB_BASE_COMM = -L$(LIB_BASE_COMM_PATH) -lbase -lpthread -lrt

LIB_BOOST_COMM_PATH := /export/newjpush/lib
LIB_BOOST_COMM := -L$(LIB_BOOST_COMM_PATH)

INC_COMM_DIR = -I../  -I/export/newjpush/include

LIB_COMM = $(LIB_BASE_COMM) $(LIB_NET_COMM) $(LIB_BOOST_COMM)

C_ARGS := -g -O -Wall -fno-common
BINARY := testTelnet

CXX = g++

OI_OBJS := telnetServ.o main.o

all:$(BINARY)

.cc.o:
	$(CXX) $(C_ARGS)  -c $^ -o $@ $(INC_COMM_DIR) -DMUDUO_STD_STRING

$(BINARY):$(OI_OBJS)
	$(CXX) $(C_ARGS) -o $@ $^ $(LIB_COMM)

clean:
	rm -f *.o $(BINARY)

	
	
	

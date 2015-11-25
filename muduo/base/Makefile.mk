
INC_INSTALL_COMM_PATH = ../include/muduo/base
LIB_INSTALL_COMM_PATH = ../../lib

INC_BASE_COMM_DIR = -I../include/ -I../../ -I/export/newjpush/include



HEADERS := $(wildcard *.h )


.SUFFIXES: .o .cc

cxx = g++


CFLAGS = -g -O -Wall $(INC_BASE_COMM_DIR)

OI_OBJS  = AsyncLogging.o Condition.o CountDownLatch.o Date.o Exception.o \
           FileUtil.o LogFile.o Logging.o LogStream.o \
           ProcessInfo.o Thread.o ThreadPool.o Timestamp.o TimeZone.o  

OUTPUT := libbase.a

all:$(OUTPUT)

.cc.o:
	$(CXX) $(CFLAGS) -c $^ -o $@  -DMUDUO_STD_STRING

mvHead:
	-mkdir -p $(INC_INSTALL_COMM_PATH)
	cp -f -R $(HEADERS) $(INC_INSTALL_COMM_PATH)/

libbase.a:$(OI_OBJS)
	ar -rs $@  $^

install:
	cp -f -R $(OUTPUT)  $(LIB_INSTALL_COMM_PATH)/

uninstall:
	rm -rf $(addprefix $(INC_INSTALL_COMM_PATH)/, $(HEADERS))
	rm -rf $(LIB_INSTALL_COMM_PATH)/$(OUTPUT)
clean:
	rm -f *.o *.a

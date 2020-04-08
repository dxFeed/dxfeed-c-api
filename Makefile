.POSIX:
.SUFFIXES:
.PHONY: all clean depends

OBJ = obj
BIN = bin
COMMON_CFLAGS	= -O2 -DUSE_PTHREADS -std=c99 -D_POSIX_SOURCE \
		  -D_POSIX_C_SOURCE=200809L -fPIC -I../include
COMMON_LDFLAGS	= -lrt -pthread
LIB	= libDXFeed.so
LIB_TARGET = $(BIN)/$(LIB)

all: $(LIB_TARGET)

clean:
	rm -f $(BIN)/*.so
	rm -f $(OBJ)/*.o

# Common

Prepare:
	mkdir -p obj
	mkdir -p bin

# DXFeed library

CFLAGS	+= $(COMMON_CFLAGS) -I../src
LDFLAGS	+= $(COMMON_LDFLAGS)

OBJS	= \
	  $(OBJ)/BufferedInput.c.o \
	  $(OBJ)/BufferedIOCommon.c.o \
	  $(OBJ)/BufferedOutput.c.o \
	  $(OBJ)/Candle.c.o \
	  $(OBJ)/ClientMessageProcessor.c.o \
	  $(OBJ)/ConfigurationDeserializer.c.o \
	  $(OBJ)/ConnectionContextData.c.o \
	  $(OBJ)/DataStructures.c.o \
	  $(OBJ)/Decimal.c.o \
	  $(OBJ)/DXAddressParser.c.o \
	  $(OBJ)/DXAlgorithms.c.o \
	  $(OBJ)/DXErrorCodes.c.o \
	  $(OBJ)/DXErrorHandling.c.o \
	  $(OBJ)/DXFeed.c.o \
	  $(OBJ)/DXMemory.c.o \
	  $(OBJ)/DXNetwork.c.o \
	  $(OBJ)/DXPMessageData.c.o \
	  $(OBJ)/DXProperties.c.o \
	  $(OBJ)/DXSockets.c.o \
	  $(OBJ)/DXThreads.c.o \
	  $(OBJ)/EventData.c.o \
	  $(OBJ)/EventManager.c.o \
	  $(OBJ)/EventSubscription.c.o \
	  $(OBJ)/Linux.c.o \
	  $(OBJ)/Logger.c.o \
	  $(OBJ)/ObjectArray.c.o \
	  $(OBJ)/PriceLevelBook.c.o \
	  $(OBJ)/RecordBuffers.c.o \
	  $(OBJ)/RecordFieldSetters.c.o \
	  $(OBJ)/RecordTranscoder.c.o \
	  $(OBJ)/RegionalBook.c.o \
	  $(OBJ)/ServerMessageProcessor.c.o \
	  $(OBJ)/Snapshot.c.o \
	  $(OBJ)/SymbolCodec.c.o \
	  $(OBJ)/TaskQueue.c.o \
	  $(OBJ)/TestParser.c.o \
	  $(OBJ)/Version.c.o

$(LIB_TARGET): Prepare $(OBJS) 
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$(LIB) -o $(LIB_TARGET) $(OBJS)

clean-$(LIB):
	rm -f $(LIB_TARGET) $(OBJS)

$(OBJ)/BufferedInput.c.o: ../src/BufferedInput.c ../include/DXFeed.h \
	../src/BufferedInput.h ../src/DXErrorHandling.h ../src/DXMemory.h \
	../src/DXAlgorithms.h ../src/ConnectionContextData.h
	$(CC) $(CFLAGS) ../src/BufferedInput.c -c -o ./$@

$(OBJ)/BufferedIOCommon.c.o: ../src/BufferedIOCommon.c ../src/BufferedIOCommon.h \
	../src/DXErrorHandling.h ../src/DXMemory.h
	$(CC) $(CFLAGS) ../src/BufferedIOCommon.c -c -o ./$@

$(OBJ)/BufferedOutput.c.o: ../src/BufferedOutput.c ../include/DXFeed.h \
	../src/BufferedOutput.h ../src/DXErrorHandling.h ../src/DXMemory.h \
	../src/DXAlgorithms.h ../src/DXThreads.h ../src/ConnectionContextData.h
	$(CC) $(CFLAGS) ../src/BufferedOutput.c -c -o $@

$(OBJ)/Candle.c.o:
	$(CC) $(CFLAGS) ../src/Candle.c -c -o $@

$(OBJ)/ClientMessageProcessor.c.o:
	$(CC) $(CFLAGS) ../src/ClientMessageProcessor.c -c -o $@

$(OBJ)/ConfigurationDeserializer.c.o:
	$(CC) $(CFLAGS) ../src/ConfigurationDeserializer.c -c -o $@

$(OBJ)/ConnectionContextData.c.o:
	$(CC) $(CFLAGS) ../src/ConnectionContextData.c -c -o $@

$(OBJ)/DataStructures.c.o:
	$(CC) $(CFLAGS) ../src/DataStructures.c -c -o $@

$(OBJ)/Decimal.c.o:
	$(CC) $(CFLAGS) ../src/Decimal.c -c -o $@

$(OBJ)/DXAddressParser.c.o:
	$(CC) $(CFLAGS) ../src/DXAddressParser.c -c -o $@

$(OBJ)/DXAlgorithms.c.o:
	$(CC) $(CFLAGS) ../src/DXAlgorithms.c -c -o $@

$(OBJ)/DXErrorCodes.c.o:
	$(CC) $(CFLAGS) ../src/DXErrorCodes.c -c -o $@

$(OBJ)/DXErrorHandling.c.o:
	$(CC) $(CFLAGS) ../src/DXErrorHandling.c -c -o $@

$(OBJ)/DXFeed.c.o:
	$(CC) $(CFLAGS) ../src/DXFeed.c -c -o $@

$(OBJ)/DXMemory.c.o:
	$(CC) $(CFLAGS) ../src/DXMemory.c -c -o $@

$(OBJ)/DXNetwork.c.o:
	$(CC) $(CFLAGS) ../src/DXNetwork.c -c -o $@

$(OBJ)/DXPMessageData.c.o:
	$(CC) $(CFLAGS) ../src/DXPMessageData.c -c -o $@

$(OBJ)/DXProperties.c.o:
	$(CC) $(CFLAGS) ../src/DXProperties.c -c -o $@

$(OBJ)/DXSockets.c.o:
	$(CC) $(CFLAGS) ../src/DXSockets.c -c -o $@

$(OBJ)/DXThreads.c.o:
	$(CC) $(CFLAGS) ../src/DXThreads.c -c -o $@

$(OBJ)/EventData.c.o:
	$(CC) $(CFLAGS) ../src/EventData.c -c -o $@

$(OBJ)/EventManager.c.o:
	$(CC) $(CFLAGS) ../src/EventManager.c -c -o $@

$(OBJ)/EventSubscription.c.o:
	$(CC) $(CFLAGS) ../src/EventSubscription.c -c -o $@

$(OBJ)/Linux.c.o:
	$(CC) $(CFLAGS) ../src/Linux.c -c -o $@

$(OBJ)/Logger.c.o:
	$(CC) $(CFLAGS) ../src/Logger.c -c -o $@

$(OBJ)/ObjectArray.c.o:
	$(CC) $(CFLAGS) ../src/ObjectArray.c -c -o $@

$(OBJ)/PriceLevelBook.c.o:
	$(CC) $(CFLAGS) ../src/PriceLevelBook.c -c -o $@

$(OBJ)/RecordBuffers.c.o:
	$(CC) $(CFLAGS) ../src/RecordBuffers.c -c -o $@

$(OBJ)/RecordFieldSetters.c.o:
	$(CC) $(CFLAGS) ../src/RecordFieldSetters.c -c -o $@

$(OBJ)/RecordTranscoder.c.o:
	$(CC) $(CFLAGS) ../src/RecordTranscoder.c -c -o $@

$(OBJ)/RegionalBook.c.o:
	$(CC) $(CFLAGS) ../src/RegionalBook.c -c -o $@

$(OBJ)/ServerMessageProcessor.c.o:
	$(CC) $(CFLAGS) ../src/ServerMessageProcessor.c -c -o $@

$(OBJ)/Snapshot.c.o:
	$(CC) $(CFLAGS) ../src/Snapshot.c -c -o $@

$(OBJ)/SymbolCodec.c.o:
	$(CC) $(CFLAGS) ../src/SymbolCodec.c -c -o $@

$(OBJ)/TaskQueue.c.o:
	$(CC) $(CFLAGS) ../src/TaskQueue.c -c -o $@

$(OBJ)/TestParser.c.o::
	$(CC) $(CFLAGS) ../src/TestParser.c -c -o $@

$(OBJ)/Version.c.o:
	$(CC) $(CFLAGS) ../src/Version.c -c -o $@


.PHONY: all clean depends

CFLAGS+=	-O2 -DUSE_PTHREADS -std=c99 -D_POSIX_C_SOURCE=200112L -fPIC -I../include
LDFLAGS+=	-rt -pthread

LIB=	libDXFeed.so
SRCS=	BufferedInput.c \
	BufferedIOCommon.c \
	BufferedOutput.c \
	Candle.c \
	ClientMessageProcessor.c \
	ConfigurationDeserializer.c \
	ConnectionContextData.c \
	DataStructures.c \
	Decimal.c \
	DXAddressParser.c \
	DXAlgorithms.c \
	DXErrorCodes.c \
	DXErrorHandling.c \
	DXFeed.c \
	DXMemory.c \
	DXNetwork.c \
	DXPMessageData.c \
	DXProperties.c \
	DXSockets.c \
	DXThreads.c \
	EventData.c \
	EventManager.c \
	EventSubscription.c \
	Linux.c \
	Logger.c \
	ObjectArray.c \
	PriceLevelBook.c \
	RecordBuffers.c \
	RecordFieldSetters.c \
	RecordTranscoder.c \
	RegionalBook.c \
	ServerMessageProcessor.c \
	Snapshot.c \
	SymbolCodec.c \
	TaskQueue.c \
	TestParser.c \
	Version.c
OBJS=	$(SRCS:.c=.o)

all: $(LIB)

$(LIB): $(OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(LIB) -o $(LIB) $(OBJS)


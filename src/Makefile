# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Initial Developer of the Original Code is Devexperts LLC.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.


.PHONY: all clean depends

CFLAGS+=	-O2 -DUSE_PTHREADS -std=gnu99 -fPIC -I../include
LDFLAGS+=	-lrt -pthread

LIB=	libdxfeed.so
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

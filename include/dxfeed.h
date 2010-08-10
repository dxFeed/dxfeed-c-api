// dxFeed plain C API

#ifndef _DXFEED_API_H_INCLUDED_
#define _DXFEED_API_H_INCLUDED_

#ifndef OUT
    #define OUT
#endif // OUT

#include "PrimitiveTypes.h"
#include "APITypes.h"

// creates a connection to the specified host
// the function returns the success/error code of the operation,
// if the function succeeds it returns the handle of the opened
// connection via the function's out-parameter

ERRORCODE createConnectedFeed (jstring host, OUT HFEED* feed);

// closes a connection, previously opened by 'openConnection'
// returns the success/error code of the operation

ERRORCODE closeConnection (HFEED* connection); //??????????????? how connection closes id java?

// subscribes to the selected type of events on the given connection
// returns the success/error code of the operation
// if the function succeeds it returns the handle of the created subscription
// via the function's out-parameter
createSubscription(events????)
ERRORCODE addSubscription (HCONNECTION connection, EventType eventType, OUT HSUBSCRIPTION* subscription);

// terminates previously created event subscription
// returns the success/error code of the operation

ERRORCODE removeSubscription (HSUBSCRIPTION subscription);

// adds selected symbols to given subscription, thus making it receive events concerning
// the selected symbols
// returns the success/error code of the operation

ERRORCODE addSymbols (HSUBSCRIPTION subscription, jstring* symbols, int symbolCount);

// removes selected symbols to given subscription, thus making it stop receiving events concerning
// the selected symbols
// returns the success/error code of the operation

ERRORCODE removeSymbols (HSUBSCRIPTION subscription, jstring* symbols, int symbolCount);

// adds symbols from selected instrument profile file to given subscription
// returns the success/error code of the operation

ERRORCODE addSymbolsIpf (HSUBSCRIPTION subscription, jstring filePath);

// removes symbols from selected instrument profile file from given subscription
// returns the success/error code of the operation

ERRORCODE removeSymbolsIpf (HSUBSCRIPTION subscription, jstring filePath);

// attaches the specified event listener function to the given subscription
// returns the success/error code of the operation

ERRORCODE attachEventListener (HSUBSCRIPTION subscription, event_listener_t eventListener, void* userData);

// detaches the specified event listener function from the given subscription
// returns the success/error code of the operation

ERRORCODE detachEventListener (HSUBSCRIPTION subscription, event_listener_t eventListener);


#endif // _DXFEED_API_H_INCLUDED_


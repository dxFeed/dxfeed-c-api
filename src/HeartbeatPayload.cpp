#ifdef __cplusplus
extern "C" {
#endif

#include "HeartbeatPayload.h"

#include "ConnectionContextData.h"
#include "DXAlgorithms.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXFeed.h"
#include "DXThreads.h"
#include "Logger.h"
#include "SymbolCodec.h"

#ifdef __cplusplus
}
#endif

#include "HeartbeatPayload.hpp"

namespace dx {

}

int dx_heartbeat_payload_parse_from(void* buffered_output_connection_context) {
	return true;
}

int dx_heartbeat_payload_compose_to(void* buffered_input_connection_context) {
	return true;
}

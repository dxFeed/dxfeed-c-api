#ifndef SNAPSHOT_TESTS_H_INCLUDED
#define SNAPSHOT_TESTS_H_INCLUDED

#include "PrimitiveTypes.h"

bool snapshot_initialization_test(void);
bool snapshot_duplicates_test(void);
bool snapshot_subscription_test(void);
bool snapshot_multiply_subscription_test(void);
bool snapshot_subscription_and_events_test(void);
bool snapshot_symbols_test(void);

bool snapshot_all_test(void);

#endif /* SNAPSHOT_TESTS_H_INCLUDED */
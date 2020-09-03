#ifndef SNAPSHOT_TESTS_H_INCLUDED
#define SNAPSHOT_TESTS_H_INCLUDED

#include "PrimitiveTypes.h"

int snapshot_initialization_test(void);
int snapshot_duplicates_test(void);
int snapshot_subscription_test(void);
int snapshot_multiply_subscription_test(void);
int snapshot_subscription_and_events_test(void);
int snapshot_symbols_test(void);

int snapshot_all_test(void);

int snapshot_all_unit_test(void);

#endif /* SNAPSHOT_TESTS_H_INCLUDED */
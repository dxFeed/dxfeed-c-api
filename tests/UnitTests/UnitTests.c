
#include "EventSubscriptionTest.h"
#include <stdio.h>
#include "EventDynamicSubscriptionTest.h"
#include "OrderSourceConfigurationTest.h"
#include "CandleTest.h"
#include "SnapshotTests.h"

const char* result_to_text (bool res) {
    if (res) {
        return "OK";
    } else {
        return "FAIL";
    }
}

int main (int argc, char* argv[]) {
        
    printf("DXFeed unit test procedure started...\n");
    printf("\tEvent subscription...\t%s\n", result_to_text(event_subscription_test()));
    printf("\tEvent dynamic subscription...\t%s\n", result_to_text(event_dynamic_subscription_all_test()));
    printf("\tOrder source configuration...\t%s\n", result_to_text(order_source_configuration_test()));
    printf("\tCandle tests:\t%s\n", result_to_text(candle_all_tests()));
    printf("\tSnapshots tests: \t%s\n", result_to_text(snapshot_all_test()));
    
    return 0;
}

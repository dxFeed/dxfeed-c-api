
#include "EventSubscriptionTest.h"
#include <stdio.h>
#include "EventDynamicSubscriptionTest.h"
#include "OrderSourceConfigurationTest.h"

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
    printf("\tEvent dynamic subscription...\t%s\n", result_to_text(event_dynamic_subscription_test()));
    printf("\tOrder source configuration...\t%s\n", result_to_text(order_source_configuration_test()));
	
	return 0;
}

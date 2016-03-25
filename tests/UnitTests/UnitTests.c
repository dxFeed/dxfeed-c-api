
#include "EventSubscriptionTest.h"
#include <stdio.h>
#include "EventDynamicSubscriptionTest.h"

const char* result_to_text (bool res) {
    if (res) {
        return "OK";
    } else {
        return "FAIL";
    }
}

int main (int argc, char* argv[]) {
        
    printf("DXFeed unit test procedure started...\n");
    //printf("\tEvent subscription...\t%s\n", result_to_text(event_subscription_test()));
    printf("\tEvent dynamic subscription...\t%s\n", result_to_text(event_dynamic_subscription_test()));
	
	return 0;
}


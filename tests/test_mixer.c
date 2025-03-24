#include <stdio.h>
#include <assert.h>
#include "mixer.h"

void test_set_volume(void);
void test_list_applications(void);
void test_invalid_inputs(void);
void test_cleanup(void);

void test_set_volume() {
    // Test setting volume for a specific application
    int result = set_volume("ApplicationName", 50);
    assert(result == 0);
    
    // Test setting max volume
    result = set_volume("ApplicationName", 100);
    assert(result == 0);
    
    // Test setting min volume
    result = set_volume("ApplicationName", 0);
    assert(result == 0);
}

void test_invalid_inputs() {
    // Test NULL application name
    int result = set_volume(NULL, 50);
    assert(result == -1);
    
    // Test invalid volume values
    result = set_volume("ApplicationName", -1);
    assert(result == -1);
    
    result = set_volume("ApplicationName", 101);
    assert(result == -1);
}

void test_list_applications() {
    char **applications = list_applications();
    assert(applications != NULL);
    
    // Count applications and verify they're valid
    int count = 0;
    while (applications[count] != NULL) {
        assert(applications[count][0] != '\0');
        count++;
    }
    
    // Clean up
    for (int i = 0; i < count; i++) {
        free(applications[i]);
    }
    free(applications);
}

void test_cleanup() {
    // Test cleanup function if available
    int result = cleanup_mixer();
    assert(result == 0);
}

int main() {
    test_set_volume();
    test_list_applications();
    test_invalid_inputs();
    test_cleanup();
    printf("All tests passed!\n");
    return 0;
}
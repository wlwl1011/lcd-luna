#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <luna-service2/lunaservice.h>

// Error handling macro
#define HANDLE_ERROR(retVal, lsError) \
    if (!retVal) { \
        LSErrorPrint(&lsError, stderr); \
        LSErrorFree(&lsError); \
        goto cleanup; \
    }

// Function prototypes
static bool getStatusHandler(LSHandle *sh, LSMessage *message, void *user_data);
static bool getArpingHandler(LSHandle *sh, LSMessage *message, void *user_data);
void callGetStatus(LSHandle *serviceHandle, GMainLoop *mainLoop);
void callArping(LSHandle *serviceHandle, GMainLoop *mainLoop, const char *ifName, const char *ipAddress);

int main(int argc, char *argv[]) {
    bool retVal;
    LSError lsError;
    LSHandle *serviceHandle = NULL;
    GMainLoop *mainLoop = NULL;

    LSErrorInit(&lsError);

    // Initialize GMainLoop
    mainLoop = g_main_loop_new(NULL, FALSE);
    if (!mainLoop) {
        fprintf(stderr, "Failed to create main loop\n");
        return EXIT_FAILURE;
    }

    // Register the service
    retVal = LSRegister("com.acp.lcd", &serviceHandle, &lsError);
    HANDLE_ERROR(retVal, lsError);

    // Attach the service to the main loop
    retVal = LSGmainAttach(serviceHandle, mainLoop, &lsError);
    HANDLE_ERROR(retVal, lsError);

    // Call the getStatus API
    callGetStatus(serviceHandle, mainLoop);

    // Run the main loop
    g_main_loop_run(mainLoop);

cleanup:
    if (serviceHandle) {
        LSUnregister(serviceHandle, &lsError);
    }
    if (mainLoop) {
        g_main_loop_unref(mainLoop);
    }
    return retVal ? EXIT_SUCCESS : EXIT_FAILURE;
}

void callGetStatus(LSHandle *serviceHandle, GMainLoop *mainLoop) {
    bool retVal;
    LSError lsError;
    LSMessageToken token = LSMESSAGE_TOKEN_INVALID;

    LSErrorInit(&lsError);

    // Call the getStatus API
    retVal = LSCallOneReply(serviceHandle,
                            "luna://com.webos.service.connectionmanager/getStatus",
                            "{}",
                            getStatusHandler,
                            mainLoop,
                            &token,
                            &lsError);
    HANDLE_ERROR(retVal, lsError);

cleanup:
    if (!retVal) {
        LSErrorPrint(&lsError, stderr);
        LSErrorFree(&lsError);
    }
}

void callArping(LSHandle *serviceHandle, GMainLoop *mainLoop, const char *ifName, const char *ipAddress) {
    bool retVal;
    LSError lsError;
    LSMessageToken token = LSMESSAGE_TOKEN_INVALID;
    char payload[256];

    LSErrorInit(&lsError);

    // Create the payload for the arping call
    snprintf(payload, sizeof(payload), "{\"ifName\":\"%s\",\"ipAddress\":\"%s\"}", ifName, ipAddress);

    // Call the arping API
    retVal = LSCallOneReply(serviceHandle,
                            "luna://com.webos.service.nettools/arping",
                            payload,
                            getArpingHandler,
                            mainLoop,
                            &token,
                            &lsError);
    HANDLE_ERROR(retVal, lsError);

cleanup:
    if (!retVal) {
        LSErrorPrint(&lsError, stderr);
        LSErrorFree(&lsError);
    }
}

static char* extract_json_value(const char* json, const char* key) {
    char* start = strstr(json, key);
    if (!start) return NULL;
    start = strchr(start, ':');
    if (!start) return NULL;
    start++;
    while (*start == ' ' || *start == '\"') start++;
    char* end = start;
    while (*end && *end != '\"' && *end != ',' && *end != '}') end++;
    size_t len = end - start;
    char* value = (char*)malloc(len + 1);
    strncpy(value, start, len);
    value[len] = '\0';
    return value;
}

static bool getStatusHandler(LSHandle *sh, LSMessage *message, void *user_data) {
    const char *payload = LSMessageGetPayload(message);
    printf("Response: %s\n", payload);

    // Extract values from JSON response
    char* returnValue = extract_json_value(payload, "\"returnValue\"");
    char* offlineMode = extract_json_value(payload, "\"offlineMode\"");
    char* isInternetConnectionAvailable = extract_json_value(payload, "\"isInternetConnectionAvailable\"");

    // Print extracted values
    printf("Return Value: %s\n", returnValue);
    printf("Offline Mode: %s\n", offlineMode);
    printf("Is Internet Connection Available: %s\n", isInternetConnectionAvailable);

    // Check Wired state
    char* wiredStateStart = strstr(payload, "\"wired\"");
    if (wiredStateStart) {
        char* wiredState = extract_json_value(wiredStateStart, "\"state\"");
        if (wiredState && strcmp(wiredState, "connected") == 0) {
            char* ipAddress = extract_json_value(wiredStateStart, "\"ipAddress\"");
            char* gateway = extract_json_value(wiredStateStart, "\"gateway\"");
            char* netmask = extract_json_value(wiredStateStart, "\"netmask\"");
            printf("Wired is connected\n");
            printf("Wired IP Address: %s\n", ipAddress);
            printf("Wired Gateway: %s\n", gateway);
            printf("Wired Netmask: %s\n", netmask);
            callArping(sh, (GMainLoop *)user_data, "eth0", ipAddress);
            free(wiredState);
            free(ipAddress);
            free(gateway);
            free(netmask);
            return true;
        } else {
            printf("Wired is not connected\n");
        }
        free(wiredState);
    }

    // Check WiFi state
    char* wifiStateStart = strstr(payload, "\"wifi\"");
    if (wifiStateStart) {
        char* wifiState = extract_json_value(wifiStateStart, "\"state\"");
        if (wifiState && strcmp(wifiState, "connected") == 0) {
            char* ipAddress = extract_json_value(wifiStateStart, "\"ipAddress\"");
            char* gateway = extract_json_value(wifiStateStart, "\"gateway\"");
            char* netmask = extract_json_value(wifiStateStart, "\"netmask\"");
            printf("WiFi is connected\n");
            printf("WiFi IP Address: %s\n", ipAddress);
            printf("WiFi Gateway: %s\n", gateway);
            printf("WiFi Netmask: %s\n", netmask);
            callArping(sh, (GMainLoop *)user_data, "wlan0", ipAddress);
            free(wifiState);
            free(ipAddress);
            free(gateway);
            free(netmask);
            return true;
        } else {
            printf("WiFi is not connected\n");
        }
        free(wifiState);
    }

    // Free allocated memory
    free(returnValue);
    free(offlineMode);
    free(isInternetConnectionAvailable);

    // Quit main loop if no further actions are needed
    GMainLoop *mainLoop = (GMainLoop *)user_data;
    g_main_loop_quit(mainLoop);

    return true;
}

static bool getArpingHandler(LSHandle *sh, LSMessage *message, void *user_data) {
    const char *payload = LSMessageGetPayload(message);
    printf("Arping Response: %s\n", payload);

    // Extract MAC address from JSON response
    char* macAddress = extract_json_value(payload, "\"macAddress\"");
    if (macAddress) {
        printf("MAC Address: %s\n", macAddress);
        free(macAddress);
    }

    GMainLoop *mainLoop = (GMainLoop *)user_data;
    g_main_loop_quit(mainLoop);
    return true;
}
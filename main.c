#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <luna-service2/lunaservice.h>

#define ENGINE_FILE "engine"

// 에러 처리 매크로
#define HANDLE_ERROR(retVal, lsError) \
    if (!retVal) { \
        LSErrorPrint(&lsError, stderr); \
        LSErrorFree(&lsError); \
        goto cleanup; \
    }

static bool getStatusHandler(LSHandle *sh, LSMessage *message, void *user_data);
static bool getNetworksHandler(LSHandle *sh, LSMessage *message, void *user_data);
void callGetStatus(LSHandle *serviceHandle);
void callGetNetworks(LSHandle *serviceHandle);
void cancelSubscriptions(void);

LSMessageToken getStatusToken = LSMESSAGE_TOKEN_INVALID;   // 구독을 취소하기 위한 토큰
LSMessageToken getNetworksToken = LSMESSAGE_TOKEN_INVALID; // 구독을 취소하기 위한 토큰

int main(int argc, char *argv[]) {
    bool retVal;
    LSError lsError;
    LSHandle *serviceHandle = NULL; // 서비스 핸들
    GMainLoop *mainLoop = NULL;     // 메인 이벤트 루프

    LSErrorInit(&lsError);

    // GMainLoop 초기화: 이벤트 루프 생성
    mainLoop = g_main_loop_new(NULL, FALSE);
    if (!mainLoop) {
        fprintf(stderr, "Failed to create main loop\n");
        return EXIT_FAILURE;
    }

    // 서비스 등록
    retVal = LSRegister("com.acp.lcd", &serviceHandle, &lsError);
    HANDLE_ERROR(retVal, lsError);

    // 서비스 핸들을 메인 루프에 연결
    retVal = LSGmainAttach(serviceHandle, mainLoop, &lsError);
    HANDLE_ERROR(retVal, lsError);

    // 네트워크 상태 구독 설정
    callGetStatus(serviceHandle);

    // Wi-Fi 네트워크 리스트 구독 설정
    callGetNetworks(serviceHandle);

    // 메인 루프 실행
    g_main_loop_run(mainLoop);

cleanup:
    // 종료 시 자원 해제
    cancelSubscriptions(); // 구독 취소
    if (serviceHandle) {
        LSUnregister(serviceHandle, &lsError);
    }
    if (mainLoop) {
        g_main_loop_unref(mainLoop);
    }
    return retVal ? EXIT_SUCCESS : EXIT_FAILURE;
}

// 네트워크 상태 정보를 요청하고 구독하는 함수
void callGetStatus(LSHandle *serviceHandle) {
    bool retVal;
    LSError lsError;

    LSErrorInit(&lsError);

    // "getStatus" API 구독 호출
    retVal = LSCall(serviceHandle,
                    "luna://com.webos.service.connectionmanager/getStatus",
                    "{\"subscribe\":true}",   // 구독 설정
                    getStatusHandler,         // 응답이 도착하면 호출될 콜백 함수
                    NULL,                     // 사용자 데이터 없음
                    &getStatusToken,          // 구독을 취소할 때 사용할 토큰
                    &lsError);
    HANDLE_ERROR(retVal, lsError);

cleanup:
    if (!retVal) {
        LSErrorPrint(&lsError, stderr);
        LSErrorFree(&lsError);
    }
}

// Wi-Fi 네트워크 리스트를 요청하고 구독하는 함수
void callGetNetworks(LSHandle *serviceHandle) {
    bool retVal;
    LSError lsError;

    LSErrorInit(&lsError);

    // "getNetworks" API 구독 호출
    retVal = LSCall(serviceHandle,
                    "luna://com.webos.service.wifi/getNetworks",
                    "{\"subscribe\":true}",   // 구독 설정
                    getNetworksHandler,       // 응답이 도착하면 호출될 콜백 함수
                    NULL,                     // 사용자 데이터 없음
                    &getNetworksToken,        // 구독을 취소할 때 사용할 토큰
                    &lsError);
    HANDLE_ERROR(retVal, lsError);

cleanup:
    if (!retVal) {
        LSErrorPrint(&lsError, stderr);
        LSErrorFree(&lsError);
    }
}

// 네트워크 상태 API 응답을 처리하는 핸들러
static bool getStatusHandler(LSHandle *sh, LSMessage *message, void *user_data) {
    const char *payload = LSMessageGetPayload(message);  // 응답으로 받은 JSON 데이터
    printf("getStatus Response: %s\n", payload);

    // JSON 응답에서 returnValue 값을 확인
    char* returnValue = extract_json_value(payload, "\"returnValue\"");
    if (strcmp(returnValue, "true") == 0) {
        // 성공 처리
        printf("Network status retrieved successfully.\n");

        // Wired(유선) 연결 상태 처리
        char* wiredStateStart = strstr(payload, "\"wired\"");
        if (wiredStateStart) {
            char* wiredState = extract_json_value(wiredStateStart, "\"state\"");
            if (wiredState && strcmp(wiredState, "connected") == 0) {
                // 유선 연결 정보 엔진 파일에 기록
                char* ipAddress = extract_json_value(wiredStateStart, "\"ipAddress\"");
                char* gateway = extract_json_value(wiredStateStart, "\"gateway\"");
                char* netmask = extract_json_value(wiredStateStart, "\"netmask\"");
                FILE *engine = fopen(ENGINE_FILE, "w");
                if (engine) {
                    fprintf(engine, "103 %s|%s|%s|false|\n", ipAddress, gateway, netmask);
                    fclose(engine);
                }
                free(ipAddress);
                free(gateway);
                free(netmask);
            }
            free(wiredState);
        }

        // Wi-Fi 연결 상태 처리
        char* wifiStateStart = strstr(payload, "\"wifi\"");
        if (wifiStateStart) {
            char* wifiState = extract_json_value(wifiStateStart, "\"state\"");
            if (wifiState && strcmp(wifiState, "connected") == 0) {
                // Wi-Fi 연결 정보 처리
                printf("Wi-Fi 연결됨\n");
            }
            free(wifiState);
        }
    } else {
        // 실패 처리
        printf("Failed to retrieve network status.\n");
    }

    free(returnValue);
    return true;
}


// Wi-Fi 네트워크 리스트를 처리하는 핸들러
static bool getNetworksHandler(LSHandle *sh, LSMessage *message, void *user_data) {
    const char *payload = LSMessageGetPayload(message);  // 응답으로 받은 JSON 데이터
    printf("getNetworks Response: %s\n", payload);

    // JSON 응답에서 returnValue 값을 확인
    char* returnValue = extract_json_value(payload, "\"returnValue\"");
    if (strcmp(returnValue, "true") == 0) {
        // 성공 처리
        printf("Wi-Fi networks retrieved successfully.\n");

        // Wi-Fi 신호 강도를 추출하여 처리
        char* signalBars = extract_json_value(payload, "\"signalBars\"");
        int signalStrength = atoi(signalBars);  // 신호 강도를 숫자로 변환
        char* signalQuality = "Weak";  // 기본적으로 약한 신호로 설정
        if (signalStrength >= 3) {
            signalQuality = "Strong";
        } else if (signalStrength == 2) {
            signalQuality = "Medium";
        }
        printf("Wi-Fi 신호 강도: %s\n", signalQuality);

        // Wi-Fi 신호 강도 정보를 엔진 파일에 기록
        FILE *engine = fopen(ENGINE_FILE, "w");
        if (engine) {
            fprintf(engine, "104 Wi-Fi|%s|\n", signalQuality);
            fclose(engine);
        }

        free(signalBars);
    } else {
        // 실패 처리
        printf("Failed to retrieve Wi-Fi networks.\n");
    }

    free(returnValue);
    return true;
}

// 구독 취소 함수
void cancelSubscriptions(void) {
    LSError lsError;
    LSErrorInit(&lsError);

    // getStatus 구독 취소
    if (getStatusToken != LSMESSAGE_TOKEN_INVALID) {
        if (!LSCallCancel(NULL, getStatusToken, &lsError)) {
            LSErrorPrint(&lsError, stderr);
        }
        getStatusToken = LSMESSAGE_TOKEN_INVALID;  // 토큰 초기화
    }

    // getNetworks 구독 취소
    if (getNetworksToken != LSMESSAGE_TOKEN_INVALID) {
        if (!LSCallCancel(NULL, getNetworksToken, &lsError)) {
            LSErrorPrint(&lsError, stderr);
        }
        getNetworksToken = LSMESSAGE_TOKEN_INVALID;  // 토큰 초기화
    }

    LSErrorFree(&lsError);
}

// JSON 응답에서 특정 키의 값을 추출하는 함수
static char* extract_json_value(const char* json, const char* key) {
    char* start = strstr(json, key);  // 키 검색
    if (!start) return NULL;
    start = strchr(start, ':');  // ':' 이후 값 시작
    if (!start) return NULL;
    start++;
    while (*start == ' ' || *start == '\"') start++;  // 공백과 따옴표 무시
    char* end = start;
    while (*end && *end != '\"' && *end != ',' && *end != '}') end++;  // 값 끝까지 이동
    size_t len = end - start;
    char* value = (char*)malloc(len + 1);
    strncpy(value, start, len);
    value[len] = '\0';
    return value;
}

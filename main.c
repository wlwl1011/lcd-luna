#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <luna-service2/lunaservice.h>

// 에러 처리 매크로
#define HANDLE_ERROR(retVal, lsError) \
    if (!retVal) { \
        LSErrorPrint(&lsError, stderr); \
        LSErrorFree(&lsError); \
        goto cleanup; \
    }

// 함수 선언부
static bool getStatusHandler(LSHandle *sh, LSMessage *message, void *user_data);
static bool getNetworksHandler(LSHandle *sh, LSMessage *message, void *user_data);
void callGetStatus(LSHandle *serviceHandle, GMainLoop *mainLoop);
void callGetNetworks(LSHandle *serviceHandle, GMainLoop *mainLoop);

int main(int argc, char *argv[]) {
    bool retVal;
    LSError lsError;
    LSHandle *serviceHandle = NULL; // 서비스 핸들
    GMainLoop *mainLoop = NULL; // 메인 이벤트 루프

    LSErrorInit(&lsError);

    // GMainLoop 초기화: 이벤트 루프 생성
    mainLoop = g_main_loop_new(NULL, FALSE);
    if (!mainLoop) {
        fprintf(stderr, "Failed to create main loop\n\n");
        return EXIT_FAILURE;
    }

    // 서비스 등록: 'com.acp.lcd'라는 이름으로 서비스 등록
    retVal = LSRegister("com.acp.lcd", &serviceHandle, &lsError);
    HANDLE_ERROR(retVal, lsError);

    // 서비스 핸들을 메인 루프에 연결
    retVal = LSGmainAttach(serviceHandle, mainLoop, &lsError);
    HANDLE_ERROR(retVal, lsError);

    // 네트워크 상태 정보를 요청하는 API 호출
    callGetStatus(serviceHandle, mainLoop);

    // 메인 루프 실행: 프로그램이 이벤트를 대기하면서 실행됨
    g_main_loop_run(mainLoop);

cleanup:
    // 종료 시 자원 해제
    if (serviceHandle) {
        LSUnregister(serviceHandle, &lsError);
    }
    if (mainLoop) {
        g_main_loop_unref(mainLoop);
    }
    return retVal ? EXIT_SUCCESS : EXIT_FAILURE;
}

// 네트워크 상태 정보를 요청하는 API 호출 함수
void callGetStatus(LSHandle *serviceHandle, GMainLoop *mainLoop) {
    bool retVal;
    LSError lsError;
    LSMessageToken token = LSMESSAGE_TOKEN_INVALID;

    LSErrorInit(&lsError);

    // "getStatus" API 호출: 네트워크 상태 정보를 가져옴
    retVal = LSCallOneReply(serviceHandle,
                            "luna://com.webos.service.connectionmanager/getStatus",
                            "{}",
                            getStatusHandler,  // 응답이 도착하면 호출될 콜백 함수
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

// Wi-Fi 신호 강도를 가져오는 API 호출 함수
void callGetNetworks(LSHandle *serviceHandle, GMainLoop *mainLoop) {
    bool retVal;
    LSError lsError;
    LSMessageToken token = LSMESSAGE_TOKEN_INVALID;

    LSErrorInit(&lsError);

    // "getNetworks" API 호출: 주변의 Wi-Fi 네트워크 신호 강도를 가져옴
    retVal = LSCallOneReply(serviceHandle,
                            "luna://com.webos.service.wifi/getNetworks",
                            "{}",
                            getNetworksHandler,  // 응답이 도착하면 호출될 콜백 함수
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

// 네트워크 상태 API 응답을 처리하는 핸들러
static bool getStatusHandler(LSHandle *sh, LSMessage *message, void *user_data) {
    const char *payload = LSMessageGetPayload(message);  // 응답으로 받은 JSON 데이터
    printf("Response: %s\n", payload);

    // JSON 응답에서 필요한 값 추출
    char* returnValue = extract_json_value(payload, "\"returnValue\"");
    char* offlineMode = extract_json_value(payload, "\"offlineMode\"");
    char* isInternetConnectionAvailable = extract_json_value(payload, "\"isInternetConnectionAvailable\"");

    // 추출한 값 출력
    printf("Return Value: %s\n", returnValue);
    printf("Offline Mode: %s\n", offlineMode);
    printf("Is Internet Connection Available: %s\n", isInternetConnectionAvailable);

    // Wired(유선) 연결 상태 처리
    char* wiredStateStart = strstr(payload, "\"wired\"");
    if (wiredStateStart) {
        char* wiredState = extract_json_value(wiredStateStart, "\"state\"");
        if (wiredState && strcmp(wiredState, "connected") == 0) {
            // 유선 연결 상태에서 IP, 게이트웨이, 서브넷 마스크 정보 추출
            char* ipAddress = extract_json_value(wiredStateStart, "\"ipAddress\"");
            char* gateway = extract_json_value(wiredStateStart, "\"gateway\"");
            char* netmask = extract_json_value(wiredStateStart, "\"netmask\"");
            printf("유선 연결됨\n");
            printf("유선 IP: %s, 게이트웨이: %s, 넷마스크: %s\n", ipAddress, gateway, netmask);
            
            // 유선 연결 정보를 engine 파일에 기록
            FILE *engine = fopen(ENGINE_FILE, "w");
            if (engine) {
                fprintf(engine, "103 %s|%s|%s|false|\n", ipAddress, gateway, netmask);
                //fprintf(engine, "104 Eth|Strong|\n");  // 유선은 필요 없음
                fclose(engine);
            }
            free(wiredState);
            free(ipAddress);
            free(gateway);
            free(netmask);
            return true;
        }
        free(wiredState);
    }

    // Wi-Fi 연결 상태 처리 -> 이더넷에 연결되어 있다면 확인 할 필요 없음
    char* wifiStateStart = strstr(payload, "\"wifi\"");
    if (wifiStateStart) {
        char* wifiState = extract_json_value(wifiStateStart, "\"state\"");
        if (wifiState && strcmp(wifiState, "connected") == 0) {
            // Wi-Fi 연결 상태일 경우 신호 강도 가져오는 API 호출
            printf("Wi-Fi 연결됨\n");
            callGetNetworks(sh, (GMainLoop *)user_data);
        } else {
            printf("Wi-Fi 연결되지 않음\n");
        }
        free(wifiState);
    }

    free(returnValue);
    free(offlineMode);
    free(isInternetConnectionAvailable);

    return true;
}

// Wi-Fi 신호 강도를 처리하는 핸들러
static bool getNetworksHandler(LSHandle *sh, LSMessage *message, void *user_data) {
    const char *payload = LSMessageGetPayload(message);  // 응답으로 받은 JSON 데이터
    printf("Wi-Fi Networks Response: %s\n", payload);

    // JSON 응답에서 Wi-Fi 신호 강도를 추출
    char* signalBars = extract_json_value(payload, "\"signalBars\"");
    int signalStrength = atoi(signalBars);  // 신호 강도를 숫자로 변환
    char* signalQuality = "Weak";  // 기본적으로 약한 신호로 설정
    if (signalStrength >= 3) {
        signalQuality = "Strong";  // 신호 강도 3 이상은 강한 신호로 판단
    } else if (signalStrength == 2) {
        signalQuality = "Medium";  // 신호 강도 2는 중간 신호로 판단
    }

    printf("Wi-Fi 신호 강도: %s\n", signalQuality);

    // Wi-Fi 신호 강도 정보를 engine 파일에 기록
    FILE *engine = fopen("engine", "w");
    if (engine) {
        fprintf(engine, "104 Wi-Fi|%s|\n", signalQuality);
        fclose(engine);
    }

    free(signalBars);
    return true;
}

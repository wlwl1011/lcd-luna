#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <luna-service2/lunaservice.h>
#include "Luna.h"
#include "Queue_manager.h"

// 전역 서비스 핸들
extern LSHandle *serviceHandle;

// 구독 토큰 및 GMainLoop 선언
LSMessageToken getStatusToken = LSMESSAGE_TOKEN_INVALID;
LSMessageToken getNetworksToken = LSMESSAGE_TOKEN_INVALID;
GMainLoop *mainLoop = NULL;

// LUNA 서비스 초기화 함수
void luna_service_init() {
    LSError lsError;
    LSErrorInit(&lsError);

    // 서비스 핸들 등록
    if (!LSRegister("com.acp.lcd.service", &serviceHandle, &lsError)) {
        LSErrorPrint(&lsError, stderr);
        LSErrorFree(&lsError);
        return;
    }

    // GMainLoop 생성 및 서비스 핸들 연결
    mainLoop = g_main_loop_new(NULL, FALSE);
    if (!LSGmainAttach(serviceHandle, mainLoop, &lsError)) {
        LSErrorPrint(&lsError, stderr);
        LSErrorFree(&lsError);
        return;
    }

    // 네트워크 상태 구독 요청 (백그라운드 스레드에서 실행)
    pthread_t subscription_thread;
    if (pthread_create(&subscription_thread, NULL, network_subscription_thread, NULL) != 0) {
        perror("Failed to create subscription thread");
    }
}

// 네트워크 상태 및 Wi-Fi 구독을 처리하는 스레드
void *network_subscription_thread(void *arg) {
    // 구독 요청
    callGetStatus(serviceHandle);  // 네트워크 상태 구독
    callGetNetworks(serviceHandle);  // Wi-Fi 신호 강도 구독

    // GMainLoop 실행 (백그라운드에서 구독 정보 처리)
    g_main_loop_run(mainLoop);

    return NULL;
}

// 네트워크 정보 구독 및 데이터 로깅 시작 등을 처리하는 단발성 요청
void callStartLoging(LSHandle *serviceHandle) {
    LSError lsError;
    LSErrorInit(&lsError);

    // 단발성 데이터 로깅 시작 요청
    if (!LSCallOneReply(serviceHandle,
                        "luna://com.webos.service.wifi/getstatus",
                        "{}",
                        setDataLogingStartHandler, 
                        NULL, 
                        NULL, 
                        &lsError)) {
        LSErrorPrint(&lsError, stderr);
        LSErrorFree(&lsError);
    }
}



#ifndef LUNA_H
#define LUNA_H

#include <luna-service2/lunaservice.h>

// 전역 LSHandle 서비스 핸들 선언
extern LSHandle *serviceHandle;

// LUNA 서비스 초기화 함수 (메인 스레드에서 호출)
void luna_service_init();

// 네트워크 구독 요청 스레드 함수
void *network_subscription_thread(void *arg);

// 데이터 로깅 시작 함수
void callStartLoging(LSHandle *serviceHandle);

#endif // LUNA_H

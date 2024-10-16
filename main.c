#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl.h"
#include "Api.h"
#include "Error.h"
#include "Queue_manager.h"
#include "Luna.h"

// 서비스 핸들을 메인 스레드에서 전역적으로 사용
LSHandle *serviceHandle = NULL;

// 메인 스레드에서 서비스 초기화 및 핸들 생성
int main() {
    // LVGL 및 기타 초기화 코드
    lv_init();
    
    // 큐 초기화
    initialize_queue(&data_queue);
    
    // LVGL Version
    lvgl_version();
    lv_log("LVGL initialization is done\n");

    // API 및 LUNA 초기화
    api_init();
    luna_service_init();  // 메인 스레드에서 서비스 핸들을 초기화
    
    // Key 및 UI 관련 초기화
    key_init();
    Construct_App();

    // 메인 루프 (GUI 처리)
    while (1) {
        lv_timer_handler();
        usleep(10 * 1000);
    }

    return 0;
}

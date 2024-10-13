cmake_minimum_required(VERSION 3.10)

project(GUI_App)

# Print the CMAKE_SOURCE_DIR
message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

set(CMAKE_C_STANDARD 99) # C99
set(CMAKE_CXX_STANDARD 17) # C17
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out)

set(CMAKE_BUILD_TYPE Debug)

# Add FB01_BOARD option
option(FB01_BOARD "Build for FB01 board" OFF)

# Find Git executable
find_package(Git REQUIRED)

# Get Git version
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Create a config header file
configure_file(
    ${CMAKE_SOURCE_DIR}/inc/version.h.in
    ${CMAKE_BINARY_DIR}/inc/version.h
)

# Include directories for LVGL, GLib, Luna Service
include_directories(
    ./inc/
    ./inc/lvgl/
    ./inc/lvgl/src/display/lv_display/
    ${GLIB_INCLUDE_DIRS}                    # GLib Include directories
    ${LUNASERVICE2_INCLUDE_DIRS}             # Luna Service2 Include directories
)

# Conditionally find SDL2 package
if (NOT FB01_BOARD)
    find_package(SDL2 REQUIRED SDL2)
    link_directories(${PROJECT_SOURCE_DIR}/lib)
endif()

# Libraries (x86 Compatible) are located in the lib folder of the source code.
# For other Arch (For instance ARM), use appropriate libs and remove this line
link_directories(${PROJECT_SOURCE_DIR}/lib)

# Find GLib and Luna Service2 via PkgConfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LUNASERVICE2 REQUIRED luna-service2)

# Source files for the entire project
FILE(GLOB_RECURSE SOURCES 
    ./src/*.c 
    ./src/app/*.c
    ./res/Images/*.c
    ./res/Fonts/*.c
    ./inc/lvgl/src/libs/qrcode/*.c  # 추가된 경로
    ./src/network_info/*.c          # NetworkInfoService 소스 파일 경로
)

# Add executable for the entire project
add_executable(gui_app ${SOURCES})

# Link libraries depending on the target platform
if (FB01_BOARD)
    set(LVGL_LIB_PATH /usr/local/nano-sdk-x86_64/sysroots/aarch64-webos-linux/usr/lib)
    find_library(LVGL_LIB NAMES lvgl PATHS ${LVGL_LIB_PATH})
    find_library(LVGL_THORVG_LIB NAMES lvgl_thorvg PATHS ${LVGL_LIB_PATH})
    target_link_libraries(gui_app ${LVGL_LIB} ${LVGL_THORVG_LIB} m pthread stdc++)
else()
    target_link_libraries(gui_app 
        liblvgl.so 
        liblvgl_thorvg.so 
        ${SDL2_LIBRARIES} 
        ${GLIB_LIBRARIES}               # GLib Libraries
        ${LUNASERVICE2_LIBRARIES}        # Luna Service2 Libraries
        m 
        pthread 
        stdc++
    )
endif()

# Set up additional paths and compile options
set(LANGUAGE_PATH "${CMAKE_SOURCE_DIR}/res/Texts/")
set(CONF_BIN "${CMAKE_SOURCE_DIR}/res/Config/")
set(KEY_PATH "${CMAKE_SOURCE_DIR}/gpio")
set(ENGINE_PATH "${CMAKE_SOURCE_DIR}/engine")
set(API_PATH "${CMAKE_SOURCE_DIR}/api")

# Add compile options to ignore specific warnings and enable debugging
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=return-type -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=return-type -g")

# Compile-time options for the application
target_compile_options(gui_app PRIVATE
    "-DLANGUAGE_PATH=\"${LANGUAGE_PATH}\""
    "-DCONF_BIN=\"${CONF_BIN}\""
    "-DPIPE_KEY_FILE=\"${KEY_PATH}\""
    "-DENGINE_FILE=\"${ENGINE_PATH}\""
    "-DAPI_FILE=\"${API_PATH}\""
    "-DEMUL_MODE" # Enable only for emulator
)

# Custom clean-all target
add_custom_target(clean-all
    COMMENT "Cleaning all build artifacts and temporary files"
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/out
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/Makefile
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/cmake_install.cmake
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/engine
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/api
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/gpio
)

# Update lv_conf.h based on the board type
if (FB01_BOARD)
    set(LV_USE_SDL 0)
    set(LV_USE_LINUX_FBDEV 1)
else()
    set(LV_USE_SDL 1)
    set(LV_USE_LINUX_FBDEV 0)
endif()

configure_file(
    ${PROJECT_SOURCE_DIR}/inc/lv_conf.h.in
    ${PROJECT_SOURCE_DIR}/inc/lv_conf.h @ONLY
)

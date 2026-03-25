cmake_minimum_required(VERSION 3.15)
project(CMS_Proxy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- POLICY ---
cmake_policy(SET CMP0167 OLD)

# --- DIPENDENZE ---
set(Boost_ROOT "C:/Users/HP840G8/vcpkg/installed/x64-windows")
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)

# --- SORGENTI ---
set(SOURCES
    src/AppConfig.cpp
    src/main.cpp
    src/ProxyEngine.cpp
    src/UdpMulticastReceiver.cpp
    src/UdpAckSender.cpp
    src/TcpSender.cpp
    src/BinaryConverter.cpp
)


add_executable(${PROJECT_NAME} ${SOURCES})

# --- TARGET CONFIGURATION (La via moderna) ---

# Invece di include_directories globale, usa target_include_directories
target_include_directories(${PROJECT_NAME} PRIVATE include)

# Definizioni specifiche
target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0A00)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4244 /wd4189)
    if(WIN32)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# --- LINK LIBRERIE ---
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    Boost::boost
    Boost::system
    Threads::Threads
    nlohmann_json::nlohmann_json
    $<$<PLATFORM_ID:Windows>:ws2_32 mswsock>
)
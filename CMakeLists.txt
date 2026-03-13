cmake_minimum_required(VERSION 3.15)

# Nome del progetto
project(CMS_Proxy VERSION 1.0.0 LANGUAGES CXX)

# Standard C++ 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- RICERCA DIPENDENZE ---

# Cerca Boost (Asio, System). vcpkg gestisce il puntamento automatico.
find_package(Boost REQUIRED COMPONENTS system)

# Necessario per la gestione dei thread
find_package(Threads REQUIRED)

# --- CONFIGURAZIONE SORGENTI ---

# Cartella degli header
include_directories(include)

# Lista completa dei file sorgente
set(SOURCES
    src/main.cpp
    src/ProxyEngine.cpp
    src/UdpMulticastReceiver.cpp
    src/TcpSender.cpp
    src/BinaryConverter.cpp
)

# Forza la versione di Windows a Windows 10/11 (evita i warning _WIN32_WINNT)
add_definitions(-D_WIN32_WINNT=0x0A00)

# --- CREAZIONE ESEGUIBILE ---

add_executable(${PROJECT_NAME} ${SOURCES})

# --- LINK LIBRERIE ---

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    Boost::boost
    Boost::system
    Threads::Threads
)

# Configurazione specifica per Windows (Winsock)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mswsock)
    # Evita i warning sulle funzioni "unsafe" di Windows (come memcpy)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- OTTIMIZZAZIONI COMPILATORE (RISOLUZIONE ERRORE D8016) ---

if(MSVC)
    # Impostazioni generali per Visual Studio
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
    target_compile_options(${PROJECT_NAME} PRIVATE /wd4244 /wd4189)
    # Applichiamo /O2 (velocità) SOLO se NON siamo in modalità Debug
    # Questo risolve l'errore D8016 evitando il conflitto con /RTC1
    target_compile_options(${PROJECT_NAME} PRIVATE 
        $<$<NOT:$<CONFIG:Debug>>:/O2>
    )
else()
    # Impostazioni per GCC/Clang (fallback)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
    target_compile_options(${PROJECT_NAME} PRIVATE 
        $<$<NOT:$<CONFIG:Debug>>:-O3>
    )
endif()

message(STATUS "Configurazione CMS_Proxy completata. Target pronto per la build.")
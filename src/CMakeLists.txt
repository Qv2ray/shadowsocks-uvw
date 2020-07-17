
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
    set(WINSOCK2 ws2_32)
    set(CRYPT32 crypt32)
elseif (NOT APPLE)
    find_library(LIBRT rt)
endif ()


set(SOURCE_FILES_LOCAL
        uvw/async.cpp
        uvw/check.cpp
        uvw/dns.cpp
        uvw/emitter.cpp
        uvw/fs.cpp
        uvw/fs_event.cpp
        uvw/fs_poll.cpp
        uvw/idle.cpp
        uvw/lib.cpp
        uvw/loop.cpp
        uvw/pipe.cpp
        uvw/poll.cpp
        uvw/prepare.cpp
        uvw/process.cpp
        uvw/signal.cpp
        uvw/stream.cpp
        uvw/tcp.cpp
        uvw/thread.cpp
        uvw/timer.cpp
        uvw/tty.cpp
        uvw/util.cpp
        uvw/work.cpp
        uvw/udp.cpp
        ssrutils.h
        Buffer.hpp
        ConnectionContext.hpp
        sockaddr_universal.h
        uvw_single.hpp
        ConnectionContext.cpp
        cache.h
        ssrutils.c
        shadowsocks.h
        CipherEnv.hpp
        sockaddr_universal.c
        CipherEnv.cpp
        uthash.h
        Buffer.cpp
        cache.c
        NetUtils.hpp
        NetUtils.cpp
        TCPRelay.hpp
        base64.c
        base64.h
        aead.c
        stream.c
        crypto.c
        ppbloom.c
        ppbloom.h
        stream.h
        crypto.h
        aead.h
        UDPConnectionContext.cpp UDPConnectionContext.hpp UDPRelay.cpp UDPRelay.hpp)
add_library(shadowsocks-uvw-common OBJECT ${SOURCE_FILES_LOCAL})
message("soium include_directories: " ${libsodium_include_dirs})
target_include_directories(shadowsocks-uvw-common PUBLIC ${libsodium_include_dirs})
target_compile_definitions(shadowsocks-uvw-common PUBLIC UVW_AS_LIB)
if (SSR_UVW_WITH_QT)
    message("find Qt5")
    find_package(Qt5 COMPONENTS Core REQUIRED)
    qt5_wrap_cpp(ssr_thread_moc SSThread.hpp)
    set(SOURCE_FILES_LOCAL_QT
            ${ssr_thread_moc}
            SSThread.cpp
            qt_ui_log.cpp
            ss_log_utils.cpp
            local_uv.cpp
            #local_uv.cpp uses macro SSR_UVW_WITH_QT,so it's not part of shadowsocks-uvw-common
            )
    add_library(${PROJECT_NAME}-qt STATIC ${SOURCE_FILES_LOCAL_QT} $<TARGET_OBJECTS:shadowsocks-uvw-common>)
    target_compile_definitions(${PROJECT_NAME}-qt PUBLIC SSR_UVW_WITH_QT UVW_AS_LIB)
    set_target_properties(${PROJECT_NAME}-qt PROPERTIES POSITION_INDEPENDENT_CODE 1)
    add_library(shadowsocks::uvw::qt ALIAS ${PROJECT_NAME}-qt)
endif ()
include_directories(${LibUV_INCLUDE_DIR})
include_directories(${libsodium_include_dirs})
include_directories(${MBEDTLS_INCLUDE_DIR})
include_directories(${LIBBLOOM_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_library(${PROJECT_NAME} STATIC local_uv.cpp ss_log_utils.cpp $<TARGET_OBJECTS:shadowsocks-uvw-common>)
#local_uv.cpp uses macro SSR_UVW_WITH_QT,so it's not part of shadowsocks-uvw-common
add_library(shadowsocks::uvw ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE 1)
target_compile_definitions(${PROJECT_NAME} PUBLIC UVW_AS_LIB)
set(
        COMMON_LINK_LIBS
        ${LIBRT}
        ${LibUV_LIBRARIES}
        ${WINSOCK2}
        ${CRYPT32}
        ${LIBBLOOM_LIBRARY}
)

set(ssr_crypto
        ${LIBCRYPTO}
        ${sodium_LIBRARIES}
        )

if(WIN32)
    target_link_libraries(shadowsocks-uvw-common 
        ${COMMON_LINK_LIBS} ${ssr_crypto})
endif()

if (SSR_UVW_WITH_QT)
    target_link_libraries(${PROJECT_NAME}-qt Qt5::Core
            ${COMMON_LINK_LIBS} ${ssr_crypto}
            )
endif ()
target_include_directories(
        ${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${PROJECT_NAME}
        ${COMMON_LINK_LIBS} ${ssr_crypto}
        )
if(NOT WIN32)
    set(SS_LOCAL_SOURCE ss_local.cpp ss_log_utils.cpp)
else()
    set(SS_LOCAL_SOURCE ss_local.cpp win/getopt.c ss_log_utils.cpp)
endif()
add_executable(ss-local ${SS_LOCAL_SOURCE})
target_compile_definitions(ss-local PUBLIC UVW_AS_LIB)
target_link_libraries(ss-local shadowsocks::uvw)

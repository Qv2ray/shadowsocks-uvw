if(WIN32)
    set(WINSOCK2 ws2_32)
elseif(NOT APPLE)
    find_library(LIBRT rt)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
function(ADD_SS_UVW_TEST TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} 
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            ${libsodium_include_dirs}
            ${MBEDTLS_INCLUDE_DIR}
    )
    target_link_libraries(
            ${TEST_NAME}
            PRIVATE
            shadowsocks::uvw
            ${LIBRT}
            ${WINSOCK2}
    )

    add_test(NAME SS_UVW_${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
endfunction()

ADD_SS_UVW_TEST(TESTBUFFER src/TestBuffer.cpp)
ADD_SS_UVW_TEST(TESTDUALSTACK src/TestDualStack.cpp)
ADD_SS_UVW_TEST(TESTNETUTILS src/TestNetUtils.cpp)


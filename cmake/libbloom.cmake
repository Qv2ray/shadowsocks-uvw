set(BLOOM_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/libbloom/bloom.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libbloom/bloom.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libbloom/murmur2/MurmurHash2.c)
add_library(LIBBLOOM ${BLOOM_SOURCE})
target_include_directories(LIBBLOOM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libbloom/murmur2)
set(LIBBLOOM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libbloom)
set(LIBBLOOM_LIBRARY LIBBLOOM)

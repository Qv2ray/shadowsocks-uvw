# Build args
if(USE_SYSTEM_LIBUV)
    if(NOT WIN32)
        find_package(LibUV REQUIRED)
    else()
        find_package(unofficial-libuv CONFIG REQUIRED)
        set(${LibUV_LIBRARIES} unofficial::libuv::libuv)
    endif()
endif()
if(USE_SYSTEM_SODIUM)
    if(NOT WIN32)
        find_package(Sodium REQUIRED)
    else()
        find_package(unofficial-sodium CONFIG REQUIRED)
        set(${sodium_LIBRARIES} unofficial-sodium::sodium)
    endif()
endif()
#todo: provide a way use system medtls
#find_package(MbedTLS REQUIRED)
set(USE_CRYPTO_MBEDTLS 1)



# Platform checks
include ( CheckFunctionExists )
include ( CheckIncludeFiles )
include ( CheckSymbolExists )
include ( CheckCSourceCompiles )
include ( CheckTypeSize )
include ( CheckSTDC )

check_include_files ( inttypes.h HAVE_INTTYPES_H )
check_include_files(stdint.h HAVE_STDINT_H)


ADD_DEFINITIONS(-DHAVE_CONFIG_H)

cmake_minimum_required(VERSION 3.10)
project(AEGIS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)


file(GLOB_RECURSE AEGIS_SRC "src/*.cpp")
file(GLOB_RECURSE AEGIS_HDR "include/*.hpp")

add_executable(AEGIS 
    ${AEGIS_SRC}
    ${AEGIS_HDR}
                )
 
target_include_directories(AEGIS PRIVATE 
    ${SODIUM_INCLUDE_DIRS} 
    ${Boost_INCLUDE_DIRS}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(AEGIS PRIVATE 
    ${SODIUM_LIBRARIES}

)

if(WIN32)
    target_link_libraries(AEGIS PRIVATE
    ws2_32
    mswsock
    )
endif()

add_compile_options(
    -DSODIUM_STATIC -lsodium
    -lboost_system -lboost_thread
    -static -static-libgcc -static-libstdc++ 
)
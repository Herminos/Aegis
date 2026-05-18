cmake_minimum_required(VERSION 3.10)
project(AEGIS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# 第一步：寻找第三方密码学和网络库
# ==========================================
# 通过 pkg-config 寻找 Libsodium
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)

# 寻找 Boost 库 (Asio 主要是 Header-only，所以找到头文件即可)
#find_package(Boost REQUIRED)

file(GLOB_RECURSE AEGIS_SRC "src/*.cpp")
file(GLOB_RECURSE AEGIS_HDR "include/*.hpp")

# ==========================================
# 第二步：生成可执行文件 (你的 P2P 节点)
# ==========================================
add_executable(AEGIS 
    ${AEGIS_SRC}
    ${AEGIS_HDR}
                )
 
# ==========================================
# 第三步：链接头文件与系统库 (极其关键)
# ==========================================
target_include_directories(AEGIS PRIVATE 
    ${SODIUM_INCLUDE_DIRS} 
    ${Boost_INCLUDE_DIRS}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 针对 Windows 环境的特殊处理：链接底层网卡驱动库
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
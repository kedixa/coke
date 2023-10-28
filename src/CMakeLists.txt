cmake_minimum_required(VERSION 3.6)

add_library(${COKE_LIBRARY_NAME} STATIC
    coke_impl.cpp
    fileio.cpp
    go.cpp
    http_impl.cpp
    latch.cpp
    qps_pool.cpp
    redis_impl.cpp
    sleep.cpp
    mysql_impl.cpp
    future.cpp
)

target_include_directories(${COKE_LIBRARY_NAME} PUBLIC
    ${COKE_INCLUDE_DIR}
)

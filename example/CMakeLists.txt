cmake_minimum_required(VERSION 3.6)

set(ALL_EXAMPLES
    ex001-helloworld
    ex002-http_get
    ex003-http_hello_server
    ex004-word_count
    ex005-merge_sort
)

foreach(example ${ALL_EXAMPLES})
    string(REGEX MATCH "[^-]*$" exec_name ${example})
    add_executable(${exec_name} ${example}.cpp)
    target_link_libraries(${exec_name}
        ${COKE_LIBRARY_NAME}
        ${WORKFLOW_LIB_DIR}/libworkflow.a
        -lpthread -lssl -lcrypto
    )
    include_directories(${exec_name} 
        ${COKE_INCLUDE_DIR}
        ${WORKFLOW_INCLUDE_DIR}
    )
endforeach()

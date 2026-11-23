cmake_minimum_required(VERSION 3.22)

project(lab2_chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Threads REQUIRED)

include(CTest)

file(GLOB SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB CONNECTION_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/conn_*.cpp"
)

list(REMOVE_ITEM SOURCES ${CONNECTION_SOURCES})

foreach(CONNECTION_SOURCE IN LISTS CONNECTION_SOURCES)
    get_filename_component(CONNECTION_NAME
            "${CONNECTION_SOURCE}" NAME_WE
    )

    string(REGEX REPLACE "^conn_" ""
            CONNECTION_CODE "${CONNECTION_NAME}"
    )

    set(TARGET_NAME "host_${CONNECTION_CODE}")

    add_executable(${TARGET_NAME}
            ${SOURCES}
            ${CONNECTION_SOURCE}
    )

    target_include_directories(${TARGET_NAME}
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_link_libraries(${TARGET_NAME}
            PRIVATE Qt6::Widgets
            Threads::Threads
    )

    target_compile_options(${TARGET_NAME}
            PRIVATE -Wall -Wextra -Werror
    )

    if(BUILD_TESTING)
        add_test(
                NAME "smoke_${CONNECTION_CODE}"
                COMMAND ${TARGET_NAME} --transport
        )

        set_tests_properties("smoke_${CONNECTION_CODE}"
                PROPERTIES
                PASS_REGULAR_EXPRESSION "Transport: ${CONNECTION_CODE}"
        )
    endif()
endforeach()

if(BUILD_TESTING)
    add_executable(chat_message_tests
            tests/test_chat_message.cpp
            src/chat_message.cpp
    )

    target_include_directories(chat_message_tests
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_compile_options(chat_message_tests
            PRIVATE -Wall -Wextra -Werror
    )

    add_test(
            NAME chat_message_tests
            COMMAND chat_message_tests
    )
endif()

if(BUILD_TESTING)
    add_executable(process_manager_tests
            tests/test_process_manager.cpp
            src/process_manager.cpp
    )

    target_include_directories(process_manager_tests
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_link_libraries(process_manager_tests
            PRIVATE Threads::Threads
    )

    target_compile_options(process_manager_tests
            PRIVATE -Wall -Wextra -Werror
    )

    add_test(
            NAME process_manager_tests
            COMMAND process_manager_tests
    )

    set_tests_properties(process_manager_tests
            PROPERTIES TIMEOUT 15
    )
endif()

if(BUILD_TESTING)
    add_executable(mq_tests
            tests/test_mq.cpp
            src/conn_mq.cpp
            src/chat_message.cpp
            src/process_manager.cpp
    )

    target_include_directories(mq_tests
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_link_libraries(mq_tests
            PRIVATE
            Threads::Threads
            rt
    )

    target_compile_options(mq_tests
            PRIVATE -Wall -Wextra -Werror
    )

    add_test(
            NAME mq_tests
            COMMAND mq_tests
    )

    set_tests_properties(mq_tests
            PROPERTIES TIMEOUT 20
    )
endif()

if(BUILD_TESTING)
    add_executable(fifo_tests
            tests/test_fifo.cpp
            src/conn_fifo.cpp
            src/chat_message.cpp
            src/process_manager.cpp
    )

    target_include_directories(fifo_tests
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_link_libraries(fifo_tests
            PRIVATE Threads::Threads
    )

    target_compile_options(fifo_tests
            PRIVATE -Wall -Wextra -Werror
    )

    add_test(
            NAME fifo_tests
            COMMAND fifo_tests
    )

    set_tests_properties(fifo_tests
            PROPERTIES TIMEOUT 15
    )
endif()
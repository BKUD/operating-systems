cmake_minimum_required(VERSION 3.22)

project(lab2_chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

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
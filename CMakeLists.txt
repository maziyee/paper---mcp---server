cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(mpc_mt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

file(GLOB_RECURSE COMMON_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/mpc_src/common/load_config/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/mpc_src/common/my_rpc/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/mpc_src/common/std_handle/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/mpc_src/common/http_rpc/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/mpc_src/common/mcp_server/*.cpp"
    )

set (SERVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/mpc_src/server/server_main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/paper_tools.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/vision_tools.cpp"
)

set (TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/test_tools/test_http.cpp"
)

set (MCP_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/test_tools/test_mcp.cpp"
)

# set(CLIENT_SOURCES
# "${CMAKE_CURRENT_SOURCE_DIR}/rpc_src/client/client_main.cpp"
# "${CMAKE_CURRENT_SOURCE_DIR}/rpc_src/client/rpc_client.cpp"
# )

add_executable(test
    ${COMMON_SOURCES}
    ${TEST_SOURCES}
)

add_executable(test_mcp
    ${COMMON_SOURCES}
    ${MCP_TEST_SOURCES}
)

add_executable(server
    ${COMMON_SOURCES}
    ${SERVER_SOURCES}
)

# add_executable(client
#     ${COMMON_SOURCES}
#     ${CLIENT_SOURCES}
# )


foreach(target test test_mcp server)
    target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
    )
    target_link_libraries(${target} PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        httplib::httplib
    )

    target_compile_options(${target} PRIVATE
        -Wno-deprecated-declarations
        -Wno-unused-parameter
    )

    target_compile_definitions(${target} PRIVATE
        _WIN32_WINNT=0x0A00
        THREADED
        ZK_DEPRECATED=1
    )
endforeach()

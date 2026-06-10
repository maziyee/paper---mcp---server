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
    )

# set (SERVER_SOURCES 
#     "${CMAKE_CURRENT_SOURCE_DIR}/rpc_src/server/server_main.cpp"
# )

set (TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/test_tools/test_http.cpp"
)

# set(CLIENT_SOURCES
# "${CMAKE_CURRENT_SOURCE_DIR}/rpc_src/client/client_main.cpp"
# "${CMAKE_CURRENT_SOURCE_DIR}/rpc_src/client/rpc_client.cpp"
# )

add_executable(test
    ${COMMON_SOURCES}
    ${TEST_SOURCES}
)

# add_executable(client
#     ${COMMON_SOURCES}
#     ${CLIENT_SOURCES}
# )


foreach(target test)
    target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
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
        THREADED
        ZK_DEPRECATED=1
    )
endforeach()

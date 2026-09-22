cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
	src/main.cpp
	src/app.cpp
	src/app.h
	src/api_handler.cpp
	src/api_handler.h
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/model.h
	src/model.cpp
	src/loot_generator.h
	src/loot_generator.cpp
	src/extra_data.h
	src/tagged.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/logger.h
	src/logger.cpp
	src/request_handler.cpp
	src/request_handler.h
)
target_link_libraries(game_server PRIVATE CONAN_PKG::boost Threads::Threads)

add_executable(game_server_tests
    src/app.h
    src/app.cpp
    src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/tagged.h
    tests/loot_generator_tests.cpp
    tests/model_tests.cpp
)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 CONAN_PKG::boost Threads::Threads)

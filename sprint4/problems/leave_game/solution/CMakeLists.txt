cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(collision_detection_lib STATIC
	src/collision_detector.h
	src/collision_detector.cpp
)

target_link_libraries(collision_detection_lib PUBLIC CONAN_PKG::boost Threads::Threads)

add_library(game_model STATIC
	src/app.cpp
	src/app.h
	src/model.cpp
	src/model.h
	src/loot_generator.cpp
	src/loot_generator.h
	src/records.h
	src/tagged.h
)
target_link_libraries(game_model PUBLIC collision_detection_lib)

add_library(game_database STATIC
	src/postgres.cpp
	src/postgres.h
)
target_link_libraries(game_database PUBLIC game_model CONAN_PKG::libpq CONAN_PKG::libpqxx)

add_library(state_serialization_lib STATIC
	src/model_serialization.h
	src/state_serialization.cpp
	src/state_serialization.h
)
target_link_libraries(state_serialization_lib PUBLIC game_model CONAN_PKG::boost)

add_executable(game_server
	src/main.cpp
	src/api_handler.cpp
	src/api_handler.h
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/extra_data.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/logger.h
	src/logger.cpp
	src/request_handler.cpp
	src/request_handler.h
)
target_link_libraries(game_server PRIVATE game_model game_database state_serialization_lib)

add_executable(game_server_tests
	tests/loot_generator_tests.cpp
	tests/model_tests.cpp
)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 game_model)

add_executable(collision_detection_tests
	tests/collision-detector-tests.cpp
)

target_link_libraries(collision_detection_tests CONAN_PKG::catch2 collision_detection_lib)

include(CTest)

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
set(CATCH2_BUILD_DIRS_VARIABLE "CONAN_BUILD_DIRS_CATCH2_${CMAKE_BUILD_TYPE_UPPER}")
include("${${CATCH2_BUILD_DIRS_VARIABLE}}/Catch.cmake")
catch_discover_tests(game_server_tests)
catch_discover_tests(collision_detection_tests)

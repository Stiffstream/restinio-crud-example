cmake_minimum_required(VERSION 3.10)

if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(PRJ crud_example)

project(${PRJ} CXX)

find_package(fmt CONFIG REQUIRED)
find_package(unofficial-http-parser CONFIG REQUIRED)
find_package(restinio CONFIG REQUIRED)
find_package(json-dto CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(optional-lite CONFIG REQUIRED)

add_executable(${PRJ}
	main.cpp
	db_layer.cpp
	request_processor.cpp)

target_link_libraries(${PRJ} PRIVATE restinio::restinio)
target_link_libraries(${PRJ} PRIVATE json-dto::json-dto)
target_link_libraries(${PRJ} PRIVATE SQLiteCpp)
target_link_libraries(${PRJ} PRIVATE nonstd::optional-lite) 

if (UNIX)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads)
	if (Threads_FOUND)
		target_link_libraries(${PRJ} PRIVATE Threads::Threads)
	endif ()
	target_link_libraries(${PRJ} PRIVATE sqlite3)
	target_link_libraries(${PRJ} PRIVATE dl)
endif ()

if (WIN32)
	target_link_libraries(${PRJ} PRIVATE wsock32 ws2_32)
endif ()

install(
	TARGETS ${PRJ}
	RUNTIME DESTINATION bin
)


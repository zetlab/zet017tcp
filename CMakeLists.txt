cmake_minimum_required (VERSION 3.10)

project (zet017tcp VERSION 1.0.1 LANGUAGES C)

option(BUILD_SHARED "Build shared library" ON)
option(BUILD_STATIC "Build static library" ON)
option(BUILD_EXAMPLES "Build examples" ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ZET017TCP_INCLUDE
	include/zet017tcp.h
)

set(ZET017TCP_SOURCES
	src/zet017tcp.c
)

if(WIN32)
	set(PLATFORM_LIBS ws2_32)
else()
	set(PLATFORM_LIBS pthread)
endif()

if(BUILD_STATIC)
	add_library(zet017tcp_static STATIC ${ZET017TCP_SOURCES} ${ZET017TCP_INCLUDE})
	target_include_directories(zet017tcp_static PRIVATE src include)
	set_target_properties(zet017tcp_static PROPERTIES
		OUTPUT_NAME "zet017tcp_s"
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_VERSION_MAJOR}
		POSITION_INDEPENDENT_CODE ON
	)
endif()

if(BUILD_SHARED)
	add_library(zet017tcp_shared SHARED ${ZET017TCP_SOURCES} ${ZET017TCP_INCLUDE})
	target_include_directories(zet017tcp_shared PRIVATE src include)
	target_link_libraries(zet017tcp_shared PRIVATE ${PLATFORM_LIBS})
	if(MSVC)
		target_sources(zet017tcp_shared PRIVATE win32/zet017tcp.def)
	endif()
	set_target_properties(zet017tcp_shared PROPERTIES
		OUTPUT_NAME "zet017tcp"
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_VERSION_MAJOR}
		POSITION_INDEPENDENT_CODE ON
	)
endif()

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(BUILD_EXAMPLES)
	add_subdirectory(example)
endif()

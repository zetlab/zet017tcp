if(WIN32)
	set(PLATFORM_LIBS ws2_32)
else()
	set(PLATFORM_LIBS pthread m)
endif()

if(BUILD_STATIC)
	add_executable(example_zet017tcp_static example_zet017tcp.c)
	target_include_directories(example_zet017tcp_static PRIVATE ../include)
	target_link_libraries(example_zet017tcp_static PRIVATE zet017tcp_static ${PLATFORM_LIBS})
endif()

if(BUILD_SHARED)
	add_executable(example_zet017tcp_shared example_zet017tcp.c)
	target_include_directories(example_zet017tcp_shared PRIVATE ../include)
	target_link_libraries(example_zet017tcp_shared PRIVATE zet017tcp_shared ${PLATFORM_LIBS})

	if (ZET017TCP_SHARED_LIB_PATH AND ZET017TCP_SHARED_LIB_NAME)
		add_custom_command(TARGET example_zet017tcp_shared POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
				${ZET017TCP_SHARED_LIB_PATH}
				${CMAKE_CURRENT_BINARY_DIR}/${ZET017TCP_SHARED_LIB_NAME}
			COMMENT "Copying zet017tcp shared library to output directory"
		)
	endif()
endif()

add_executable(example_zet017tcp_source example_zet017tcp.c ../include/zet017tcp.h ../src/zet017tcp.c)
target_include_directories(example_zet017tcp_source PRIVATE ../include)
target_link_libraries(example_zet017tcp_source PRIVATE ${PLATFORM_LIBS})

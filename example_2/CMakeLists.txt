add_custom_target(copy_files ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_CURRENT_SOURCE_DIR}/copyonly
		${CMAKE_CURRENT_BINARY_DIR}/files
	COMMENT "Copying examples of configuration files to output directory"
)

if(WIN32)
	set(PLATFORM_LIBS ws2_32 shlwapi)
else()
	set(PLATFORM_LIBS m pthread)
endif()

find_package(LibXml2 REQUIRED)

if(BUILD_STATIC)
	add_executable(example2_zet017tcp_static example2_zet017tcp.c)
	target_include_directories(example2_zet017tcp_static PRIVATE ../include)
	target_link_libraries(example2_zet017tcp_static PRIVATE zet017tcp_static ${PLATFORM_LIBS} LibXml2::LibXml2)
endif()

if(BUILD_SHARED)
	add_executable(example2_zet017tcp_shared example2_zet017tcp.c)
	target_include_directories(example2_zet017tcp_shared PRIVATE ../include)
	target_link_libraries(example2_zet017tcp_shared PRIVATE zet017tcp_shared ${PLATFORM_LIBS} LibXml2::LibXml2)

	if (ZET017TCP_SHARED_LIB_PATH AND ZET017TCP_SHARED_LIB_NAME)
		add_custom_command(TARGET example2_zet017tcp_shared POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
				${ZET017TCP_SHARED_LIB_PATH}
				${CMAKE_CURRENT_BINARY_DIR}/${ZET017TCP_SHARED_LIB_NAME}
			COMMENT "Copying zet017tcp shared library to output directory"
		)
	endif()
endif()

add_executable(example2_zet017tcp_source example2_zet017tcp.c ../include/zet017tcp.h ../src/zet017tcp.c)
target_include_directories(example2_zet017tcp_source PRIVATE ../include)
target_link_libraries(example2_zet017tcp_source PRIVATE ${PLATFORM_LIBS} LibXml2::LibXml2)

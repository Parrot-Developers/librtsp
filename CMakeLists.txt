cmake_minimum_required(VERSION 3.15)
project(rtsp VERSION 1.0)

# checks if set up rpath exists for install
if(COMMAND set_up_rpath)
    set_up_rpath()
else()
    message("Set up rpath not defined!")
endif()

add_library(${PROJECT_NAME}-internal INTERFACE)
target_include_directories(${PROJECT_NAME}-internal INTERFACE
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/internal>
                            $<INSTALL_INTERFACE:include>
)


set(LIB_SOURCES
      src/rtsp.c
      src/rtsp_client.c
      src/rtsp_client_session.c
      src/rtsp_server.c
      src/rtsp_server_request.c
      src/rtsp_server_session.c
)

add_library(${PROJECT_NAME} ${LIB_SOURCES})

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
set_target_properties(rtsp PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_include_directories(${PROJECT_NAME} PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE "RTSP_API_EXPORTS")
target_compile_definitions(${PROJECT_NAME} PRIVATE "_GNU_SOURCE")
target_compile_options(${PROJECT_NAME} PRIVATE "-fvisibility=hidden")
target_compile_options(${PROJECT_NAME} PRIVATE "-std=gnu99")

if(WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE "_WIN32_WINNT=0x0600")
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 ${PROJECT_NAME}-internal futils pomp ulog)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}-internal futils pomp ulog)
endif()

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        )

cmake_minimum_required(VERSION 3.15)
project(librtsp VERSION 1.0)

add_library(rtsp-internal INTERFACE)
target_include_directories(rtsp-internal INTERFACE
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

add_library(rtsp ${LIB_SOURCES})

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
set_target_properties(rtsp PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_include_directories(rtsp PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)

target_compile_definitions(rtsp PRIVATE "RTSP_API_EXPORTS")
target_compile_definitions(rtsp PRIVATE "_GNU_SOURCE")
target_compile_options(rtsp PRIVATE "-fvisibility=hidden")
target_compile_options(rtsp PRIVATE "-std=gnu99")

if(WIN32)
  target_compile_definitions(rtsp PRIVATE "_WIN32_WINNT=0x0600")
  target_link_libraries(rtsp ws2_32)
endif()

target_link_libraries(rtsp rtsp-internal futils pomp ulog)
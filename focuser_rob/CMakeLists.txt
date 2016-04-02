PROJECT (indi-rob-focuser C CXX)
cmake_minimum_required (VERSION 2.4.7)

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set (BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set (CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

find_package (INDI REQUIRED)

set (ROB_FOCUSER_VERSION_MAJOR 0)
set (ROB_FOCUSER_VERSION_MINOR 1)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
set (INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
include_directories (${INDI_INCLUDE_DIR})

########### Focuser Rob ###############
set (rob_focuser_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/focuser_rob.cpp)
add_executable (indi_rob_focuser ${rob_focuser_SRCS})
target_link_libraries (indi_rob_focuser libwiringX.so ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES})

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries (indi_rob_focuser rt)
endif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")

install (TARGETS indi_rob_focuser RUNTIME DESTINATION bin )
install (FILES indi_focuser_rob.xml DESTINATION ${INDI_DATA_DIR})
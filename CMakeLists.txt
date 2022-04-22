cmake_minimum_required(VERSION 3.10)

project(aeronautics)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(include)

add_executable(aeronautics main.cpp)

# Link against wiringPi and wiringSerial
find_library(WIRINGPI_LIBRARIES NAMES wiringPi)
#find_library(WIRINGSERIAL_LIBRARIES NAMES wiringSerial)

#target_link_libraries(aeronautics PUBLIC datalogger modbus IEEE754 pthread ${WIRINGPI_LIBRARIES} ${WIRINGSERIAL_LIBRARIES})
target_link_libraries(aeronautics PUBLIC datalogger modbus IEEE754 pthread ${WIRINGPI_LIBRARIES} ncurses json-c jsoncpp)


target_include_directories(aeronautics PUBLIC 
	/usr/include/modbus
	/usr/include/json-c
	/usr/include/jsoncpp
	${PROJECT_SOURCE_DIR}/include)

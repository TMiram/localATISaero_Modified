cmake_minimum_required(VERSION 3.10)

project(run)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(run main.cpp)

find_package(PythonLibs REQUIRED)
include_directories(${PYTHONPATH})

# Link against wiringPi and wiringSerial
#find_library(WIRINGSERIAL_LIBRARIES NAMES wiringSerial)

#target_link_libraries(aeronautics PUBLIC datalogger modbus IEEE754 pthread ${WIRINGPI_LIBRARIES} ${WIRINGSERIAL_LIBRARIES})
target_link_libraries(run PUBLIC pthread rtlsdr fftw3 python3.9)


target_include_directories(run PUBLIC 
	/rtl-sdr/include
	/usr/include/python3.9
	/pythonModules)

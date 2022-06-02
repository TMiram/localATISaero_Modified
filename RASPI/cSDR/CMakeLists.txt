cmake_minimum_required(VERSION 3.10)

project(run)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(run main.cpp)

# Link against wiringPi and wiringSerial
#find_library(WIRINGSERIAL_LIBRARIES NAMES wiringSerial)

#target_link_libraries(aeronautics PUBLIC datalogger modbus IEEE754 pthread ${WIRINGPI_LIBRARIES} ${WIRINGSERIAL_LIBRARIES})
target_link_libraries(run PUBLIC pthread rtlsdr fftw3)


target_include_directories(run PUBLIC 
	/rtl-sdr/include)

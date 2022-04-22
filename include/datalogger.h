#ifndef __DATALOGGER__
#define __DATALOGGER__

#include <iostream>
#include <vector>

#ifndef RASP_MODBUS_DEVICE
#define RASP_MODBUS_DEVICE "/dev/ttyUSB0"
#endif

struct modbus_register {
	uint16_t tab_reg[2];
};

float modbusRead(uint8_t address);
float calcAvg(const std::vector<float> vec);
void printArray(const std::vector<float> vec, size_t size);

#endif

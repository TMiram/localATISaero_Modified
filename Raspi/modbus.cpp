#include <iostream>
#include <modbus/modbus.h>
#include <chrono>
#include <thread>
#include "IEEE754.h"


#ifndef RASP_MODBUS_DEVICE
#define RASP_MODBUS_DEVICE "/dev/ttyUSB0"
#endif
//also defined in dalalogger.h

int main() {
	modbus_t *mb;
	uint16_t tab_reg[2];
	int rc;
	float wind_speed{0};

	mb = modbus_new_rtu(RASP_MODBUS_DEVICE, 19200, 'N', 8, 1);
	if(mb == NULL) {
		std::cout << "Unable to create the libmodbus context" << std::endl;
		return -1;
	}

	modbus_set_slave(mb, 10);
	if(modbus_connect(mb) == -1) {
		std::cout << "modbus.cpp -> Connection failed: " << modbus_strerror(errno) << std::endl;
		modbus_free(mb);
		return -1;
	}

	while(1) {
		rc = modbus_read_registers(mb, 0, 2, tab_reg);
		if(rc == -1) {
			std::cout << "Error: " << modbus_strerror(errno) << std::endl;
			return -1;
		}

		for(int i=0; i < rc; i++) {
			std::cout << "reg[" << i << "]=" << std::hex << tab_reg[i] << std::endl;
		}

		wind_speed = b32toFloat(tab_reg[0] + (tab_reg[1] << 16));
		std::cout << "Wind Speed: " << wind_speed << " m/s" << std::endl;


		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(1));
	}

	return 0;
}

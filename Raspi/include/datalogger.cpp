#include <modbus/modbus.h>
#include "IEEE754.h"
#include "datalogger.h"

static const uint8_t data_size{2};

float modbusRead(uint8_t address) {
	modbus_t *mb;
	modbus_register mb_reg;
	int rc;
	float wind_speed{0};

	//Open Serial
	mb = modbus_new_rtu(RASP_MODBUS_DEVICE, 9600, 'N', 8, 1);
	if(mb == NULL) {
		std::cout << "Unable to create the libmodbus context" << std::endl;
		return -1;
	}

	//Connect slave modbus
	modbus_set_slave(mb, address);
	if(modbus_connect(mb) == -1) {
		std::cout << "datalogger.cpp -> Modbus: Connection failed with" << RASP_MODBUS_DEVICE << " " << modbus_strerror(errno) << std::endl;
		modbus_free(mb);
		return -1;
	}

	rc = modbus_read_registers(mb, 0, data_size, mb_reg.tab_reg);
	if(rc == -1) {
		std::cout << "Error: " << modbus_strerror(errno) << std::endl;
		return -1;
	}

	wind_speed = b32toFloat(mb_reg.tab_reg[0] + (mb_reg.tab_reg[1] << 16));

	return wind_speed;
}

/*
float calcAvg(const std::vector<float> vec) {
	float sum{0};

	for(auto i:vec)
		sum += i;

	return (sum/vec.size());
}*/

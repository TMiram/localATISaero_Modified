#include "IEEE754.h"

float b32toFloat(uint32_t b32) {
	float result;
	int32_t shift;
	uint16_t bias = 0x7f;

	if(b32 == 0)
		return 0.0;

	//Get matissa
	result = (0x7fffff & b32);
	//Convert to float
	result /= 0x800000;
	//Add 1
	result += 1.0f;

	//Exponent
	shift = ((b32 >> 23) & 0xff) - bias;
	while(shift > 0) {
		result *= 2.0;
		shift--;
	}
	while(shift < 0) {
		result /= 2.0;
		shift++;
	}

	//Sign
	result *= (b32 >> 31)&1 ? -1.0 : 1.0;

	return result;
}

uint32_t floatToB32(float f) {
	float normalized;
	int16_t shift = 0;
	int32_t sign, exponent, significand;

	if(f == 0.0)
		return 0;

	//Sign
	if(f < 0) {
		sign = 1;
		normalized = -f;
	} else {
		sign = 0;
		normalized = f;
	}
	
	//Shift
	while(normalized >= 2.0) {
		normalized /= 2.0;
		shift++;
	}	
	while(normalized < 1.0) {
		normalized *= 2.0;
		shift--;
	}
	normalized -= 1.0;

	//Significand part
	significand = normalized*(0x800000 + 0.5f);

	//Exponent
	exponent = shift + 0x7f;

	return (sign << 31) | (exponent << 23) | significand;
}

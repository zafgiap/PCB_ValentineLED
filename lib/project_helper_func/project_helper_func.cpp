#include "project_helper_func.h"

float intermediate_voltage_result = 0;

float battery_volt_calc(uint16_t raw_adc){

    // intermediate, voltage devided result in the 0 - 3.3V range
    intermediate_voltage_result = (raw_adc/4096.0) * 3.3;

    // real battery voltage measurement (voltage devider resistors: 1st: 47kohm 2nd: 100kohm)
    return (intermediate_voltage_result * (147.0 / 100.0));

}
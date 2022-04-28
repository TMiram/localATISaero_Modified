#include <Adafruit_MPL115A2.h>

Adafruit_MPL115A2 PT_sensor; //Pressure and Temperature sensor
float temperature_K=0.0;
float pressure_hPa=0.0;

void setup() {
  Serial.begin(9600);
  Serial.println("Serial done");
  if (PT_sensor.begin()){
    Serial.print("Start Reading...\n");
  }
  else{
    while(!PT_sensor.begin()){
      Serial.println("waiting for MPL115A2 to be conencted");
      delay(1000);
    }
  }
}

void loop() {
  pressure_hPa=10*PT_sensor.getPressure();        //getting pressure in kilo Pa so we change in hecto Pa
  temperature_K=PT_sensor.getTemperature()+273.15;//getting temperature in celsius , so we convert in kelvin
  Serial.println("Values");
  Serial.print("Pressure: ");
  Serial.println(pressure_hPa);
  Serial.print("Pressure: ");
  Serial.println(temperature_K);
}

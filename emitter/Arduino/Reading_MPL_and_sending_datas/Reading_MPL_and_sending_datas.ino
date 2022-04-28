
////////////////////////////
/////INCLUDING LIBRARIES////
////////////////////////////

#include <time.h>
#include <Adafruit_MPL115A2.h>

////////////////////////////
///////////DEFINING PINS////
////////////////////////////
//nothing here for the moment

////////////////////////////
//////DEFINING VARIABLES////
////////////////////////////

////////////DATE////////////
int int_day=22;
int int_month=02;
int int_year=2022;

////////////TIME////////////
tm tm_time;
int int_sec=0;
int int_minutes=30;
int int_hour= 12;

////////////HEXA VALUES/////
int hex_counter_t_line=0xFF;
int hex_counter_a_line=0xFF;

////////////WEATHER/////////
int int_wind_speed=3;           //in m/s                    is s1
int int_wind_direction=0;       //in degres                 is d1
int int_humidity=50;             //in percentage             is h1
int int_temperature=3000;          //in Kelvin*10              is t1
int int_atmospheric_pressure=10; //in mBar or hectoPascals   is b1

////////////OTHERS//////////
char str_my_string_to_send_one[33];//always 30
char str_my_string_to_send_two[46];//46 is the maximum size, it can be shorter
char str_final_message[79]; //using two subchains to not have a ridiculously long expression in the sprintf later. those subchains are concatenate in the final message


////////////////////////////
//////////////SETUP LOOP////
////////////////////////////

void setup() {
//////////USUAL SETUP////////
Serial.begin(9600);
Serial.setTimeout(100);
}

////////////////////////////
///////////////MAIN LOOP////
////////////////////////////

void loop() {
  int_atmospheric_pressure=(int) 10*PT_sensor.getPressure();
  int_temperature=(int) 10*(PT_sensor.getTemperature()+273.15);

  //WRITING ON SERIAL
  sprintf(str_my_string_to_send_one,"\n #t %.2d/%.2d/%.2d %.2d:%.2d:%.2d $%.2X",int_day,int_month,int_year,int_sec,int_minutes,int_hour,hex_counter_t_line);              //line: "#t jj/mm/yyyy ss:mm:hh $hexa1"
  sprintf(str_my_string_to_send_two,"\n #a s1=%d d1=%d h1=%d t1=%d b1=%d $%.2X",int_wind_speed,int_wind_direction,int_humidity,int_temperature,int_atmospheric_pressure); //line: "#a s1=value d1=value h1=value t1=value b1=value $hexa""
  sprintf(str_final_message,"%s%s",str_my_string_to_send_one,str_my_string_to_send_two);
  Serial.print(str_final_message);

  //Serial.println(str_my_string_to_send_one);
  Serial.flush();
  //delay(10); //delay between tzo consecutives send of information
  
}

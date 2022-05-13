//don't forget to modify User_Setup.h in library folder

////////////////////////////
/////INCLUDING LIBRARIES////
////////////////////////////

#include <Adafruit_MPL115A2.h>
#include <SoftwareSerial.h>
#include <TFT_eSPI.h> 
#include <SPI.h>

////////////////////////////
///////////DEFINING PINS////
////////////////////////////


#define TX_PORT 12
#define RX_PORT 13

////////////////////////////
//////DEFINING VARIABLES////
////////////////////////////

Adafruit_MPL115A2 PT_sensor = Adafruit_MPL115A2(); //creating a MPL115A2 object (pressure and temperature object)
SoftwareSerial myport; //creating a software  serial port object
TFT_eSPI tft = TFT_eSPI();// Invoke library, pins defined in User_Setup.h

////////////DATE////////////
int int_day=22;
int int_month=02;
int int_year=2022;
char char_date[10];
////////////TIME////////////

int int_sec=0;
int int_minutes=30;
int int_hour= 12;
char char_time[8];
////////////HEXA VALUES/////
int hex_counter_t_line=0xFF;
int hex_counter_a_line=0xFF;

////////////WEATHER/////////
int int_wind_speed=30;           //in m/s                    is s1
int int_wind_direction=180;       //in degres                 is d1
int int_humidity=50;             //in percentage             is h1
int int_temperature=3000;          //in Kelvin*10              is t1
int int_atmospheric_pressure=10; //in mBar or hectoPascals   is b1

////////////OTHERS//////////
char str_my_string_to_send_one[33];//always 30
char str_my_string_to_send_two[46];//46 is the maximum size, it can be shorter
char str_final_message[79]; //using two subchains to not have a ridiculously long expression in the sprintf later. those subchains are concatenate in the final message

////////////////////////////
//////DEFINING FUNCTIONS////
////////////////////////////

////////////////////////////
//////////////SETUP LOOP////
////////////////////////////

void setup() {
  

//////////USUAL SETUP////////
  Serial.begin(9600); //to display on the computer
  myport.begin(9600,SWSERIAL_8N1, RX_PORT, TX_PORT, false); //to send datas to the raspi 
  
  PT_sensor.begin();
  tft.init();
  tft.setRotation(3); //Horizontal display , use 1 to get the other horizontal
}

////////////////////////////
///////////////MAIN LOOP////
////////////////////////////

void loop() {
  int_atmospheric_pressure=(int) 10*PT_sensor.getPressure();
  int_temperature=(int) 10*(PT_sensor.getTemperature()+273.15);
  sprintf(char_date,"%.2d/%.2d/%.2d",int_day,int_month,int_year);
  sprintf(char_time,"%.2d:%.2d:%.2d",int_hour,int_minutes,int_sec);
//DISPLAYING DATAS
  tft.fillScreen(TFT_BLACK); //BLACK BACKGROUND
  tft.setCursor(0, 0, 1); //POSITION AND FONT
  // Set the font colour to be white , set text size multiplier to 2
  tft.setTextColor(TFT_WHITE);  tft.setTextSize(2);
  // We can now plot text on screen using the "print" class
  tft.print("date: "); tft.println(char_date); 
  tft.print("hour: "); tft.println(char_time); 
  tft.print("wind sp.: "); tft.print(int_wind_speed); tft.println(" m/s"); 
  tft.print("wind d.: "); tft.print(int_wind_direction);  tft.println(" deg"); 
  tft.print("humidity: "); tft.print(int_humidity);  tft.println(" %"); 
  tft.print("temp.: "); tft.print(int_temperature);  tft.println(" K.10"); 
  tft.print("pressure: "); tft.print(int_atmospheric_pressure);  tft.println(" hPa");

//WRITING ON SERIAL
  sprintf(str_my_string_to_send_one,"\n #t %.2d/%.2d/%.2d %.2d:%.2d:%.2d $%.2X",int_day,int_month,int_year,int_sec,int_minutes,int_hour,hex_counter_t_line);              //line: "#t jj/mm/yyyy ss:mm:hh $hexa1"
  sprintf(str_my_string_to_send_two,"\n #a s1=%d d1=%d h1=%d t1=%d b1=%d $%.2X",int_wind_speed,int_wind_direction,int_humidity,int_temperature,int_atmospheric_pressure); //line: "#a s1=value d1=value h1=value t1=value b1=value $hexa""
  sprintf(str_final_message,"%s%s",str_my_string_to_send_one,str_my_string_to_send_two);
  //Serial.println(str_final_message); //DEBUG ONLY
  myport.println(str_final_message);
  Serial.println(str_final_message);
  //Serial.println(str_my_string_to_send_one);
  Serial.flush();



  delay(1000); //delay between tzo consecutives send of information
  
}

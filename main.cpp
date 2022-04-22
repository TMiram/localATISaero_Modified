#include <iostream>	//cout
#include <stdio.h>
#include <unistd.h>	//usleep
#include <fcntl.h>	//open
#include <vector>	//vector
#include <string>	//string
#include <string.h> //memset
#include <sstream>	//stream
#include <thread>
#include <memory>
#include "datalogger.h"	//modbusRead
#include <wiringPi.h>
#include <wiringSerial.h>
#include <ncurses.h> //getch to capture keyboard 
#include <stdlib.h>
#include <signal.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/json.h>
#include <fstream>


using namespace std;
using std::string;



const std::string PICO_STRING{"pico2wave -w res.wav "};
const std::string GOOGLE_STRING{".././speech.sh "};
const std::string WIND_STRING{"Wind "};

const std::string marker_s1 = "s1="; //Velocidde do vento
const std::string marker_d1 = "d1="; //Direcao do vento
const std::string marker_h1 = "h1="; //humidade do ar, em porcentagem
const std::string marker_t1 = "t1="; //temperatura, em Kelvin*10
const std::string marker_b1 = "b1="; //QNH - pressao atmosferica em hPa
const std::string marker_dolar = "$"; 
const std::string marker_date = "#t";

std::string devicePort;

const char *meteo32_RS485_Port = "/dev/ttyUSB0"; //Serial Device default (can change in config.json)
unsigned int speakDelay = 10; //10 seconds default (can change in config.json)
unsigned int numMeasures = 10; //10 measures per average (can change in config.json)



int meteo32_RS485_fd;

int exit_program = 0;
//void sig_handler(int);


int kbhit(void)
{
    int ch, r;
       
    // Turn off getch() blocking and echo 
    nodelay(stdscr, TRUE);
    noecho();
       
    // Check for input 
    ch = getch();
    if (ch == ERR)  // no input 
        r = FALSE;
    else            // input, push it back 
    {	
		fprintf(stderr,"ch=%d",ch);
        r = TRUE;
        ungetch(ch);
    }
       
    // Restore blocking and echo 
    echo();
    nodelay(stdscr, FALSE);
    return r;
}

float calcAvg(const std::vector<float> vec) {
	float sum{0};

	for(auto i:vec)
		sum += i;

	return (sum/vec.size());
}

std::string roundtoint(float num) {
	std::stringstream stream;
	stream.precision(0);
	stream << std::fixed;
	stream << num;
	std::string str = stream.str();	

	return str;
}

std::string roundoff(float num) {
	std::stringstream stream;
	stream.precision(2);
	stream << std::fixed;
	stream << num;
	std::string str = stream.str();	

	return str;
}

void picoTTS(float avg,float avg_d1,float avg_h1,float avg_t1,float avg_b1) {
	std::string strWSAvg{roundtoint(avg)};
	std::string strWDAvg{roundtoint(avg_d1)};
	std::string strHuAvg{roundtoint(avg_h1)};
	std::string strTpAvg{roundtoint(avg_t1)};
	std::string strQNHAvg{roundtoint(avg_b1)};
	
	std::string command{PICO_STRING + "\"" + WIND_STRING + strWSAvg + " kilometers per hour from" + strWDAvg + ", Air Humidity is " + strHuAvg + "percent, Temperature is " + strTpAvg + " degrees Celsius, " + "Q N H " + strQNHAvg + "\""};

	std::cout << command << std::endl;

	system(command.c_str());
	system("aplay res.wav");
}

void googleTTS(float avg, float avg_d1,float avg_h1,float avg_t1,float avg_b1) {
	std::string strWSAvg{roundtoint(avg)};
	std::string strWDAvg{roundtoint(avg_d1)};
	std::string strHuAvg{roundtoint(avg_h1)};
	std::string strTpAvg{roundtoint(avg_t1)};
	std::string strQNHAvg{roundtoint(avg_b1)};
	
	std::string command{GOOGLE_STRING + "\"" + WIND_STRING + strWSAvg + " kilometers per hour from" + strWDAvg + ", Air Humidity is " + strHuAvg + "percent, Temperature is " + strTpAvg + " degrees Celsius, " + "Q N H " + strQNHAvg + "\""};

	std::cout << command << std::endl;

	system(command.c_str());
}

int readADC() {
	int fp{0};
	char ch[5]; 

	fp = open("/sys/bus/iio/devices/iio:device0/in_voltage0_raw", O_RDONLY | O_NONBLOCK);
	if(fp < 0) {
		std::cout << "main.cpp -> readADC: Failed to open: iio:device0" << std::endl;
		return -1;
	}

	if(read(fp, ch, 5) < 0) {
		std::cout << "Failed to read iio!" << std::endl;
		close(fp);
		return -1;
	}
	
	close(fp);
	return atoi(ch);
}

//Thread th1 main method
void speak(std::shared_ptr<float> avg,
           std::shared_ptr<float> avg_d1,
           std::shared_ptr<float> avg_h1,
           std::shared_ptr<float> avg_t1,
           std::shared_ptr<float> avg_b1, uint8_t tts) {
			   
	std::shared_ptr<float> avg_value = avg;
	std::shared_ptr<float> avg_value_d1 = avg_d1;
	std::shared_ptr<float> avg_value_h1 = avg_h1;
	std::shared_ptr<float> avg_value_t1 = avg_t1;
	std::shared_ptr<float> avg_value_b1 = avg_b1;
	
	
	while(exit_program==0){
	
		std::cout << "\r" << std::flush;
		std::cout << std::endl; // all done
			
		//if(readADC() < 25000){
			if(tts)
				googleTTS(*avg_value,*avg_value_d1,*avg_value_h1,*avg_value_t1,*avg_value_b1);
			else
				picoTTS(*avg_value,*avg_value_d1,*avg_value_h1,*avg_value_t1,*avg_value_b1);
		//}
		
		std::cout << "\r" << std::flush;
		std::cout << std::endl; // all done
		
		usleep(speakDelay*1000000); //Configurable seconds between each speach
	}
	fprintf(stderr,"\nExiting speak thread th1.");
}

std::string bufferToString(char* buffer, int bufflen)
{
    std::string ret(buffer, bufflen);
    return ret;
}

bool isNumber(const string& str)
{
    for (char const &c : str) {
        if (std::isdigit(c) == 0) return false;
    }
    return true;
}

int parseWindSpeed(string buffer_str){
	//Parse Wind Speed - cannot go to the first marker directly because it is almost never there, in some cases maybe is.
	//Approach is to find the second marker and move backwards
	int windspeed=-1;
	string wsc;
   	int pos2;
   	int pos1;
   	int begin=0;
   	
   	pos2 = buffer_str.find(marker_d1);
   	
	if (pos2 != string::npos) {
		//.. found "d1=". ir para tras	
		if(pos2-8>=0){begin=pos2-8;}else{begin=0;};
		wsc = buffer_str.substr(begin, pos2-begin-1); //to be between begin and pos2 (and remode " d" at the end)
	} 
	
	pos1 = wsc.find(marker_s1);
	
	if (pos1 != string::npos) {
		//.. found "s1="
		string wsc2 = wsc.substr(pos1+3, pos2-(pos1+3)+1);
		wsc=wsc2;
	}	
	
	if(isNumber(wsc)){
		try{
		windspeed = stoi(wsc);
		}
		catch(...){} //ignore if fail, will return -1
	}		
	//fprintf(stderr," int Windspeed=%d   ",windspeed);
	return windspeed;
}

int parseWindDirection(string buffer_str){
	
	int winddirection=-1;
	string wsc;
   	int pos2;
   	int pos1;
   	int begin=0;
   	
   	pos2 = buffer_str.find(marker_h1);
   	
	if (pos2 != string::npos) {
		//.. found "h1=". ir para tras	
		if(pos2-7>=0){begin=pos2-7;}else{begin=0;};
		wsc = buffer_str.substr(begin, pos2-begin-1); //to be between begin and pos2 (and remode " d" at the end)
	} 
	
	pos1 = wsc.find(marker_d1);
	
	if (pos1 != string::npos) {
		string wsc2 = wsc.substr(pos1+3, pos2-(pos1+3)+1);
		wsc=wsc2;
	}	
	
	if(isNumber(wsc)){
		try{
		winddirection = stoi(wsc);
		}
		catch(...){} //ignore if fail
	}		
	//fprintf(stderr," int winddirection=%d   ",winddirection);	
	return winddirection;
}

int parseAirHumidity(string buffer_str){
	
	int airhumidity=-1;
	string wsc;
   	int pos2;
   	int pos1;
   	int begin=0;
   	
   	pos2 = buffer_str.find(marker_t1); 
   	
	if (pos2 != string::npos) {
		//.. found "t1=". ir para tras	
		if(pos2-7>=0){begin=pos2-7;}else{begin=0;};
		wsc = buffer_str.substr(begin, pos2-begin-1); //to be between begin and pos2 (and remode " d" at the end)
	} 
	
	pos1 = wsc.find(marker_h1);
	
	if (pos1 != string::npos) {
		string wsc2 = wsc.substr(pos1+3, pos2-(pos1+3)+1);
		wsc=wsc2;
	}	
	
	if(isNumber(wsc)){
		try{
		airhumidity = stoi(wsc);
		}
		catch(...){} //ignore if fail
	}		
	//fprintf(stderr," int airhumidity=%d   ",airhumidity);	
	return airhumidity;
}

int parseAirTemperatureK(string buffer_str){
	
	int airtempK=-1;
	string wsc;
   	int pos2;
   	int pos1;
   	int begin=0;
   	
   	pos2 = buffer_str.find(marker_b1); //right marker
   	
	if (pos2 != string::npos) {
		//.. found "b1=". ir para tras	
		if(pos2-8>=0){begin=pos2-8;}else{begin=0;};
		wsc = buffer_str.substr(begin, pos2-begin-1); //to be between begin and pos2 (and remode " d" at the end)
	} 
	
	pos1 = wsc.find(marker_t1); //left marker
	
	if (pos1 != string::npos) {
		string wsc2 = wsc.substr(pos1+3, pos2-(pos1+3)+1);
		wsc=wsc2;
	}	
	
	if(isNumber(wsc)){
		try{
		airtempK = stoi(wsc);
		}
		catch(...){} //ignore if fail
	}		
	//fprintf(stderr," int airtempK=%d   ",airtempK);	
	return airtempK;
}

int parsePressureQNH(string buffer_str){
	
	int QNH=-1;
	string wsc;
   	int pos2;
   	int pos1;
   	int begin=0;
   	   	
	pos1 = buffer_str.find(marker_b1); //left marker
		
	if (pos1 != string::npos) {
		wsc = buffer_str.substr(pos1+3, 4); //4 digitos a partir do marker
	}
	
	if(isNumber(wsc)){
		try{
			QNH = stoi(wsc);
		}
		catch(...){} //ignore if fail 
	}else{
		wsc = buffer_str.substr(pos1+3, 3); //3 digitos a partir do marker
		try{
			QNH = stoi(wsc);
		}
		catch(...){} //ignore if fail 
	}
				
	//fprintf(stderr," int QNH=%d   ",QNH);	
	//cout << "DEBUG: QNHtext:" << wsc << endl;
	//fprintf(stderr,"DEBUG: QNH:%d ",QNH);	
			
	return QNH;
}

void 
displayCfg(const Json::Value &cfg_root)
{
    devicePort   = cfg_root["Config"]["devicePort"].asString();
    speakDelay  = cfg_root["Config"]["speakDelay"].asUInt();
    numMeasures = cfg_root["Config"]["numMeasures"].asUInt();

	meteo32_RS485_Port = devicePort.c_str();

    std::cout << "______ Configuration ______" << std::endl;
    std::cout << "devicePort  :" << meteo32_RS485_Port << std::endl;
    std::cout << "speakDelay  :" << speakDelay << std::endl;
    std::cout << "numMeasures :" << numMeasures<< std::endl;
}
		
int main(int argc, char *argv[]) {
	
	//signal(SIGSEGV, sig_handler);
	
	std::vector<float> measures;
	std::vector<float> measures_d1;
	std::vector<float> measures_h1;
	std::vector<float> measures_t1;
	std::vector<float> measures_b1;
	
	uint8_t reads{0};
	uint8_t reads_d1{0};
	uint8_t reads_h1{0};
	uint8_t reads_t1{0};
	uint8_t reads_b1{0};
	
	auto avg = std::make_shared<float>();
	auto avg_d1 = std::make_shared<float>();
	auto avg_h1 = std::make_shared<float>();
	auto avg_t1 = std::make_shared<float>();
	auto avg_b1 = std::make_shared<float>();
	
	uint8_t tts{0};
	
	//Reading config file json
	Json::Reader reader;
    Json::Value cfg_root;
    std::ifstream cfgfile("config.json");
    cfgfile >> cfg_root;

    std::cout << "______ config_root : start ______" << std::endl;
    std::cout << cfg_root << std::endl;
    std::cout << "______ config_root : end ________" << std::endl;

    displayCfg(cfg_root);

	
	const uint8_t NUM_MEANSURES{(uint8_t)numMeasures};
	const uint8_t NUM_MEANSURES_d1{(uint8_t)numMeasures};
	const uint8_t NUM_MEANSURES_h1{(uint8_t)numMeasures};
	const uint8_t NUM_MEANSURES_t1{(uint8_t)numMeasures};
	const uint8_t NUM_MEANSURES_b1{(uint8_t)numMeasures};
	
	
	initscr(); // You need to initialize ncurses before you use its functions getch(), and end with endwin() later

	std::cout << "-----------------------------------------------------------------\r" << std::endl;
	std::cout << "AUTO Local ATIS --- Start...\r" << std::endl;
	
	if(argc == 2) {
		if(atoi(argv[1]) > 2){
			std::cout << "Invalid input\r" << std::endl;
			return -1;
		}
		tts = atoi(argv[1]);
		
		if(tts){
			std::cout << "Speech method selected is: " << tts << " Google\r" << std::endl;
		}else
		{
			std::cout << "Speech method selected is: " << tts << " pico2wave\r" << std::endl;
		}
	}else
		{
			std::cout << "No parameter passed, default is 0\r" << std::endl;
			std::cout << "Speech method selected is: " << tts << " pico2wave\r" << std::endl;
			std::cout << "Pass 1 as argument for Google speech\r" << std::endl;
		}

	std::cout << "-----------------------------------------------------------------" << std::endl;

//uncomment to reenable Spech
	std::thread th1(speak, avg, avg_d1,avg_h1,avg_t1,avg_b1, tts);

	std::cout << "meteo32_RS485_Port:" << meteo32_RS485_Port <<  "\r" << std::endl;
   
    //Open Serial Port
    meteo32_RS485_fd = serialOpen(meteo32_RS485_Port, 9600);
    
    if(meteo32_RS485_fd == -1){
		fprintf(stderr,"\n---Error Openning Serial Port---\n");
		exit(1);
	}
	wiringPiSetup();
	
	char buffer[100];
	
	int windSpeed = 0;
	int windSpeedKmh = 0;
	int windDirection = 0;
	int airHumidity =0;
	int airTemperatureK=0;
	int airTemperatureC=0;
	int pressureQNH=0;			
	
	string str_buffer;
	ssize_t length;
	
	
    while(exit_program == 0)
	{	
		if(kbhit()){
			//detect the key to exit
			exit_program=true;
		}
		
		std::cout << "\r" << std::flush;
		std::cout << std::endl; // all done
		
		buffer[0]= '\0';	
		length = read(meteo32_RS485_fd, &buffer, sizeof(buffer));
		if (length == -1)
		{
			cerr << "Error reading from serial port\r" << endl;
			break;
		}
		else if (length == 0)
		{
			cerr << "No more data\r" << endl;
			break;
		}
		else
		{
			buffer[length] = '\0';
			//cout << buffer << " END " << endl;
			
		}
		
		//Start analyzing buffer and strip 5 variables:
		str_buffer = bufferToString(buffer,length);
		
		if (str_buffer.find(marker_d1) != string::npos) {
		//.. found the one with the five varibles
		
			//cout << str_buffer << endl;
			
			windSpeed = parseWindSpeed(str_buffer);
			windSpeedKmh = windSpeed*3.6;
			windDirection = parseWindDirection(str_buffer);
			airHumidity = parseAirHumidity(str_buffer);
			airTemperatureK = parseAirTemperatureK(str_buffer);
			airTemperatureC = ((airTemperatureK/10)-273);
			if(airTemperatureC<(-200)){airTemperatureC=0;}//remove outliers
			//fprintf(stderr," int airTemperatureC=%d   ",airTemperatureC);	
			pressureQNH = parsePressureQNH(str_buffer);
			
			//usleep(100000);
				
			fprintf(stderr,"\nKmh:%d WD:%d HU:%d Tp:%d QNH:%d \r",windSpeedKmh,windDirection,airHumidity,airTemperatureC,pressureQNH);	
			
			//Prep Speach
			//WIND SPEED
			if(measures.size() < NUM_MEANSURES){
				measures.push_back((float)windSpeedKmh); //modbusRead retornava float 
			} else {
				measures.at(reads) = (float)windSpeedKmh;
				reads = (reads < NUM_MEANSURES-1) ? reads+1 : 0;
			}
			
			//wind DIRECTION
			if(measures_d1.size() < NUM_MEANSURES_d1){
				measures_d1.push_back((float)windDirection);
			} else {
				measures_d1.at(reads_d1) = (float)windDirection;
				reads_d1 = (reads_d1 < NUM_MEANSURES_d1-1) ? reads_d1+1 : 0;
			}
			
			//Hidrometro
			if(measures_h1.size() < NUM_MEANSURES_h1){
				measures_h1.push_back((float)airHumidity);
			} else {
				measures_h1.at(reads_h1) = (float)airHumidity;
				reads_h1 = (reads_h1 < NUM_MEANSURES_h1-1) ? reads_h1+1 : 0;
			}
			
			//Termometro
			if(measures_t1.size() < NUM_MEANSURES_t1){
				measures_t1.push_back((float)airTemperatureC);
			} else {
				measures_t1.at(reads_t1) = (float)airTemperatureC;
				reads_t1 = (reads_t1 < NUM_MEANSURES_t1-1) ? reads_t1+1 : 0;
			}
			
			//Barometro
			if(measures_b1.size() < NUM_MEANSURES_b1){
				measures_b1.push_back((float)pressureQNH);
			} else {
				measures_b1.at(reads_b1) = (float)pressureQNH;
				reads_b1 = (reads_b1 < NUM_MEANSURES_b1-1) ? reads_b1+1 : 0;
			}

			*avg = calcAvg(measures);
			*avg_d1 = calcAvg(measures_d1);
			*avg_h1 = calcAvg(measures_h1);
			*avg_t1 = calcAvg(measures_t1);
			*avg_b1 = calcAvg(measures_b1);
			
			//std::cout << "The average WS is: " << *avg << std::endl;
            //std::cout << "The average WD is: " << *avg_d1 << std::endl;
			
			
			
			
		}else if (str_buffer.find(marker_date) != string::npos) {
		//.. found the one with the date
			//Uncomment to print unparsed timestamp
			//cout << str_buffer << endl;
		}else{
			//Ignoring other buffers
		}
		
		//Flush serial port buffer before reading again
		serialFlush(meteo32_RS485_fd);
		usleep(1000000);
		
	}

	if(exit_program==1){
		fprintf(stderr,"\nExiting gracefully...");
		endwin(); /* Close ncurses before you end your program */
		fprintf(stderr,"\nncurses ended.");
		serialClose(meteo32_RS485_fd); 
		fprintf(stderr,"\nSerial Port Closed.");
		fprintf(stderr,"\nBye.\n\n");
		usleep(1000000); //dar tempo para a outra thread fechar
	}else{
		//uncomment to reenable speaking thread
		th1.join();
		return 0;
	}
	return 0;
}

/*
void abort(){
	
	fprintf(stderr,"Aborting...");
	endwin(); 
	fprintf(stderr,"ncurses ended.");
	serialClose(meteo32_RS485_fd); 
	fprintf(stderr,"Serial Port Closed. Bye.");
	
}

void sig_handler(int sig) {
    switch (sig) {
    case SIGSEGV:
        fprintf(stderr, "give out a backtrace or something...\n");
        abort();
    default:
        fprintf(stderr, "wasn't expecting that!\n");
        abort();
    }
}*/

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
#include <chrono>
#include <complex.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/mman.h> 
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <ctype.h>

#include "rtl-sdr.h"

#include <Python.h>
#include "numpy/arrayobject.h"

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Constants
/////////////////////////////////////////////////////
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#define DEFAULT_FREQ_RADIO    446000000 //central frequency in Hz for the walkie talkie (might change)
#define DEFAULT_FREQ		  105300000  //frequency of the radio station to listen to music
//#define DEFAULT_FREQ_OFFSET   250000 //to take out the DC component
#define DEFAULT_FREQ_OFFSET   0 //to take out the DC component
//#define BANDWIDTH             200000    //BW in Hz , this one is for music on radio station
#define BANDWIDTH             8000    //BW in Hz , this one is (supposed to be) for walkie talkie

#define DEFAULT_DEVICE_INDEX  0
#define DEFAULT_SAMPLE_RATE   1140000 //snumber of samples read by the SDR in 1sec
#define SDR_GAIN              400   //ginof the SDR
#define THRESHOLD             17000 //threshold to detect the pressure on the button
#define PRESS_DELAY           4000 //delay milliseconds before the last pressure on the button is erased
#define PRESS_THRESHOLD       3 //Number of time that the button has tobe pressed before it runs an action
#define BUFFER_SIZE           512 //sif of the reading window, we are getting 512 samples on every reading time

#define DEFAULT_SAMPLES 	  44100 //audio output frequency
#define MAX_SAMPLES			  8192000 //this is the maximum size readable at once. it is7.4 sec for asample rate of 1140000
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Namespaces
/////////////////////////////////////////////////////

using namespace std;
using std::string;
using std::chrono::milliseconds;
using std::chrono::system_clock;

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Variables
/////////////////////////////////////////////////////

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

FILE* opened_wav; //our wav file , the one we put the output in

std::string devicePort;
rtlsdr_dev_t *device;
//SDR INFOS
int *gain_number;
int button_pressed=0;
bool button_is_low=true;
typedef int (*get_samples_f)(uint8_t *buffer, int buffer_len);
//

const char *meteo32_RS485_Port = "/dev/ttyUSB0"; //Serial Device default (can change in config.json)
unsigned int speakDelay = 10; //10 seconds default (can change in config.json)
unsigned int numMeasures = 10; //10 measures per average (can change in config.json)

int meteo32_RS485_fd;

int exit_program = 0;
bool run_speach = false;
//void sig_handler(int);

//time counters:
int last_time_press=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(); //get the time in ms
int current_time=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

//PYTHON
PyObject *pName = NULL;
PyObject *pModule = NULL;
const char *module_name="decimate";//our file conaining the function is called deciamte
const char *cplx_func_name = "decimatecplx";//our function iscalled decimate
const char *func_name = "decimate";//our function iscalled decimate


//BUFFERS : those are declared here so that enough memory is allocated for it
std::vector<std::complex<double>> my_buffer;
std::complex<double>* final_data;
std::vector<std::complex<double>> my_products;

std::vector<std::complex<double>> cplx_information_signal;
std::vector<std::complex<double>> conjugate_products;
std::vector<double> my_angles;

////////////////
////////////////Wav header : contains all the header information, is initialized is wav_open and edited in the wav_open and wav_close functions
////////////////

struct wav_header {

	char	riff_tag[4];
	int	riff_length;
	char	wave_tag[4];
	char	fmt_tag[4];
	int	fmt_length;
	short	audio_format;
	short	num_channels;
	int	samples_per_second;
	int	byte_rate;
	short	block_align;
	short	sample_size;
	char	data_tag[4];
	int	data_length;

};

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Functions
/////////////////////////////////////////////////////

////////////////////////////
////////////////////////////WAV FILE
////////////////////////////


////////////////
////////////////wav Open: open a wav file
////////////////

FILE * wav_open(const char* myfile){

  int audio_freq=44100;

  struct wav_header header;

	int samples_per_second = DEFAULT_SAMPLES; //to have the same as the one we use wih the SDR, else we will read at the wrong speed
	int sample_size = 16;//in bits, here 16 because we are using short type as input

  //Tags
	strncpy(header.riff_tag,"RIFF",4);
	strncpy(header.wave_tag,"WAVE",4);
	strncpy(header.fmt_tag,"fmt ",4);
	strncpy(header.data_tag,"data",4);


	header.riff_length = 0;
	header.fmt_length = 16;
	header.audio_format = 1;
	header.num_channels = 1;
	header.samples_per_second = samples_per_second;
	header.byte_rate = samples_per_second*(sample_size/8);
	header.block_align = sample_size/8;
	header.sample_size = sample_size;
	header.data_length = 0;

	FILE * file = fopen(myfile,"wb+");
	if(!file) return 0;

	fwrite(&header,sizeof(header),1,file);

	fflush(file);

	return file;
}

////////////////
////////////////wav Close: close a wav file. THIS IS MANDATORY !!
////////////////

void wav_close( FILE *file )
{
	int file_size = ftell(file);
	int data_length = file_size - sizeof(struct wav_header); //size of data block isfile of the file minus size of the header
  int riff_length = file_size - 8;

	fseek(file,sizeof(struct wav_header) - sizeof(int),SEEK_SET);
	fwrite(&data_length,sizeof(data_length),1,file);

	fseek(file,4,SEEK_SET);
	fwrite(&riff_length,sizeof(riff_length),1,file);

	fclose(file);
}

////////////////
////////////////wav Write: write the datas in the wav file
////////////////

void wav_write(FILE* fp,int16_t* data, long int data_size){
  fwrite(data, sizeof(int16_t), data_size, fp);
}


////////////////
////////////////to audio
////////////////

bool toAudio(std::complex<double>* my_samples, int samples_n){ //should return the data block of the wav file => raw audio datas

  int freq_audio=DEFAULT_SAMPLES;//Hz
  int bw = 200000; //bandwidth of the signal around the central frequency
  int N=samples_n; //number of samples received


  double temp_angle;
  double max_value=0.00001;
  double sr=DEFAULT_SAMPLE_RATE;
  double f_offset=DEFAULT_FREQ_OFFSET;
  double tmp_re_max=0;
  double tmp_im_max=0;
  double tmp_re=0;
  double tmp_im=0;
  double phasis = f_offset/sr;
  double angle= -2*M_PI*phasis;

  std::complex<double> my_exp;
  std::cout<<"sizeof my_samples: \r"<<N<<endl;
  std::cout<<"phasis is: "<<phasis<<"\r"<<endl;

  for(int i=0;i<N;i++){ //getting the max to normalize
    tmp_im=abs(imag(my_samples[i]));
    tmp_re=abs(real(my_samples[i]));
    if (tmp_im_max<tmp_im) { tmp_im_max=tmp_im; }
    if (tmp_re_max<tmp_re) { tmp_re_max=tmp_re; }
  }

  for (int i=0;i<N;i++){ //is probably time consuming,this is "sorting" the voice signal out of the received signal => shifting the voice around f=0Hz (from f=Default_freq) 
    std::complex<double> tmp_cplx_angle=angle*i*I;
    std::complex<double> tmp_norm (std::real(my_samples[i])/tmp_re_max,std::imag(my_samples[i])/tmp_im_max);
    my_exp=std::exp(tmp_cplx_angle);
    my_samples[i]=my_exp *tmp_norm;//change 0.05 to tmp_norm to normalize
  }


//PYTHON DECIMATE
  npy_intp dims[2]{N,2};//my product instad of my samples
  PyObject *pArray = PyArray_SimpleNewFromData(1,dims,NPY_COMPLEX128,reinterpret_cast<void*>(my_samples)); //setiing up our python list,complex128 is for complex values with doubles (2x64)
  if(pArray==NULL){printf("Error creating pArray\n\r");exit(-1);}

  int new_sampling_rate = (int) (sr/bw); //can be float ? , fs_y
  std::cout<<"N="<<N<<"\r"<<endl;

  PyObject *pFunc = PyObject_GetAttrString(pModule,cplx_func_name);
  if(pFunc==NULL){printf("Error creating pFunc\n\r");exit(-1);}

  std::cout<<"does it run ?\r"<<endl;
  PyObject *pReturn = PyObject_CallFunctionObjArgs(pFunc,pArray,NULL);//pArray au miieu
  if(pReturn==NULL){printf("Error python funtion call returned an empty value\n\r");exit(-1);}
  std::cout<<"it does\r"<<endl;
  
  PyArrayObject *np_ret = reinterpret_cast<PyArrayObject*>(pReturn); //we want to get that back to a vector or an array
  
  int siz=PyArray_SHAPE(np_ret)[0];
  int my_length;
  my_length= siz;
  std::complex<double>* py_output = (std::complex<double>*) PyArray_BYTES((np_ret));

  std::cout<<"np_ret of size "<<siz<<"\r"<<endl;
  std::cout<<"array is "<<py_output[0]<<"\r"<<endl;
  std::cout<<"array is "<<py_output[siz-1]<<"\r"<<endl;
  std::cout<<"my first lenght: "<<my_length<<"\r"<<endl;

//END OF PYTHON DECIMATE
  Py_DECREF(pArray);
  Py_DECREF(pFunc);


  for (int i=1; i<my_length; i++){ //this is taking out the central frequency
    //polar discrimination
    std::complex<double> tmp_prod=py_output[i]*std::conj(py_output[i-1]);
    temp_angle= std::arg(tmp_prod);
    my_angles.push_back(temp_angle); // atan( Im(z)/Re(z) ) = arg(z)
  }

//SECOND DECIMATION
  int number_of_angles= my_angles.size();
  double* angles_ptr;
  angles_ptr=(double*) malloc(number_of_angles*sizeof(double));
  npy_intp dims2[2]{number_of_angles,1};//my product instad of my samples

  std::cout<<"copying"<<"\r"<<endl;
  std::copy(my_angles.begin(),my_angles.end(),angles_ptr);
  std::cout<<"angles_ptr[0] = "<<angles_ptr[0]<<"\r"<<endl;

  PyObject *pArray2 = PyArray_SimpleNewFromData(1,dims2,NPY_DOUBLE,angles_ptr);
  PyObject *pFunc2 = PyObject_GetAttrString(pModule,func_name);
  PyObject *pReturn2 = PyObject_CallFunctionObjArgs(pFunc2,pArray2,NULL);//pArray au miieu
  if(pReturn2==NULL){printf("Error python function2 call returned an empty value\n\r");exit(-1);}

  PyArrayObject *np_ret2 = reinterpret_cast<PyArrayObject*>(pReturn2);

  int siz2=PyArray_SHAPE(np_ret2)[0];

  std::cout<<"siz2 is "<<siz2<<"\r"<<endl;
  std::complex<double>* py_output2 = (std::complex<double>*)( PyArray_BYTES(np_ret2) );

//END OF SECOND DECIMATION
  free(angles_ptr);
  Py_DECREF(pArray2);
  Py_DECREF(pFunc2);

  int dec_audio=(int) (new_sampling_rate/freq_audio);
  //int aa=new_sampling_rate/dec_audio;
  int file_samples=siz2;
  std::vector<short> audio_samples;
  
  //std::cout<<"FS Audio: "<<aa<<"\r"<<endl;
  std::cout<<"dec audio: "<<dec_audio<<"\r"<<endl; //is supposed to be 5 or so
  std::cout<<"my lenght: "<<my_length<<"\r"<<endl;
  std::cout<<"number of S: "<<file_samples<<"\r"<<endl;

  //getting max value to normalise
  for (int i=1; i<file_samples; i++){ //this is taking out the central frequency
    temp_angle =1*std::real(py_output2[i]);
    if (max_value<abs(temp_angle)){
      max_value=abs(temp_angle);
    }
  }

  temp_angle=0; //useless
  short int temp_ang;
  std::cout<<"max is "<<max_value<<"\r"<<endl;
  max_value=10000/max_value;
  std::cout<<"max is "<<max_value<<"\r"<<endl;
  std::cout<<"getting samples ready for output..."<<"\r"<<endl;

  for (int i=0; i<file_samples; i++){ //getting the information signal at the right speed
    temp_ang=1*max_value*std::real(py_output2[i]);
    audio_samples.push_back((short int) temp_ang);
  }

  int audio_size=audio_samples.size();
  std::cout<<audio_size<<" Is the right size"<<"\r"<<endl;


/////////////////////////////////////////////////////////////// this is to seethe conent of audio_samples in a file
/*
  ofstream myfilet;
  myfilet.open("tmpOut.txt");
  for(int i=0;i<file_samples;i++){
    myfilet<<audio_samples[i];
    myfilet<<"\n";
  }
  myfilet.close();
*/
///////////////////////////////////////////////////////////////

//Starting this point it's only writing in the file,audio_samples contains our data and is of size file_samples

  int16_t audio_to_file[audio_size];
  std::copy(audio_samples.begin(),audio_samples.end(),audio_to_file);

  opened_wav=wav_open("test.wav"); //opening the wav file
  if(NULL==opened_wav){std::cout<<"couldn't open wav file"<<endl;exit(-1);}
  wav_write(opened_wav,audio_to_file, audio_size);
  wav_close(opened_wav); //closing the wav file: MANDATORY
  
  std::cout<<"wrote data successfully"<<endl;

  return true;
}

int get_samples_rtl_sdr(uint8_t *buffer, int buffer_len) {
  int len;
  rtlsdr_read_sync(device, buffer, buffer_len, &len);
  return len == buffer_len;
}

void receive(get_samples_f get_samples , int buffer_size) {
  
  uint8_t buffer[BUFFER_SIZE];
  int len;
  bool myflag=false;

  std::complex<double> sample (0,0); // = 0+0*I
  std::complex<double> product (0,0);
  std::complex<double> prev_sample (0,0);
  std::complex<double> product_list[]={(0,0),(0,0),(0,0),(0,0)};

  final_data= (std::complex<double>*) calloc(MAX_SAMPLES,sizeof(std::complex<double>));
  int number_of_samples=0; //number of samples put in final data
  bool run_processing=false;

  //std::thread th3(toAudio,final_data,number_of_samples);

  while(get_samples(buffer, buffer_size) == 1) {

    for (int j = 0; j < buffer_size; j+=2) {
      
      sample = (buffer[j] - 127) + (buffer[j+1] - 127)*I; //use the phase difference beetween two consecutives measures to create/find the amplitude
      //productlist keeps a track of the previous values to not detect a false trigger
      product = sample * std::conj(prev_sample);
      product_list[0]=product_list[1];
      product_list[1]=product_list[2];
      product_list[2]=product_list[3];
      product_list[3]=product;

      current_time=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

	  if(true==run_processing || MAX_SAMPLES<=number_of_samples){ //if the max size is reached or if the buton was released
		//lancer thread Audio
		toAudio(final_data,number_of_samples);
		//th3.join();
		number_of_samples=0;
		run_processing=false;
	  }

      if (real(product)>THRESHOLD && true==button_is_low && current_time-last_time_press>=1000){ //if real part of incoming signal is higher than threshold. AND the button on radio was realeased /not pressed for 1 second
        
		myflag=true;

        for(int i=0;i<3;i++){
          if (abs(real(product_list[i]))<THRESHOLD){ //if all of the 4 previous product are higher than the threshold
            myflag=false;
          }
        }
        if (myflag){ //if all the 4 previous product are higher than threshold
          myflag=false;
          std::cout<<"it was pressed! \r"<<endl;
          button_is_low=false;  //the button is considered pressed (will be updated on release)
        }
      }
      //SAME AS THE IF but it is for released so we are looking for a real part below threshold/2 to make sure it was released
      else if(abs(real(product))<(THRESHOLD/2) && false==button_is_low && current_time-last_time_press>=1000){
        myflag=true;

        for(int i=0;i<3;i++){

          if (abs(real(product_list[i]))>THRESHOLD/2){
            myflag=false;
          }
        }
        if(myflag){
          myflag=false;
          std::cout<<"it was released! \r"<<endl;
		  run_processing=true;//Run toAudio at the next loop
          button_is_low=true;
          button_pressed+=1; //update the value
          last_time_press=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }
      }

	  if (false==button_is_low){
		final_data[number_of_samples]=sample;
		number_of_samples+=1;
	  }

      current_time=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
      if(button_pressed>=PRESS_THRESHOLD){
        button_pressed=0;
        std::cout<<"running your function \r"<<endl;    
		run_speach=true;  
      }

      else if (current_time-last_time_press>PRESS_DELAY && button_pressed>0){
        button_pressed=0;
        std::cout<<"RESET \r"<<endl;
      }

      prev_sample = sample;
    }
  }
}

static void check (int test, const char * message, ...)
{
    if (test) {
        va_list args;
        va_start (args, message);
        vfprintf (stderr, message, args);
        va_end (args);
        fprintf (stderr, "\n");
        exit (EXIT_FAILURE);
    }
}

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
	
	std::string command{PICO_STRING + "\"" + WIND_STRING + strWSAvg + " kilometers per hour from" + strWDAvg + ", Air Humidity is " + strHuAvg + "percent, Temperature is " + strTpAvg + " degrees Celsius, " + "Q N H " + strQNHAvg + "\n\r" +"\""};

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
		if(true==run_speach){
			run_speach=false; //or while loop
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
			
		}
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

//SDR 
	  /* SDR settings */
  uint32_t device_index = DEFAULT_DEVICE_INDEX; //0 by default
  uint32_t sample_rate = DEFAULT_SAMPLE_RATE;
  int buffer_size = BUFFER_SIZE;
  uint32_t freq = DEFAULT_FREQ_RADIO-DEFAULT_FREQ_OFFSET;

  
  if (rtlsdr_get_device_count() - 1 < device_index) {
    fprintf(stderr, "SDR Device %d not found\n", device_index);
    exit(EXIT_FAILURE);
  }
  
  if (rtlsdr_open(&device, device_index) != 0) {
    fprintf(stderr, "SDR Error opening device %d: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  	/* SETTING UP SDR*/
  rtlsdr_set_tuner_gain_mode(device, 0); //0 meansgain is manual. Here it should be manual because of the threshold
  rtlsdr_set_tuner_gain(device,SDR_GAIN);
  rtlsdr_set_agc_mode(device, 0);// RTL2832 auto gain off, put 1 for auto gain
  rtlsdr_set_sample_rate(device, sample_rate);
  rtlsdr_set_center_freq(device, freq);
  rtlsdr_set_tuner_bandwidth(device,(uint32_t)BANDWIDTH);
  
  rtlsdr_reset_buffer(device); // emptying/clearing buffer
//End of SDR


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

	std::cout << "-----------------------------------------------------------------\r" << std::endl;

//uncomment to reenable Spech
	std::thread th1(speak, avg, avg_d1,avg_h1,avg_t1,avg_b1, tts);
	std::thread th2(receive,get_samples_rtl_sdr,buffer_size);
	


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
	
	int fd_shared;
    const char * file_name = "../SDR/shared.txt";
    fd_shared = open (file_name, O_RDWR);
		
//EMBED PYTHON
	Py_Initialize();
	setenv("PYTHONPATH",".",0);
	import_array();
	PyRun_SimpleString("import sys; sys.path.insert(0,'pythonModules/')");
	//importing our python module
	pName=PyUnicode_FromString(module_name);
	pModule= PyImport_Import(pName);
	if(pModule==NULL){
		printf("Error creating pModule\n\r");
		exit(-1);
	}
	
	std::cout<<"python initialized\r"<<endl;

//MAIN LOOP
    while(exit_program == 0)
	{	
		//Waiting for pilot torequest data

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
				
			fprintf(stderr,"\nKmh:%d WD:%d HU:%d Tp:%d QNH:%d \r\n",windSpeedKmh,windDirection,airHumidity,airTemperatureC,pressureQNH);	
			
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
		Py_Finalize(); //closing python session
		endwin(); /* Close ncurses before you end your program */
		fprintf(stderr,"\nncurses ended.");
		serialClose(meteo32_RS485_fd); 
		fprintf(stderr,"\nSerial Port Closed.");
		fprintf(stderr,"\nBye.\n\n");
		usleep(1000000); //dar tempo para a outra thread fechar
	}else{
		//uncomment to reenable speaking thread
		th1.join();
		th2.join(); //reading the SDR 
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

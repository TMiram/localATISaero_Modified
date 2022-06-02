#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <complex.h>
#include <fftw3.h>
#include <time.h>
#include <chrono>

#include "rtl-sdr.h"


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Constant
/////////////////////////////////////////////////////
#define DEFAULT_SAMPLE_RATE   400000
#define DEFAULT_DEVICE_INDEX  0
#define BUFFER_SIZE           (1024*2)
#define FFT_SIZE              (BUFFER_SIZE/2)
//#define DEFAULT_FREQ          104100000
#define DEFAULT_FREQ          446000000 //central frequency in Hz
#define BANDWIDTH             400000    //BW in Hz
#define SDR_GAIN              400   //ginof the SDR
#define THRESHOLD             17000 //threshold to detect the pressure on the button
#define PRESS_DELAY           4000 //delay milliseconds before the last pressure on the button is erased
#define PRESS_THRESHOLD       3 //Number of time that the button has tobe pressed before it runs an action

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Namespaces
/////////////////////////////////////////////////////
using namespace std;
using std::chrono::milliseconds;
using std::chrono::system_clock;
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Variables
/////////////////////////////////////////////////////

rtlsdr_dev_t *device;

int *gain_number;
bool button_is_low=true;
int button_pressed=0;
typedef int (*get_samples_f)(uint8_t *buffer, int buffer_len);
//time couters:
int last_time_press=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(); //get the time in ms
int current_time=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Functions
/////////////////////////////////////////////////////

////////////////
////////////////rtl-sdr reader : read the sdr
////////////////
int get_samples_rtl_sdr(uint8_t *buffer, int buffer_len) {
  int len;
  rtlsdr_read_sync(device, buffer, buffer_len, &len);
  return len == buffer_len;
}

////////////////
////////////////receive
////////////////
void receive(get_samples_f get_samples , int buffer_size) {
  
  uint8_t buffer[BUFFER_SIZE];
  int len;
  bool myflag=false;

  std::complex<double> sample (0,0); // = 0+0*I
  std::complex<double> product (0,0);
  std::complex<double> prev_sample (0,0);
  std::complex<double> product_list[]={(0,0),(0,0),(0,0),(0,0)};

  
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
      if (real(product)>THRESHOLD && true==button_is_low && current_time-last_time_press>=1000){ //if real part of incoming signal is higher than threshold. AND the button on radio was realeased /not pressed for 1 second
        myflag=true;

        for(int i=0;i<3;i++){
          if (abs(real(product_list[i]))<THRESHOLD){ //if all of the 4 previous product are higher than the threshold
            myflag=false;
          }
        }
        if (myflag){ //if all the 4 previous product arehigher than threshold
          myflag=false;
          std::cout<<"it was pressed!"<<endl;
          button_is_low=false;  //tehe button is considered pressed (will be udated on release)
          
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
          std::cout<<"it was released!"<<endl;
          button_is_low=true;
          button_pressed+=1; //update the value
          last_time_press=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
          std::cout<<last_time_press<<endl;
          
        }
      }

      current_time=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
      if(button_pressed>=PRESS_THRESHOLD){
        button_pressed=0;
        std::cout<<"running your function"<<endl;      
      }

      else if (current_time-last_time_press>PRESS_DELAY && button_pressed>0){
        button_pressed=0;
        std::cout<<"RESET"<<endl;
      }

      prev_sample = sample;
    }
  }
}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////MAIN
/////////////////////////////////////////////////////

int main(int argc, char **argv) {

  /* SDR settings */
  uint32_t device_index = DEFAULT_DEVICE_INDEX; //0 by default
  uint32_t sample_rate = DEFAULT_SAMPLE_RATE;
  int buffer_size = BUFFER_SIZE;
  uint32_t freq = DEFAULT_FREQ;

  
  if (rtlsdr_get_device_count() - 1 < device_index) {
    fprintf(stderr, "Device %d not found\n", device_index);
    exit(EXIT_FAILURE);
  }
  
  if (rtlsdr_open(&device, device_index) != 0) {
    fprintf(stderr, "Error opening device %d: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // SETTING UP SDR
  rtlsdr_set_tuner_gain_mode(device, 0); //0 meansgain is manual. Here it should be manual because of the threshold
  rtlsdr_set_tuner_gain(device,SDR_GAIN);
  // RTL2832 auto gain off
  rtlsdr_set_agc_mode(device, 0);
  rtlsdr_set_sample_rate(device, sample_rate);
  rtlsdr_set_center_freq(device, freq);
  rtlsdr_set_tuner_bandwidth(device,(uint32_t)BANDWIDTH);
  
  rtlsdr_reset_buffer(device); // emptying/clearing buffer
  receive(get_samples_rtl_sdr, buffer_size); //reqding coninuously the sdr

  return 0;
}

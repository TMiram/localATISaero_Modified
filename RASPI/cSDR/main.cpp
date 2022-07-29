
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <iterator>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <complex.h>
#include <math.h>
#include <fftw3.h>
#include <time.h>
#include <chrono>
#include <fstream>
#include <vector>

#include <Python.h>
#include "numpy/arrayobject.h"

#include "rtl-sdr.h"


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Constant
/////////////////////////////////////////////////////
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#define DEFAULT_SAMPLES 44100 //Hz , is he sample rate of the output audio file

#define DEFAULT_SAMPLE_RATE 1140000//Hz
#define DEFAULT_DEVICE_INDEX 0
#define BUFFER_SIZE 512
#define FFT_SIZE (BUFFER_SIZE / 2) //
//#define DEFAULT_FREQ 105300000  //frequency of the chanel
#define DEFAULT_FREQ 446018979  //frequency of the chanel

#define SDR_BW (uint32_t)8000
//#define DEFAULT_FREQ 446018800  //frequency of the chanel
//#define DEFAULT_FREQ_OFFSET 250000 //to take out the DC component
#define DEFAULT_FREQ_OFFSET 0 //to take out the DC component
//#define DEFAULT_FREQ_OFFSET 10000 //to take out the DC component
#define SDR_GAIN 400      // ginof the SDR
#define THRESHOLD 15000   // threshold to detect the pressure on the button
#define PRESS_DELAY 400  // delay seconds before the last pressure on the button is erased
#define PRESS_THRESHOLD 3 // Number of time that the button has to be pressed before it runs an action



/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Namespaces
/////////////////////////////////////////////////////
using namespace std;
using std::chrono::milliseconds;
using std::chrono::system_clock;


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////Variables
/////////////////////////////////////////////////////


int counter=0; //to see how much samples we took from the sdr
const int number_of_samples=8192000;
//int number_of_samples=200000;

FILE* opened_wav;

rtlsdr_dev_t *device;

int *gain_number;
bool button_is_low= true;
int button_pressed= 0;
typedef int (*get_samples_f)(uint8_t *buffer, int buffer_len);
// time couters:
int last_time_press = std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
int current_time = std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

std::ofstream myf("sample.bin", std::ios::binary);

//BUFFERS

std::vector<std::complex<double>> my_buffer;
std::complex<double>* final_data;
std::vector<std::complex<double>> my_products;

std::vector<std::complex<double>> cplx_information_signal;
std::vector<std::complex<double>> conjugate_products;
std::vector<double> my_angles;

//PYTHON
PyObject *pName = NULL;
PyObject *pModule = NULL;
const char *module_name="decimate";//our file conaining the function is called deciamte
const char *cplx_func_name = "demod";//our function iscalled decimate
const char *func_name = "decimate";//our function iscalled decimate


/////////////////////////////////////////////////////
/////////////////////////////////////////////////_is_open////Structures
/////////////////////////////////////////////////////

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
////////////

void wav_write(FILE* fp,int16_t* data, long int data_size){
  fwrite(data, sizeof(int16_t), data_size, fp);
}


////////////////
////////////////to audio
////////////////

std::vector<short> toAudio(std::complex<double>* my_samples){ //should return the data block of the wav file => raw audio datas

  int freq_audio=44100;//Hz
  int bw = 200000; //bandwidth of the signal around the central frequency
  //int bw = 8000; //bandwidth of the signal around the central frequency
  int N=number_of_samples;


  double temp_angle;
  double max_value=0;
  double sr=DEFAULT_SAMPLE_RATE;
  double f_offset=DEFAULT_FREQ_OFFSET;
  double tmp_re_max=0;
  double tmp_im_max=0;
  double tmp_re=0;
  double tmp_im=0;
  double phasis = f_offset/sr;
  double angle= -2*M_PI*phasis;

  std::complex<double> my_exp;
  std::cout<<"sizeof my_samples: "<<N<<endl;
  std::cout<<"phasis is: "<<phasis<<endl;

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
  npy_intp dims[2]{number_of_samples,2};//my product instad of my samples
  PyObject *pArray = PyArray_SimpleNewFromData(1,dims,NPY_COMPLEX128,reinterpret_cast<void*>(my_samples)); //setiing up our python list,complex128 is for complex values with doubles (2x64)
  if(pArray==NULL){printf("Error creating pArray\n\r");exit(-1);}

  int dec_step= (int) (sr/bw);
  int new_sampling_rate = (int) (sr/dec_step); //can be float ? , fs_y
  int my_length=(int) (N/dec_step);
  std::cout<<"N="<<N<<endl;
  std::cout<<"dec_step="<<dec_step<<endl;

  PyObject *pFunc = PyObject_GetAttrString(pModule,"decimatecplx");
  if(pFunc==NULL){printf("Error creating pFunc\n\r");exit(-1);}

  std::cout<<"does it run ?\r"<<endl;
  PyObject *pReturn = PyObject_CallFunctionObjArgs(pFunc,pArray,NULL);//pArray au miieu
  if(pReturn==NULL){printf("Error python funtion call returned an empty value\n\r");exit(-1);}
  std::cout<<"it does\r"<<endl;
  
  PyArrayObject *np_ret = reinterpret_cast<PyArrayObject*>(pReturn); //we want to get that back to a vector or an array
  
  int siz=PyArray_SHAPE(np_ret)[0];
  my_length= siz;
  std::complex<double>* py_output = (std::complex<double>*) PyArray_BYTES((np_ret));

  std::cout<<"np_ret of size "<<siz<<endl;
  std::cout<<"array is "<<py_output[0]<<endl;
  std::cout<<"array is "<<py_output[siz-1]<<endl;
  std::cout<<"my first lenght: "<<my_length<<endl;

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

  std::cout<<"copying"<<endl;
  std::copy(my_angles.begin(),my_angles.end(),angles_ptr);
  std::cout<<"angles_ptr[0] = "<<angles_ptr[0]<<endl;

  PyObject *pArray2 = PyArray_SimpleNewFromData(1,dims2,NPY_DOUBLE,angles_ptr);
  PyObject *pFunc2 = PyObject_GetAttrString(pModule,"decimate");
  PyObject *pReturn2 = PyObject_CallFunctionObjArgs(pFunc2,pArray2,NULL);//pArray au miieu
  if(pReturn2==NULL){printf("Error python function2 call returned an empty value\n\r");exit(-1);}

  PyArrayObject *np_ret2 = reinterpret_cast<PyArrayObject*>(pReturn2);

  int siz2=PyArray_SHAPE(np_ret2)[0];

  std::cout<<"siz2 is "<<siz2<<endl;
  std::complex<double>* py_output2 = (std::complex<double>*)( PyArray_BYTES(np_ret2) );

//END OF SECOND DECIMATION
  free(angles_ptr);
  Py_DECREF(pArray2);
  Py_DECREF(pFunc2);

  int dec_audio=(long long int) (new_sampling_rate/freq_audio);
  int aa=new_sampling_rate/dec_audio;
  int file_samples=siz2;
  std::vector<short> audio_samples;

  std::cout<<"FS Audio: "<<aa<<endl;
  std::cout<<"dec audio: "<<dec_audio<<endl;
  std::cout<<"my lenght: "<<my_length<<endl;
  std::cout<<"number of S: "<<file_samples<<endl;

  //getting max value to normalise
  for (int i=1; i<file_samples; i++){ //this is taking out the central frequency
    temp_angle =1*std::real(py_output2[i]);
    if (max_value<abs(temp_angle)){
      max_value=abs(temp_angle);
    }
  }

  temp_angle=0; //useless
  short int temp_ang;
  std::cout<<"max is "<<max_value<<endl;
  max_value=10000/max_value;
  std::cout<<"max is "<<max_value<<endl;
  std::cout<<"getting samples ready for output..."<<endl;

  for (int i=0; i<file_samples; i++){ //getting the information signal at the right speed
    temp_ang=1*max_value*std::real(py_output2[i]);
    audio_samples.push_back((short int) temp_ang);
  }


  std::cout<<audio_samples.size()<<" Is the right size"<<endl;


////////////////////////////////////////////////////////////////
  ofstream myfilet;
  myfilet.open("tmpOut.txt");
  for(int i=0;i<file_samples;i++){
    myfilet<<audio_samples[i];
    myfilet<<"\n";
  }
  myfilet.close();
////////////////////////////////////////////////////////////////


  return audio_samples;
}

////////////////
////////////////to_cplx_sample  convert input into a complex value
////////////////

/*
sample is the value of the real sample
indice_sample is 0 for the first samle, 1 for the second...
sr is he sample rate of the signal
*/

std::complex<double> to_cplx_sample(uint8_t sample,int indice_sample ,int sr){
  double phasis = 2*M_PI*(indice_sample/sr);
  std::complex<double> new_sample= sample*( cos(phasis) + sin(phasis)*I );
  return new_sample;
}

////////////////
////////////////rtl-sdr reader
////////////////
int get_samples_rtl_sdr(uint8_t *buffer, int buffer_len)
{
  int len;
  rtlsdr_read_sync(device, buffer, buffer_len, &len);
  return len == buffer_len;
}

////////////////
////////////////receive
////////////////
void receive(get_samples_f get_samples, int buffer_size)
{
  int len;
  bool myflag = false;
  uint8_t buffer[BUFFER_SIZE];
  final_data= (std::complex<double>*) calloc(number_of_samples,sizeof(std::complex<double>));
  std::complex<double> sample(0, 0);
  std::complex<double> product(0, 0);
  std::complex<double> prev_sample;
  prev_sample=(0, 0);
  std::complex<double> product_list[4];
  product_list[3]=(0,0);
  product_list[2]=(0,0);
  product_list[1]=(0,0);
  product_list[0]=(0,0);
  int fft_size = buffer_size / 2;
  int16_t output_buffer[fft_size];

  //will get all the values sampled

  int start_time=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  std::cout<<"you have 7.4 sec to speak"<<endl;
  while (get_samples(buffer, buffer_size) == 1)
  {
    /* Compute amplitudes in the time domain
       from phase difference between successive samples */

    for (int j = 0; j < buffer_size; j += 2){

      sample = (buffer[j] - 127) + (buffer[j + 1] - 127) * I;
      my_buffer.push_back(sample);
      counter+=1;
      //keeping track of product to not get a false detection of a spike
      product = sample * std::conj(prev_sample);  //y[n]=x[n].x*[n-1] : angle of this is instantaneous freq
      prev_sample=sample;
      
      //Doing a buffer to get a more precise mesure to detect the pressure on the button
      product_list[0] = product_list[1];
      product_list[1] = product_list[2];
      product_list[2] = product_list[3];
      product_list[3] = product;
      

     //START OF DETECT CLICK FrOM THE RADIO
      current_time = std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(); //getting time to reset the click detect counter
/*
      if (real(product) > THRESHOLD && true == button_is_low){
        myflag = true;
        //checking if we detect a presure : a power spike over a threshold
        for (int i = 0; i < 3; i++){
          if (abs(real(product_list[i])) < THRESHOLD){
            myflag = false;
          }
        }
        if (myflag){
          myflag = false;
          std::cout << "it was pressed!" << endl;
          button_is_low = false;
        }
      }


      else if (abs(real(product)) < (THRESHOLD / 2) && false == button_is_low && current_time - last_time_press >= 1000){ //need a delay else the click is detected twice, 1000 is the minimum to keep it reliable
           myflag = true;

        for (int i = 0; i < 3; i++){
          if (abs(real(product_list[i])) > THRESHOLD / 2){
            myflag = false;
          }
        }

        if (myflag){
          myflag = false;
          std::cout << "it was released!" << endl;
          button_is_low = true;
          button_pressed += 1;
          last_time_press = std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }

      }

      current_time = std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      if (button_pressed >= PRESS_THRESHOLD){
        button_pressed = 0;
        std::cout << "running the function" << endl;
        // while(true){};
      }

      else if (current_time - last_time_press > PRESS_DELAY && button_pressed > 0){
        button_pressed = 0;
        std::cout << "RESET" << endl;
      }
     //END OF DETECT CLICK FROM RADIO       
*/
    }
    
    //taking the samples here
    if (counter>=number_of_samples){

      std::cout<<counter<<"samples read"<<endl;

      opened_wav=wav_open("test.wav"); //opening the wav file
      if(NULL==opened_wav){std::cout<<"couldn't open wav file"<<endl;exit(-1);}
      std::cout<<"wav file location ptr: "<<opened_wav<<endl;
      int end_time=std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

/*
      my_buffer.clear();
      int cont=0;
      string lineRe;
      string lineIm;
      ifstream inFileIm ("../test/idata.raw");
      ifstream inFileRe ("../test/rdata.raw");
      while ( (cont<number_of_samples) && (getline(inFileRe,lineRe))&& (getline(inFileIm,lineIm))){
        cont+=1;
        std::complex<double> tmp_val (stod(lineRe),stod(lineIm));
        my_buffer.push_back(tmp_val);
      }
      inFileIm.close();
      inFileRe.close();
*/
      std::copy(my_buffer.begin(),my_buffer.end(),final_data);


////////////////////////////////////////////////////////////////
      ofstream myfilet;
      myfilet.open("tmpOut.txt");
      for(int i=0;i<number_of_samples;i++){
        myfilet<<final_data[i];
        myfilet<<"\n";
      }
      myfilet.close();
////////////////////////////////////////////////////////////////

      std::vector<short> audio_vector=toAudio(final_data);

      free(final_data);

      int audio_size=audio_vector.size();
      int16_t my_audio[audio_size];
      std::copy(audio_vector.begin(),audio_vector.end(),my_audio);

      std::cout<<audio_size<<" samples"<<endl;
      wav_write(opened_wav,my_audio, audio_size);
/*
      
      ofstream myfile1;
      myfile1.open("CppOutput.txt");
      for(int i=0;i<audio_size;i++){
        char tmp_str[15];
        sprintf(tmp_str,"%d \n",my_audio[i]);
        myfile1<<tmp_str;
      }
      myfile1.close();
*/

      double duration=((double) end_time-(double) start_time)/1000;
      std::cout<<duration<<endl;

      wav_close(opened_wav); //closing the wav file: MANDATORY
      std::cout<<"finished"<<endl;
      exit(-1);
    }

  }
}

////////////////
////////////////print usage
////////////////
// shows the options that can be passed as input

void print_usage()
{
  fprintf(stderr,
          "Usage: fm_receiver [OPTIONS]\n\n\
Valid options:\n\
  -r <file>         Use recorded data from <file> instead of an rtl-sdr device\n\
  -f <frequency>    Frequency to tune to, in Hz (default: %.2f MHz)\n\
  -d <device_index> Rtl-sdr device index (default: 0)\n\
  -g                show possible values for gain\n\
  -h                Show this\n",
          DEFAULT_FREQ / 1000000.0);
}




/////////////////////////////////////////////////////MAIN
/////////////////////////////////////////////////////

int main(int argc, char **argv)
{
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
  

  /* SDR settings */
  uint32_t device_index = DEFAULT_DEVICE_INDEX;
  uint32_t sample_rate = DEFAULT_SAMPLE_RATE;
  int buffer_size = BUFFER_SIZE;
  uint32_t freq = DEFAULT_FREQ-DEFAULT_FREQ_OFFSET;

  /* Parse command line arguments */
  errno = 0;

    /* Open device and set it up */
    if (rtlsdr_get_device_count() - 1 < device_index)
    {
      fprintf(stderr, "Device %d not found\n", device_index);
      print_usage();
      exit(EXIT_FAILURE);
    }

    if (rtlsdr_open(&device, device_index) != 0)
    {
      fprintf(stderr, "Error opening device %d: %s", strerror(errno));
      print_usage();
      exit(EXIT_FAILURE);                       
    }


    // Tuner (ie. E4K/R820T/etc) auto gain
    rtlsdr_set_tuner_gain_mode(device, 1);
    rtlsdr_set_tuner_gain(device, SDR_GAIN);
    // RTL2832 auto gain off
    rtlsdr_set_agc_mode(device, 0);
    rtlsdr_set_sample_rate(device, sample_rate);
    rtlsdr_set_center_freq(device, freq);
    rtlsdr_set_tuner_bandwidth(device, SDR_BW);
    fprintf(stderr, "Frequency set to %d\n", rtlsdr_get_center_freq(device));
    
    rtlsdr_reset_buffer(device);// clears the buffer

    receive(get_samples_rtl_sdr, buffer_size);

    Py_DECREF(pName);
    Py_Finalize(); //closing pythonsesson
    
  return 0;
}

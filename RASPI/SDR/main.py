from pylab import *
from rtlsdr import *
import time



centerF=474.8e6 #that is the frequency we are looking at

thresholddB=-20 #in dB
picWidth=1 #in MHz, its the intervall in wich we look for the pic

ttw=3 # time to wait before reseting the clicks

#CREATING THE SDR
sdr = RtlSdr()

sdr.sample_rate = 2.4e6 #width we will look at
sdr.center_freq = centerF+sdr.sample_rate/5 #that will be a pic so we shift it to not disturb the pic we are looking at
sdr.gain = 30

picIsHigh=False #to detect: end off pressing the button on talkie

picIndices=[]
foundPicIndices=False;


samplesNb=128 #number of mesures

highPicsCounter=0
threshold=10**(thresholddB/10)

refTime=time.time() #proc time in seconds
currentTime=time.time()

#INITIALIZE SOME DATAS
samples = sdr.read_samples(1024*256)
mytable = psd(samples, NFFT=samplesNb, Fs=sdr.sample_rate/1e6, Fc=sdr.center_freq/1e6) 


indice=0
while(not(foundPicIndices) and indice<samplesNb) :
    minValue=(centerF/1e6)-(picWidth/2)
    maxValue=(centerF/1e6)+(picWidth/2)
    if minValue<=mytable[1][indice]<=maxValue : #mytable[1] is the list of the frequencies sampled
        picIndices.append(indice)
    indice+=1

##END OF SETUP
##DEFINE FUNCTIONS
def scan(mysdr):
    samples = sdr.read_samples(1024*256)
    table=psd(samples, NFFT=samplesNb, Fs=sdr.sample_rate/1e6, Fc=sdr.center_freq/1e6)
    return table


##MAIN    
if (__name__=="__main__"):
    print("setup done")
    
    myTable=[],[] #two dimension table
    fIndice=0
    
    while True:
    
        myTable=scan(sdr)
        
        if not(picIsHigh): #if the buton was released, we check if it is pressed
            while(not(picIsHigh) and fIndice<len(picIndices)):
                i=picIndices[fIndice]
                if myTable[0][i]>threshold:
                    picIsHigh=True;
                    highPicsCounter+=1
                    refTime=time.time() #reset of refTime
                    print("it was pressed")
                fIndice+=1
                
            fIndice=0
            
        else: #if the button is hold, we wait forit to go down
            picIsHigh=False
            while(not(picIsHigh) and fIndice<len(picIndices)):
                i=picIndices[fIndice]
                if myTable[0][i]>threshold:
                    picIsHigh=True;
                fIndice+=1
            if not(picIsHigh):
                print("it was released")    
            fIndice=0


        if (highPicsCounter>=3):
            print(highPicsCounter)
            highPicsCounter=0
        if(time.time()-refTime>ttw and highPicsCounter>0):
            highPicsCounter=0
            print("RESET")
        
    #xlabel('Frequency (MHz)')
    #ylabel('Relative power (dB)')

    #show()
    
sdr.close()

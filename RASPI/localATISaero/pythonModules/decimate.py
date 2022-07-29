import numpy as np
import scipy.signal as signal


def decimatecplx(vector):
    print(vector[0])

    output= signal.decimate(vector,5) #5 is dec rate , change it
    print("decimated")
    output=np.stack((output.real,output.imag),axis=1)
    print(output[0])
    print(output[-1])
    print(len(output))
    return output


def decimate(vector):

    #this filter is not necessary
    Fs_y=int(1140000/5)
    d = Fs_y * 75e-6   # Calculate the # of samples to hit the -3dB point  
    x = np.exp(-1/d)   # Calculate the decay between each sample  
    b = [1-x]          
    a = [1,-x]  
    x5 = signal.lfilter(b,a,vector)
    #temp=np.random.rand(8000000).astype("complex64")
    output= signal.decimate(x5,5) #4 is dec rate , change it

    #output=np.stack((output,np.zeros(len(output))),axis=1)
    output=np.stack((output,np.zeros(len(output))),axis=1)
    print(len(output))
    print(output)
    return output

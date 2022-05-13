
from pylab import *
from rtlsdr import *




if (__name__=="__main__"):
    sdr = RtlSdr()

    centerF=474.8e6

    # configure device
    sdr.sample_rate = 2.4e6
    sdr.center_freq = centerF+sdr.sample_rate/5
    sdr.gain = 100

    samples = sdr.read_samples(1024*256)
    sdr.close()
    samples=128
    # use matplotlib to estimate and plot the PSD
    
    mytable=psd(samples, NFFT=samples, Fs=sdr.sample_rate/1e6, Fc=sdr.center_freq/1e6)


    print("size[0]")
    print(mytable[0])
    print("size[1]")
    print(mytable[1])
    #xlabel('Frequency (MHz)')
    #ylabel('Relative power (dB)')

    show()

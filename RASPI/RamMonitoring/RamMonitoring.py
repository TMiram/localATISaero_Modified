import os
import time
import matplotlib.pyplot as plt
import numpy as np

flag = True #a flag to get out of the main loop once the time of the process is over

mem_usage= 0.0 #total memory used in percentage
current_iteration=-1 #is the number of element that are currently placed in mem_usage_list
list_flag= True
temp_mem=0.0 #memory used by one program ,in percentage
my_line="" #value of the line readed will be in here


duration_min= float(input("duration of the measure (in minutes): ")) #let the user choose how long the process has to be
start_time= time.time()
last_time_shown=start_time
duration_sec= int((60*duration_min)+0.5) #duration in second for the whole process
period=0.01 #time period in second between two measures,is fable but it limits the number of measures
mem_usage_list=[] #Number f Measures, the last measures might be 0 but it is not a real problem

if os.path.exists("temp.txt") : #remove older file if it exists
	os.remove("temp.txt")

mem_get_command = """ top -bn 1 |grep \"^ \" |awk -F\" \"  \'{print($10);}\'>> temp.txt """  # -bn 1 force the mesure to be made only once. And we send this value into a temporary file


print("Processing...")

while flag:

	current_iteration+=1

	os.system(mem_get_command)
	file = open("temp.txt","r")
	my_line=file.readline()

	for l in file: #for each line , we are getting the values (for each process)
		if (my_line.strip() != "%MEM") and (my_line.strip() != ""): #taking  out odd values and the first line
			temp_mem = float(my_line.strip())
			mem_usage+=temp_mem
		my_line=file.readline()

	mem_usage=float(f'{mem_usage:.3f}') #we truncate the float , no need to have too many numbers
	#The following if, elif, else are just to fill the buffer mem_usage_list and to shift old value to le left of the array

	mem_usage_list.append(mem_usage)
	mem_usage=0

	file.close()
	open("temp.txt","w").close() #We are here erasing old values in temp.txt ,it becomes empty

	#print(mem_usage_list)  #Debug,do not use with large scale list

	#If the duration is expired we stop , else we are showing the remaining time
	if (time.time()-start_time >= duration_sec):
		flag = False
		print("Measures done")
	elif (time.time()-last_time_shown >= 5 ):
		print(str(int(duration_sec-time.time()+start_time)) + "s remaining until completion")
		last_time_shown=time.time()

	time.sleep(period)


#PLOT TIME!

plt.title("MEMORY USE over the last "+str(duration_min)+" mins")
plt.xlabel("time (in s)")
plt.ylabel("MEMORY USE (in %)")
plt.axis([0,duration_sec,0,100])
plt.plot(mem_usage_list)
plt.show()


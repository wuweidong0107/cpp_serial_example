serial: main.cpp serial.cpp
	g++ $^ -o $@ 
clean:
	rm -f serial
# Multiple-Producer-Consumer
A generalization of the Producer-Consumer problem that uses many producers and consumers. It is implemented using **pthreads** 
parallelization library.
### Compile (Suppose pthreads installed)
gcc -pthread -lm timer.c -o timer.out
### Run 
./timer.out

### Compile for Raspberry pi & run
PATH=/home/kitsiosk/Downloads/cross-pi-gcc-6.3.0-2/bin:$PATH
LD_LIBRARY_PATH=/home/kitsiosk/Downloads/cross-pi-gcc-6.3.0-2/lib:$LD_LIBRARY_PATH
arm-linux-gnueabihf-gcc -pthread -lm timer.c -o timer.out
scp timer.out root@192.168.1.11:/root
#rm *o *.so  # Remove any files beforehand
 
gcc -fPIC -Wall -g -O0 -c memory.c
gcc -fPIC -Wall -g -O0 -c implementation.c
gcc -fPIC -shared -o memory.so memory.o implementation.o -lpthread
 
export LD_LIBARY_PATH=`pwd`:"$LD_LIBRARY_PATH"
export LD_PRELOAD=`pwd`/memory.so
export MEMORY_DEBUG=yes

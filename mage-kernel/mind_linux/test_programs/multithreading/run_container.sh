# nthreads=$1

# make test_multithreading
# make test_lock

make clean
make launcher_thread
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
./launcher_thread



nthreads=$1
trace_dir=$2
res_dir=$3

make test_multithreading
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
./test_mltthrd $nthreads $trace_dir $res_dir

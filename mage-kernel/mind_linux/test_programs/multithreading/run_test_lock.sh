nblades=$1
nthreads=$2
res_dir=$3

make test_lock
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
./test_mltthrd $nblades $nthreads $res_dir

cp test_mltthrd exmp_mltthrd
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
#echo 0 | sudo tee /proc/sys/kernel/vsyscall64
./exmp_mltthrd

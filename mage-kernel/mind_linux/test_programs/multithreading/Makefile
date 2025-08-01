CXX := g++
MULTITHREADING := test_mltthrd
LOCK := test_mltthrd
LAUNCHER := launcher_thread

test_multithreading: test_mltthrd.cpp coherence_test.cpp coherence_test.hpp
	$(CXX) test_mltthrd.cpp coherence_test.cpp -o $(MULTITHREADING) -g -lpthread -static

# test_lock: test_lock_main.cpp lock_test.hpp lock_test.cpp mind_sync_util.hpp mind_sync_util.cpp
# 	$(CXX) test_lock_main.cpp lock_test.cpp mind_sync_util.cpp -o $(LOCK) -g -lpthread -static

clean:
	rm $(LAUNCHER)
	# rm $(LOCK)

#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <shared_mutex>

class Counter { 
    public:
        Counter() {}
	Counter(Counter&& a) {
        	std::unique_lock<std::mutex> rhs_lk(a.mtx);
        	target = std::move(a.target);
        	count = std::move(a.count);
	}
	Counter(const Counter& a)
    	{
       		std::unique_lock<std::mutex> rhs_lk(a.mtx);
        	target = a.target;
        	count = a.count;
    	}
    	int target = 0;
    	int count = 0;
    	mutable std::mutex mtx;
};

int main() {
    int max_count = 10000000;
    std::vector<Counter> counters(max_count, Counter());
    #pragma omp parallel
    {
	std::random_device rd;
    	std::mt19937 rng(rd());
    	std::uniform_int_distribution<int> rand(0, max_count - 1);
        #pragma omp for
    	for(int i = 0; i < max_count; i++) {
            counters[i].target = rand(rng);
    	}
    }

    unsigned long start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	
    #pragma omp parallel for shared(counters)
    for(int i = 0; i < counters.size(); i++) {
        int target = counters[i].target;
	std::unique_lock<std::mutex> rhs_lk(counters[target].mtx);
        counters[target].count += 1; 
    }

    unsigned long end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << "Time needed: " << (end-start) << " (ms)" << std::endl;

    int debug_count = 0;
    #pragma omp parallel for reduction(+:debug_count)
    for(int i = 0; i < counters.size(); i++) {
	debug_count += counters[i].count;
    }

    std::cout << debug_count << std::endl;

    return 0;
}

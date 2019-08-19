#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <shared_mutex>

class Account {
public:
    Account() {}
    Account(Account&& a) {
        std::unique_lock<std::mutex> rhs_lk(a.mtx);
        target = std::move(a.target);
        balance = std::move(a.balance);
        reserved = std::move(a.reserved);
        max_balance = std::move(a.max_balance);
    }
    Account(const Account& a)
    {
        std::unique_lock<std::mutex> rhs_lk(a.mtx);
        target = a.target;
        balance = a.balance;
        reserved = a.reserved;
        max_balance = a.max_balance;
    }
    int target = 0;
    int balance = 0;
    int reserved = 0;
    int max_balance = 100;
    mutable std::mutex mtx;
};

int main() {
    int max_count = 10000000;
    std::vector<Account> counters(max_count, Account());
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

    #pragma omp parallel
    {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> rand(0, 10);
        #pragma omp for
        for(int i = 0; i < counters.size(); i++) {
            int target = counters[i].target;
            int transfer = rand(rng);

            int remainder = transfer;
            int amount_over_balance = 0;

            {
                std::unique_lock<std::mutex> lock_to(counters[i].mtx);
                if(counters[i].balance - transfer >= 0) {
                    counters[i].balance -= transfer;
                } else {
                    remainder = counters[i].balance;
                    counters[i].balance = 0;
                }
                counters[i].reserved += remainder;
            }

            {
                std::unique_lock<std::mutex> lock_from(counters[target].mtx);
                if(counters[target].balance + remainder < counters[target].max_balance) {
                    counters[target].balance += remainder;
                } else {
                    amount_over_balance = counters[target].balance + remainder - counters[target].max_balance;
                    counters[target].balance = counters[target].max_balance;
                }
            }

            {
                std::unique_lock<std::mutex> lock_to(counters[i].mtx);
                counters[i].balance += amount_over_balance;
                counters[i].reserved -= remainder;
            }
            //std::lock(counters[target].mtx, counters[i].mtx);

            //std::scoped_lock lck(counters[target].mtx, counters[i].mtx);
        }
    }

    unsigned long end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << "Time needed: " << (end-start) << " (ms)" << std::endl;

    int debug_count = 0;
    #pragma omp parallel for reduction(+:debug_count)
    for(int i = 0; i < counters.size(); i++) {
        debug_count += counters[i].balance;
        if(counters[i].balance < 0) {
            std::cout << "CONSTRAINT VIOLATION (BALANCE)" << std::endl;
        }
        if(counters[i].balance > counters[i].max_balance) {
            std::cout << "CONSTRAINT VIOLATION (MAX_BALANCE)" << std::endl;
        }
        if(counters[i].reserved != 0) {
            std::cout << "CONSTRAINT VIOLATION (RESERVED)" << std::endl;
        }
    }

    std::cout << debug_count << std::endl;

    return 0;
}

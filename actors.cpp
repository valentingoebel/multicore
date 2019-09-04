#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <shared_mutex>

class Message;

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
    void receive(Message& m) {
        //TODO implement receiving functionality
    }
    int target = 0;
    int balance = 0;
    int reserved = 0;
    int max_balance = 100;
    mutable std::mutex mtx;
};


class Actor {
    public:
        Actor() {}
        Actor(Actor&& a) {}
        Actor(const Actor& a) {}
        void receive(Message& m) {
            //TODO implement receiving functionality
        }
        mutable std::mutex mtx;
};

class Message {
    public:
        Account* target;
};

class ActorGroup {
    public:
        ActorGroup() : actors(10000000, Account()) {}
        std::vector<Account> actors; //TODO support multiple types of actors, messages, etc
        std::vector<Message> messages;
        std::vector<Message> local_messages;
        int message_count = 0;

        /*
        actorGroup.process(actor, [] (Actor& a) {
            cout << x * 50 << endl; return x * 100;
        });
        */
        template<typename Functor>
        void process(Account& actor, Functor function)
        {
            std::unique_lock<std::mutex> lock_to(actor.mtx);
            function(actor);
            if(message_count == 0) {
                return;
            }
            process_local_messages();
        }

        bool is_remote_actor(Account* actor) {
            return false;
        }

        void send_message(Message& message) {
            if(is_remote_actor(message.target)) {
                messages.push_back(message);
            } else {
                message_count++;
                local_messages.push_back(message);
            }
        }

        void process_local_messages() {
            for(int i = 0; i < local_messages.size(); i++) {
                Message& m = local_messages[i];
                Account* a = m.target;
                std::unique_lock<std::mutex> lock_to(a->mtx);
                a->receive(m);
            }
            local_messages.clear();
        }

        void process_remaining_messages() {
            for(int i = 0; i < messages.size(); i++) {
                Message& m = messages[i];
                Account* a = m.target;
                std::unique_lock<std::mutex> lock_to(a->mtx);
                a->receive(m);
                //TODO send messages over network.
            }
            message_count = 0;
            messages.clear();
        }
};

int main() {
    int max_count = 10000000;
    ActorGroup actorGroup;

    #pragma omp parallel
    {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> rand(0, max_count - 1);
        #pragma omp for
        for(int i = 0; i < max_count; i++) {
            actorGroup.process(actorGroup.actors[i], [&rand, &rng](Account& account) {
                account.target = rand(rng);
            });
        }
    }

    /*#pragma omp parallel
    {
        #pragma omp for
        for(int i = 0; i < max_count; i++) {
            actorGroup.actors[i].balance += 50;
        }
    }*/

    unsigned long start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    #pragma omp parallel
    {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> rand(0, 10);
        #pragma omp for
        for(int i = 0; i < actorGroup.actors.size(); i++) {
            int target = actorGroup.actors[i].target;
            int transfer = rand(rng);

            int remainder = transfer;
            int amount_over_balance = 0;

            actorGroup.process(actorGroup.actors[i], [&remainder, &transfer](Account& account) {
                if(account.balance - transfer >= 0) {
                    account.balance -= transfer;
                } else {
                    remainder = account.balance;
                    account.balance = 0;
                }
                account.reserved += remainder;
            });

            actorGroup.process(actorGroup.actors[target], [&remainder, &amount_over_balance](Account& target) {
                if(target.balance + remainder < target.max_balance) {
                    target.balance += remainder;
                } else {
                    amount_over_balance = target.balance + remainder - target.max_balance;
                    target.balance = target.max_balance;
                }
            });

            actorGroup.process(actorGroup.actors[i], [&remainder, &amount_over_balance](Account& account) {
                account.balance += amount_over_balance;
                account.reserved -= remainder;
            });
        }
    }

    unsigned long end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << "Time needed: " << (end-start) << " (ms)" << std::endl;

    int debug_count = 0;
    #pragma omp parallel for reduction(+:debug_count)
    for(int i = 0; i < actorGroup.actors.size(); i++) {
        debug_count += actorGroup.actors[i].balance;
        if(actorGroup.actors[i].balance < 0) {
            std::cout << "CONSTRAINT VIOLATION (BALANCE)" << std::endl;
        }
        if(actorGroup.actors[i].balance > actorGroup.actors[i].max_balance) {
            std::cout << "CONSTRAINT VIOLATION (MAX_BALANCE)" << std::endl;
        }
        if(actorGroup.actors[i].reserved != 0) {
            std::cout << "CONSTRAINT VIOLATION (RESERVED)" << std::endl;
        }
    }

    std::cout << debug_count << std::endl;

    return 0;
}

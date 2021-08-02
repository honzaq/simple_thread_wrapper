// testing-app.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <functional>

#include "simple_thread_wrapper.h"
#include "helper.h"

int main()
{
    std::cout << log_time() << " Thread Test\n";

    std::atomic_bool event1 = false;
    std::string aaa = "abc";

    simple_thread test1;
    test1.start(
        std::chrono::seconds(1),
        [&]() -> bool{
            return event1.load();
        },
        [&](simple_thread_context_intf & ctx, const std::string& aaa) {
            std::cout << log_time() << " In the thread " << ctx.was_timeout() << " " << aaa << "\n";
            auto unlock_holder = ctx.unlock();
            if (!ctx.was_timeout() && event1.load()) {
                std::cout << log_time()  << "   event1 signaled" << "\n";
                event1.store(false);
                ctx.set_timeout(std::chrono::seconds(2));
                throw std::bad_function_call();
            }
            unlock_holder.reset();
            //ctx.set_timeout(std::chrono::seconds(2));
        }
        , std::move(aaa)
    );

    std::this_thread::sleep_for(std::chrono::seconds(5));

    event1.store(true);
    test1.notify();
    std::cout << log_time() << " notify signaled" << "\n";

    std::this_thread::sleep_for(std::chrono::seconds(5));

    //std::this_thread::sleep_for(std::chrono::seconds(20));

    std::cout << log_time() << " Stopping...\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

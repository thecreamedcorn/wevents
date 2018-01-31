//
// Created by Administrator on 9/23/2017.
//

#include <iostream>
#include <mutex>
#include <windows.h>
#include <chrono>
#include <string>
#include <thread>

#include "src/w_event.h"
#include "src/w_property.h"

using namespace wevents;
using namespace std::chrono;

std::mutex cout_mutex;

void testWProperty()
    {
    //set some random values
    WProperty<int> value1(5);
    WProperty<int> value2(10);
    WProperty<int> value3(15);

    //bind result1 property to expression involving value1 and value2
    WProperty<int> result1(
            [](int i1, int i2)
                { return (i1 + i2); },
            value1,
            value2
    );

    //bind result2 property to expression involving result1 and value3
    WProperty<int> result2(
            [](int i1, int i2)
                { return i1 + i2 - 20; },
            result1,
            value3
    );

    std::cout << result2.get() << std::endl;

    //value gets incremented
    value1.operate([](int& i) { i++; });
    //value1 = value1.get() + 1; //alternate synax
    std::cout << result2.get() << std::endl;

    //result1 has binding stripped transparently and is set to it's current value plus 1
    result1 = result1.get() + 1;
    std::cout << result2.get() << std::endl;

    //delete value3 to simulate object being destroyed
    //result2's binding to value3 handles the issue transparently
    value3.~WProperty();
    result1 = 30;
    std::cout << result2.get() << std::endl;
    }

class SensativeDataClass : public WSlotObject
    {
private:
    int i;

public:
    SensativeDataClass()
            : i(0)
        {}

    int get() const
        { return i; }

    void inc(const std::string& thread_name, int add)
        {
        //std::this_thread::sleep_for(1000ms);
        for (int num = 0; num < add; num++)
            {
            i++;

            cout_mutex.lock();
            std::cout << thread_name << ": " << i << std::endl;
            cout_mutex.unlock();
            }
        }

    std::mutex mutex;
    };

SensativeDataClass sensative_data_obj;

void blocking_mutex_signal_thread(std::string thread_name, int num)
    {
    WSignal<const std::string&, int> sig;
    connect(sig, &SensativeDataClass::inc, &sensative_data_obj, ConOps().mutex(sensative_data_obj.mutex));
    sig.emit(thread_name, num);

    cout_mutex.lock();
    std::cout << "executed on signal finish" << std::endl;
    cout_mutex.unlock();
    }

void nonblocking_mutex_signal_thread(std::string thread_name, int num)
    {
    WSignal<const std::string&, int> sig;
    connect(sig, &SensativeDataClass::inc, &sensative_data_obj, ConOps().mutex(sensative_data_obj.mutex).blocking(false));
    sig.emit(thread_name, num);

    cout_mutex.lock();
    //std::cout << "executed imediatly after signal emit" << std::endl;
    cout_mutex.unlock();
    }

void test_mutex_event_handling()
    {
    std::thread th2(nonblocking_mutex_signal_thread, "thread 2", 10);
    //std::this_thread::sleep_for(1000ms);
    //std::thread th1(blocking_mutex_signal_thread, "thread 1", 5);
    //th1.join();
    th2.join();
    }


int main()
    {
    test_mutex_event_handling();

    return 0;
    }

#include <thread>
#include <condition_variable>
#include <iostream>
class Gate
{
private:
    std::mutex mtx, condVarMutex, lockMutex;
    std::unique_lock<std::mutex> lk = std::unique_lock<std::mutex>(lockMutex);
    std::condition_variable cv;
    bool IsActivated = true;
public:
    // Blocks the execution of the thread until the Open method is called. 
    // If the Run method was called before this method, then the thread is not blocked.
    // The Close and Open methods are synchronized with each other using a mutex
    void Close()
    {
        mtx.lock();
        if (IsActivated)
        {
            condVarMutex.lock();
            mtx.unlock();
            cv.wait(lk);
            condVarMutex.unlock();
            mtx.lock();
        }
        else
        {
            IsActivated = true;
        }
        mtx.unlock();
    }

    // Causes the thread to continue executing after the Close method. 
    // If called before the Sleep method, then the Sleep method will not block the thread.
    // The Close and Open methods are synchronized with each other using a mutex
    void Open()
    {
        mtx.lock();
        if (condVarMutex.try_lock())
        {
            IsActivated = false;
            condVarMutex.unlock();
        }
        else
        {
            while (condVarMutex.try_lock() != true)
                cv.notify_all();

            condVarMutex.unlock();
        }
        mtx.unlock();
    }
};
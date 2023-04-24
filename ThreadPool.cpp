#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "ThreadGate.h"

namespace sc = std::chrono;


class ParentTask
{
protected:
    int Priority = 1;
public:
    virtual void Call() {};
    int GetPriority() const { return Priority; }
};

struct TaskPriorityComparator
{
    bool operator()(const ParentTask* t1, const ParentTask* t2) const
    {
        return t1->GetPriority() < t2->GetPriority();
    }
};

template <class T>
class Task : public ParentTask
{
private:
    T Func;
public:
    Task(T func, int priority) : Func(func) { Priority = priority; };
    void Call() override { Func(); }
};


class ThreadPool
{
private:
    std::priority_queue<ParentTask*, std::vector<ParentTask*>, TaskPriorityComparator> TasksQueue;

    std::mutex QueueMutex;

    std::vector<std::thread> Threads;
    Gate RunGate;
    std::vector<Gate*> ThreadsGates;
    std::vector<bool> ThreadsFinishedTasks;

    bool IsStopped = false;
    int ThreadsAmount = 0;

    void ThreadQueueHandler(int threadId)
    {
        ParentTask* threadTask;
        std::mutex threadMtx;
        std::unique_lock<std::mutex> threadLock(threadMtx);

        ThreadsGates[threadId]->Close();

        while (true)
        {
            if (IsStopped)
            {
                delete ThreadsGates[threadId];
                return;
            }

            QueueMutex.lock();

            if (!TasksQueue.empty())
            {
                threadTask = TasksQueue.top();
                TasksQueue.pop();
                QueueMutex.unlock();
            }
            else
            {
                ThreadsFinishedTasks[threadId] = true;
                
                bool isAllThreadedEnds = true;
                for (const auto& it : ThreadsFinishedTasks)
                    if (!it)
                        isAllThreadedEnds = false;
                QueueMutex.unlock();

                if (isAllThreadedEnds)
                    RunGate.Open();

                ThreadsGates[threadId]->Close();
                continue;
            }

            threadTask->Call();
            delete threadTask;
        }
    }

public:
    template <class F>
    void AddTask(F task)
    {
        AddTask(task, 1);
    }

    template <class F>
    void AddTask(F task, int priority)
    {
        Task<F>* newTask = new Task<F>(task, priority);
        TasksQueue.push((ParentTask*)newTask);
    }

    template <class F, typename ...Args>
    void AddTask(F& task, Args&&... args)
    {
        AddTask(task, 1, args...);
    }

    template <class F, typename ...Args>
    void AddTask(F& task, int priority, Args&&... args)
    {
        auto func = [&]() { task(args...); };
        Task<decltype(func)>* newTask = new Task<decltype(func)>(func, priority);
        TasksQueue.push((ParentTask*)newTask);
    }

    void SetThreadsAmount(int threadsAmount)
    {
        if (threadsAmount > ThreadsAmount)
        {
            for (int i = ThreadsAmount; i < threadsAmount; ++i)
            {
                ThreadsGates.push_back(new Gate);
                ThreadsFinishedTasks.push_back(false);

                Threads.push_back(std::thread([&, i]() { ThreadQueueHandler(i); }));
            }
        }

        if (threadsAmount < ThreadsAmount)
        {
            IsStopped = true;
            for (int i = --ThreadsAmount; i >= threadsAmount; --i)
            {
                ThreadsGates[i]->Open();
                ThreadsGates.pop_back();
                Threads[i].join();
                Threads.pop_back();
                ThreadsFinishedTasks.pop_back();
            }
            IsStopped = false;
        }

        ThreadsAmount = threadsAmount;
    }

    void Run()
    {
        if (ThreadsAmount == 0)
            return;

        QueueMutex.lock();
        for (int i = 0; i < ThreadsFinishedTasks.size(); ++i)
            ThreadsFinishedTasks[i] = false;
        QueueMutex.unlock();

        for (auto& it : ThreadsGates)
            it->Open();
        
        RunGate.Close();
    }

    ~ThreadPool()
    {
        IsStopped = true;
        for (auto& it : ThreadsGates)
            it->Open();

        for (auto& it : Threads)
            it.join();
    }
};


int main()
{
    std::vector<int> vec;
    for (int i = 0; i < 65536; ++i)
        vec.push_back(rand());

    auto start(std::chrono::high_resolution_clock::now());

    for (int i = 0; i < 1000; ++i)
    {
        for (int i = 0; i < vec.size() / 2; ++i)
        {
            vec[i] = vec[i] * 2 / 12 * 77 / 63;
            vec[vec.size() - 1 - i] = vec[vec.size() - 1 - i] * 3 / 8 * 55 / 23;
        }
    }
    
    auto end(std::chrono::high_resolution_clock::now());
    auto duration(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));

    std::cout << "Time to operations: " << duration.count() / 1000 << std::endl;


    ThreadPool p;
    p.SetThreadsAmount(2);
    
    start = (std::chrono::high_resolution_clock::now());

    p.AddTask([&]()
        {
            for (int j = 0; j < 1000; ++j)
                for (int i = 0; i < vec.size() / 2; ++i)
                    vec[i] = vec[i] * 2 / 12 * 77 / 63;
        });

    p.AddTask([&]()
        {
            for (int j = 0; j < 1000; ++j)
                for (int i = 0; i < vec.size() / 2; ++i)
                    vec[vec.size() - 1 - i] = vec[vec.size() - 1 - i] * 3 / 8 * 55 / 23;
        });


        p.Run();
    

    end = (std::chrono::high_resolution_clock::now());
    duration = (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));

    std::cout << "Time to operations: " << duration.count() / 1000 << std::endl;

    start = (std::chrono::high_resolution_clock::now());

    for (int j = 0; j < 1000; ++j)
    {
        p.AddTask([&]()
            {
                for (int i = 0; i < vec.size() / 2; ++i)
                    vec[i] = vec[i] * 2 / 12 * 77 / 63;
            });

        p.AddTask([&]()
            {
                for (int i = 0; i < vec.size() / 2; ++i)
                    vec[vec.size() - 1 - i] = vec[vec.size() - 1 - i] * 3 / 8 * 55 / 23;
            });

        p.Run();
    }

    end = (std::chrono::high_resolution_clock::now());
    duration = (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));

    std::cout << "Time to operations: " << duration.count() / 1000 << std::endl;
}
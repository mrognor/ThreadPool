#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>

#include <Windows.h>

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
    std::vector<std::thread> Threads;
    std::mutex mtx;

    void ThreadQueueHandler(int threadId)
    {
        ParentTask* threadTask;
        while (true)
        {
            mtx.lock();
            if (!TasksQueue.empty())
            {
                threadTask = TasksQueue.top();
                TasksQueue.pop();
                mtx.unlock();
            }
            else 
            {
                mtx.unlock();
                return;
            }

            std::cout << "Priority: " << threadTask->GetPriority() << " ";

            threadTask->Call();
            delete threadTask;
        }

    }

public:
    template <class F>
    void AddTask(F task, int priority)
    {
        Task<F>* newTask = new Task<F>(task, priority);
        TasksQueue.push((ParentTask*)newTask);
    }

    template <class F, typename ...Args>
    void AddTask(F& task, int priority, Args&&... args)
    {
        auto func = [&]() { task(args...); };
        Task<decltype(func)>* newTask = new Task<decltype(func)>(func, priority);
        TasksQueue.push((ParentTask*)newTask);
    }

    void Run(int threadsAmount)
    {
        for (int i = Threads.size(); i < threadsAmount; ++i)
            Threads.push_back(std::thread([&]() { ThreadQueueHandler(i); }));

        for (auto& it : Threads)
            it.join();
    }

    ~ThreadPool()
    {
        while (!TasksQueue.empty())
        {
            delete TasksQueue.top();
            TasksQueue.pop();
        }
    }
};


void FuncA()
{
    Sleep(3000);
    std::cout << "This is FuncA" << std::endl;
}

void FuncB()
{
    std::cout << "This is FuncB" << std::endl;
}

void FuncC(int a, int b)
{
    std::cout << a + b << std::endl;
}

void FuncD(int& d)
{
    d = 1111;
}

void FuncE(int* d)
{
    *d = 2222;
}

template<class T>
void FuncF(T a)
{
    std::cout << "Template: " << a << std::endl;
}

int main()
{
    ThreadPool pool;
    int N1 = 1, N2 = 1;

    pool.AddTask(FuncA, 2);

    pool.AddTask([]() { std::cout << "This is Lambda func" << std::endl; }, 1);

    pool.AddTask(FuncC, 3, 9, 9);

    pool.AddTask([]() { FuncC(2, 5); }, 1);

    pool.AddTask(FuncB, 1);

    pool.AddTask(FuncD, 1, N1);
    pool.AddTask(FuncE, 6, &N2);

    pool.AddTask(FuncF<int>, 1, 12);

    pool.Run(2);

    std::cout << N1 << std::endl;
    std::cout << N2 << std::endl;
}
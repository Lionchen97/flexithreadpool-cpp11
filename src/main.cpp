#include "threadpool.h"
#include <chrono>

using Ulong = unsigned long long;
class MyTask : public Task
{
public:
    MyTask(Ulong begin, Ulong end)
        : begin_(begin), end_(end)
    {
    }
    // 问题一：怎么设计run函数的返回值，可以表示任意的类型（模板类和虚函数不能一起用） C++17 Any类型类似于Object是所有其他类型的基类。

    Any run() // run方法最终就在线程池分配的线程中做执行了
    {
        std::cout << "begin threadFunc tid: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        Ulong sum = 0;
        for (Ulong i = begin_; i <= end_; i++)
        {
            sum += i;
        }
        std::cout << "end threadFunc tid: " << std::this_thread::get_id() << std::endl;
        return sum;
    }

private:
    Ulong begin_;
    Ulong end_;
};
int main()
{
    {
        ThreadPool pool;
        // 用户自己设置线程池的工作模式
        pool.setPoolMode(PoolMode::MODE_CACHED);
        // 开始启动线程池
        pool.start(4);
        // Master -Slave线程模型
        // Master线程用来分解任务，然后给各个Slave线程分配任务
        // 等待各个Slave线程执行完任务结果，输出
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        Result res4 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        Result res5 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        Result res6 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        // 用户线程，涉及线程的通信，如果用户过早去取结果，而线程函数还未执行完毕，则需要阻塞。
        // 任务失败，则不需要阻塞。
        // 随着task被执行完，task对象没了，依赖于task对象的Result对象也没了
        Ulong sum1 = res1.get().cast_<Ulong>();
        Ulong sum2 = res2.get().cast_<Ulong>();
        Ulong sum3 = res3.get().cast_<Ulong>();
        std::cout << sum1 + sum2 + sum3 << std::endl;
    } // 这里Result对象会析构 在VS中
    std::cout << "main over" << std::endl;
    getchar();
}
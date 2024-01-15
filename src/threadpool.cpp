#include "threadpool.h"

////////////////////////////////////// 线程池方法实现

// 线程池的构造
ThreadPool::ThreadPool()
    : initThreadSize_(0), taskSize_(0), taskQueMaxThreshHold_(TASK_MAX_THRESHOLD), threadSizeThreshHold_(THREAD_MAX_THRESHHOLD), curThreadSize_(0), poolMode_(PoolMode::MODE_FIXED), isPoolRunning_(false), threadIdelSize_(0)
{
}

// 线程池的析构
ThreadPool::~ThreadPool()
{
    isPoolRunning_ = false;

    // 等待线程池中所有的线程返回（系统线程：阻塞&正在运行中）
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all(); // 唤醒所有阻塞的线程
    exitCond_.wait(lock, [&]() -> bool
                   { return threads_.size() == 0; }); // 主线程阻塞
}

// 设置线程池的工作模式
void ThreadPool::setPoolMode(PoolMode mode)
{
    if (checkRunningState())
        return;
    poolMode_ = mode;
}

// 设置task任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold(size_t threshhold)
{
    if (checkRunningState())
        return;
    taskQueMaxThreshHold_ = threshhold;
}

// 设置线程池cached模式下的线程上限阈值
void ThreadPool::setThreadMaxThreshHold(size_t threshhold)
{
    if (checkRunningState())
        return;
    if (poolMode_ == PoolMode::MODE_CACHED)
    {
        threadSizeThreshHold_ = threshhold;
    }
}

// 给线程池提交任务，用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    /*
    满足1：线程的通信 等待任务队列有空余
        wait 等待，直到满足条件。
        写法1：
        while(taskQue_.size()==taskQueMaxThreshHold_)
        {
            notFull_.wait(lock); // 阻塞状态
        }
        写法2：
        notFull_.wait(lock,[&]()->bool{return taskQue_.size() <taskQueMaxThreshHold_;});
    满足2：用户提交任务，最长不能阻塞超过1s，否则判断任务提交失败，返回
        wait_for 最多等待一段时间 wait_until等待到一个时间点，且都有返回值。
    */
    if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool
                           { return taskQue_.size() < taskQueMaxThreshHold_; }))
    {
        // false，表示not_Full_等待1s钟，条件依然没有满足
        std::cerr << "task queue is full, sumbit task failed." << std::endl;
        // return task->getResult(); // 不允许的操作，因为线程函数执行完任务，任务就被析构了

        /* 
        返回值优化 (RVO)：编译器可以构造返回值直接在接收对象的内存位置，而不是在函数内部构造然后移动或拷贝到外部。由于 C++17 的保证拷贝省略，编译器直接在调用者期望的位置构造这个对象，而不是先在函数内构造然后通过移动构造函数移动到调用者那里。这意味着即使你定义了移动构造函数，它也可能不会被调用。 
        C++11，需要禁用拷贝构造，重写移动构造。
        */
        return Result(sp, false);
    }

    // 如果有空余，把任务放入任务队列中
    taskQue_.emplace(sp);
    taskSize_++;

    // 因为放了新任务，任务队列不为空，在notEmpty_上通知，赶快分配执行任务。
    notEmpty_.notify_all();

    /* 需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来
        cached模式 任务处理比较紧急 场景：小而快的任务*/
    if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > threadIdelSize_ && curThreadSize_ < threadSizeThreshHold_)
    {
        std::cout << ">>> create new thread..." << std::endl;
        // 创建新的线程对象
        // auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1)); // C++14
        std::unique_ptr<Thread> ptr(new Thread(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1)));




        size_t threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
        // 启动新增的线程
        threads_[threadId]->start();
        // 修改线程个数相关的变量
        curThreadSize_++;
        threadIdelSize_++;
    }

    // 返回任务的Result对象
    std::cout << "succseeful submition !" << std::endl;
    return Result(sp); // 调用右值拷贝
}

// 开启线程池，创建线程，为每个线程分配线程函数。
void ThreadPool::start(size_t initThreadSize)
{
    // 设置线程池的运行状态
    isPoolRunning_ = true;
    // 记录初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;
    // 创建线程对象的时候，把线程函数给到thread线程对象
    for (int i = 0; i < initThreadSize_; i++)
    {
        // 创建新线程
        // auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1)); // C++14

        std::unique_ptr<Thread> ptr(new Thread(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1))); // c++11

        // std::unique_ptr<Thread> ptr(new Thread(std::bind(&ThreadPool::threadHandler, this))); // C++11
        size_t threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
        // threads_.emplace_back(std::move(ptr)); //  emplace_back会进行拷贝，而unique_ptr不支持拷贝
    }

    // 启动所有对象
    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start(); // 需要执行一个线程函数
        threadIdelSize_++;    // 记录初始空闲线程的数量
    }
}
// 定义线程函数
void ThreadPool::threadHandler(size_t threadid)
{
    auto lastTime = std::chrono::high_resolution_clock().now();
    // 所有任务必须执行完成，线程池才可以回收所有线程资源
    for (;;)
    {
        std::shared_ptr<Task> task; // 延长task的生命周期
        {

            /*先获取锁（线程会发生死锁，和析构的主线程争夺同一把锁）
            线程池析构时：
            1.如主线程先获得锁，进入wait状态，释放锁；由于线程没有任务，也会进入wait状态，此时就没有线程进行notify
            2.线程池线程先获得锁
             */
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            std::cout << "tid: " << std::this_thread::get_id() << " try to get the task ..." << std::endl;
            // cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该回收多余的线程。
            // 当前时间-上次线程执行时间
            while (taskQue_.size() == 0)
            {
                // 线程池结束
                if (!isPoolRunning_)
                {
                    threads_.erase(threadid); // 自己生成的线程id
                    std::cout << "threadid: " << std::this_thread::get_id() << " exit!" << std::endl;
                    exitCond_.notify_all(); // 通知主线程
                    return;
                }
                // MODE_CACHED模式：开始回收空闲线程
                if (poolMode_ == PoolMode::MODE_CACHED)
                {
                    // 超时返回
                    if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                    {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (dur.count() >= THREAD_IDLE_TIME && curThreadSize_ > initThreadSize_)
                        {
                            // 把线程对象从线程列表容器中删除
                            threads_.erase(threadid); // 自己生成的线程id
                            // 记录线程数量的相关变量的值修改
                            curThreadSize_--;
                            threadIdelSize_--;
                            std::cout << "空闲线程："
                                      << "threadid: " << std::this_thread::get_id() << " exit!" << std::endl;
                            return;
                        }
                    }
                }
                else
                {
                    notEmpty_.wait(lock); // 最终问题点
                }
                //  // 线程池关闭，回收线程资源
                // if (!isPoolRunning_)
                // {
                //     std::cout<<"剩余线程："<<threads_.size()<<std::endl;
                //     // 把线程对象从线程列表容器中删除
                //     threads_.erase(threadid); // 自己生成的线程id
                //     // 记录线程数量的相关变量的值修改
                //     // curThreadSize_--;
                //     // threadIdelSize_--;
                //     std::cout << "threadid: " << std::this_thread::get_id() << " exit!" << std::endl;
                //     exitCond_.notify_all(); // 通知主线程
                //     return; // 结束线程函数，就是结束当前线程。
                // }
            }

            threadIdelSize_--;
            std::cout << "tid: " << std::this_thread::get_id() << " got the task.." << std::endl;
            // 从任务队列中取一个任务
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            // 如果依然有剩余任务，继续通知其他线程执行任务
            if (taskQue_.size() > 0)
            {
                notEmpty_.notify_all();
            }
            // 取出一个任务，进行通知，任务队列不为满，可以继续提交生产任务
            notFull_.notify_all();
        } // 让锁释放

        // 当前线程负责执行这个任务，不能等任务执行完才释放锁
        if (task != nullptr)
        {
            // task->run();
            //  执行任务，并把任务的返回值给setVal

            task->exec();
        }
        threadIdelSize_++;
        lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
    }
}
bool ThreadPool::checkRunningState() const
{
    return isPoolRunning_;
}
////////////////////////////////////// 线程方法实现
// 线程构造函数
size_t Thread::generateId_ = 0;
Thread::Thread(ThreadFunc func)
    : func_(func), threadId_(generateId_++)
{
}

// 线程析构函数
Thread::~Thread(){};

// 启动线程
void Thread::start()
{
    /*  创建一个线程来执行线程函数 线程池的所有线程从任务队列里面消费任务
      线程对象t 线程函数func_(C++11)pthread_detach(Linux) */
    std::thread t(func_, threadId_);

    /*  出了作用域线程对象会析构，为了保护线程线程函数，需要设置为分离线程 pthread_detach (Linux) 如果需要回收线程的pcb需要主线程执行join */
    t.detach();
}

// 获取线程Id
size_t Thread::getId() const
{
    return threadId_;
}
///////////////////////////////////////// Task方法的实现
Task::Task()
    : result_(nullptr)
{
}

void Task::exec()
{
    if (result_ != nullptr) // 增加安全性
    {
        result_->setVal(run()); // 这里发生多态
    }
}

void Task::setResult(Result *res)
{
    result_ = res;
}

///////////////////////////////////// Result方法的实现
Result::Result(Result &&other) : task_(std::move(other.task_)),  // 移动 shared_ptr
                                 isValid_(other.isValid_.load()) // 复制 atomic_bool 的值
{
    // other 对象的状态已被修改，因此可以显式地设置它的成员变量
    // 对于 std::atomic_bool，这不是必需的，但可以显式地设置为默认值
    other.isValid_ = false;
}

Result::Result(std::shared_ptr<Task> task, bool isValid)
    : task_(task), isValid_(isValid)
{
    task_->setResult(this); // 构造时，把当前的Result对象绑定给task_对象
}

// setVal方法，获取任务执行完的返回值
void Result::setVal(Any any)
{
    // 存储task的返回值
    this->any_ = std::move(any);
    sem_.post(); // 已经获取的任务的返回值，增加信号量资源
}

// get方法，用户调用这个方法获取task的返回值
Any Result::get()
{
    if (!isValid_)
    {
        return "";
    }
    sem_.wait();            // task任务如果没有执行完，这里会阻塞用户的线程
    return std::move(any_); // Any不允许拷贝构造
}
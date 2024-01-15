// #ifdef THREADPOOL_H
// #define THREADPOOL_H
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <iostream>
#include <unordered_map>

// 参数设置
const size_t TASK_MAX_THRESHOLD = INT32_MAX;
const size_t THREAD_MAX_THRESHHOLD = 1024;
const size_t THREAD_IDLE_TIME = 2; // 单位：秒

// 线程池支持的模式
enum class PoolMode // C++防止不同枚举类型，但是枚举项同名
{
    MODE_FIXED,  // 固定数量的线程
    MODE_CACHED, // 线程数量可动态增长
};
/* 模板类需要先进行实例化才能使用,实例化的过程需要模板的定义。如果模板类定义在源文件中,使用时编译器无法访问其定义。 */

// 仿C++17 Any类型：可以接受任意数据类型
class Any
{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any &) = delete;
    Any &operator=(const Any &) = delete;
    Any(Any &&) = default;
    Any &operator=(Any &&) = default;

    // 这个构造函数可以让Any类型接收任意其它的数据
    template <typename T> // T:int Derive<int>
    // Any(T data) : base_(std::make_unique<Derive<T>>(data)){}; // c++14
    Any(T data): base_(new Derive<T>(data)){}  // c++11
    // 这个方法能把Any对象里面存储的data数据提取出来
    template <typename T>
    T cast_()
    {
        // 我们怎么从base_找到它所指向的Derive对象，从他里面取出data成员变量
        // 基类指针 =》派生类指针 RTTI类型识别
        Derive<T> *pd = dynamic_cast<Derive<T> *>(base_.get()); // 取出裸指针
        if (pd == nullptr)                                      // 如 T:int 但是用户传入了非int
        {
            throw "type is unmatch!";
        }
        return pd->data_;
    }

private:
    class Base
    {
    public:
        /* 如果不将基类析构函数设为虚函数,那么删除派生类对象时,只会调用基类的析构函数,导致内存泄露和未定义行为。 */
        virtual ~Base() = default; // 如果是默认实现，建议使用default会得到编译器优化
    };
    template <typename T>
    class Derive : public Base
    {
    public:
        Derive(T data) : data_(data) {}
        T data_; // 保存了任意其他类型
    };

private:
    // 定义一个基类指针
    std::unique_ptr<Base> base_;
};

// 实现一个信号量
class Semaphore
{
public:
    Semaphore(size_t limit = 0) : resLimit_(limit), isExit_(false) {}
    ~Semaphore()
    {
        isExit_ = true;
    }
    // 获取一个信号量资源
    void wait()
    {
        if (isExit_)
            return;
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待信号量有资源，没有资源的话，会阻塞当前线程
        cond_.wait(lock, [&]() -> bool
                   { return resLimit_ > 0; });
        resLimit_--;
    }

    // 增加一个信号量资源
    void post()
    {
        if (isExit_)
            return;
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        cond_.notify_all();
    }

private:
    size_t resLimit_;
    std::atomic_bool isExit_; // 为了Linux环境下的Result析构能够正常
    std::mutex mtx_;
    std::condition_variable cond_;
};

// Task类型的前置声明
class Task;
class Result
{
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;
    Result(const Result &) = delete;
    Result &operator=(const Result &) = delete;
    Result(Result &&other);
     
    Result &operator=(Result &&) = default;
    // setVal方法，获取任务执行完的返回值
    void setVal(Any any);
    // get方法，用户调用这个方法获取task的返回值
    Any get();

private:
    Any any_;                    // 存储任务的返回值，已经初始化了
    Semaphore sem_;              // 线程通信信号量，已经初始化了
    std::shared_ptr<Task> task_; // 指向对应获取返回值的任务对象
    std::atomic_bool isValid_;   // 判断返回值是否有效
};

// 任务抽象基类
class Task
{
public:
    Task();
    ~Task() = default;
    void exec();
    void setResult(Result *res);
    virtual Any run() = 0;

private:
    Result *result_; // Result对象的生命周期>Task对象
};

// 线程类型
class Thread
{
public:
    // 定义线程函数对象类型
    using ThreadFunc = std::function<void(size_t)>;
    // 线程构造函数
    Thread(ThreadFunc func);
    // 线程析构函数
    ~Thread();
    // 启动线程
    void start();
    // h获取线程id
    size_t getId() const;

private:
    ThreadFunc func_;
    static size_t generateId_; // 自定义线程id
    size_t threadId_;          // 保存线程id
};
/*
example:
ThreadPool pool;
pool.start(4);

class Mytask : public Task
{
    public:
        void run(){ // task coding ...}
};

pool.submitTask(std::make_shared<MyTask>());
*/

// 线程池类型
class ThreadPool
{
public:
    // 线程池构造
    ThreadPool();
    // 线程池析构
    ~ThreadPool();

    // 设置线程池的工作模式
    void setPoolMode(PoolMode mode);

    // 设置task任务队列上限阈值
    void setTaskQueMaxThreshHold(size_t threshhold);

    // 设置线程池cached模式下的线程上限阈值
    void setThreadMaxThreshHold(size_t threshhold);

    // 给线程池提交任务
    Result submitTask(std::shared_ptr<Task> sp);

    // 指定初始化线程数量，并开启线程池
    void start(size_t initThreadSize = std::thread::hardware_concurrency());

    // 禁止拷贝、赋值
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // 定义线程函数
    void threadHandler(size_t threadId);
    // 检查线程池的运行状态
    bool checkRunningState() const;
    // std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
    std::unordered_map<size_t, std::unique_ptr<Thread>> threads_; // 线程列表
    size_t initThreadSize_;                                       // 初始的线程数量（无符号整形）
    std::atomic_size_t curThreadSize_;                            // 记录当前线程池中线程的总数量
    std::atomic_size_t threadIdelSize_;                           // 记录空闲线程数量
    size_t threadSizeThreshHold_;                                 // 线程数量上限阈值
    std::queue<std::shared_ptr<Task>> taskQue_;                   // 任务队列（考虑到用户可能传入临时变量，生命周期不够长的变量）
    std::atomic_size_t taskSize_;                                 // 任务的数量（无符号原子整形）
    size_t taskQueMaxThreshHold_;                                 // 任务队列数量的上限阈值
    std::mutex taskQueMtx_;                                       // 保证任务队列的线程安全
    std::condition_variable notFull_;                             // 表示任务队列不满
    std::condition_variable notEmpty_;                            // 表示任务队列不空
    std::condition_variable exitCond_;                            // 等待线程资源全部回收
    PoolMode poolMode_;                                           // 当前线程池的工作模式
    std::atomic_bool isPoolRunning_;                              // 表示当前线程池的启动状态
};

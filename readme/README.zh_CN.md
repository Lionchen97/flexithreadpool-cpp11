# FlexiThreadPool

## 项目介绍

FlexiThreadPool是一个高度灵活且功能丰富的 C++ 线程池库，它为并发任务处理提供了一个高效、可扩展的解决方案，特别适用于需要处理大量异步任务的场景。用户可以自定义创建线程、管理线程，并让它处理任何你想实现的任务。

## 运行环境
C++标准：C++11及以上，GNU：4.5及以上，CMake：2.8及以上，操作系统: Linux/Win

## 知识背景

- 熟练基于C++ 11标准的面向对象编程 

  组合和继承、继承多态、STL容器、智能指针、函数对象、绑定器、可变参模板编程等。

- 熟悉C++11多线程编程

   thread、mutex、atomic、condition_variable、unique_lock等。

- C++17和C++20标准的内容

   C++17的any类型和C++20的信号量semaphore，该项目用C++11模拟实现。 

- 熟悉多线程理论 

  多线程基本知识、线程互斥、线程同步、原子操作、CAS等。

## 主要特点

- **可配置模式**：支持固定模式（`MODE_FIXED`）和缓存模式（`MODE_CACHED`），允许根据应用需求灵活配置线程池行为。
- **动态任务处理**：通过一个队列管理提交的任务，提供线程安全的任务提交和执行机制。
- **异步结果获取**：任务提交者可以获取一个 `Result` 对象，用于异步获取任务执行结果。
- **资源管理优化**：自动回收长时间空闲的线程，优化资源使用。
- **线程同步**：使用条件变量和互斥量实现线程间的同步。
- **通用结果存储**：通过 `Any` 类型封装，支持返回不同类型的任务结果。
- **跨平台兼容性**：项目构建配置（通过 CMake）兼容 Linux 和 Windows，确保在不同操作系统上的可移植性。

## 技术挑战

- 实现高效的线程同步机制，确保线程安全。
- 设计通用的结果返回机制，使其能够适应不同类型的任务。
- 管理动态增长的线程和任务队列，优化性能和资源利用。

## 技术细节

#### `Any` 类

- **目的**：存储任何类型的数据。
- 关键方法：
  - `Any(T data)`: 模板构造函数，接受任何数据类型。
  - `T cast_()`: 模板方法，用于提取存储的数据，使用 RTTI（运行时类型信息）进行类型检查。

#### `Semaphore` 类

- **目的**：使用信号量模式管理固定数量的资源。
- 关键方法：
  - `void wait()`: 减少信号量计数，如果没有资源则阻塞。
  - `void post()`: 增加信号量计数并通知被阻塞的线程。

#### `Task` 类

- **目的**：任务的抽象基类，由线程池执行。
- **关键方法**：`virtual Any run() = 0`: 纯虚方法，用于任务执行。

#### `Result` 类

- **目的**：存储任务的结果。
- 关键方法：
  - `void setVal(Any any)`: 存储任务执行的结果。
  - `Any get()`: 检索存储的结果，如果任务尚未完成则阻塞。

#### `ThreadPool` 类

- **目的**：管理线程池以执行任务。
- 关键方法：
  - `Result submitTask(std::shared_ptr<Task> sp)`: 提交执行任务。
  - `void start(size_t initThreadSize)`: 使用指定数量的线程初始化线程池。
  - `void threadHandler(size_t threadId)`: 每个线程执行的函数，处理任务执行和生命周期。

#### `Thread` 类

- **目的**：表示池中的线程。
- 关键方法：
  - `void start()`: 开始线程执行。

## 使用说明

### 1. 启动设置

```c++
// "threadpool.h"
// 可选模式
enum class PoolMode
{
    MODE_FIXED, 
    MODE_CACHED,
};
// 设置模式
void ThreadPool::setPoolMode(PoolMode mode);
// 设置初始线程
void start(size_t initThreadSize = std::thread::hardware_concurrency());
```

#### fixed模式(默认模式)

线程池里面的线程个数是固定，默认根据当前机器的CPU核心数量进行指定。

#### cached模式

线程池里面的线程个数是可动态增长的，根据任务的数量动态的增加线程的数量，但是会设置一个线程数量的阈值，任务处理完成，如果动态增长的线程空闲一段时间还没有处理其它任务，那么关闭线程，保持池中最初数量的线程。

#### 示例

```c++
#include "threadpool.h"
int main()
{
    Threadpool pool;
    pool.setPoolMode(PoolMode::MODE_FIXED); // 设置为cached模式
    pool.start(4);  // 设置4个初始线程
    /*
    code
    */
}
```

### 2. 其他参数

```c++
// "threadpool.h"
const size_t TASK_MAX_THRESHOLD = INT32_MAX;
const size_t THREAD_MAX_THRESHHOLD = 1024;
const size_t THREAD_IDLE_TIME = 2; // 单位：秒
```

- TASK_MAX_THRESHOLD 设置任务队列最大任务数量
- THREAD_MAX_THRESHHOLD 设置cached模式线程上限阈值
- THREAD_IDLE_TIME 设置cached模式线程最大空闲时间

### 3. 设置并提交任务

```c++
#include "threadpool.h"
ThreadPool pool;
pool.start(4);

// 创建Task的继承类，在run()方法中写任务
class Mytask : public Task
{
    public:
        void run(){ // task coding ...}
};

pool.submitTask(std::make_shared<MyTask>());
```

### 4. 完整示例

**Example:**  Master -Slave线程模型实现1到300000000的加法

```cpp
// "main.cpp"
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
        
        pool.setPoolMode(PoolMode::MODE_CACHED);
      
        pool.start(4);
        
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
      
        Ulong sum1 = res1.get().cast_<Ulong>();
        Ulong sum2 = res2.get().cast_<Ulong>();
        Ulong sum3 = res3.get().cast_<Ulong>();
        
        std::cout << sum1 + sum2 + sum3 << std::endl;
    }
    std::cout<<"main over"<<std::endl;
    getchar();
}
```

### 5. 编译
#### Window/Linux
##### CMake

```shell
$ mkdir build && cd build
$ cmake ..
$ make
```
#### Linux
#####  1). 直接编译
```shell
$ g++ -o main ./src/*.cpp -pthread -I ./inc -std=c++11
```
##### 2). 动态库
##### 生成动态库

```shell
$ mkdir lib
$ g++ -shared -fPIC -o lib/libtdpool.so src/threadpool.cpp -Iinc -std=c++11 -pthread
```

##### 安装库到标准位置并更新系统库缓存

```shell
# 1. 存储动态库，头文件（用于编译）
$ mv libtdpool.so /usr/local/lib
$ mv ./inc/threadpool.h /usr/local/include/ 
# 2. 编译
$ g++ -o main ./src/main.cpp -std=c++11 -ltdpool -lpthread
# 3. 添加自定义动态库路径
$ cd /etc/ld.so.conf.d
 # 创建新的配置文件
$ vim mylib.conf
 # 写入动态库路径
 /usr/local/lib
# 4. 刷新缓存
$ ldconfig
```

##### 使用本地库和头文件

```shell
$ g++ -o main src/main.cpp -Iinc -Llib -ltdpool -pthread -std=c++11
```

## 待升级

### 1. 使用packed_task封装任务，让线程池提交任务更加方便

### 2. 使用future来代替Result节省线程池代码

**C++20 版本：**

## 项目输出

### 应用到项目中

- 高并发网络服务器 

  处理io线程 void io_thread 接收listenfd 网络连接事件

  worker线程 void thread(int clientfd)处理已连接用户的读写线程

- master-slave线程模型 

  耗时计算

- 耗时任务处理

  文件传输 

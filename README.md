# FlexiThreadPool

## Project Introduction

FlexiThreadPool is a highly flexible and feature-rich C++ thread pool library that offers an efficient and scalable solution for concurrent task processing. It is particularly suitable for scenarios requiring handling a large number of asynchronous tasks. Users can customize thread creation and management, and use it to execute any task they wish to implement.

## README.md

- zh_CN [简体中文](readme/README.zh_CN.md)
- en [English](README.md)

## Operating Environment

- C++ Standard: C++11 and above
- GNU: 4.5 and above
- CMake: 2.8 and above
- Operating Systems: Linux/Win

## Knowledge Background

- Proficient in object-oriented programming based on the C++ 11 standard, including composition and inheritance, polymorphism, STL containers, smart pointers, function objects, binders, variadic template programming, etc.
- Familiar with C++11 multi-threading programming, including thread, mutex, atomic, condition_variable, unique_lock, etc.
- Content from C++17 and C++20 standards, like the any type from C++17 and the semaphore from C++20, which are simulated in this project using C++11.
- Familiar with multi-threading theory, including basic knowledge of multi-threading, thread mutex, thread synchronization, atomic operations, CAS, etc.

## Main Features

- **Configurable Modes**: Supports fixed (`MODE_FIXED`) and cached (`MODE_CACHED`) modes, allowing flexible configuration of the thread pool behavior according to application needs.
- **Dynamic Task Processing**: Manages submitted tasks through a queue, providing a thread-safe task submission and execution mechanism.
- **Asynchronous Result Retrieval**: Task submitters can obtain a `Result` object to asynchronously retrieve the results of task execution.
- **Resource Management Optimization**: Automatically recycles long-idle threads, optimizing resource usage.
- **Thread Synchronization**: Implements inter-thread synchronization using condition variables and mutexes.
- **Generic Result Storage**: Encapsulates results using the `Any` type, supporting tasks that return various data types.
- **Cross-Platform Compatibility**: The project's build configuration (through CMake) is compatible with both Linux and Windows, ensuring portability across different operating systems.

## Technical Challenges

- Implement an efficient thread synchronization mechanism to ensure thread safety.
- Design a generic mechanism for result returns that can adapt to different types of tasks.
- Manage dynamically growing threads and task queues to optimize performance and resource utilization.

## Technical Details

#### `Any` Class

- **Purpose**: To store data of any type.
- Key Methods:
  - `Any(T data)`: A template constructor that accepts any data type.
  - `T cast_()`: A template method used to extract the stored data, with RTTI (Run-Time Type Information) for type checking.

#### `Semaphore` Class

- **Purpose**: To manage a fixed number of resources using a semaphore pattern.
- Key Methods:
  - `void wait()`: Decreases semaphore count; blocks if no resources are available.
  - `void post()`: Increases semaphore count and notifies blocked threads.

#### `Task` Class

- **Purpose**: An abstract base class for tasks to be executed by the thread pool.
- **Key Method**: `virtual Any run() = 0`: A pure virtual method for task execution.

#### `Result` Class

- **Purpose**: To store the result of a task.
- Key Methods:
  - `void setVal(Any any)`: Stores the result of task execution.
  - `Any get()`: Retrieves the stored result; blocks if the task is not yet complete.

#### `ThreadPool` Class

- **Purpose**: To manage a pool of threads for executing tasks.
- Key Methods:
  - `Result submitTask(std::shared_ptr<Task> sp)`: Submits a task for execution.
  - `void start(size_t initThreadSize)`: Initializes the thread pool with a specified number of threads.
  - `void threadHandler(size_t threadId)`: The function executed by each thread, handling task execution and lifecycle.

#### `Thread` Class

- **Purpose**: Represents a thread in the pool.
- Key Methods:
  - `void start()`: Starts the thread execution.

## Usage Instructions

### 1. Setup

```c++
// "threadpool.h"
// Available modes
enum class PoolMode
{
    MODE_FIXED, 
    MODE_CACHED,
};
// Set mode
void ThreadPool::setPoolMode(PoolMode mode);
// Set initial threads
void start(size_t initThreadSize = std::thread::hardware_concurrency());
```

#### Fixed mode (default mode)

The number of threads in the pool is fixed, by default determined by the number of CPU cores of the machine.

#### Cached mode

The number of threads in the pool can grow dynamically according to the number of tasks, but a threshold for the number of threads is set. If threads created dynamically are idle for a certain time without processing other tasks, they will be closed, maintaining the original number of threads in the pool.

#### Example

```c++
#include "threadpool.h"
int main()
{
    Threadpool pool;
    pool.setPoolMode(PoolMode::MODE_FIXED); // Set to cached mode
    pool.start(4);  // Set 4 initial threads
    /*
    code
    */
}
```

### 2. Other Parameters

```c++
// "threadpool.h"
const size_t TASK_MAX_THRESHOLD = INT32_MAX;
const size_t THREAD_MAX_THRESHHOLD = 1024;
const size_t THREAD_IDLE_TIME = 2; // In seconds
```

- TASK_MAX_THRESHOLD sets the maximum number of tasks in the queue.
- THREAD_MAX_THRESHHOLD sets the upper limit of threads in cached mode.
- THREAD_IDLE_TIME sets the maximum idle time for threads in cached mode.

### 3. Set Up and Submit Tasks

```c++
#include "threadpool.h"
ThreadPool pool;
pool.start(4);

// Create a Task subclass and write tasks in the run() method
class Mytask : public Task
{
    public:
        void run(){ // task coding ...}
};

pool.submitTask(std::make_shared<MyTask>());
```

### 4. Complete Example

**Example:** Implementing a master-slave thread model for adding numbers from 1 to 300,000,000.

```c++
// "main.cpp"
#include "threadpool.h"
#include <chrono>

using Ulong = unsigned long long;
class MyTask : public Task
{
public:
    MyTask(Ulong begin, Ulong end)
        :begin_(begin), end_(end)
{}
    Any run() // The run method is ultimately executed by the thread allocated by the thread pool
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
std::cout << "main over" << std::endl;
getchar();
}
```


Certainly! Here's the translation of the compilation section and onwards into English:

------

## Compilation

#### Windows/Linux

##### CMake

```shell
$ mkdir build && cd build
$ cmake ..
$ make
```

#### Linux

##### 1). Direct Compilation

```shell
$ g++ -o main ./src/*.cpp -pthread -I ./inc -std=c++11
```

##### 2). Dynamic Library

##### Building the Dynamic Library

```shell
$ mkdir lib
$ g++ -shared -fPIC -o lib/libtdpool.so src/threadpool.cpp -Iinc -std=c++11 -pthread
```

##### Installing the Library to Standard Location and Updating System Library Cache

```shell
# 1. Store the dynamic library and header file (for compilation)
$ mv libtdpool.so /usr/local/lib
$ mv ./inc/threadpool.h /usr/local/include/ 
# 2. Compile
$ g++ -o main ./src/main.cpp -std=c++11 -ltdpool -lpthread
# 3. Add Custom Dynamic Library Path
$ cd /etc/ld.so.conf.d
 # Create a new configuration file
$ vim mylib.conf
 # Write the dynamic library path
 /usr/local/lib
# 4. Refresh Cache
$ ldconfig
```

##### Using Local Libraries and Headers

```shell
shellCopy code
$ g++ -o main src/main.cpp -Iinc -Llib -ltdpool -pthread -std=c++11
```

## Upcoming Upgrades（Completed）

### 1. Use packed_task to Encapsulate Tasks for More Convenient Task Submission in the Thread Pool

### 2. Use future Instead of Result to Simplify Thread Pool Code

**C++20 Version:** [address](https://github.com/Lionchen97/flexithreadpool-cpp20/tree/main)

## Project Output

### Application in Projects

- High Concurrency Network Server

  Handle io thread `void io_thread` to accept listenfd network connection events

  Worker thread `void thread(int clientfd)` to handle read/write events for connected users

- Master-Slave Thread Model

  Time-consuming calculations

- Handling Time-Consuming Tasks

  File transfer

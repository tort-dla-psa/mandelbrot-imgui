#pragma once
#include <type_traits>
#include <vector>
#include <queue>
#include <thread>
#include <future>
#include <functional>
#include <condition_variable>

namespace tortique{

class thread_pool{
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    std::condition_variable cond_var;
    std::mutex queue_mt;
    bool stop = false;
public:
    thread_pool(const size_t &size);
    ~thread_pool();

    template<class F, class... Args>
    auto emplace(F&& f, Args&&... args);
        //-> std::future<typename std::result_of<F(Args...)>::type>;
};
 
// the constructor just launches some amount of workers
inline thread_pool::thread_pool(const size_t &size) {
    for(size_t i=0; i<size; ++i){
        workers.emplace_back([this] {
            while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mt);
                    cond_var.wait(lock, [this]{
                        return stop || !tasks.empty();
                    });
                    if(stop && tasks.empty())
                        return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }

                task();
            }
        });
    }
}

template<class F, class... Args>
auto thread_pool::emplace(F&& f, Args&&... args){
    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mt);

        if(stop)
            throw std::runtime_error("enqueue on stopped thread_pool");

        tasks.emplace([task](){ (*task)(); });
    }
    cond_var.notify_one();
    return res;
}

// the destructor joins all threads
inline thread_pool::~thread_pool() {
    {
        std::unique_lock<std::mutex> lock(queue_mt);
        stop = true;
    }
    cond_var.notify_all();
    for(auto &worker:workers){
        if(worker.joinable()){
            worker.join();
        }
    }
}

}

#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

//异步写日志的日志队列
template<typename T>
class LoggerQueue
{
public:
    void Push(T &data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(data);
        m_cond.notify_one();
    }
    
    T Pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while(m_queue.empty())
        {
            m_cond.wait(lock);
        }

        std::string data = m_queue.front();
        m_queue.pop();
        return data;
    }
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};
#ifndef __BLOCK_H__
#define __BLOCK_H__

/**
 * 作用: 用于异步日志的阻塞队列
 * user: garteryang
 * 邮箱: 910319432@qq.com
 */

#include <time.h>
#include <sys/time.h>
#include "locker.h"

template<typename T>
class BlockQueue {
private:
    /* 基本属性 */
    T       *m_queue;        // 阻塞队列
    int     m_size;         // 当前的队列大小
    int     m_size_max;     // 队列的总大小
    int     m_front;        // 队首
    int     m_back;         // 队尾

    /* 锁和条件变量 */
    cond    m_cond;         // 条件变量
    locker  m_mutex;        // 互斥锁

public:
    /* 构造和析构 */
    BlockQueue(int size=BLOCK_QUEUE_MAX_LEN) {
        if (size <= 0) {
            throw "BlockQueue: form parameter size <= 0";
        }

        m_size = 0;
        m_size_max = size;
        m_front = -1;
        m_back = -1;
        m_queue = new T[m_size_max];
    }

    ~BlockQueue() {
        if (m_queue) delete [] m_queue;
    }

public:
    // 清空队列
    void clear() {
        m_mutex.lock();
        /* 清空阻塞队列 */
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    // 队列是否为空
    bool isempty() {
        m_mutex.lock();

        if (!m_size) {  // 为空
            m_mutex.unlock();
            return true;
        }

        m_mutex.unlock();
        return false;
    }

    // 队列是否已满
    bool isfull() {
        m_mutex.lock();

        if (m_size == m_size_max) {
            m_mutex.unlock();
            return true;
        }

        m_mutex.unlock();
        return false;
    }

    // 获取队列当前长度
    int size() {
        int size = 0;
        
        m_mutex.lock();
        size = m_size;
        m_mutex.unlock();

        return size;
    }

    // 获取队列最大长度
    int sizemax() {
        int size_max = 0;

        m_mutex.lock();
        size_max = m_size_max;
        m_mutex.unlock();

        return size_max;
    }

    // 获取队首元素
    bool front(T &t) {
        m_mutex.lock();

        if (isempty()) {
            m_mutex.unlock();
            return false;
        }

        t = m_queue[m_front];

        m_mutex.unlock();
        return true;
    }

    // 获取队尾元素
    bool back(T &t) {
        m_mutex.lock();

        if (isempty()) {
            m_mutex.unlock();
            return false;
        }

        t = m_queue[m_back];

        m_mutex.unlock();
        return true;
    }

    // 往队列添加元素，需要将所有使用队列的线程先唤醒
    // 当有元素push进队列,相当于生产者生产了一个元素
    // 若当前没有线程等待条件变量,则唤醒无意义
    bool push(const T &value) {
        m_mutex.lock();

        if (m_size == m_size_max) { // 如果队列满了，则可以优化为等待队列当前有值在写入
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_size_max;
        m_queue[m_back] = value;
        ++m_size;

        m_cond.broadcast();     // 表示队列中有数据需要处理
        m_mutex.unlock();
        return true;
    }

    bool pop(T &value) {    // 出队，没有设置超时时间
        m_mutex.lock();

        while (m_size <= 0) {
            if (!m_cond.wait(m_mutex.getMutex())) {
                m_mutex.unlock();
                return false;
            }
        }
        
        // 出队
        m_front = (m_front + 1) % m_size_max;
        value = m_queue[m_front];
        --m_size;

        m_mutex.unlock();
        return true;
    }

    bool pop(T &value, int timeout) {   // 出队，设置超时时间
        struct timespec t = {0, 0};
        struct timeval  now = {0, 0};

        // 获取当前时间
        gettimeofday(&now, nullptr);

        // 计算超时时间
        t.tv_sec = now.tv_sec + timeout / 1000;
        t.tv_nsec = (timeout % 1000) * 1000;        

        while (m_size <= m_size_max) {
            if (!m_cond.timedwait(m_mutex.getMutex(), &t)) { // 等待若干秒，如果没有出队元素，则返回失败 false
                m_mutex.unlock();
                return false;
            }
        }

        // 出队
        m_front = (m_front + 1) % m_size_max;
        value = m_queue[m_front];
        --m_size;

        m_mutex.unlock();
        return true;
    }
};

#endif // __BLOCK_H__
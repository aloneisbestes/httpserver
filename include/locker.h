#ifndef __LOCKER_H__
#define __LOCKER_H__

/**
 * 作用: 对 sem、lock、cond 的封装
 * user： garteryang
 * 邮箱: 910319432qq.com
 */

#include <exception>        // 异常头文件
#include <semaphore.h>      // 信号量头文件
#include <pthread.h>        // 线程头文件

/* 封装 semaphore 信号量类 */
class sem {
private:
    sem_t m_sem;    // 信号量变量

public:
    sem() {
        if (sem_init(&m_sem, 0, 0) != 0) {
            // 信号量初始化失败，抛出异常
            throw std::exception();
        }
    }

    sem(int n) {
        if (sem_init(&m_sem, 0, n) != 0) {
            // 抛出异常
            throw std::exception();
        }
    }

    ~sem() {
        sem_destroy(&m_sem);
    }


    // 使信号量减一，如果信号量为0，则阻塞
    bool wait() {
        return sem_wait(&m_sem) == 0 ? true : false;
    }

    // 使信号量加一
    bool post() {
        return sem_post(&m_sem) == 0 ? true : false;
    }
};

/**
 * 条件变量的封装
 */
class cond {
private:
    pthread_cond_t m_cond;      // 条件变量

public:
    cond() {
        if (pthread_cond_init(&m_cond, nullptr) != 0) {
            throw std::exception();
        }
    }

    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    // 条件成立，至少唤醒一个线程
    bool signal() {
        return pthread_cond_signal(&m_cond) == 0 ? true : false;
    }

    // 条件成立，唤醒所有等待该条件变量的线程
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0 ? true : false;
    }

    // 等待条件成立
    bool wait(pthread_mutex_t *mutex) {
        return pthread_cond_wait(&m_cond, mutex) == 0 ? true : false;
    }

    // 等待条件成立，设置超时时间
    bool timedwait(pthread_mutex_t *mutex, const struct timespec *abstime) {
        return pthread_cond_timedwait(&m_cond, mutex, abstime);
    }
};

/**
 * 互斥锁的封装
 */
class locker {
private:
    pthread_mutex_t m_mutex;    // 互斥锁变量

public:
    locker() {
        if (pthread_mutex_init(&m_mutex, nullptr) != 0) {
            // 如果互斥锁初始化失败，则抛出异常
            throw std::exception();
        }
    }

    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    // 加锁，如果该锁被占用，则阻塞等待
    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0 ? true : false;
    }

    // 加锁，如果该锁被占用，则不阻塞等待
    bool trylock() {
        return pthread_mutex_trylock(&m_mutex) == 0 ? true : false;
    }

    // 解锁
    bool unlock() {
        return pthread_mutex_unlock(&m_mutex);
    }

    // 获取互斥锁
    pthread_mutex_t *getMutex() {
        return &m_mutex;
    }
};

#endif // __LOCKER_H_
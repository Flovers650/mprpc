#pragma once
#include "loggerqueue.hpp"

// 定义宏
#define LOG_INFO(logmsgformat, ...) \
    do \
    {   \
        MprpcLogger &logger = MprpcLogger::GetInstance(); \
        logger.setlevel(INFO); \
        char c[1024] = {0}; \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(c); \
    } while(0)


#define LOG_ERROR(logmsgformat, ...) \
    do \
    {   \
        MprpcLogger &logger = MprpcLogger::GetInstance(); \
        logger.setlevel(ERROR); \
        char c[1024] = {0}; \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(c); \
    } while(0)


enum Loglevel
{
    INFO, // 普通信息
    ERROR, // 错误信息
};

//Mprpc框架提供的日志系统
class MprpcLogger
{
public:
    // 获取日志的单例
    static MprpcLogger& GetInstance();
    // 写日志
    void log(std::string msg);
    // 设置日志级别
    void setlevel(Loglevel level);
private:
    MprpcLogger();
    MprpcLogger(MprpcLogger&) = delete;
    MprpcLogger(MprpcLogger&&) = delete;

    int m_loglevel; // 记录日志级别
    LoggerQueue<std::string> m_logqueue; // 日志缓冲队列
};



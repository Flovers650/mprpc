#pragma once
#include "noncopyable.hpp"
#include <string>

#define LOG_INFO(LogmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLoglevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_ERROR(LogmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLoglevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_FATAL(LogmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLoglevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
        exit(-1); \
    } while(0)

#ifdef MUDEBUG
#define LOG_DEBUG(LogmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLoglevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, LogmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)
#else
    #define LOG_DEBUG(LogmsgFormat, ...)
#endif


//定义日志的级别 INFO ERROR FATAL DEBUG
enum Loglevel
{
    INFO,  //普通信息
    ERROR, //错误信息
    FATAL, //core信息
    DEBUG, //调试信息
};

//输出一个日志类
class Logger : noncopyable
{
public:
    //获取日志唯一的实例
    static Logger& instance();

    //设置日志级别
    void setLoglevel(int level);

    //写日志
    void log(std::string msg);
private:
    int loglevel_;
    Logger(){}
};
#include "logger.hpp"
#include <time.h>
#include <iostream>

// 获取日志的单例
MprpcLogger& MprpcLogger::GetInstance()
{
    static MprpcLogger logger;
    return logger;
}

MprpcLogger::MprpcLogger()
{
    // 启动专门的写日志线程
    std::thread writeLogTask([&](){
        for(;;)
        {
            // 获取当前的日期，然后取日志信息，写入相应的日志文件当中
            time_t now = time(nullptr);
            tm *nowtm = localtime(&now);

            char file_name[128] = {0};
            sprintf(file_name, "%d-%d-%d-log.txt", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday);

            FILE *fp = fopen(file_name, "a+");
            if(!fp)
            {
                std::cout << "logger file: " << file_name << "open error!" << std::endl;
                exit(EXIT_FAILURE);
            }

            std::string msg = m_logqueue.Pop();
            char time_buf[128] = {0};
            sprintf(time_buf, "%d:%d:%d =>[%s]", nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec, (m_loglevel == INFO ? "info": "error"));
            msg.insert(0, time_buf);
            msg.append("\n");
            
            fputs(msg.c_str(), fp);
            fclose(fp);
        }
    });
    
    // 设置分离线程，守护线程
    writeLogTask.detach();
}

// 写日志
void MprpcLogger::log(std::string msg)
{
    m_logqueue.Push(msg);
}

// 设置日志级别
void MprpcLogger::setlevel(Loglevel level)
{
    m_loglevel = level;
}
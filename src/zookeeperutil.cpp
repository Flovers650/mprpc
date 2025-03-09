#include "zookeeperutil.hpp"
#include "mprpcapplication.hpp"
#include <unistd.h>

// 全局的watcher观察器   zkserver给zkclient的通知
void global_watch(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    // 回调的消息类型是和会话相关的消息类型
    if(type == ZOO_SESSION_EVENT)
    {
        if(state == ZOO_CONNECTED_STATE)
        {
            sem_t *sem = (sem_t*)zoo_get_context(zh);
            sem_post(sem);
        }
    }
}

ZkClient::ZkClient() : m_zhandle(nullptr)
    {}

ZkClient::~ZkClient()
{
    if(m_zhandle != nullptr)
    {
        // 关闭句柄，释放资源
        zookeeper_close(m_zhandle);
    }
}

// zkclient启动连接zkserver
void ZkClient::Start()
{
    std::string host = MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    std::string connstr = host + ":" + port;

    m_zhandle = zookeeper_init(connstr.c_str(), global_watch, 30000, nullptr, nullptr, 0);
    if(nullptr == m_zhandle)
    {
        std::cout << "zookeeper_init error!" << std::endl;
        exit(EXIT_FAILURE);
    }

    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(m_zhandle, &sem);

    sem_wait(&sem);
    std::cout << "zookeeper_init success!" << std::endl;
}

// 在zkserver上根据指定的path创建znode节点
void ZkClient::Create(const char *path, const char *data, int datalen, int state)
{
    char path_buffer[128] = {0};
    int bufferlen = sizeof(path_buffer);
    int flag;

    // 先判断path表示的znode节点是否存在，如果存在，就不再重复创建了
    flag = zoo_exists(m_zhandle, path, 0, nullptr);
    if(ZNONODE == flag)
    {
        // 创建指定的path的znode节点
        flag = zoo_create(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if(flag == ZOK)
        {
            std::cout << "znode create success... path:" << path << std::endl;
        }
        else
        {
            std::cout << "flag:" << flag << std::endl;
            std::cout << "znode create error... path:" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

// 根据参数指定的znode节点路径，获得znode节点的值
std::string ZkClient::GetData(const char *path)
{
    char buffer[64];
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
    if(flag == ZOK)
    {
        return buffer;
    }
    else
    {
        std::cout << "get znode error... path:" << path << std::endl;
        return "";
    }
}
#include "rpcprovider.hpp"
#include "mprpcapplication.hpp"
#include "rpcheader.pb.h"
#include "zookeeperutil.hpp"


//这里是框架提供给外部使用的，可以发布rpc方法的函数接口
void RpcProvider::NotifyService(google::protobuf::Service *service) 
{
    // 获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    // 获取服务的名字
    std::string service_name = pserviceDesc->name();
    // 获取服务器对象service的方法的数量
    int methodCnt = pserviceDesc->method_count();
    
    //std::cout << "service_name: " << service_name << std::endl;
    LOG_INFO("service_name: %s", service_name.c_str());

    ServiceInfo service_info;
    service_info.m_service = service;

    for(int i = 0; i < methodCnt; i++)
    {
        // 获取了服务对象指定下标的服务方法的描述
        const google::protobuf::MethodDescriptor *pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});
        LOG_INFO("method_name: %s", method_name.c_str());
    }
    m_serviceMap.insert({service_name, service_info});
}


void RpcProvider::Run()
{
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    InetAddress address(ip, port);

    //创建TcpServer对象
    TcpServer server(&m_eventLoop, address, "RpcProvider");

    //绑定连接回调和消息读写回调
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, 
        std::placeholders::_2, std::placeholders::_3));

    //设置muduo库的线程数量
    server.setThreadNum(4);

    // 把当前rpc节点上要发布的服务全部都注册到zk上面，让rpc client可以从zk上发现服务
    ZkClient zkCli;
    zkCli.Start();
    // service_name 为持久性节点   method_name 为临时性节点
    for(auto &sp : m_serviceMap)
    {
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        for(auto &mp : sp.second.m_methodMap)
        {
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL); 
        }
    }

    // rpc 服务器准备启动，打印信息
    std::cout << "RpcProvider start service at ip:" << ip << "port:" << port << std::endl;

    //启动网络服务
    server.start();
    m_eventLoop.loop();
}

//新的socket连接回调
void RpcProvider::OnConnection(const TcpConnectionPtr& conn)
{
    if(!conn->connected())
    {
        conn->shutdown();
    }
}

//已建立连接用户的读写事件回调
void RpcProvider::OnMessage(const TcpConnectionPtr& conn, 
                            Buffer* buffer, 
                            Timestamp timeout)
{
    std::string recv_buf = buffer->retrieveAllAsString();
    // 从字符流中读取前4个字符的内容
    uint32_t header_size = 0;
    recv_buf.copy((char*)&header_size, 4, 0);

    // 根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if(rpcHeader.ParseFromString(rpc_header_str))
    {
        // 数据反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        // 数据头反序列化失败
        std::cout << "rpc_header_str: " << rpc_header_str << "parse error!" << std::endl;
        return;
    }
    // 获取rpc方法参数的字符流数据
    std::string args_ptr = recv_buf.substr(header_size + 4, args_size);

    // 打印调试信息
    std::cout << "==============================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_size: " << args_size << std::endl;
    std::cout << "args_ptr: " << args_ptr << std::endl;
    std::cout << "==============================" << std::endl;

    // 获取service对象和method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end())
    {
        std::cout << service_name << " is not exist!" << std::endl;
        return;
    }
    
    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end())
    {
        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        return;
    }

    google::protobuf::Service *service = it->second.m_service; // 获取service对象
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取method对象

    // 生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if(!request->ParseFromString(args_ptr))
    {
        std::cout << "response parse error, content: " << args_ptr << std::endl;
        return;
    }

    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给下面的method方法的调用, 绑定一个Closure的回调函数
    google::protobuf::Closure* done = google::protobuf::NewCallback<RpcProvider, 
                                            const TcpConnectionPtr&, google::protobuf::Message*>
                                            (this, &RpcProvider::SendRpcResponse, conn, response);

    //在框架上根据远端rpc请求，调用当前rpc节点上发布的节点
    service->CallMethod(method, nullptr, request, response, done);
}

// Closure的回调操作, 用于序列化rpc的响应和网络发送
void RpcProvider::SendRpcResponse(const TcpConnectionPtr& conn, google::protobuf::Message* response)
{
    std::string response_ptr;
    if(response->SerializeToString(&response_ptr))
    {
        // 序列化成功后，通过网络把rpc方法执行的结果发送给rpc的调用方
        conn->send(response_ptr);
    }
    else
    {
        std::cout << "serialize response_str error!" << std::endl;
    }
    conn->shutdown(); // 模拟http的短连接服务，由rpcprovider主动断开连接
}
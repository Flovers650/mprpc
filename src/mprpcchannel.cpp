#include "mprpcchannel.hpp"
#include <string>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "rpcheader.pb.h"
#include <unistd.h>
#include <sys/socket.h>
#include "mprpcapplication.hpp"
#include <arpa/inet.h>
#include "zookeeperutil.hpp"

void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                            google::protobuf::RpcController *controller, 
                            const google::protobuf::Message * request,
                            google::protobuf::Message *response,
                            google::protobuf::Closure *done)
{
    const google::protobuf::ServiceDescriptor *sd = method->service();
    std::string service_name = sd->name();
    std::string method_name = method->name();

    // 获取参数的序列化字符串长度
    int args_size = 0;
    std::string args_str;
    if(request->SerializeToString(&args_str))
    {
        args_size = sizeof(args_str);
    }
    else
    {
        std::cout << "serialize request error!" << std::endl;
        return;
    }

    // 定义rpc请求的header
    mprpc::RpcHeader header;
    header.set_service_name(service_name);
    header.set_method_name(method_name);
    header.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if(header.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        std::cout << "serialize request error!" << std::endl;
        return;
    }

    // 组织待发送的rpc请求的字符串
    std::string send_rpc_ptr;
    send_rpc_ptr.insert(0, std::string((char*)&header_size, 4));
    send_rpc_ptr += rpc_header_str;
    send_rpc_ptr += args_str;

    // 打印调试信息
    std::cout << "==============================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_size: " << args_size << std::endl;
    std::cout << "args_ptr: " << args_str << std::endl;
    std::cout << "==============================" << std::endl;

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd)
    {
        std::cout << "errno" << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    ZkClient zkCli;
    zkCli.Start();
    std::string method_path = "/" + service_name + "/" + method_name;
    std::string host_data = zkCli.GetData(method_path.c_str());
    if(host_data == "")
    {
        controller->SetFailed(method_path + "is not exist!");
        return;
    }
    int idx = host_data.find(':');
    if(idx == -1)
    {
        controller->SetFailed(method_name + "address is invalid!");
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint32_t port = atoi(host_data.substr(idx + 1, host_data.size() - idx - 1).c_str());
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    // 连接rpc服务节点
    if(-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        std::cout << "connect error! errno: " << errno << std::endl;
        close(clientfd);
        exit(EXIT_FAILURE);
    }
    // 发送rpc请求
    if(-1 == send(clientfd, send_rpc_ptr.c_str(), send_rpc_ptr.size(), 0))
    {
        std::cout << "send error! errno: " << errno << std::endl;
        close(clientfd);
        exit(EXIT_FAILURE);
    }

    // 接受rpc请求的数据
    char buf[1024] = {0};
    int recv_size = 0;
    if(-1 == (recv_size = recv(clientfd, buf, sizeof(buf), 0)))
    {
        std::cout << "recv error! errno: " << errno << std::endl;
        close(clientfd);
        exit(EXIT_FAILURE);
    }

    if(!response->ParseFromArray(buf, recv_size))
    {
        std::cout << "parse error! response_str: " << buf << std::endl;
        close(clientfd);
        return;
    }

    close(clientfd);
}               
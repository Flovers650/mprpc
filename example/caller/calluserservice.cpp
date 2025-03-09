#include <iostream>
#include "mprpcapplication.hpp"
#include "user.pb.h"

int main(int argc, char **argv)
{
    MprpcApplication::Init(argc, argv);

    // 调用远程发布的rpc方法login
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel());
    //stub.login(); // RpcChannel::callmethod 集中来做所有rpc方法调用的参数序列化和网络发送
    fixbug::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("123456");
    fixbug::LoginResponse response;
    MprpcController controller;
    // 发起rpc方法的调用 同步的rpc调用过程 
    stub.login(&controller, &request, &response, nullptr);

    //一次rpc调用完成，读调用的结果
    if(0 == response.result().errcode())
    {
        std::cout << "rpc login response: " << response.success() << std::endl;
    }
    else
    {
        std::cout << "rpc login response error :" << response.result().errmsg() << std::endl;
    }
    
    return 0;
}
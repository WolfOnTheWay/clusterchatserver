#include"chatserver.hpp"
#include"chatservice.hpp"
#include<functional>
#include<iostream>
#include<string>
#include"json.hpp"
using namespace std;
using namespace placeholders;
using json=nlohmann::json;
ChatServer::ChatServer(EventLoop *loop,const InetAddress& listenAddr,const string& namearg):
    _server(loop,listenAddr,namearg),_loop(loop)
{
    //注册回调函数
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));
    _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));
    //设置线程数量
    _server.setThreadNum(4);

}
 //启动服务
void ChatServer::start()
{
    _server.start();
}
//上报连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    //用户断开连接
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseExcept(conn);
        conn->shutdown();
    }

}
//上报读写相关的回调函数
void ChatServer::onMessage(const TcpConnectionPtr& conn,Buffer* buffer,Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    //数据的反序列化
    json js = json::parse(buf);
    //目的：完全解偶网络模块和业务模块的代码
    //通过js['msgid']获取业务handler >conn js time
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    //回调消息对应的事件处理器来执行相应的业务处理
    msgHandler(conn,js,time);
}
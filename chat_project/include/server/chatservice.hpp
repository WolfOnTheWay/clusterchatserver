#ifndef CHATSERVICE_H
#define CHATSERVICE_H
#include<unordered_map>
#include<functional>
#include<muduo/net/TcpConnection.h>
#include<mutex>
#include"json.hpp"
#include"usermodel.hpp"
#include"offlinemessagemode.hpp"
#include"friendmodel.hpp"
#include"groupmodel.hpp"
#include"redis.hpp"
using json = nlohmann::json;
using namespace std;
using namespace muduo;
using namespace muduo::net;

//处理消息的事件回调类型
using MsgHandler = std::function<void(const TcpConnectionPtr& conn,json &js,Timestamp time)>;
//聊天服务器业务类
class ChatService
{
public:
    //获取单例对象的接口函数
    static ChatService* instance();
    //处理登录业务
    void login(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    //处理客户端异常退出
    void clientCloseExcept(const TcpConnectionPtr& conn);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //添加好友
     void addFriend(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //创建群组
    void createGroup(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //加入群组
    void addGroup(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //群聊
    void groupChat(const TcpConnectionPtr& conn,json &js,Timestamp time);
     //退出
    void loginOut(const TcpConnectionPtr& conn,json &js,Timestamp time);
    //服务器异常业务重置
    void reset();
    //redis回调函数
    void handleRedisSubscribeMessage(int userid,string msg);

private: 
    ChatService();
    //存储消息id和对应的处理方法
    unordered_map<int,MsgHandler> _msgHandlerMap;
   
    //存储在线用户的连接
    unordered_map<int,TcpConnectionPtr> _userConnMap;//要考虑线程安全
    //定义互斥锁
    mutex _connMutex;

    //数据操作对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;
    Redis _redis;
};

#endif
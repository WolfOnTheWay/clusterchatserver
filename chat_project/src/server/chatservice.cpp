#include"chatservice.hpp"
#include"public.hpp"
#include<muduo/base/Logging.h>
#include<vector>
#include<map>
using namespace std;
using namespace placeholders;
using namespace muduo;
 //获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService chatService;
    return &chatService;
}
//构造函数，将消息id和对应的回调函数加入到map当中
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});

    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});

    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});

    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginOut,this,_1,_2,_3)});

    //连接redis服务器
    if(_redis.connect())
    {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}
//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    //记录错误日志，msgid没有对应的回调
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end())
    {
        
        //返回一个默认的处理器
        return [=](const TcpConnectionPtr& conn,json &js,Timestamp time)
        {
            LOG_ERROR<<"msgId:"<<msgid<<"can not find handler";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
    
}

//处理登录业务
void ChatService::login(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    //LOG_INFO<<"do login service";    
    //id password
    int id = js["id"].get<int>();
    string pwd = js["password"];
    User user = _userModel.query(id);
    if(user.getId()==id&&user.getPassword()==pwd)
    {
        if(user.getState()=="online")
        {
            //该用户已经登录，不允许重复
            //登录失败:用户不存在
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"]=2;//响应成功
            response["errmsg"]="该帐号已经登录";
            conn->send(response.dump());
        }
        else
        {
            //登录成功,记录连接信息
            //加括号减少锁的力度
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({user.getId(),conn});
            }
            
            //登录成功
            //id登录成功后向redis订阅通道为id的消息
            _redis.subscribe(id);
            //更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);
            //返回
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"]=0;//响应成功
            response["id"] = user.getId();
            response["name"]=user.getName();
            //查询用户是否有离线消息，有的话进行返回
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"]=vec;
                //读取后，删除离线消息
                _offlineMsgModel.remove(id);
            }
            //查询好友列表并返回
            vector<User> friendVec = _friendModel.query(id);
            if(!friendVec.empty())
            {
                vector<string> vec2;
                for(User &user:friendVec)
                {
                    json js;
                    js["id"]=user.getId();
                    js["name"]=user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            //查询群组消息
            vector<Group> groupVec = _groupModel.queryGroups(id);
            if(!groupVec.empty())
            {
                //group:[{groupid:{xxxx,xxx,xx}}]
                vector<string> groupV;
                for(Group &group:groupVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for(GroupUser &user:group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;

            }
            conn->send(response.dump());

        }
    }
    else
    {
        //登录失败:用户不存在或者密码错误
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"]=1;//响应成功
        response["errmsg"]="用户名或者密码错误";
        conn->send(response.dump());
    }
    
}
//处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPassword(pwd);
    bool flag = _userModel.insert(user);
    if(flag)
    {
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"]=0;//响应成功
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        //注册失败
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"]=1;//响应失败
        conn->send(response.dump());
    }
    
}

//处理客户端异常退出
void ChatService::clientCloseExcept(const TcpConnectionPtr& conn)
{
    //找到从_userConnMap中找到conn对应的键值对删除，并且进行对应的更新操作
    User user;
    {
         lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin();it!=_userConnMap.end();++it)
        {
            if(it->second==conn)
            {
                //删除连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    //取消通道
    _redis.unSubscribe(user.getId());
    //更新用户的状态信息
    if(user.getId()!=-1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
    
}

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn,json &js,Timestamp time)
{

    int toId = js["toid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toId);
        if(it!=_userConnMap.end())
        {
            //在线，转发消息
            it->second->send(js.dump());
            return;
        }
    }
    //查询是否在线，如果此时在线，那么一定在其他服务器登录
    User user = _userModel.query(toId);
    if(user.getState()=="online")
    {
        _redis.publish(toId,js.dump());
        return;
    }
    //不在线，存储离线消息
    _offlineMsgModel.insert(toId,js.dump());
}

//添加好友 msgid, id, friend id
void ChatService::addFriend(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    //存储好友信息
    _friendModel.insert(userid,friendid);
}
//重置业务
void ChatService::reset()
{
    //把所有用户的状态进行改变
    _userModel.resetState();
}
//创建群组 id,groupname,groupdesc
void ChatService::createGroup(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    int id = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1,name,desc);
    if(_groupModel.createGroup(group))
    {
        //储存创建人信息
        _groupModel.addGroup(id,group.getId(),"creator");
    }
}

 //加入群组 id,groupid
void ChatService::addGroup(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    int userId = js["id"].get<int>();
    int groupId = js["groupid"].get<int>();
    _groupModel.addGroup(userId,groupId,"normal");
}

 //群聊 id,groupid
void ChatService::groupChat(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid= js["groupid"].get<int>();
    vector<int> userIdVec = _groupModel.queryGroupUsers(userid,groupid);
    lock_guard<mutex> lock(_connMutex);
    for(int id:userIdVec)
    {
        auto it = _userConnMap.find(id);
        //在线
        if(it != _userConnMap.end())
        {
            it->second->send(js.dump());
        }
        //不在线
        else
        {
              //查询是否在线，如果此时在线，那么一定在其他服务器登录
            User user = _userModel.query(id);
            if(user.getState()=="online")
            {
                _redis.publish(id,js.dump());
            }
            else
            {
                _offlineMsgModel.insert(id,js.dump());

            }
        }
    }
}

//退出
void ChatService::loginOut(const TcpConnectionPtr& conn,json &js,Timestamp time)
{
    int userid = js["id"].get<int>();
    unordered_map<int,TcpConnectionPtr>::iterator it;
    //加括号减少锁的力度
    {
        lock_guard<mutex> lock(_connMutex);
        it = _userConnMap.find(userid);
        if(it!=_userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }
    //取消用户订阅的通道
    _redis.unSubscribe(userid);
    //更新用户信息
    User user(userid,"","","offline");
    _userModel.updateState(user);

}


//redis回调函数
void ChatService::handleRedisSubscribeMessage(int userid,string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it!=_userConnMap.end())
    {
        it->second->send(msg);
    }

    //可能在发送消息的过程中用户下线，因此要存储离线消息
    _offlineMsgModel.insert(userid,msg);
}
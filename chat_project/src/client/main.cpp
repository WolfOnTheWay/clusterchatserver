#include"json.hpp"
#include<iostream>
#include<thread>
#include<string>
#include<vector>
#include<chrono>
#include<ctime>
using namespace std;
using json = nlohmann::json;

#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

//记录当前系统登录的用户信息
User g_currentUser;
//记录当前登录用户的好友列表信息
vector<User> g_currentFriendList;
//记录当前用户的群组列表信息
vector<Group> g_currentGroupList;
//控制聊天页面程序
bool isMainMenuRunning = false;
void help(int fd= 0,string str = "");
void chat(int,string);
void addfriend(int,string);
void creategroup(int,string);
void addgroup(int,string);
void groupchat(int,string);
void loginout(int,string);
//系统支持的客户端命令列表
unordered_map<string,string> commandMap={
    {"help","显示所有支持的命令，格式help"},
    {"chat","一对一聊天，格式chat:friendid:message"},
    {"addfriend","添加好友，格式addfriend:friendid"},
    {"creategroup","创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup","加入群组，格式addgroup:groupid"},
    {"groupchat","群聊，格式groupchat:groupid:message"},
    {"loginout","注销，格式loginout"}
};
//注册系统支持的客户端命令处理
unordered_map<string,function<void(int,string)>> commandHandlerMap = {
    {"help",help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"loginout",loginout}};
//显示当前登录成功用户的基本信息
void showCurrentUserData();

//接收线程
void readTaskHandler(int clientfd);
//获取系统时间，聊天信息时需要加载
string getCurrentTime();
//主聊天页面程序
void mainMenu(int);

//主线程作为发送线程，子线程作为接收线程
int main(int argc,char** argv )
{
    if(argc<3)
    {
        cerr<<"command invaild example: ./ChatClient 127.0.0.1 6000"<<endl;
        exit(-1);
    }
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);
    //创建client的socket
    int clientfd= socket(AF_INET,SOCK_STREAM,0);
    if(clientfd==-1)
    {
        cerr<<"socket create error"<<endl;
        exit(-1);
    }
    //填写client需要链接的server信息ip+port
    sockaddr_in server;
    memset(&server,0,sizeof(sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    //client和server进行链接
    if(-1==connect(clientfd,(sockaddr*)&server,sizeof(sockaddr_in)))
    {
        cerr<<"connect server error"<<endl;
        close(clientfd);
        exit(-1);
    }
    //main线程用于接收用户暑促，负责发送数据
    while(1)
    {
        //显示首页菜单
        cout<<"===================="<<endl;
        cout<<"1. login"<<endl;
        cout<<"2. register"<<endl;
        cout<<"3. quit"<<endl;
        cout<<"===================="<<endl;
        cout<<"choice:";
        int choice = 0;
        cin>>choice;
        cin.get();//去掉缓冲区残留的回车
        switch(choice)
        {
            case 1://login 业务
            {
                int id = 0;
                char pw[50] = {0};
                cout<<"userid:";
                cin>>id;
                cin.get();
                cout<<"password:";
                cin.getline(pw,50);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pw;

                string request = js.dump();

                int len = send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len ==-1)
                {
                    cerr<<" send login msg error "<< request <<endl;
                }
                else
                {
                   char buffer[1024] = {0};
                   len = recv(clientfd,buffer,1024,0);
                   if(-1==len)
                   {
                       cerr<<"recv login response err!"<<endl;
                   }
                   else
                   {
                       json responseJs = json::parse(buffer);
                       if(0!=responseJs["errno"].get<int>())
                       {
                           cout<<responseJs["errmsg"]<<endl;
                       }
                       else//登录成功
                       {
                           isMainMenuRunning = true;
                           //记录当前用户的id和name
                           g_currentUser.setId(responseJs["id"].get<int>());
                           g_currentUser.setName(responseJs["name"]);
                           //记录当前用户的好友列表
                           if(responseJs.contains("friends"))
                           {
                               vector<string> vec = responseJs["friends"];
                               for(string &str:vec)
                               {
                                   json js = json::parse(str);
                                   User user;
                                   user.setId(js["id"].get<int>());
                                   user.setName(js["name"]);
                                   user.setState(js["state"]);
                                   g_currentFriendList.push_back(user);
                               }
                           }
                           //记录当前用户的群组列表信息
                           if(responseJs.contains("groups"))
                           {
                               vector<string> vec1 = responseJs["groups"];
                               cout<<"进入群组列表"<<endl;
                               for(string &str1:vec1)
                               {
                                   
                                   json groupJs = json::parse(str1);
                                   Group group;
                                   group.setId(groupJs["id"].get<int>());
                                   group.setName(groupJs["groupname"]);
                                   group.setDesc(groupJs["groupdesc"]);
                                   vector<string> vec2 = groupJs["users"];
                                   for(string &str2:vec2)
                                   {
                                       GroupUser user;
                                       json js = json::parse(str2);
                                       user.setId(js["id"].get<int>());
                                       user.setName(js["name"]);
                                       user.setState(js["state"]);
                                       user.setRole(js["role"]);
                                       group.getUsers().push_back(user);
                                   }
                                   g_currentGroupList.push_back(group);
                               }
                               cout<<"退出群组列表"<<endl;
                           }
                           //显示登录信息
                           showCurrentUserData();
                           //显示当前用户的离线消息 个人聊天和群组聊天
                           if(responseJs.contains("offlinemsg"))
                           {
                                cout<<"====================offlinemsg===================="<<endl;
                                vector<string> vec = responseJs["offlinemsg"];
                                for(string &str:vec)
                                {
                                    json js = json::parse(str);
                                    int msgtype = js["msgid"].get<int>();
                                    if(ONE_CHAT_MSG==msgtype)
                                    {
                                        cout<<endl;
                                        cout<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<" said:"
                                        <<js["msg"].get<string>()<<endl;
                                    }
                                    else if(GROUP_CHAT_MSG==msgtype)
                                    {
                                        cout<<endl;
                                        cout<<"群消息"<<"["<<js["groupid"]<<"]"<<js["time"].get<string>()<<"["<<js["id"]<<"]"
                                        <<js["name"].get<string>()<<"  said: "
                                        <<js["msg"].get<string>()<<endl;
                                    }
                                }
                           }
                           static int threadNum = 0;
                           if(threadNum==0)
                           {
                               //登录成功，启动接收线程负责接收数据
                                std::thread readTask(readTaskHandler,clientfd);
                                readTask.detach();
                                threadNum++;
                           }
                           
                           //进入主界面
                           mainMenu(clientfd);
                       }
                       
                   }
                   
                }
              
            }
            break;
            case 2://注册业务
            {
                char name[50]={0};
                char pw[50] = {0};
                cout<<"username:";
                cin.getline(name,50);
                cout<<"password:";
                cin.getline(pw,50);

                json js;
                js["msgid"] = REG_MSG;  //注册
                js["name"] = name;
                js["password"] = pw;

                string request = js.dump();

                int len = send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len==-1)
                {
                    cerr<<"send reg msg error"<<endl;
                }
                else
                {
                    char buffer[1024]={0};
                    len = recv(clientfd,buffer,1024,0);
                    if(len==-1)
                    {
                        cerr<<"recv reg response error"<<endl;
                    }
                    else
                    {
                        json response  = json::parse(buffer);//失败
                        if(0!=response["errno"].get<int>())
                        {
                            cerr<<name<<"is already exit,register error!"<<endl;
                        }
                        else//成功
                        {
                            cout<<name<<"register success,userid is  "<<response["id"].get<int>()
                            <<" ,do not forget it!"<<endl;
                        }
                        
                    }
                    
                }
                
            }
            break;
            case 3://退出业务
                close(clientfd);
                exit(0);
            default:
                cerr<<"input error ,please retry!";
                break;
        }
    }
       
    return 0;
}
//显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout<<"====================login user===================="<<endl;
    cout<<"current login user =>id:"<<g_currentUser.getId()<<"  name:"<<g_currentUser.getName()<<endl;
    cout<<"--------------------friend list-------------------"<<endl;
    if(!g_currentFriendList.empty())
    {
        for(auto &user:g_currentFriendList)
        {
            cout<<user.getId()<<" "<<user.getName()<<" "<<user.getState()<<endl;
        }
    }
    cout<<"--------------------group list-------------------"<<endl;
    if(!g_currentGroupList.empty())
    {
        for(Group &group:g_currentGroupList)
        {
            cout<<group.getId()<<" "<<group.getName()<<" "<<group.getDesc()<<endl;
            for(auto &user:group.getUsers())
            {
                cout<<user.getId()<<" "<<user.getName()<<" "<<user.getState()<<" "<<user.getRole()<<endl;
            }
        }
    }
    cout<<"==================================================="<<endl;
}

//接收线程
void readTaskHandler(int clientfd)
{
    while(isMainMenuRunning==true)
    {
        char buffer[1024]={0};
        int len = recv(clientfd,buffer,1024,0);
        if(-1==len||0==len)
        {
            close(clientfd);
            exit(-1);
        }
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if(ONE_CHAT_MSG==msgtype)
        {
            cout<<endl;
            cout<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<" said:"
            <<js["msg"].get<string>()<<endl;
            continue;
        }
        else if(GROUP_CHAT_MSG==msgtype)
        {
            cout<<endl;
            cout<<"群消息"<<"["<<js["groupid"]<<"]"<<js["time"].get<string>()<<"["<<js["id"]<<"]"
            <<js["name"].get<string>()<<" said:"
            <<js["msg"].get<string>()<<endl;
            continue;
        }
    }
}
//主界面
void mainMenu(int clientfd)
{
    help();
    char buffer[1024]={0};
    while(isMainMenuRunning==true)
    {
        cout<<"please input command:";
        cin.getline(buffer,1024);
        string commandbuf(buffer);
        string command;
        int idx = commandbuf.find(":");
        if(idx==-1)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0,idx);
        }
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end())
        {
            cout<<"invalid input command"<<endl;
            continue;
        }
        it->second(clientfd,commandbuf.substr(idx+1,commandbuf.size()-idx));
        
    }
}

void help(int,string)
{
    cout<<"====================show command list=================="<<endl;
    for(auto &e:commandMap)
    {
        cout<<e.first<<"--->"<<e.second<<endl;
    }
    cout<<endl;
}
void chat(int clientfd,string str)
{
    int idx = str.find(":");
    if(-1==idx)
    {
        cerr<<"chat command invalid"<<endl;
        return;
    }

    int friendid = atoi(str.substr(0,idx).c_str());
    string message = str.substr(idx+1,str.size()-idx);
    
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
     string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len)
    {
        cerr<<"send chat msg error:"<<buffer<<endl;
    }
}
void addfriend(int clientfd,string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len)
    {
        cerr<<"send addfriend msg error:"<<buffer<<endl;
    }
}
void creategroup(int clientfd,string str)
{
   int idx = str.find(":");
   if(-1==idx)
   {
        cerr<<"creategroup command invalid"<<endl;
        return;
   }
   string name = str.substr(0,idx);
   string desc = str.substr(idx+1,str.size()-idx);
   json js;
   js["msgid"] = CREATE_GROUP_MSG;
   js["id"] = g_currentUser.getId();
   js["groupname"] = name;
   js["groupdesc"] = desc;
   string buffer = js.dump();
   int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len)
    {
        cerr<<"send creategroup msg error:"<<buffer<<endl;
    }
}
void addgroup(int clientfd ,string str)
{
   int gorupid = atoi(str.c_str());
   json js;
   js["msgid"] = ADD_GROUP_MSG;
   js["id"] = g_currentUser.getId();
   js["groupid"] = gorupid;
   string buffer = js.dump();
   int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len)
    {
        cerr<<"send addgroup msg error:"<<buffer<<endl;
    }
}
//"groupchat","群聊，格式groupchat:groupid:message"
void groupchat(int clientfd ,string str)
{
    int idx = str.find(":");
    if(-1==idx)
    {
        cerr<<"chat command invalid"<<endl;
        return;
    }

    int groupid = atoi(str.substr(0,idx).c_str());
    string message = str.substr(idx+1,str.size()-idx);
    
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
     string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len)
    {
        cerr<<"send groupchat msg error:"<<buffer<<endl;
    }
}
void loginout(int clientfd ,string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(-1==len)
    {
        cerr<<"send groupchat msg error:"<<buffer<<endl;
    }
    isMainMenuRunning = false;
    //清楚当前记录信息
    g_currentFriendList.clear();
    g_currentGroupList.clear();
}
//获取系统时间，聊天信息时需要加载
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date,"%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year+1900,(int)ptm->tm_mon+1,(int)ptm->tm_mday,
            (int)ptm->tm_hour,(int)ptm->tm_min,(int)ptm->tm_sec);
    return std::string(date);
}
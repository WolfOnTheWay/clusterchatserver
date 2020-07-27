 #include"redis.hpp"
 #include<iostream>
 using namespace std;

 Redis::Redis()
    :_publish_context(nullptr),_subscribe_context(nullptr)
 {
 }
Redis::~Redis()
{
    if(_publish_context!=nullptr)
    {
        redisFree(_publish_context);
    }
    if(_subscribe_context!=nullptr)
    {
        redisFree(_subscribe_context);
    }
}

//连接redis服务器
bool Redis::connect()
{
    _publish_context=redisConnect("127.0.0.1",6379);
    if(_publish_context==nullptr)
    {
        cerr<<"connected redis failed"<<endl;
        return false;
    }

    _subscribe_context=redisConnect("127.0.0.1",6379);
    if(_subscribe_context==nullptr)
    {
        cerr<<"connected redis failed"<<endl;
        return false;
    }
    //在单独的线程里面监听通道中的事件，有消息就给业务层进行上报
    thread t([&](){
        observer_channel_message();});
    t.detach();
    cout<<"connect redis-server success!"<<endl;
    return true;

}
//向redsi指定的通道channel发布消息
bool Redis::publish(int channel,string message)
{
    redisReply *reply=(redisReply*)redisCommand(_publish_context,"PUBLISH %d %s",channel,message.c_str());
    if(reply==nullptr)
    {
        //发布失败即代表此用户没有订阅
        cerr<<"publich command failed!"<<endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}
//向redis指定的通道例订阅消息
bool Redis::subscribe(int channel)
{
    //subscribe命令会造成线程阻塞，等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    //通道消息的接收在observer_channel_message函数中的独立线程中运行
    //只负责发送命令，不阻塞接收redis——server响应消息，否则和notifyMsg线程抢占线程资源
    if(REDIS_ERR==redisAppendCommand(this->_subscribe_context,"SUBSCRIBE %d",channel))
    {
        cerr<<"subscribe command failed!"<<endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置为1)
    int done =0;
    while(!done)
    {
        if(REDIS_ERR==redisBufferWrite(this->_subscribe_context,&done))
        {
            cerr<<"subscribe bufferwrite error!"<<endl;
            return false;
        }
    }

    //redisGetReply
    return true;

}
//取消订阅
bool Redis::unSubscribe(int channel)
{
    if(REDIS_ERR==redisAppendCommand(this->_subscribe_context,"UNSUBSCRIBE %d",channel))
    {
        cerr<<"unsubscribe command failed!"<<endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置为1)
    int done =0;
    while(!done)
    {
        if(REDIS_ERR==redisBufferWrite(this->_subscribe_context,&done))
        {
            cerr<<"unsubscribe bufferwrite error!"<<endl;
            return false;
        }
    }

    //redisGetReply
    return true;
}

//在独立的献策和嗯中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while(REDIS_OK==redisGetReply(this->_subscribe_context,(void**)&reply))
    {
        if(reply!=nullptr&&reply->element[2]!=nullptr&&reply->element[2]->str!=nullptr)
        {
            //通道上发生消息
            _notify_message_handler(atoi(reply->element[1]->str),reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
    cerr<<">>>>>>>>>>>>>observer_channel_message quit<<<<<<<<<<<<<"<<endl;
}

//初始化向业务层上报通道消息的回调函数
void Redis::init_notify_handler(function<void(int,string)> fn)
{
    this->_notify_message_handler=fn;
}
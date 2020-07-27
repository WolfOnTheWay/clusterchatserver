#ifndef DB_H
#define DB_H


#include<string>
using namespace std;
#include<muduo/base/Logging.h>
#include<mysql/mysql.h>


class MySQL
{
public:
    //初始化数据库链接
    MySQL();
    //释放
    ~MySQL();
    //数据库连接操作
    bool connenct();
    //更新操作
    bool update(string sql);
    //查询操作
    MYSQL_RES* query(string sql);
    //获取链接
    MYSQL *getConnection();
private:  
    MYSQL *_conn;
};

#include<mysql/mysql.h>


#endif
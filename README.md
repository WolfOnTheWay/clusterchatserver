# clusterchatserver
基于nginx负载均衡的集群聊天系统，其中技术点有网络方面、数据库方面、线程安全方面；功能点有添加好友、单人聊天、群组聊天等
编译方式：
首先进入build目录，删除rm -rf * 删除所有内容
cmake ..
make

相应的可执行文件将在bin目录中生成

在启动之间，要进行nginx的负载均衡的配置，同时将nginx服务启动

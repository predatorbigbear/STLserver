# STLserver
这是一次用C++ STL做服务器的尝试，顺便验证STL高性能的传说

于腾讯云香港轻量服务器上2c4g环境下进行了测试，

![00](https://user-images.githubusercontent.com/20834575/133877843-1cb936da-0fdd-49ca-86cb-ba5094cbfae0.jpg)

具体测试环境为centos8  GCC10.2.0   编译选项需使用C++17  

采用搜狗workflow的例子进行对比

https://github.com/sogou/workflow/tree/master/benchmark

其中，STLserver内对比测试函数为httpService.cpp中的testCompareWorkFlow，

STLserver内添加上从body解析获取返回string长度的逻辑进行对比

测试中，workflow前两项参数默认设置为12  8085  表示12个poller和开启监听端口8085

先上两张wrk测试结果分别为body长度为11和22的情况，两张图中上面为STL的，下面为workflow

11的情况


![011](https://user-images.githubusercontent.com/20834575/133878189-e6def590-5fe4-443b-aa3f-2099168f4470.jpg)


22的情况

![022](https://user-images.githubusercontent.com/20834575/133878196-88eb2967-a087-4812-a580-67f8ea3d240f.jpg)


44的情况

![044](https://user-images.githubusercontent.com/20834575/133878752-7008f52a-9067-4915-9e30-7e92b82decd4.jpg)

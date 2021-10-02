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

编译说明：
安装vs2017或2019过程中，需要勾上linux开发的组件
![07](https://user-images.githubusercontent.com/20834575/135704938-afa9cfb2-4526-436c-9b19-9037fa3efcf1.jpg)


首先下载源码压缩包，解压后得到以下文件夹
![01](https://user-images.githubusercontent.com/20834575/135703908-e9029251-7c8d-4b46-b609-6b74e6f49b64.jpg)


使用vs2017或2019打开linCloud.sln，并选择VS里面的工具-选项-跨平台，进入到以下界面：

![02](https://user-images.githubusercontent.com/20834575/135703938-5c3e7b3b-d7ba-447c-b447-eb3d95f28227.jpg)

![03](https://user-images.githubusercontent.com/20834575/135703998-43b61f43-ead6-4783-8cd7-0d263ebed985.jpg)

点击添加，主机名输入远程linux主机IP，用户名为登录用户名，
身份验证类型为密码的情况下，密码输入登录密码
为私钥的情况下，选择一个AES加密过的RSA私钥文件（可使用openssl生成），密码填写解密该私钥的密码
完成之后点击连接，等待文件同步

同步完毕之后进入工程属性页，选择远程主机
![04](https://user-images.githubusercontent.com/20834575/135704036-7412a2a2-723d-48bc-8155-626c4631c66e.jpg)

然后点击生成即可，
![05](https://user-images.githubusercontent.com/20834575/135704054-6c0b67dd-3e8a-4a20-8d59-e762d1bf8219.jpg)
默认生成路径为/root/projects下


值得注意的是，本工程需要远程linux主机上安装好mysql8.0,redis、openssl以及bosst库和支持C++17的GCC编译器







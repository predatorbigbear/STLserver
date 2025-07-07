




//////////////////////////////////////////////////////
20250703  因生活压力大，两周后找不到好工作决定转行了，如果有公司需要的话，可以进行基础组件性能优化开发，顺便可以传授一些STL高性能编程知识，有很多是业界不为人知的，虽然可能教出来的人达不到我的水平，不过肯定比其他公司的厉害，工资要求不高，不会漫天开价，可以先拿一些简单的模块让我优化试试，试用期可以先拿岗位最低工资，达到要求后转正再加到我期望的薪资



重点：
这个服务器高效的实现是基于生命周期实现的

///////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////



20250629更新  具体编译方法请查看本人boss上的简历   联系电话是135*****610



wrk测试接口4 qps图：
![QQ20250630-152617](https://github.com/user-attachments/assets/da3ea4b5-2657-4553-aa7a-e976055663bc)

测试视频：
https://www.bilibili.com/video/BV1pAgCzqEaP

经过优化后的wrk测试接口4  qps图：
![屏幕录制 2025-07-01 130647 mp4_20250701_131046711](https://github.com/user-attachments/assets/2b226413-54ba-4e75-bfbc-6b76f7176f50)

测试视频：
https://b23.tv/ENpHX9d


 

主要优化点是将其中几个调用频繁，时间比较长的函数针对性优化了：
![profile1](https://github.com/user-attachments/assets/e0a2ae49-6852-4cd9-bad1-dc4382dbe819)
![profil2](https://github.com/user-attachments/assets/27e4dcde-9714-4b9a-bf70-a0e8ba0ab7df)














vs2022编译选项设置：
![QQ20250630-075541](https://github.com/user-attachments/assets/e48d1fee-c0fb-440d-bb53-1fc784cbc389)

![QQ20250630-075551](https://github.com/user-attachments/assets/6fd00da1-3e50-4a33-9172-64032b01911d)

![QQ20250630-075602](https://github.com/user-attachments/assets/b80ccb84-048f-4090-bc79-b6c5ac38d156)

![QQ20250630-075621](https://github.com/user-attachments/assets/64aeff04-3978-4fe6-a21f-99cbec8498dc)

![QQ20250630-081443](https://github.com/user-attachments/assets/544e0d9f-1f97-4960-8c10-c828d2b75d34)
![QQ20250630-081454](https://github.com/user-attachments/assets/97f3efe0-2a98-4dbb-bf97-fba6ebd34549)

![QQ20250630-152617](https://github.com/user-attachments/assets/0a570152-e83f-4566-be2b-ad7934b90886)

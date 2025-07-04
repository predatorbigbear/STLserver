20250703  因生活压力大，两周后找不到好工作决定转行了，如果有公司需要的话，可以进行基础组件性能优化开发，顺便可以传授一些STL高性能编程知识，有很多是业界不为人知的，虽然可能教出来的人达不到我的水平，不过肯定比其他公司的厉害，工资要求不高，不会漫天开价，可以先拿一些简单的模块让我优化试试，试用期可以先拿岗位最低工资，达到要求后转正再加到我期望的薪资



重点：
rust语言有一个很了不起的概念，叫做生命周期，无论你用的是不是C++还是C，理解透彻了这个概念并且付诸行动去改进代码习惯，性能会有突破性提升，，这是个巨大的性能突破点(需要强调的是，几乎所有语言理解了这一点性能都会有提升)

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





问了一下deepseek，答案有所变化，应该是有厂家联系过deepseek改过测试数据，但是你依然可以这么问，限定一下测试条件:
![1](https://github.com/user-attachments/assets/124e547b-f275-4c6b-bfa9-8e848f925e84)
![2](https://github.com/user-attachments/assets/8f247d72-cae1-4fca-8e40-ce114dd1bbd0)
![3](https://github.com/user-attachments/assets/26003e29-161d-45ea-8ff3-f8e313df45e1)
![4](https://github.com/user-attachments/assets/60b50fc1-d306-4557-82b8-e135832f27ad)
或者更极端一点，这么问 

在2ghz多核环境中，仅使用1个核并且用C++实现http服务器，使用wrk创建100条连接测试，本机内解析一次http请求，再查询一次redis，将查询结果以http响应和json格式返回，并且访问redis的tcp连接只有1条，内部数据还有加锁解锁，并且没有绑定cpu的情况下，qps达到5万是什么水平










vs2022编译选项设置：
![QQ20250630-075541](https://github.com/user-attachments/assets/e48d1fee-c0fb-440d-bb53-1fc784cbc389)

![QQ20250630-075551](https://github.com/user-attachments/assets/6fd00da1-3e50-4a33-9172-64032b01911d)

![QQ20250630-075602](https://github.com/user-attachments/assets/b80ccb84-048f-4090-bc79-b6c5ac38d156)

![QQ20250630-075621](https://github.com/user-attachments/assets/64aeff04-3978-4fe6-a21f-99cbec8498dc)

![QQ20250630-081443](https://github.com/user-attachments/assets/544e0d9f-1f97-4960-8c10-c828d2b75d34)
![QQ20250630-081454](https://github.com/user-attachments/assets/97f3efe0-2a98-4dbb-bf97-fba6ebd34549)

![QQ20250630-152617](https://github.com/user-attachments/assets/0a570152-e83f-4566-be2b-ad7934b90886)

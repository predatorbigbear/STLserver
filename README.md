


http处理模块中http头部主要处理了6个，分别是 Host   Transfer_Encoding    Connection    Content_Length    Content_Type     Expect












在main中设置ioPool->setThreadNum(success, 1);         m1.setMultiRedisRead(ioPool, success, "127.0.0.1", 6379, 1);         

使用单核单redis连接测试

wrk测试命令如下：
wrk -t4 -c100 -d60s  -s /home/download/post.lua http://127.0.0.1:8085/4

post.lua中Transfer-Encoding 如果要设置chunked的话，那么自己需要准备一些数据进行测试，所以我现在post.lua中Transfer-Encoding中改了不带chunked参数







//////////////////////////////////////////////////////


20250713  因生活压力大，再过几天找不到好工作决定转行了，看看有没有公司可以给个岗位。


我反省了一下，我要向别人学习的地方还很多，比如算法（其实我算法很菜，像 深度优先搜索   广度优先搜索  优先队列‌  拓扑排序 动态规划  回溯法 分治策略优化  滑动窗口  贪心算法  图算法  树算法都是弱点，在解决复杂问题时处理办法会很低效, 我编程最主要的性能优势就两点   第一点是大规模变量复用，避免被创建析构开销影响算法效率，这个复用的程序可以做到整个工程的级别（在完全由我设计的情况下）    第二点是精准判断是否需要拷贝，这个时机是基于生命周期进行考虑的，然后根据最大化零拷贝的原则对程序实现进行优化（然后这个零拷贝也是可以做到整个工程的级别）   ），另外像高效率排查问题的能力也有待加强，而且掌握的编程语言技能也比较单一，主要局限于C++， 像框架设计，团队协作能力也有待加强，并且八股也回答得不是很好，面试很吃亏， 所以还是打算虚心学习的。但是，嵌入式和游戏开发真的不要找我了，这些真的不会。这个工程最多维护到20号了，21号要去转行了






重点：
这个服务器的实现思想是基于生命周期实现的


















///////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////



20250629更新  具体编译方法请查看本人boss上的简历   联系电话是135*****610



20250708   包含头部解析的接口4 wrk测试qps过程视频，经过多次性能优化，qps对比原本没有解析http头部字段的成绩相比，仅仅只是下降了1500-2000：
https://b23.tv/RyW1sNP




旧的测试视频：

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
/////////////////////////////////////////////////////////////////////////////////////////////////



 































vs2022编译选项设置：
![QQ20250630-075541](https://github.com/user-attachments/assets/e48d1fee-c0fb-440d-bb53-1fc784cbc389)

![QQ20250630-075551](https://github.com/user-attachments/assets/6fd00da1-3e50-4a33-9172-64032b01911d)

![QQ20250630-075602](https://github.com/user-attachments/assets/b80ccb84-048f-4090-bc79-b6c5ac38d156)

![QQ20250630-075621](https://github.com/user-attachments/assets/64aeff04-3978-4fe6-a21f-99cbec8498dc)

![QQ20250630-081443](https://github.com/user-attachments/assets/544e0d9f-1f97-4960-8c10-c828d2b75d34)
![QQ20250630-081454](https://github.com/user-attachments/assets/97f3efe0-2a98-4dbb-bf97-fba6ebd34549)

![QQ20250630-152617](https://github.com/user-attachments/assets/0a570152-e83f-4566-be2b-ad7934b90886)

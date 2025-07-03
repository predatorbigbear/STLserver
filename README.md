20250703  因生活压力大，两周后找不到好工作决定转行了，如果有公司需要的话，可以进行基础组件性能优化开发，顺便可以传授一些STL高性能编程知识，有很多是业界不为人知的，虽然可能教出来的人达不到我的水平，不过肯定比其他公司的厉害，工资要求不高，不会漫天开价，可以先拿一些简单的模块让我优化试试，试用期可以先拿岗位最低工资，达到要求后转正再加到我期望的薪资

看过一些大厂的C++开源产品，举例两个来说说：
首先是腾讯的TARS：
1腾讯tars TC_URL类返回string的接口可以改造成返回string_view的
2 类似TC_URL::type2String这样的接口可以预先在类中定义几个静态的const类型的string_view返回
3 至于一些在函数内临时生成string的函数，比如TC_URL::getRequest，可以预先在类中创建一个string类数组或者vector，预先分配一些string，然后在函数中使用时用引用调用数组或者容器中的某个string使用，使用引用的好处是你还可以命名成自己喜欢的名字，性能会强很多，类似string，vector这样的容器反复创建对性能开销影响非常大，90%以上的程序员不能良好做到高效复用重型数据结构，根本不懂得什么类型变量可以临时创建，什么类型应该复用，有很多本来实现很高效的算法，最后性能都被不断创建的重型对象拖垮
4像一些string=""的操作直接使用.clear()函数就行了
5像一些substr之类的操作可以改用stl算法实现，返回迭代器区间就行，
6TC_URL::simplePath这个函数里面sNewPath.erase(dotPos, 2);可以改用stl算法实现，返回迭代器区间，然后sNewPath.substr(sNewPath.length()-2)可以复用前面的迭代器区间，使用std::equal高效比对，根本没有必要又去生成string，接下来后面的操作也是一样了，可以一直用迭代器完成，直到最后要返回的时候直接用{首迭代器，尾迭代器}高效返回即可，过程中可以避免不断生成新的string对象
7push_back改成empalce_back提升性能，emplace_back可以在尾部位置直接生成，性能更强
8 TC_Http::getLine 一开始reserve倒是不错的做法，不过前面还是说了，应该使用复用变量的做法，这样性能能提升好几倍
9 所有返回string的函数如果不方便使用string_view返回的话，可以考虑在参数区间增加一个string &变量传入，改变string&变量的值，性能会更强
10 setHeader函数可以改成存储string_view数据，因为存储的内容在发送数据之前都不会被清空，所以用string_view就行了，在发送数据时再将string_view数据拷贝即可

然后是阿里的async_simple：
1 阿里的async_simple，水平确实很高，主要是async_simple文件夹内部的实现，模板应用水平比腾讯的强很多，想不到这么强，因为内部没有什么运算相关的操作，所以不好评价
2  其主要的性能问题存在于demo example中，像临时生成string ifstream问题都存在，比较严重的问题就是istringstream在循环中反复创建了，因为这些主要是demo，所以不好评价是不是高级工程师写的，至于在字符串操作，容器使用方面， 两者基本都是停留在使用容器阶段，不过以阿里在模板方面的能力，将来如果掌握了STL高性能编程技术，可能会吊打腾讯




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

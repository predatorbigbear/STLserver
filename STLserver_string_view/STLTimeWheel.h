#pragma once


//参考网上时间轮的概念和实现，但做出调整以实现更高性能与效率
//每一秒时间轮盘内存储的容器为一个链表，链表内的每一个节点内为everySecondFunctionNumber个回调函数格子
//如果目标秒内空间已满，则分两步走
//首先在当前目标秒内链表尝试添加一个新节点，将回调函数放入
//若上面失败则尝试放进目标秒+1秒的空间内，直至所有秒都装满为止则放弃放置，返回失败
//如此每秒内只需要维护首尾指针即可，放置和复位都更为高效

//这样比单纯链表节点装载单个回调函数处理高效
//比纯动态数组 扩容时需要双倍空间要省空间


#include <memory>
#include <functional>
#include <tuple>
#include <mutex>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <chrono>
#include <forward_list>

struct STLTimeWheel
{
	//用于设置定时器的io_context
	//定时器每次轮转定时时间
	//定时轮盘数量
	//每个定时轮盘内容纳回调函数的格子数量
	STLTimeWheel(const std::shared_ptr<boost::asio::io_context> &ioc,
		const unsigned int checkSecond = 1 ,const unsigned int wheelNum = 60, const unsigned int everySecondFunctionNumber = 1000);



	//插入函数 
	//turnNum 表示从当前m_WheelInfoIter的跳转间隔
	bool insert(const std::function<void()> &callBack ,const unsigned int turnNum);




private:
	struct WheelInformation
	{
		//装载回调函数的单链表
		//节点类型为everySecondFunctionNumber个回调函数格子
		std::forward_list<std::unique_ptr<std::function<void()>[]>> functionList{};

		//用于追加节点时用的迭代器
		std::forward_list<std::unique_ptr<std::function<void()>[]>>::const_iterator listAppendIter{ functionList.cbefore_begin() };

		//当前轮盘内格子节点位置
		std::forward_list<std::unique_ptr<std::function<void()>[]>>::const_iterator listIter{};

		//每个轮盘内格子当前有效指向的最后地址
		std::function<void()> *bufferIter{};
	};


	//开始执行
	void start();




private:
	//定时器
	std::unique_ptr<boost::asio::steady_timer> m_timer{};  

	//定时器每次轮转定时时间
	int m_checkSecond{};

	//定时轮盘数量
	int m_wheelNum{};

	//每个定时轮盘内容纳回调函数的格子数量
	int m_everySecondFunctionNumber{};

	//操作锁
	std::mutex m_mutex;

	//装载存储轮盘信息的数组
	std::unique_ptr<WheelInformation[]>m_WheelInformationPtr{};

	//首地址
	WheelInformation *m_WheelInfoBegin{};

	//尾地址
	WheelInformation *m_WheelInfoEnd{};

	//当前地址
	WheelInformation *m_WheelInfoIter{};

	bool m_hasInit{false};
};

#pragma once



#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <ext/stdio_filebuf.h>


#include "concurrentqueue.h"
#include "IOcontextPool.h"
#include <cstdio>
#include <ctime>
#include<fstream>
#include<iostream>
#include<filesystem>
#include<string_view>
#include<atomic>
#include<vector>


struct PRINTHEX
{

	PRINTHEX(const char* data, const unsigned int len)
		:m_data(data), m_len(len)
	{}

	PRINTHEX(const unsigned char* data, const unsigned int len)
		:m_data(reinterpret_cast<const char*>(data)), m_len(len)
	{}

	const char* m_data{};
	const unsigned int m_len{};
};

// 日志模块采取直接写入缓存部分的方式以提高性能
// 异步log通过在一块超大缓冲区内反复写入实现
// 发送给无锁队列的为string_view，不作拷贝，根据一个临界值大小来保证数据不被覆写
struct ASYNCLOG
{
	ASYNCLOG(const char* logFileName,const std::shared_ptr<IOcontextPool> &ioPool, bool& result,
		const int overTime = 60, const int bufferSize = 20480);                                         //构造函数


	template<typename T, size_t N, typename ...ARGS>
	void writeLog(const T(&arr)[N], const ARGS&...args)
	{
		//std::lock_guard<std::mutex>l1{ m_logMutex };
		m_logMutex.lock();
		makeReadyTime();
		sendLog(arr, args...);
	}


	template<typename T, typename ...ARGS>
	void writeLog(const T& t1, const ARGS&...args)
	{
		//std::lock_guard<std::mutex>l1{ m_logMutex 
		m_logMutex.lock();
		makeReadyTime();
		sendLog(t1, args...);
	}
	


	template<typename T>
	void writeLog(const T& t1)
	{
		//std::lock_guard<std::mutex>l1{ m_logMutex };
		m_logMutex.lock();
		makeReadyTime();
		sendLog(t1);
	}


	template<typename T, size_t N>
	void writeLog(const T(&arr)[N])
	{
		//std::lock_guard<std::mutex>l1{ m_logMutex };
		m_logMutex.lock();
		makeReadyTime();
		sendLog(arr);
	}


private:
	std::ofstream m_file;
	std::unique_ptr<boost::asio::posix::stream_descriptor>m_asyncFile{};           //异步写入文件描述符



	//待投递队列，未/等待拼凑消息的队列
	//使用开源无锁队列进一步提升qps ，这是github地址  https://github.com/cameron314/concurrentqueue
	moodycamel::ConcurrentQueue<std::string_view>m_messageList;

	int m_overTime{};                                                             //定时触发时间
	std::unique_ptr<boost::asio::steady_timer>m_timer{};

	std::atomic<bool>m_write{ false };                                            //是否开启异步写入操作

	std::mutex m_logMutex;
	time_t m_time_seconds{};
	struct tm* m_ptm{};
	char m_date[19]{ ' ' };
	int m_num{};
	int m_temp{};
	int m_temp1{};
	std::unique_ptr<char[]>m_Buffer{};
	unsigned int m_bufferSize{};                              //buffer缓冲区空间
	unsigned int m_beginSize{};                               //本次写入开始位置
	unsigned int m_nowSize{};                                 //本次写入内容长度
	const unsigned int m_checkSize{};                         //需要进行写入检查的临界值
	static constexpr int m_dataSize{ 19 };
	char* m_ch{};
	bool m_check{ false };


private:
	void StartCheckLog();                                                         //开启循环处理日志函数

	void startWrite();                                                            //开始写入函数

	void writeLoop();

	void fastReadyTime();                                                          //获取当前时间直接写入buffer方式

	void makeReadyTime();

	ASYNCLOG& operator<<(const char* log);

	ASYNCLOG& operator<<(const std::string& log);

	ASYNCLOG& operator<<(const std::vector<std::string>& log);

	ASYNCLOG& operator<<(const int num);

	ASYNCLOG& operator<<(const unsigned int num);

	ASYNCLOG& operator<<(const char ch);

	ASYNCLOG& operator<<(const std::string_view log);

	ASYNCLOG& operator<<(const PRINTHEX &log);

	template<typename T, size_t N>
	ASYNCLOG& operator<<(const T(&arr)[N])
	{
		// TODO: 在此处插入 return 语句
		for (m_num = 0; m_num != N; ++m_num)
		{
			*this << arr[m_num];
		}
		return *this;
	}

	template<typename T, size_t N, typename ...ARGS>
	void sendLog(const T(&arr)[N], const ARGS&...args)
	{
		*this << arr << ' ';
		sendLog(args...);
	}


	template<typename T, typename ...ARGS>
	void sendLog(const T& t1, const ARGS&...args)
	{
		*this << t1 << ' ';
		sendLog(args...);
	}



	template<typename T>
	void sendLog(const T& t1)
	{
		*this << t1 << '\n';
		if (m_nowSize >= m_checkSize)
		{
			m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
			m_beginSize += m_nowSize;
			m_nowSize = 0;
			startWrite();
		}

		m_logMutex.unlock();
	}


	template<typename T, size_t N>
	void sendLog(const T(&arr)[N])
	{
		*this << arr << '\n';
		if (m_nowSize >= m_checkSize)
		{
			m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
			m_beginSize += m_nowSize;
			m_nowSize = 0;
			startWrite();
		}
		m_logMutex.unlock();
	}
};




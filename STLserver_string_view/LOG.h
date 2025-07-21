#pragma once



#include <boost/asio/steady_timer.hpp>


#include "IOcontextPool.h"
#include <cstdio>
#include <time.h>
#include<fstream>
#include<iostream>
#include<filesystem>

// 日志模块采取直接写入缓存部分的方式以提高性能
struct LOG
{
	LOG(const char *logFileName , std::shared_ptr<IOcontextPool> ioPool ,bool &result , const int overTime=60,const int bufferSize=20480);                                         //构造函数


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


	int m_overTime{};                                                             //定时触发时间
	std::shared_ptr<boost::asio::steady_timer>m_timer{};
	bool m_hasLog{ false };

	std::mutex m_logMutex;
	time_t m_time_seconds{};
	struct tm m_ptm{};
	char m_date[19]{ ' ' };
	int m_num{};
	int m_temp{};
	int m_temp1{};
	std::unique_ptr<char[]>m_Buffer{};
	int m_bufferSize{};
	int m_nowSize{};
	static constexpr int m_dataSize{ 19 };
	char *m_ch{};
	bool m_check{ false };

	const int m_delNum{ 1000000000 };

private:
	void StartCheckLog();                                                         //开启循环处理日志函数

	void fastReadyTime();                                                          //获取当前时间直接写入buffer方式

	void makeReadyTime();

	LOG& operator<<(const char *log);

	LOG& operator<<(const std::string &log);

	LOG& operator<<(const int num);

	LOG& operator<<(const unsigned int num);

	LOG& operator<<(const char ch);

	LOG& operator<<(const std::string_view log);

	template<typename T, size_t N>
	LOG& operator<<(const T(&arr)[N])
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
		m_logMutex.unlock();
	}

	template<typename T, size_t N>
	void sendLog(const T(&arr)[N])
	{
		*this << arr << '\n';
		m_logMutex.unlock();
	}
};





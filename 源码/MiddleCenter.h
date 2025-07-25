﻿#pragma once


#include "listener.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"
#include "multiSqlWriteSWPool.h"
#include "STLTimeWheel.h"
#include<unordered_map>
#include <zlib.h>

struct MiddleCenter
{
	MiddleCenter();

	~MiddleCenter();



	// 检测写入文件的定时时间
	// 临时存储日志数据的buffer空间大小
	// log池内元素大小
	void setLog(const char *logFileName, std::shared_ptr<IOcontextPool> ioPool, bool &success , const int overTime = 60 ,const int bufferSize = 99999999, const int bufferNum = 1);

	// 默认网页文件目录
	// http处理池内元素个数
	// 超时时间
	void setHTTPServer(std::shared_ptr<IOcontextPool> ioPool, bool& success, const std::string &tcpAddress, const std::string &doc_root ,
		std::vector<std::string> &&fileVec,
		const int socketNum , const int timeOut);

	// 是否是主redis，
	// 连接redis的连接数
	// 存放redis数据的buffer大小
	// 存放回环情况下跨越内存首尾数据buffer的大小
	// 最大一次性处理的命令大小
	void setMultiRedisRead(std::shared_ptr<IOcontextPool> ioPool, bool& success, const std::string &redisIP, const unsigned int redisPort, const unsigned int bufferNum = 1,
		const unsigned int memorySize = 819200, const unsigned int outRangeMaxSize = 65535, const unsigned int commandSize = 50
	);


	// 
	void setMultiRedisWrite(std::shared_ptr<IOcontextPool> ioPool, bool& success, const std::string &redisIP, const unsigned int redisPort, const unsigned int bufferNum = 1,
		const unsigned int memorySize = 4096, const unsigned int outRangeMaxSize = 65535, const unsigned int commandSize = 50
	);


	// 
	// 
	// SQL密码
	// SQL  DB
	// 是否是主sql
	// 最大一次性处理的命令大小
	// 连接sql的连接数
	void setMultiSqlReadSW(std::shared_ptr<IOcontextPool> ioPool, bool& success, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, 
		const int bufferNum = 1, const unsigned int commandMaxSize = 100,  const unsigned int bufferSize = 819200);



	void setMultiSqlWriteSW(std::shared_ptr<IOcontextPool> ioPool, bool &success , const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const unsigned int commandMaxSize = 50, const int bufferNum = 1);


	void initMysql(bool& success);
	
	void freeMysql();

	void unlock();

	//参数设置详细看STLTimeWheel.h内说明
	void setTimeWheel(std::shared_ptr<IOcontextPool> ioPool, bool &success , const unsigned int checkSecond = 1, const unsigned int wheelNum = 120, const unsigned int everySecondFunctionNumber = 100);


private:
	std::unique_ptr<listener>m_listener;

	bool m_hasSetLog{ false };
	bool m_hasSetListener{false};
	bool m_initSql{ false };

	
	std::shared_ptr<LOGPOOL>m_logPool{};                                        //日志池
	std::shared_ptr<function<void()>>m_unlockFun{};
	std::shared_ptr<std::unordered_map<std::string_view, std::string>>m_fileMap{};       //文件缓存map，用于GET方法时获取文件使用
	std::vector<std::string>m_fileVec{};               //文件名vector，只需要传入doc_root目录下的文件名即可，不可以清空

	std::ifstream file;


	std::shared_ptr<MULTISQLWRITESWPOOL>m_multiSqlWriteSWPoolMaster{};         //   主数据库写入连接池
	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolMaster{};           //   主数据库读取连接池

	std::shared_ptr<MULTIREDISWRITEPOOL>m_multiRedisWritePoolMaster{};         //   主redis写入连接池
	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolMaster{};           //   主redis读取连接池

	std::shared_ptr<STLTimeWheel>m_timeWheel{};                                 //时间轮定时器


	std::mutex m_mutex;


	//保存时间轮内设置的checkSecond，
	//则所有socket的检查时间为
	//if(!(超时检查时间%checkSecond))
	//      turnNum= 超时检查时间 / checkSecond
	//否则turnNum= 超时检查时间 / checkSecond + 1
	unsigned int m_checkSecond{};


	z_stream zs;

	const unsigned int outbufferLen{ 8096 };
	char* outbuffer{ new char[outbufferLen] };

private:
	//因为主流浏览器，手机都默认支持gzip，所以首先在一开始对文件资源全部进行gzip最高等级压缩
	bool gzip(const char* source, const int sourLen, std::string& outPut);

};

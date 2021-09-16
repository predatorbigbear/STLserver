#pragma once


#include "publicHeader.h"
#include "listener.h"
#include "LOG.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"
#include "multiSqlWriteSWPool.h"
#include "STLTimeWheel.h"


struct MiddleCenter
{
	MiddleCenter();


	void setKeyPlace(const char *publicKey, const char *privateKey);


	// 检测写入文件的定时时间
	// 临时存储日志数据的buffer空间大小
	// log池内元素大小
	void setLog(const char *logFileName, std::shared_ptr<IOcontextPool> ioPool, const int overTime = 60 ,const int bufferSize = 20480, const int bufferNum = 16);

	// 默认网页文件目录
	// http处理池内元素个数
	// 超时时间
	void setHTTPServer(std::shared_ptr<IOcontextPool> ioPool, const std::string &tcpAddress, const std::string &doc_root , const int socketNum , const int timeOut);

	// 是否是主redis，
	// 连接redis的连接数
	// 存放redis数据的buffer大小
	// 存放回环情况下跨越内存首尾数据buffer的大小
	// 最大一次性处理的命令大小
	void setMultiRedisRead(std::shared_ptr<IOcontextPool> ioPool, const std::string &redisIP, const unsigned int redisPort, const bool isMaster = true, const unsigned int bufferNum = 1,
		const unsigned int memorySize = 65535000, const unsigned int outRangeMaxSize = 65535, const unsigned int commandSize = 50
	);


	// 
	void setMultiRedisWrite(std::shared_ptr<IOcontextPool> ioPool, const std::string &redisIP, const unsigned int redisPort, const bool isMaster = true, const unsigned int bufferNum = 1,
		const unsigned int memorySize = 65535000, const unsigned int outRangeMaxSize = 65535, const unsigned int commandSize = 50
	);


	// 
	// 
	// SQL密码
	// SQL  DB
	// 是否是主sql
	// 最大一次性处理的命令大小
	// 连接sql的连接数
	void setMultiSqlReadSW(std::shared_ptr<IOcontextPool> ioPool, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const bool isMaster = true, const unsigned int commandMaxSize = 50, const int bufferNum = 4);



	void setMultiSqlWriteSW(std::shared_ptr<IOcontextPool> ioPool, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const bool isMaster = true, const unsigned int commandMaxSize = 50, const int bufferNum = 4);


	void unlock();

	//参数设置详细看STLTimeWheel.h内说明
	void setTimeWheel(std::shared_ptr<IOcontextPool> ioPool, const unsigned int checkSecond = 1, const unsigned int wheelNum = 60, const unsigned int everySecondFunctionNumber = 1000);


private:
	std::unique_ptr<listener>m_listener;

	bool m_hasSetLog{ false };
	bool m_hasSetListener{false};

	std::shared_ptr<LOGPOOL>m_logPool{};
	std::shared_ptr<function<void()>>m_unlockFun{};

	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolSlave{};            //   分数据库读取连接池
	std::shared_ptr<MULTISQLWRITESWPOOL>m_multiSqlWriteSWPoolMaster{};         //   主数据库写入连接池
	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolMaster{};           //   主数据库读取连接池

	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolSlave{};            //   分redis读取连接池
	std::shared_ptr<MULTIREDISWRITEPOOL>m_multiRedisWritePoolMaster{};         //   主redis写入连接池
	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolMaster{};           //   主redis读取连接池

	std::shared_ptr<STLTimeWheel>m_timeWheel{};                                 //时间轮定时器


	std::mutex m_mutex;

	char *m_publicKeyBuffer{};
	char *m_privateKeyBuffer{};

	char *m_publicKeyBegin{};
	char *m_publicKeyEnd{};
	int m_publicKeyLen{};


	char *m_privateKeyBegin{};
	char *m_privateKeyEnd{};
	int m_privateKeyLen{};

	RSA* m_rsaPublic{};
	RSA* m_rsaPrivate{};

	//保存时间轮内设置的checkSecond，
	//则所有socket的检查时间为
	//if(!(超时检查时间%checkSecond))
	//      turnNum= 超时检查时间 / checkSecond
	//否则turnNum= 超时检查时间 / checkSecond + 1
	unsigned int m_checkSecond{};

};

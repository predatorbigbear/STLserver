#pragma once


#include "publicHeader.h"
#include "readBuffer.h"
#include "httpService.h"
#include "socketPool.h"
#include "LOG.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"


struct HTTPSERVICEPOOL
{
	HTTPSERVICEPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<SocketPool> sockPool ,
		std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPool, std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPool, std::shared_ptr<MULTIREDISWRITEPOOL>multiRedisWritePool,
		std::shared_ptr<std::function<void()>>start , std::shared_ptr<LOGPOOL> logPool , const unsigned int timeOut, int beginSize = 200, int newSize = 10);

	void getNextBuffer(std::shared_ptr<HTTPSERVICE> &outBuffer);

	void getBackElem(std::shared_ptr<HTTPSERVICE> &buffer);













private:
	std::shared_ptr<io_context> m_ioc;

	std::shared_ptr<SocketPool> m_sockPool;

	std::mutex m_listMutex;

	std::list<std::shared_ptr<HTTPSERVICE>> m_bufferList;
	std::list<std::shared_ptr<HTTPSERVICE>>::iterator m_iterNow;
	bool m_success{};

	int m_newSize{};
	int i{};

	unsigned int m_timeOut;
	std::shared_ptr<std::function<void()>>m_start;
	std::unique_ptr<boost::asio::steady_timer> m_timer;
	std::shared_ptr<LOG> m_log{};
	std::shared_ptr<LOGPOOL>m_logPool{};
	std::shared_ptr<IOcontextPool> m_ioPool{};
	
	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPool{};
	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPool{};
	std::shared_ptr<MULTIREDISWRITEPOOL>m_multiRedisWritePool{};


	std::shared_ptr<HTTPSERVICE> m_temp;

private:
	void ready(int beginSize);
};
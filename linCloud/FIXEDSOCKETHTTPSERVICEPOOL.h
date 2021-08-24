#pragma once



#include "publicHeader.h"
#include "readBuffer.h"
#include "LOG.h"
#include "httpService.h"
#include "FixedSocketPool.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"




struct FIXSOCKETHTTPSERVICEPOOL
{
	FIXSOCKETHTTPSERVICEPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>> startFunction, std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPool, 
		std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPool, std::shared_ptr<MULTIREDISWRITEPOOL>multiRedisWritePool,std::shared_ptr<LOGPOOL> logPool,
		const unsigned int timeOut, int beginSize = 200);


	//void getNextBuffer(std::shared_ptr<ReadBuffer> & outBffer);

	//void getBackElem(const std::shared_ptr<ReadBuffer> &buffer);

	//void setNotifyAccept();








private:
	std::shared_ptr<io_context> m_ioc{};

	std::mutex m_listMutex;

	std::unique_ptr<std::shared_ptr<ReadBuffer>[]> m_bufferList{};
	std::shared_ptr<ReadBuffer> *m_iterNow{};
	std::shared_ptr<ReadBuffer> *m_iterEnd{};
	std::shared_ptr<ReadBuffer> *m_tempIter{};

	int i{};
	int m_nowSize{};
	int m_temp{};
	int m_beginSize{};
	bool m_startAccept{ true };
	std::shared_ptr<std::function<void()>>m_startFunction{};
	std::shared_ptr<std::function<void()>>m_starthttpServicePool{};
	std::unique_ptr<boost::asio::steady_timer> m_timer{};
	std::shared_ptr<LOG>m_log{};
	std::shared_ptr<IOcontextPool> m_ioPool{};


	std::shared_ptr<ReadBuffer> m_tempReadBuffer{};

private:
	//void ready(int beginSize);

};
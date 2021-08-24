#pragma once


#include "publicHeader.h"
#include "readBuffer.h"
#include "httpService.h"
#include "LOG.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"
#include "multiSqlWriteSWPool.h"


struct FixedHTTPSERVICEPOOL
{
	FixedHTTPSERVICEPOOL(std::shared_ptr<IOcontextPool> ioPool,
		std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPoolSlave,
		std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPoolMaster, std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPoolSlave,
		std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPoolMaster,
		std::shared_ptr<MULTIREDISWRITEPOOL>multiRedisWritePoolMaster, std::shared_ptr<MULTISQLWRITESWPOOL>multiSqlWriteSWPoolMaster,
	    std::shared_ptr<std::function<void()>>reAccept, std::shared_ptr<LOGPOOL> logPool, const unsigned int timeOut, 
		char *publicKeyBegin, char *publicKeyEnd, int publicKeyLen, char *privateKeyBegin, char *privateKeyEnd, int privateKeyLen,
		RSA* rsaPublic, RSA* rsaPrivate,
		int beginSize = 200);

	void getNextBuffer(std::shared_ptr<HTTPSERVICE> &outBuffer);

	void getBackElem(std::shared_ptr<HTTPSERVICE> &buffer);

	void setNotifyAccept();

	bool ready();



private:
	std::shared_ptr<io_context> m_ioc{};

	std::mutex m_listMutex;

	std::unique_ptr<std::shared_ptr<HTTPSERVICE>[]> m_bufferList{};
	std::shared_ptr<HTTPSERVICE> *m_iterNow{};
	std::shared_ptr<HTTPSERVICE> *m_iterEnd{};

	bool m_success{};

	int i{};

	unsigned int m_timeOut{};
	unsigned int m_beginSize{};
	std::shared_ptr<std::function<void()>> m_reAccept{};

	std::shared_ptr<LOG> m_log{};
	std::shared_ptr<LOGPOOL>m_logPool{};
	std::shared_ptr<IOcontextPool> m_ioPool{};

	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolSlave{};            //   分数据库读取连接池
	std::shared_ptr<MULTISQLWRITESWPOOL>m_multiSqlWriteSWPoolMaster{};         //   主数据库写入连接池
	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolMaster{};           //   主数据库读取连接池

	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolSlave{};            //   分redis读取连接池
	std::shared_ptr<MULTIREDISWRITEPOOL>m_multiRedisWritePoolMaster{};         //   主redis写入连接池
	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolMaster{};           //   主redis读取连接池

	bool m_startAccept{ true };

	std::shared_ptr<HTTPSERVICE> m_temp{};

	char *m_publicKeyBegin{};
	char *m_publicKeyEnd{};
	int m_publicKeyLen{};


	char *m_privateKeyBegin{};
	char *m_privateKeyEnd{};
	int m_privateKeyLen{};

	RSA* m_rsaPublic{};
	RSA* m_rsaPrivate{};

};
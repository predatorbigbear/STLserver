#pragma once


#include "spinLock.h"
#include "readBuffer.h"
#include "webService.h"
#include "ASYNCLOG.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"
#include "multiSqlWriteSWPool.h"
#include "CheckIP.h"
#include "randomCodeGenerator.h"




struct FixedWEBSERVICEPOOL
{
	FixedWEBSERVICEPOOL(const std::shared_ptr<IOcontextPool>& ioPool,
		const std::string& doc_root,
		const std::shared_ptr<MULTISQLREADSWPOOL>& multiSqlReadSWPoolMaster,
		const std::shared_ptr<MULTIREDISREADPOOL>& multiRedisReadPoolMaster,
		const std::shared_ptr<MULTIREDISWRITEPOOL>& multiRedisWritePoolMaster,
		const std::shared_ptr<MULTISQLWRITESWPOOL>& multiSqlWriteSWPoolMaster,
		const std::shared_ptr<std::function<void()>>& reAccept,
		const std::shared_ptr<LOGPOOL>& logPool,
		const std::shared_ptr<STLTimeWheel>& timeWheel,
		const std::shared_ptr<std::vector<std::string>>& fileVec,
		const std::shared_ptr<std::vector<std::string>>& BGfileVec,
		const unsigned int timeOut,
		const std::shared_ptr<std::function<void(std::shared_ptr<WEBSERVICE>&)>>& cleanFun,
		const std::shared_ptr<CHECKIP>& checkIP,
		const std::shared_ptr<RandomCodeGenerator>& randomCode,
		int beginSize = 200);

	void getNextBuffer(std::shared_ptr<WEBSERVICE> &outBuffer);

	void getBackElem(std::shared_ptr<WEBSERVICE> &buffer);

	void setNotifyAccept();

	bool ready();



private:
	const std::shared_ptr<RandomCodeGenerator>m_randomCode{};

	const std::shared_ptr<CHECKIP>m_checkIP{};

	const std::shared_ptr<std::function<void(std::shared_ptr<WEBSERVICE>&)>>  m_cleanFun{};

	std::shared_ptr<io_context> m_ioc{};

	SpinLock m_listMutex;

	const std::string &m_doc_root;

	std::unique_ptr<std::shared_ptr<WEBSERVICE>[]> m_bufferList{};
	std::shared_ptr<WEBSERVICE> *m_iterNow{};
	std::shared_ptr<WEBSERVICE> *m_iterEnd{};
	const std::shared_ptr<STLTimeWheel> m_timeWheel{};
	const std::shared_ptr<std::vector<std::string>>m_fileVec{};
	const std::shared_ptr<std::vector<std::string>>m_BGfileVec{};

	bool m_success{};

	int i{};

	unsigned int m_timeOut{};
	unsigned int m_beginSize{};
	const std::shared_ptr<std::function<void()>> m_reAccept{};

	std::shared_ptr<ASYNCLOG> m_log{};
	const std::shared_ptr<LOGPOOL>m_logPool{};
	const std::shared_ptr<IOcontextPool> m_ioPool{};

	const std::shared_ptr<MULTISQLWRITESWPOOL>m_multiSqlWriteSWPoolMaster{};         //   主数据库写入连接池
	const std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolMaster{};           //   主数据库读取连接池

	const std::shared_ptr<MULTIREDISWRITEPOOL>m_multiRedisWritePoolMaster{};         //   主redis写入连接池
	const std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolMaster{};           //   主redis读取连接池

	bool m_startAccept{ true };

	std::shared_ptr<WEBSERVICE> m_temp{};

};
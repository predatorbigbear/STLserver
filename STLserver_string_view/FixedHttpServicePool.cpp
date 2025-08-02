#include "FixedHttpServicePool.h"


FixedHTTPSERVICEPOOL::FixedHTTPSERVICEPOOL(std::shared_ptr<IOcontextPool> ioPool, const std::string &doc_root,
	std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPoolMaster,
	std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPoolMaster,
	std::shared_ptr<MULTIREDISWRITEPOOL>multiRedisWritePoolMaster, std::shared_ptr<MULTISQLWRITESWPOOL>multiSqlWriteSWPoolMaster,
	std::shared_ptr<std::function<void()>>reAccept,
	std::shared_ptr<LOGPOOL> logPool, std::shared_ptr<STLTimeWheel> timeWheel,
	const std::shared_ptr<std::unordered_map<std::string_view, std::string>>fileMap,
	const unsigned int timeOut, const std::shared_ptr<std::function<void(std::shared_ptr<HTTPSERVICE>&)>> &cleanFun,
	int beginSize):
	m_ioPool(ioPool), m_reAccept(reAccept), m_logPool(logPool), m_doc_root(doc_root),
	m_multiSqlReadSWPoolMaster(multiSqlReadSWPoolMaster), m_fileMap(fileMap),
	m_multiRedisReadPoolMaster(multiRedisReadPoolMaster), m_multiRedisWritePoolMaster(multiRedisWritePoolMaster), m_multiSqlWriteSWPoolMaster(multiSqlWriteSWPoolMaster),
	m_timeOut(timeOut), m_beginSize(beginSize), m_timeWheel(timeWheel), m_cleanFun(cleanFun)
{
	try
	{
		if (!ioPool)
			throw std::runtime_error("ioPool is null");
		if (beginSize <= 0)
			throw std::runtime_error("beginSize is invaild");
		if (!reAccept)
			throw std::runtime_error("reAccept is invaild");
		if (!logPool)
			throw std::runtime_error("logPool is null");
		if (!timeOut)
			throw std::runtime_error("timeOut is invaild");
		if (!multiSqlReadSWPoolMaster)
			throw std::runtime_error("multiSqlReadSWPoolMaster is null");
		if (!multiRedisReadPoolMaster)
			throw std::runtime_error("multiRedisReadPoolMaster is null");
		if (!multiRedisWritePoolMaster)
			throw std::runtime_error("multiRedisWritePoolMaster is null");
		if (!multiSqlWriteSWPoolMaster)
			throw std::runtime_error("multiSqlWriteSWPoolMaster is null");
		if (doc_root.empty())
			throw std::runtime_error("doc_root is empty");
		if(!timeWheel)
			throw std::runtime_error("timeWheel is nullptr");


		m_log = m_logPool->getLogNext();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << '\n';
	}
}





bool FixedHTTPSERVICEPOOL::ready()
{
	try
	{
		m_iterEnd = nullptr;
		if (!m_bufferList)
		{
			m_bufferList.reset(new std::shared_ptr<HTTPSERVICE>[m_beginSize]);
			m_iterNow = m_bufferList.get();
			for (i = 0; i != m_beginSize; ++i)
			{
				m_success = true;
				*m_iterNow++ = std::make_shared<HTTPSERVICE>(m_ioPool->getIoNext(), m_logPool->getLogNext(), m_doc_root,
					m_multiSqlReadSWPoolMaster->getSqlNext(),
					m_multiRedisReadPoolMaster->getRedisNext(),
					m_multiRedisWritePoolMaster->getRedisNext(), m_multiSqlWriteSWPoolMaster->getSqlNext(),
					m_timeWheel,m_fileMap,
					m_timeOut, m_success,i+1, m_cleanFun
					);
				if (!m_success)
					break;
				m_iterEnd = m_iterNow;
			}
			if (m_bufferList && m_iterEnd)
			{
				m_iterNow = m_bufferList.get();
				return true;
			}
		}
	}
	catch (const std::exception &e)
	{
		if (m_bufferList && m_iterEnd)
		{
			m_iterNow = m_bufferList.get();
		}
	}
	return false;
}





void FixedHTTPSERVICEPOOL::getNextBuffer(std::shared_ptr<HTTPSERVICE>& outBuffer)
{
	//std::lock_guard<std::mutex>m1{ m_listMutex };
	m_listMutex.lock();
	if (m_bufferList)
	{
		if (m_iterNow == m_iterEnd)
		{
			outBuffer = nullptr;
		}
		else
		{
			outBuffer = *m_iterNow;
			++m_iterNow;
		}
	}
	else
		outBuffer = nullptr;
	m_listMutex.unlock();
}





void FixedHTTPSERVICEPOOL::getBackElem(std::shared_ptr<HTTPSERVICE>& buffer)
{
	//std::lock_guard<mutex>m1{ m_listMutex };
	m_listMutex.lock();
	if (buffer)
	{
		if (m_bufferList && m_iterNow != m_bufferList.get())
		{
			--m_iterNow;
			*m_iterNow = std::move(buffer);
			if (!m_startAccept)
			{
				m_startAccept = true;
				//l1.~lock_guard();
				m_listMutex.unlock();
				(*m_reAccept)();
				return;
			}
		}
	}
	m_listMutex.unlock();
}



void FixedHTTPSERVICEPOOL::setNotifyAccept()
{
	m_listMutex.lock();
	m_startAccept = false;
	m_listMutex.unlock();
}


#include "FixedHttpServicePool.h"


FixedHTTPSERVICEPOOL::FixedHTTPSERVICEPOOL(std::shared_ptr<IOcontextPool> ioPool, 
	std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPoolSlave,
	std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPoolMaster, std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPoolSlave,
	std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPoolMaster,
	std::shared_ptr<MULTIREDISWRITEPOOL>multiRedisWritePoolMaster, std::shared_ptr<MULTISQLWRITESWPOOL>multiSqlWriteSWPoolMaster,
	std::shared_ptr<std::function<void()>>reAccept,
	std::shared_ptr<LOGPOOL> logPool,
	const unsigned int timeOut, 
	char *publicKeyBegin, char *publicKeyEnd, int publicKeyLen, char *privateKeyBegin, char *privateKeyEnd, int privateKeyLen,
	RSA* rsaPublic, RSA* rsaPrivate,
	int beginSize) :
	m_ioPool(ioPool), m_reAccept(reAccept), m_logPool(logPool), 
	m_multiSqlReadSWPoolSlave(multiSqlReadSWPoolSlave), m_multiSqlReadSWPoolMaster(multiSqlReadSWPoolMaster), m_multiRedisReadPoolSlave(multiRedisReadPoolSlave),
	m_multiRedisReadPoolMaster(multiRedisReadPoolMaster), m_multiRedisWritePoolMaster(multiRedisWritePoolMaster), m_multiSqlWriteSWPoolMaster(multiSqlWriteSWPoolMaster),
	m_publicKeyBegin(publicKeyBegin), m_publicKeyEnd(publicKeyEnd), m_publicKeyLen(publicKeyLen), m_privateKeyBegin(privateKeyBegin), m_privateKeyEnd(privateKeyEnd), m_privateKeyLen(privateKeyLen),
	m_timeOut(timeOut), m_beginSize(beginSize),
	m_rsaPublic(rsaPublic), m_rsaPrivate(rsaPrivate)
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
		if (!multiSqlReadSWPoolSlave)
			throw std::runtime_error("multiSqlReadSWPoolSlave is null");
		if (!multiSqlReadSWPoolMaster)
			throw std::runtime_error("multiSqlReadSWPoolMaster is null");
		if (!multiRedisReadPoolSlave)
			throw std::runtime_error("multiRedisReadPoolSlave is null");
		if (!multiRedisReadPoolMaster)
			throw std::runtime_error("multiRedisReadPoolMaster is null");
		if (!multiRedisWritePoolMaster)
			throw std::runtime_error("multiRedisWritePoolMaster is null");
		if (!multiSqlWriteSWPoolMaster)
			throw std::runtime_error("multiSqlWriteSWPoolMaster is null");
		if(!publicKeyBegin)
			throw std::runtime_error("publicKeyBegin is null");
		if (!publicKeyEnd)
			throw std::runtime_error("publicKeyEnd is null");
		if (!publicKeyLen)
			throw std::runtime_error("publicKeyLen is null");
		if (!privateKeyBegin)
			throw std::runtime_error("privateKeyBegin is null");
		if (!privateKeyEnd)
			throw std::runtime_error("privateKeyEnd is null");
		if (!privateKeyLen)
			throw std::runtime_error("privateKeyLen is null");
		if (!rsaPublic)
			throw std::runtime_error("rsaPublic is null");
		if (!rsaPrivate)
			throw std::runtime_error("rsaPrivate is null");


		m_log = m_logPool->getLogNext();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << '\n';
		throw;
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
				*m_iterNow++ = std::make_shared<HTTPSERVICE>(m_ioPool->getIoNext(), m_logPool->getLogNext(), 
					m_multiSqlReadSWPoolSlave->getSqlNext(),
					m_multiSqlReadSWPoolMaster->getSqlNext(), m_multiRedisReadPoolSlave->getRedisNext(),
					m_multiRedisReadPoolMaster->getRedisNext(),
					m_multiRedisWritePoolMaster->getRedisNext(), m_multiSqlWriteSWPoolMaster->getSqlNext(),
					m_timeOut, m_success,
					m_publicKeyBegin, m_publicKeyEnd, m_publicKeyLen, m_privateKeyBegin, m_privateKeyEnd, m_privateKeyLen,
					m_rsaPublic, m_rsaPrivate
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
			*m_iterNow = buffer;
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


#include "FixedSocketPool.h"

FixedSocketPool::FixedSocketPool(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>> startFunction,
	std::shared_ptr<std::function<void()>> starthttpServicePool,std::shared_ptr<LOG> log, int beginSize) :
	m_ioPool(ioPool), m_beginSize(beginSize), m_startFunction(startFunction), m_starthttpServicePool(starthttpServicePool), m_log(log)
{
	try
	{
		if (!ioPool)
			throw std::runtime_error("ioPool is null");
		if (beginSize <= 0)
			throw std::runtime_error("beginSize is invaild");
		if (!startFunction)
			throw std::runtime_error("startFunction is invaild");
		if (!starthttpServicePool)
			throw std::runtime_error("starthttpServicePool is invaild");
		m_timer.reset(new boost::asio::steady_timer(*(m_ioPool->getIoNext())));
		ready(m_beginSize);
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  , please restart server\n";
	}
}





void FixedSocketPool::ready(int beginSize)
{
	try
	{
		m_iterEnd = nullptr;
		if (!m_bufferList)
		{
			m_bufferList.reset(new std::shared_ptr<ReadBuffer>[beginSize]);
			m_iterNow = m_bufferList.get();
			for (i = 0; i != beginSize; ++i)
			{
				*m_iterNow++ = std::make_shared<ReadBuffer>(m_ioPool->getIoNext());
				m_iterEnd = m_iterNow;
			}
		}
		if (m_bufferList && m_iterEnd)
		{
			m_iterNow = m_bufferList.get();
		}
		m_timer->expires_after(std::chrono::seconds(5));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if (!err)
				(*m_starthttpServicePool)();
		});
	}
	catch (const std::exception &e)
	{
		//m_log->writeLog(__FILE__, __LINE__, e.what());
		if (m_bufferList && m_iterEnd)
		{
			m_iterNow = m_bufferList.get();
		}
	}
}




void FixedSocketPool::getNextBuffer(std::shared_ptr<ReadBuffer>& outBuffer)
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




void FixedSocketPool::getBackElem(const std::shared_ptr<ReadBuffer> &buffer)
{
	//std::lock_guard<std::mutex>l1{ m_listMutex };
	m_listMutex.lock();
	if (buffer)
	{
		buffer->getHasRead() = 0;
		boost::system::error_code err{};
		{
			do
			{
				buffer->getErrCode() = {};
				buffer->getSock()->cancel(buffer->getErrCode());
				if (buffer->getErrCode())
				{
					//m_log->writeLog(__FILE__, __LINE__, buffer->getErrCode().message());
				}
			} while (buffer->getErrCode());
			do
			{
				buffer->getErrCode() = {};
				buffer->getSock()->shutdown(boost::asio::socket_base::shutdown_send, buffer->getErrCode());
				if (buffer->getErrCode())
				{
					if (buffer->getErrCode().message() == "Transport endpoint is not connected")
					{
						break;
					}
					//m_log->writeLog(__FILE__, __LINE__, buffer->getErrCode().message());
				}
			} while (buffer->getErrCode());
			do
			{
				buffer->getErrCode() = {};
				buffer->getSock()->close(buffer->getErrCode());
				if (buffer->getErrCode())
				{
					//m_log->writeLog(__FILE__, __LINE__, buffer->getErrCode().message());
				}
			} while (buffer->getErrCode());
		}
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
					(*m_startFunction)();

					/*      池元素数量少的时候需要使用这个，避免外层锁还没切换好出现通知不了的情况，超过2000以上这里可以去掉了
					m_timer->expires_after(std::chrono::seconds(3));
					m_timer->async_wait([this](const boost::system::error_code &err)
					{
						if (err)
						{


						}
						else
						{
							(*m_startFunction)();
						}
					});  */
					return;
				}
			}
		}
	}
	m_listMutex.unlock();
}





void FixedSocketPool::setNotifyAccept()
{
	m_listMutex.lock();
	m_startAccept = false;
	m_listMutex.unlock();
}

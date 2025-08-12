#include "LOG.h"


LOG::LOG(const char *logFileName, const std::shared_ptr<IOcontextPool> &ioPool, bool &result ,
	const int overTime , const int bufferSize ):m_overTime(overTime),m_bufferSize(bufferSize)
{
	try
	{
		if (overTime <= 0)
			throw std::runtime_error("overTime is null");
		if (!logFileName)
			throw std::runtime_error("logFileName is null");
		if (bufferSize <= 0)
			throw std::runtime_error("bufferSize is invaild");

		m_Buffer.reset(new char[m_bufferSize]);
		m_nowSize = 0;

		m_timer.reset(new boost::asio::steady_timer(*ioPool->getIoNext()));

		std::error_code err{};
		if (std::filesystem::exists(logFileName,err))
		{
			m_file.open(logFileName, std::ios::out);
			if (!m_file)
				throw std::runtime_error(std::string(logFileName) + " can not open");
			m_file << "";
			m_file.close();
		}
		else
		{
			std::ofstream fileTemp(logFileName, std::ios::out);
			if (!fileTemp)
				throw std::runtime_error(std::string(logFileName) + " can not create");
			fileTemp.close();
		}

		m_file.open(logFileName, std::ios::app);
		if (!m_file)
			throw std::runtime_error("can not open " + std::string(logFileName));
		
		result = true;
		StartCheckLog();
	}
	catch (const std::exception &e)
	{
		std::cout << e.what() << "  ,please restart server\n";
		result = false;
	}
}



void LOG::StartCheckLog()
{
	
	m_timer->expires_after(std::chrono::seconds(m_overTime));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		//std::lock_guard<mutex>l1{ m_logMutex };
		if (err)
		{


		}
		else
		{
			m_logMutex.lock();
			if (m_hasLog)
			{
				m_file.write(reinterpret_cast<char*>(m_Buffer.get()), m_nowSize);
				m_nowSize = 0;
				m_file.flush();
				m_hasLog = false;
			}

			m_logMutex.unlock();
			StartCheckLog();
		}
	});
}




void LOG::fastReadyTime()
{
	m_time_seconds = time(0);
	m_ptm = localtime(&m_time_seconds);
	struct tm& ptm{ *m_ptm };
	if (m_bufferSize - m_nowSize > m_dataSize)
	{

		m_ch = m_Buffer.get() + m_nowSize;
		int m_num{ ptm.tm_year + 1900 };
		*m_ch++ = (m_num / 1000) + '0';
		*m_ch++ = (m_num / 100) % 10 + '0';
		*m_ch++ = (m_num / 10) % 10 + '0';
		*m_ch++ = m_num % 10 + '0';
		*m_ch++ = '-';

		m_num = ptm.tm_mon + 1;
		*m_ch++ = (m_num / 10) + '0';
		*m_ch++ = m_num % 10 + '0';
		*m_ch++ = '-';

		*m_ch++ = (ptm.tm_mday / 10) + '0';
		*m_ch++ = ptm.tm_mday % 10 + '0';
		*m_ch++ = ' ';

		*m_ch++ = (ptm.tm_hour / 10) + '0';
		*m_ch++ = ptm.tm_hour % 10 + '0';
		*m_ch++ = ':';

		*m_ch++ = (ptm.tm_min / 10) + '0';
		*m_ch++ = ptm.tm_min % 10 + '0';
		*m_ch++ = ':';

		*m_ch++ = (ptm.tm_sec / 10) + '0';
		*m_ch++ = ptm.tm_sec % 10 + '0';
		m_nowSize += m_dataSize;
	}
	else
	{
		m_timer->cancel();
		m_file.write(reinterpret_cast<char*>(m_Buffer.get()), m_nowSize);
		StartCheckLog();
		m_nowSize = 0;
		if (m_bufferSize > m_dataSize)
		{

			m_ch = m_Buffer.get();
			int m_num{ ptm.tm_year + 1900 };
			*m_ch++ = (m_num / 1000) + '0';
			*m_ch++ = (m_num / 100) % 10 + '0';
			*m_ch++ = (m_num / 10) % 10 + '0';
			*m_ch++ = m_num % 10 + '0';
			*m_ch++ = '-';

			m_num = ptm.tm_mon + 1;
			*m_ch++ = (m_num / 10) + '0';
			*m_ch++ = m_num % 10 + '0';
			*m_ch++ = '-';

			*m_ch++ = (ptm.tm_mday / 10) + '0';
			*m_ch++ = ptm.tm_mday % 10 + '0';
			*m_ch++ = ' ';

			*m_ch++ = (ptm.tm_hour / 10) + '0';
			*m_ch++ = ptm.tm_hour % 10 + '0';
			*m_ch++ = ':';

			*m_ch++ = (ptm.tm_min / 10) + '0';
			*m_ch++ = ptm.tm_min % 10 + '0';
			*m_ch++ = ':';

			*m_ch++ = (ptm.tm_sec / 10) + '0';
			*m_ch++ = ptm.tm_sec % 10 + '0';
			m_nowSize += m_dataSize;
		}
		else
		{

			int m_num{ ptm.tm_year + 1900 };
			m_file << (m_num / 1000) + '0';
			m_file << (m_num / 100) % 10 + '0';
			m_file << (m_num / 10) % 10 + '0';
			m_file << m_num % 10 + '0';
			m_file << '-';

			m_num = ptm.tm_mon + 1;
			m_file << (m_num / 10) + '0';
			m_file << m_num % 10 + '0';
			m_file << '-';

			m_file << (ptm.tm_mday / 10) + '0';
			m_file << ptm.tm_mday % 10 + '0';
			m_file << ' ';

			m_file << (ptm.tm_hour / 10) + '0';
			m_file << ptm.tm_hour % 10 + '0';
			m_file << ':';

			m_file << (ptm.tm_min / 10) + '0';
			m_file << ptm.tm_min % 10 + '0';
			m_file << ':';

			m_file << (ptm.tm_sec / 10) + '0';
			m_file << ptm.tm_sec % 10 + '0';

		}
	}
}




void LOG::makeReadyTime()
{
	fastReadyTime();
	*this << ' ';
}





LOG & LOG::operator<<(const char * log)
{
	// TODO: 在此处插入 return 语句
	if (log)
	{
		int m_num{ strlen(log) };
		if (m_bufferSize - m_nowSize > m_num)
		{
			std::copy(log, log + m_num, m_Buffer.get() + m_nowSize);
			m_nowSize += m_num;
			m_hasLog = true;
		}
		else
		{
			m_timer->cancel();
			m_file.write(reinterpret_cast<char*>(m_Buffer.get()), m_nowSize);
			StartCheckLog();
			m_nowSize = 0;
			if (m_bufferSize > m_num)
			{
				std::copy(log, log + m_num, m_Buffer.get());
				m_nowSize += m_num;
				m_hasLog = true;
			}
			else
			{
				m_file.write(const_cast<char*>(log), m_num);
			}
		}
	}
	return *this;
}



LOG & LOG::operator<<(const std::string &log)
{
	// TODO: 在此处插入 return 语句
	if (!log.empty())
	{
		int m_num{ log.size() };
		if (m_bufferSize - m_nowSize > m_num)
		{
			std::copy(log.cbegin(), log.cend(), m_Buffer.get() + m_nowSize);
			m_nowSize += m_num;
			m_hasLog = true;
		}
		else
		{
			m_timer->cancel();
			m_file.write(reinterpret_cast<char*>(m_Buffer.get()), m_nowSize);
			StartCheckLog();
			m_nowSize = 0;
			if (m_bufferSize > m_num)
			{
				std::copy(log.cbegin(), log.cend(), m_Buffer.get());
				m_nowSize += m_num;
				m_hasLog = true;
			}
			else
			{
				m_file << log;
			}
		}
	}
	return *this;
}


LOG & LOG::operator<<(const int  num)
{
	// TODO: 在此处插入 return 语句
	bool m_check{ false };
	if (m_bufferSize - m_nowSize > 11)
	{
		m_ch = m_Buffer.get() + m_nowSize;
		int m_temp{ num };
		if (num < 0)
		{
			m_temp *= -1;
			*m_ch++ = '-';
			++m_nowSize;
		}

		int m_num{ m_temp / 1000000000 };
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 100000000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 10000000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 1000000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 100000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 10000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 1000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 100 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 10 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_hasLog = true;
	}
	else
	{
	    m_timer->cancel();
	    m_file.write(reinterpret_cast<char*>(m_Buffer.get()), m_nowSize);
		StartCheckLog();
	    m_nowSize = 0;
		if (m_bufferSize > 11)
		{
			m_ch = m_Buffer.get();
			int m_temp{ num };
			if (num < 0)
			{
				m_temp *= -1;
				*m_ch++ = '-';
				++m_nowSize;
			}
			int m_num{ m_temp / 1000000000 };
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 100000000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 10000000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 1000000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 100000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 10000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 1000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 100 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 10 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}
			m_hasLog = true;
		}
		else
		{

			int m_temp{ num };
			if (num < 0)
			{
				m_temp *= -1;
				m_file << '-';
			}
			int m_num{ m_temp / 1000000000 };
			if (m_check)
			{
				m_file << m_num + '0';
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
				}
			}

			m_num = m_temp / 100000000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
				
				}
			}

			m_num = m_temp / 10000000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
			
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 1000000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 100000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 10000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 1000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 100 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 10 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}
		}
	}
	return *this;
}


LOG & LOG::operator<<(const unsigned int  num)
{
	// TODO: 在此处插入 return 语句
	bool m_check{ false };
	if (m_bufferSize - m_nowSize > 11)
	{
		m_ch = m_Buffer.get() + m_nowSize;
		int m_temp{ num };

		int m_num{ m_temp / 1000000000 };
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 100000000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 10000000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 1000000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 100000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 10000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 1000 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 100 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp / 10 % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_num = m_temp % 10;
		if (m_check)
		{
			*m_ch++ = m_num + '0';
			++m_nowSize;
		}
		else
		{
			if (m_num)
			{
				m_check = true;
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
		}

		m_hasLog = true;
	}
	else
	{
	    m_timer->cancel();
		m_file.write(reinterpret_cast<char*>(m_Buffer.get()), m_nowSize);
		StartCheckLog();
		m_nowSize = 0;
		if (m_bufferSize > 11)
		{
			m_ch = m_Buffer.get();
			int m_temp{ num };
			

			int m_num{ m_temp / 1000000000 };
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 100000000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 10000000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 1000000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 100000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 10000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 1000 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 100 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp / 10 % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}

			m_num = m_temp % 10;
			if (m_check)
			{
				*m_ch++ = m_num + '0';
				++m_nowSize;
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					*m_ch++ = m_num + '0';
					++m_nowSize;
				}
			}
			m_hasLog = true;
		}
		else
		{
			int m_temp{ num };


			int m_num{ m_temp / 1000000000 };
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 100000000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 10000000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 1000000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 100000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 10000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 1000 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 100 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp / 10 % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

			m_num = m_temp % 10;
			if (m_check)
			{
				m_file << m_num + '0';
				
			}
			else
			{
				if (m_num)
				{
					m_check = true;
					m_file << m_num + '0';
					
				}
			}

		}
	}
	return *this;
}


LOG & LOG::operator<<(const char ch)
{
	// TODO: 在此处插入 return 语句
	if (m_bufferSize - m_nowSize > 1)
	{
		m_ch = m_Buffer.get() + m_nowSize;
		*m_ch++ = ch;
		++m_nowSize;
		m_hasLog = true;
	}
	else
	{
		m_timer->cancel();
		m_file.write(reinterpret_cast<char*>(m_Buffer.get()), m_nowSize);
		StartCheckLog();
		m_nowSize = 0;
		if (m_bufferSize > 1)
		{
			*m_ch++ = ch;
			++m_nowSize;
			m_hasLog = true;
		}
		else
		{
			m_file << static_cast<char>(ch);
		}
	}
	return *this;
}


LOG & LOG::operator<<(const std::string_view log)
{
	// TODO: 在此处插入 return 语句
	if (!log.empty())
	{
		int m_num{ log.size() };
		if (m_bufferSize - m_nowSize > m_num)
		{
			std::copy(log.cbegin(), log.cend(), m_Buffer.get() + m_nowSize);
			m_nowSize += m_num;
			m_hasLog = true;
		}
		else
		{
			m_timer->cancel();
			m_file.write(reinterpret_cast<char*>(m_Buffer.get()), m_nowSize);
			StartCheckLog();
			m_nowSize = 0;
			if (m_bufferSize > m_num)
			{
				std::copy(log.cbegin(), log.cend(), m_Buffer.get());
				m_nowSize += m_num;
				m_hasLog = true;
			}
			else
			{
				m_file << log;
			}
		}
	}
	return *this;
}








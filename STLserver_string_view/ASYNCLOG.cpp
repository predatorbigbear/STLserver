#include "ASYNCLOG.h"


ASYNCLOG::ASYNCLOG(const char* logFileName, std::shared_ptr<IOcontextPool> ioPool, bool& result, const int overTime, const int bufferSize) :
	m_overTime(overTime), m_bufferSize(bufferSize * 10), m_messageList(100), m_checkSize(bufferSize * 2)
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
		m_nowSize = m_beginSize = 0;

		m_timer.reset(new boost::asio::steady_timer(*ioPool->getIoNext()));

		std::error_code err{};
		if (std::filesystem::exists(logFileName, err))
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

		m_asyncFile.reset(new boost::asio::posix::stream_descriptor(*ioPool->getIoNext(), static_cast<__gnu_cxx::stdio_filebuf<char>*>(m_file.rdbuf())->fd()));

		result = true;
		StartCheckLog();
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << "  ,please restart server\n";
		result = false;
	}
}



void ASYNCLOG::StartCheckLog()
{
	m_timer->expires_after(std::chrono::seconds(m_overTime));
	m_timer->async_wait([this](const boost::system::error_code& err)
	{
		if (err)
		{

		}
		else
		{
			m_logMutex.lock();
			if (m_nowSize)
			{
				m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
				m_beginSize += m_nowSize;
				m_nowSize = 0;
			}
			m_logMutex.unlock();
			startWrite();
			StartCheckLog();
		}
	});
}

void ASYNCLOG::startWrite()
{
	if (!m_write.load())
	{
		m_write.store(true);
		writeLoop();
	}

}


void ASYNCLOG::writeLoop()
{
	std::string_view tempView;
	if (m_messageList.try_dequeue(tempView))
	{
		boost::asio::async_write(*m_asyncFile, boost::asio::buffer(tempView.data(), tempView.size()), [this](const boost::system::error_code& ec, size_t bytes_written)
		{
			writeLoop();
		});
	}
	else
	{
		m_write.store(false);
	}
}




void ASYNCLOG::fastReadyTime()
{
	m_time_seconds = time(0);
	m_ptm = localtime(&m_time_seconds);
	struct tm& ptm{ *m_ptm };
	if (m_bufferSize - (m_beginSize + m_nowSize) > m_dataSize)
	{
		m_ch = m_Buffer.get() + m_beginSize + m_nowSize;
		m_num = ptm.tm_year + 1900;
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
		if (m_nowSize)
		{
			m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
			startWrite();
		}
		m_beginSize = m_nowSize = 0;
		if (m_bufferSize > m_dataSize)
		{

			m_ch = m_Buffer.get();
			m_num = ptm.tm_year + 1900;
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
	}
}




void ASYNCLOG::makeReadyTime()
{
	fastReadyTime();
	*this << ' ';
}





ASYNCLOG& ASYNCLOG::operator<<(const char* log)
{
	// TODO: 在此处插入 return 语句
	if (log)
	{
		m_num = strlen(log);
		if (m_bufferSize - (m_beginSize + m_nowSize) > m_num)
		{
			std::copy(log, log + m_num, m_Buffer.get() + m_beginSize + m_nowSize);
			m_nowSize += m_num;
		}
		else
		{
			if (m_nowSize)
			{
				m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
				startWrite();
			}
			m_beginSize = m_nowSize = 0;
			if (m_bufferSize > m_num)
			{
				std::copy(log, log + m_num, m_Buffer.get());
				m_nowSize += m_num;
			}
		}
	}
	return *this;
}



ASYNCLOG& ASYNCLOG::operator<<(const std::string& log)
{
	// TODO: 在此处插入 return 语句
	if (!log.empty())
	{
		m_num = log.size();
		if (m_bufferSize - (m_beginSize + m_nowSize) > m_num)
		{
			std::copy(log.cbegin(), log.cend(), m_Buffer.get() + m_beginSize + m_nowSize);
			m_nowSize += m_num;
		}
		else
		{
			if (m_nowSize)
			{
				m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
				startWrite();
			}
			m_beginSize = m_nowSize = 0;
			if (m_bufferSize > m_num)
			{
				std::copy(log.cbegin(), log.cend(), m_Buffer.get());
				m_nowSize += m_num;
			}
		}
	}
	return *this;
}


ASYNCLOG& ASYNCLOG::operator<<(const int  num)
{
	// TODO: 在此处插入 return 语句
	m_check = false;
	if (m_bufferSize - (m_beginSize + m_nowSize) > 11)
	{
		m_ch = m_Buffer.get() + m_nowSize + m_beginSize;
		m_temp = num;
		if (num < 0)
		{
			m_temp *= -1;
			*m_ch++ = '-';
			++m_nowSize;
		}

		m_num = m_temp / 1000000000;
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

	}
	else
	{
		if (m_nowSize)
		{
			m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
			startWrite();
		}
		m_beginSize = m_nowSize = 0;
		if (m_bufferSize > 11)
		{
			m_ch = m_Buffer.get();
			m_temp = num;
			if (num < 0)
			{
				m_temp *= -1;
				*m_ch++ = '-';
				++m_nowSize;
			}
			m_num = m_temp / 1000000000;
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

		}
	}
	return *this;
}


ASYNCLOG& ASYNCLOG::operator<<(const unsigned int  num)
{
	// TODO: 在此处插入 return 语句
	m_check = false;
	if (m_bufferSize - (m_beginSize + m_nowSize) > 11)
	{
		m_ch = m_Buffer.get() + m_nowSize + m_beginSize;
		m_temp = num;

		m_num = m_temp / 1000000000;
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

	}
	else
	{
		if (m_nowSize)
		{
			m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
			startWrite();
		}
		m_beginSize = m_nowSize = 0;
		if (m_bufferSize > 11)
		{
			m_ch = m_Buffer.get();
			m_temp = num;


			m_num = m_temp / 1000000000;
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

		}
	}
	return *this;
}


ASYNCLOG& ASYNCLOG::operator<<(const char ch)
{
	// TODO: 在此处插入 return 语句
	if (m_bufferSize - (m_beginSize + m_nowSize) > 1)
	{
		m_ch = m_Buffer.get() + m_nowSize + m_beginSize;
		*m_ch++ = ch;
		++m_nowSize;
	}
	else
	{
		if (m_nowSize)
		{
			m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
			startWrite();
		}
		m_beginSize = m_nowSize = 0;
		if (m_bufferSize > 1)
		{
			m_ch = m_Buffer.get();
			*m_ch++ = ch;
			++m_nowSize;

		}
	}
	return *this;
}


ASYNCLOG& ASYNCLOG::operator<<(const std::string_view log)
{
	// TODO: 在此处插入 return 语句
	if (!log.empty())
	{
		m_num = log.size();
		if (m_bufferSize - (m_beginSize + m_nowSize) > m_num)
		{
			std::copy(log.cbegin(), log.cend(), m_Buffer.get() + m_nowSize + m_beginSize);
			m_nowSize += m_num;

		}
		else
		{
			if (m_nowSize)
			{
				m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
				startWrite();
			}
			m_beginSize = m_nowSize = 0;
			if (m_bufferSize > m_num)
			{
				std::copy(log.cbegin(), log.cend(), m_Buffer.get());
				m_nowSize += m_num;

			}
		}
	}
	return *this;
}







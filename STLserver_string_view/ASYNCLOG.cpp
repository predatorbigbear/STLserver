#include "ASYNCLOG.h"


ASYNCLOG::ASYNCLOG(const char* logFileName, const std::shared_ptr<IOcontextPool> &ioPool, bool& result,
	const int overTime, const int bufferSize) :
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


# ifdef __linux__
		m_file.open(logFileName, std::ios::app);
		if (!m_file)
			throw std::runtime_error("can not open " + std::string(logFileName));

		m_asyncFile.reset(new boost::asio::posix::stream_descriptor(*ioPool->getIoNext(), static_cast<__gnu_cxx::stdio_filebuf<char>*>(m_file.rdbuf())->fd()));
#elif define _WIN32

		// 1. 打开文件
		// 注意：FILE_FLAG_OVERLAPPED 是必须的
		// GENERIC_WRITE: 写权限
		// OPEN_ALWAYS: 如果文件存在则打开，不存在则创建
		file_handle = ::CreateFileA(
			logFileName,
			GENERIC_WRITE,
			FILE_SHARE_READ, // 允许其他进程读取
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL
		);

		if (file_handle == INVALID_HANDLE_VALUE) 
			throw std::runtime_error(std::string("无法打开文件，错误代码: ") + GetLastError());

		// 2. 获取当前文件大小（用于确定追加位置）
		LARGE_INTEGER file_size;
		if (!::GetFileSizeEx(file_handle, &file_size)) 
		{
			::CloseHandle(file_handle);
			throw std::runtime_error(std::string("无法获取文件大小，错误代码: ")+GetLastError());
			::CloseHandle(file_handle);
		}

		// 文件末尾的偏移量即为当前文件大小
		offset = static_cast<uint64_t>(file_size.QuadPart);

		m_asyncFile.reset(new boost::asio::windows::random_access_handle(*ioPool->getIoNext(), file_handle));

#endif

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
	if (!m_write.load(std::memory_order_acquire))
	{
		m_write.store(true, std::memory_order_release);
		writeLoop();
	}

}


void ASYNCLOG::writeLoop()
{
	std::string_view tempView;
	if (m_messageList.try_dequeue(tempView))
	{
# ifdef __linux__
		boost::asio::async_write(*m_asyncFile, boost::asio::buffer(tempView.data(), tempView.size()), [this](const boost::system::error_code& ec, size_t bytes_written)
		{
			writeLoop();
		});
#elif define _WIN32
		GetFileSizeEx(file_handle, &file_size);

		offset = static_cast<uint64_t>(file_size.QuadPart);

		// 5. 发起异步写入到指定偏移量（文件末尾）
		boost::asio::async_write_at(
			*m_asyncFile,
			offset,              // 关键：写入位置设为当前文件大小
			boost::asio::buffer(tempView.data(), tempView.size()),
			[this](const boost::system::error_code& ec, std::size_t bytes_written)
			{
				writeLoop();
			}
		);

#endif

	}
	else
	{
		m_write.store(false, std::memory_order_release);
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



ASYNCLOG& ASYNCLOG::operator<<(const std::vector<std::string>& log)
{
	// TODO: 在此处插入 return 语句
	if(log.empty())
		return *this;
	for (auto& str : log)
		this->operator<<(str);
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



ASYNCLOG& ASYNCLOG::operator<<(const PRINTHEX& log)
{
	// TODO: 在此处插入 return 语句
	if (log.m_data && log.m_len)
	{
		static const std::string hexBegin{ "hexBegin:" }, hexEnd{ "hexEnd" };
		int neeSize = log.m_len * 3 + hexBegin.size() + hexEnd.size();
		static const char hexBuf[] = "0123456789ABCDEF";
		const char* begin{ log.m_data }, * end{ log.m_data + log.m_len };
		char* beginBuf{ };
		if (m_bufferSize - (m_beginSize + m_nowSize) > neeSize)
		{
			beginBuf = m_Buffer.get() + m_beginSize + m_nowSize;
			std::copy(hexBegin.cbegin(), hexBegin.cend(), beginBuf);
			beginBuf += hexBegin.size();
			do
			{	
				*beginBuf++ = hexBuf[static_cast<unsigned char>(*begin) / 16];
				*beginBuf++ = hexBuf[static_cast<unsigned char>(*begin) % 16];
				*beginBuf++ = ' ';
			} while (++begin != end);
			std::copy(hexEnd.cbegin(), hexEnd.cend(), beginBuf);
			beginBuf += hexEnd.size();
			m_nowSize += neeSize;

		}
		else
		{
			if (m_nowSize)
			{
				m_messageList.try_enqueue(std::string_view(m_Buffer.get() + m_beginSize, m_nowSize));
				startWrite();
			}
			m_beginSize = m_nowSize = 0;
			if (m_bufferSize > neeSize)
			{
				beginBuf = m_Buffer.get();
				std::copy(hexBegin.cbegin(), hexBegin.cend(), beginBuf);
				beginBuf += hexBegin.size();
				do
				{
					*beginBuf++ = hexBuf[static_cast<unsigned char>(*begin) / 16];
					*beginBuf++ = hexBuf[static_cast<unsigned char>(*begin) % 16];
					*beginBuf++ = ' ';
				} while (++begin != end);
				std::copy(hexEnd.cbegin(), hexEnd.cend(), beginBuf);
				beginBuf += hexEnd.size();
				m_nowSize += neeSize;

			}
		}
	}

	return *this;
}







#include "CheckIP.h"



CHECKIP::CHECKIP(const std::shared_ptr<IOcontextPool> &ioPool,const std::shared_ptr<ASYNCLOG> &log, 
	const std::shared_ptr<STLTimeWheel> &timeWheel,
	const std::string& host,
	const std::string& port, const std::string& country, 
	const std::string& saveFile,
	const unsigned int checkTime):m_host(host), m_port(port), m_log(log),
	m_country(country), m_timeWheel(timeWheel), m_saveFile(saveFile),m_checkTime(checkTime)
{
	m_ioc = ioPool->getIoNext();

	m_vec.resize(256, {});

	m_temp.resize(256, {});

	m_timer.reset(new boost::asio::steady_timer(*ioPool->getIoNext()));

	update();

}





bool CHECKIP::is_china_ipv4(const std::string& ip)
{
	if (ip.empty())
		return false;
	std::string::const_iterator numBegin{}, numEnd{};
	int num{1}, index{ -1 };

	numBegin = ip.cbegin();
	numEnd = std::find(numBegin, ip.cend(), '.');
	if (numEnd == ip.cend())
		return false;
	num = std::accumulate(std::make_reverse_iterator(numEnd), std::make_reverse_iterator(numBegin), 0, [&](int sum, const char ch)
	{
		if (++index)
			num *= 10;
		return sum += (ch - '0') * num;
	});
	if (num < 0 || num>255)
		return false;

	unsigned int target_ip = ip_to_int(ip);

	m_mutex.lock_shared();

	for (const auto &pair : m_vec[num])
	{
		if ((target_ip & pair.first) == pair.second)
		{
			m_mutex.unlock_shared();
			return true;
		}
	}


	m_mutex.unlock_shared();
	return false;
}




unsigned int CHECKIP::ip_to_int(const std::string& ip)
{
	struct in_addr addr;
	inet_pton(AF_INET, ip.c_str(), &addr);
	return ntohl(addr.s_addr);
}


unsigned int CHECKIP::ip_to_int_char(const char* ip)
{
	struct in_addr addr;
	inet_pton(AF_INET, ip, &addr);
	return ntohl(addr.s_addr);
}



void CHECKIP::updateLoop()
{
	m_timer->expires_after(std::chrono::seconds(m_checkTime));
	m_timer->async_wait([this](const boost::system::error_code& err)
	{
		if (err)
		{


		}
		else
		{
			update();
		}
	});
}



void CHECKIP::update()
{
	resetParse();
	resetResult();
	resetResolver();
	resetSocket();
	resetReadLen();
	resetVector();
	resetFile();
	startResolver();
	updateLoop();
}

void CHECKIP::resetResult()
{
	m_result = false;
}

void CHECKIP::logResult()
{
	if (m_result)
	{
		m_file << "end\n";
		m_file.close();
		m_mutex.lock();
		m_vec.swap(m_temp);
		m_mutex.unlock();
		m_log->writeLog("update ip success");
	}
	else
	{
		if (m_file)
		{
			m_file << "error\n";
			m_file.close();
		}
		m_log->writeLog("update ip fail");
	}
}


void CHECKIP::resetResolver()
{
	m_resolver.reset(new boost::asio::ip::tcp::resolver(*m_ioc));
}


void CHECKIP::resetSocket()
{
	m_socket.reset(new boost::asio::ip::tcp::socket(*m_ioc));
	boost::system::error_code ec{};
	m_socket->set_option(boost::asio::socket_base::keep_alive(true), ec);
}


void CHECKIP::resetReadLen()
{
	m_readLen = 0;
}

void CHECKIP::resetVector()
{
	for (int i = 0; i != 256; ++i)
		m_temp[i].clear();
}


void CHECKIP::startResolver()
{
	m_resolver->async_resolve(m_host, m_port,
		boost::bind(&CHECKIP::handle_resolve, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::results));
}

void CHECKIP::handle_resolve(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::results_type endpoints)
{
	if (!err) 
	{
		boost::asio::async_connect(*m_socket, endpoints,
			boost::bind(&CHECKIP::handle_connect, this,
				boost::asio::placeholders::error));
	}
	else 
	{
		logResult();
	}
}


void CHECKIP::handle_connect(const boost::system::error_code& err)
{
	if (!err)
	{
		// 启动异步读取
		sendCommand();
	}
	else
	{
		logResult();
	}
}

void CHECKIP::sendCommand()
{
	boost::asio::async_write(*m_socket, boost::asio::buffer(m_msg),
		boost::bind(&CHECKIP::handle_write, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}


void CHECKIP::handle_write(const boost::system::error_code& err, size_t len)
{
	if (!err)
	{
		readMessage();
	}
	else
	{
		logResult();
	}
}



void CHECKIP::readMessage()
{
	m_socket->async_read_some(boost::asio::buffer(m_buf.get() + m_readLen, m_bufLen - m_readLen),
		boost::bind(&CHECKIP::handle_read, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}


void CHECKIP::handle_read(const boost::system::error_code& err, size_t len)
{
	if (!err)
	{
		parseMessage(len);
	}
	else
	{
		//启动回收流程
		logResult();
		m_ec = {};
		m_socket->shutdown(boost::asio::socket_base::shutdown_both, m_ec);

		//等待异步shutdown完成
		m_timeWheel->insert([this]() {shutdownSocket(); }, 5);
	}
}


void CHECKIP::parseMessage(const size_t len)
{
	if (m_parse)
	{
		const char* strBegin{ m_buf.get() }, * strEnd{ m_buf.get() + m_readLen + len };

		const char* iterBegin{};

		const char* countryBegin{}, * countryEnd{};

		const char* ipBegin{}, * ipEnd{};

		const char* ipAddressBegin{}, * ipAddressEnd{};

		const char* hostNumBegin{}, * hostNumEnd{};

		static const std::string ipv4{ "ipv4" };

		size_t pos{};
		unsigned int mask_len{};
		unsigned int network{};
		unsigned int mask{};
		int num{}, index{}, result{};

		const char* numBegin{}, *numEnd{};
		while (strBegin != strEnd)
		{

			iterBegin = std::find(strBegin, strEnd, *m_country.cbegin());
			if (iterBegin == strEnd)
			{
				m_readLen = 0;
				break;
			}

			if (std::distance(iterBegin, strEnd) < 100)
			{
				std::copy(iterBegin, strEnd, m_buf.get());
				m_readLen = std::distance(iterBegin, strEnd);
				break;
			}

			countryBegin = iterBegin;

			countryEnd = std::find(countryBegin, strEnd, '|');
			if (countryEnd == strEnd)
			{
				std::copy(iterBegin, strEnd, m_buf.get());
				m_readLen = std::distance(iterBegin, strEnd);
				break;
			}

			if (!std::equal(countryBegin, countryEnd, m_country.cbegin(), m_country.cend()))
			{
				strBegin = countryEnd + 1;
				continue;
			}

			ipBegin = countryEnd + 1;

			ipEnd = std::find(ipBegin, strEnd, '|');
			if (ipEnd == strEnd)
			{
				std::copy(iterBegin, strEnd, m_buf.get());
				m_readLen = std::distance(iterBegin, strEnd);
				break;
			}


			if (!std::equal(ipBegin, ipEnd, ipv4.cbegin(), ipv4.cend()))
			{
				//IPV6情况，直接退出解析
				if (std::distance(ipBegin, ipEnd) == 4)
				{
					m_result = true;
					m_parse = false;
					m_readLen = 0;
					break;
				}
				strBegin = ipEnd + 1;
				continue;
			}


			ipAddressBegin = ipEnd + 1;


			ipAddressEnd = std::find(ipAddressBegin + 1, strEnd, '.');
			if (ipAddressEnd == strEnd)
			{
				std::copy(iterBegin, strEnd, m_buf.get());
				m_readLen = std::distance(iterBegin, strEnd);
				break;
			}

			numBegin = ipAddressBegin;
			numEnd = ipAddressEnd;


			ipAddressEnd = std::find(ipAddressEnd + 1, strEnd, '|');
			if (ipAddressEnd == strEnd)
			{
				std::copy(iterBegin, strEnd, m_buf.get());
				m_readLen = std::distance(iterBegin, strEnd);
				break;
			}

			hostNumBegin = ipAddressEnd + 1;

			hostNumEnd = std::find(hostNumBegin, strEnd, '|');
			if (hostNumEnd == strEnd)
			{
				std::copy(iterBegin, strEnd, m_buf.get());
				m_readLen = std::distance(iterBegin, strEnd);
				break;
			}

			/////////////////////////////////////////////////

			
			index = -1, num = 1;
			result = std::accumulate(std::make_reverse_iterator(numEnd), std::make_reverse_iterator(numBegin), 0, [&](int sum, const char ch)
			{
				if (++index)
					num *= 10;
				return sum += (ch - '0') * num;
			});
			if (result < 0 || result>255)
			{
				strBegin = hostNumEnd + 1;
				continue;
			}

		

			pos = std::distance(numBegin, ipAddressEnd);
			if (pos >= m_strBufLen)
				continue;
			std::copy(numBegin, ipAddressEnd, m_strBuf.get());
			*(m_strBuf.get() + pos) = 0;
			index = -1, num = 1;
			mask_len = std::accumulate(std::make_reverse_iterator(hostNumEnd), std::make_reverse_iterator(hostNumBegin), 0, [&](int sum, const char ch)
			{
				if (++index)
					num *= 10;
				return sum += (ch - '0') * num;
			});

			mask_len = FastLog2(mask_len);
			if (!mask_len)
			{
				strBegin = hostNumEnd + 1;
				continue;
			}
			mask_len = 32 - mask_len;
			network = ip_to_int_char(m_strBuf.get());
			mask = (0xFFFFFFFF << (32 - mask_len));

			m_temp[result].emplace_back(std::make_pair(mask, network & mask));


			///////////////////////////////////////////////////////////////////

			m_file.write(ipAddressBegin, std::distance(ipAddressBegin, ipAddressEnd));
			m_file << '/';
			m_file << mask_len << '\n';



			///////////////////////////////////////////////////////////////////////

			strBegin = hostNumEnd + 1;
			continue;
		}
	}

	readMessage();
}



int CHECKIP::FastLog2(const unsigned int num)
{
	switch (num)
	{
	case 2:
		return 1;
		break;

	case 4:
		return 2;
		break;

	case 8:
		return 3;
		break;

	case 16:
		return 4;
		break;

	case 32:
		return 5;
		break;

	case 64:
		return 6;
		break;

	case 128:
		return 7;
		break;

	case 256:
		return 8;
		break;

	case 512:
		return 9;
		break;

	case 1024:
		return 10;
		break;

	case 2048:
		return 11;
		break;

	case 4096:
		return 12;
		break;

	case 8192:
		return 13;
		break;

	case 16384:
		return 14;
		break;

	case 32768:
		return 15;
		break;

	case 65536:
		return 16;
		break;

	case 131072:
		return 17;
		break;

	case 262144:
		return 18;
		break;

	case 524288:
		return 19;
		break;

	case 1048576:
		return 20;
		break;

	case 2097152:
		return 21;
		break;

	case 4194304:
		return 22;
		break;

	case 8388608:
		return 23;
		break;

	case 16777216:
		return 24;
		break;

	case 33554432:
		return 25;
		break;

	case 67108864:
		return 26;
		break;

	case 134217728:
		return 27;
		break;

	case 268435456:
		return 28;
		break;

	case 536870912:
		return 29;
		break;

	case 1073741824:
		return 30;
		break;

	case 2147483648:
		return 31;
		break;

	default:
		return 0;
		break;
	}
	return 0;
}


void CHECKIP::resetFile()
{
	m_file.open(m_saveFile, std::ios::out);
	if(!m_file)
		return logResult();
	m_file.close();

	m_file.open(m_saveFile, std::ios::app | std::ios::binary);
	if (!m_file)
		return logResult();
	m_file << "start:\n";
}


void CHECKIP::resetParse()
{
	m_parse = true;
}


void CHECKIP::shutdownSocket()
{
	if (m_ec.value() != 107 && m_ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, m_ec.value(), m_ec.message());
		m_socket->shutdown(boost::asio::socket_base::shutdown_both, m_ec);
		m_timeWheel->insert([this]() {shutdownSocket(); }, 5);
	}
	else
	{
		m_socket->cancel(m_ec);
		//等待异步cancel完成
		m_timeWheel->insert([this]() {cancelSocket(); }, 5);
	}
}


void CHECKIP::cancelSocket()
{
	if (m_ec.value() != 107 && m_ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, m_ec.value(), m_ec.message());
		m_socket->cancel(m_ec);
		m_timeWheel->insert([this]() {cancelSocket(); }, 5);
	}
	else
	{
		m_socket->close(m_ec);
		//等待异步cancel完成
		m_timeWheel->insert([this]() {closeSocket(); }, 5);
	}

}


void CHECKIP::closeSocket()
{
	if (m_ec.value() != 107 && m_ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, m_ec.value(), m_ec.message());
		m_socket->close(m_ec);
		m_timeWheel->insert([this]() {closeSocket(); }, 5);
	}
}

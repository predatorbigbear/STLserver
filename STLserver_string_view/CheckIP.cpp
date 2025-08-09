#include "CheckIP.h"



CHECKIP::CHECKIP(const char* ipFileName, std::shared_ptr<IOcontextPool> ioPool, const char* command, std::shared_ptr<ASYNCLOG>log, 
	const unsigned int checkTime):
	m_checkTime(checkTime), m_ipFileName(ipFileName), m_log(log)
{
	if (!log)
		throw std::runtime_error("log is nullptr");

	m_timer.reset(new boost::asio::steady_timer(*ioPool->getIoNext()));

	m_command = command;
	m_command.append(ipFileName);
	m_vec.resize(256, {});

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

	uint32_t target_ip = ip_to_int(ip);

	m_mutex.lock_shared();

	for (const auto pair : m_vec[num])
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




void CHECKIP::executeCommand()
{
	system(m_command.c_str());
}



void CHECKIP::readFile()
{
	bool success{ false };
	try
	{
		std::ifstream file;
		file.open(m_ipFileName, std::ios::binary);
		if (!file)
			return;
		size_t pos{};
		uint32_t mask_len{};
		uint32_t network{};
		uint32_t mask{};
		int num{}, index{}, result{};
		std::string str;
		std::string::iterator strEnd{}, strBegin{};
		const int bufLen{ 100 };
		std::unique_ptr<char[]>buf{ new char[bufLen] };
		std::string::iterator numBegin{}, numEnd{};
		std::vector<std::vector<std::pair<unsigned int, unsigned int>>> temp;
		temp.resize(256, {});
		while (!file.eof())
		{
			std::getline(file, str);
			if (str.empty())
				continue;
			strBegin = str.begin(), strEnd = str.end();
			strEnd = std::remove(strBegin, strEnd, ' ');
			if (strBegin == strEnd)
				continue;

			numBegin = strBegin;
			numEnd = std::find(numBegin, strEnd, '.');
			if (numEnd == strEnd)
				continue;
			index = -1, num = 1;
			result = std::accumulate(std::make_reverse_iterator(numEnd), std::make_reverse_iterator(numBegin), 0, [&](int sum, const char ch)
			{
				if (++index)
					num *= 10;
				return sum += (ch - '0') * num;
			});
			if (result < 0 || result>255)
				continue;
			numEnd = std::find(numEnd + 1, strEnd, '/');
			if (numEnd == strEnd)
				continue;

			pos = std::distance(numBegin, numEnd);
			if (pos >= bufLen)
				continue;
			std::copy(numBegin, numEnd, buf.get());
			*(buf.get() + pos) = 0;
			index = -1, num = 1;
			mask_len = std::accumulate(std::make_reverse_iterator(strEnd), std::make_reverse_iterator(numEnd + 1), 0, [&](int sum, const char ch)
			{
				if (++index)
					num *= 10;
				return sum += (ch - '0') * num;
			});

			network = ip_to_int_char(buf.get());
			mask = (0xFFFFFFFF << (32 - mask_len));

			temp[result].emplace_back(std::make_pair(mask, network & mask));
		}
		file.close();
		

		m_mutex.lock();
		m_vec.swap(temp);
		m_mutex.unlock();
		success = true;
	}
	catch (std::exception& e)
	{
		
	}

	if (success)
	{
		m_log->writeLog("update ip success");
	}
	else
		m_log->writeLog("update ip fail");
}




uint32_t CHECKIP::ip_to_int(const std::string& ip) 
{
	struct in_addr addr;
	inet_pton(AF_INET, ip.c_str(), &addr);
	return ntohl(addr.s_addr);
}


uint32_t CHECKIP::ip_to_int_char(const char* ip)
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
	executeCommand();
	readFile();
	updateLoop();
}

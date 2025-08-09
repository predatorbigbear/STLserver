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
	try
	{
		m_cidr.clear();
		std::ifstream file;
		file.open(m_ipFileName, std::ios::binary);
		if (!file)
			return;
		std::string str;
		while (!file.eof())
		{
			std::getline(file, str);
			if (str.empty())
				continue;
			str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
			if (str.empty())
				continue;
			m_cidr.emplace_back(str);
		}
		file.close();
	}
	catch (std::exception& e)
	{
		m_cidr.clear();
	}
}



void CHECKIP::makeRecord()
{
	bool success{ false };
	if (!m_cidr.empty())
	{
		size_t pos{};
		uint32_t mask_len{};
		uint32_t network{};
		uint32_t mask{};
		int num{}, index{}, result{};
		try
		{
			const int bufLen{ 100 };
			std::unique_ptr<char[]>buf{new char[bufLen]};
			//std::string net_str{};
			std::string::const_iterator numBegin{}, numEnd{};
			std::vector<std::vector<std::pair<unsigned int, unsigned int>>> temp;
			temp.resize(256, {});
			for (const auto& cidr : m_cidr)
			{
				numBegin = cidr.cbegin();
				numEnd = std::find(numBegin, cidr.cend(), '.');
				if (numEnd == cidr.cend())
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
				numEnd = std::find(numEnd + 1, cidr.cend(), '/');
				if (numEnd == cidr.cend())
					continue;

				pos = std::distance(cidr.cbegin(), numEnd);
				if (pos >= bufLen)
					continue;
				std::copy(cidr.cbegin(), numEnd, buf.get());
				*(buf.get() + pos) = 0;
				//net_str = cidr.substr(0, pos);
				index = -1, num = 1;
				mask_len = std::accumulate(std::make_reverse_iterator(cidr.cend()), std::make_reverse_iterator(numEnd + 1), 0, [&](int sum, const char ch)
				{
					if (++index)
						num *= 10;
					return sum += (ch - '0') * num;
				});

				network = ip_to_int_char(buf.get());
				mask = (0xFFFFFFFF << (32 - mask_len));

				temp[result].emplace_back(std::make_pair(mask, network & mask));
			}

			m_mutex.lock();
			m_vec.swap(temp);
			m_mutex.unlock();
			success = true;
		}
		catch (std::exception &e)
		{
			//记录日志
		}
		m_cidr.clear();
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
	makeRecord();
	updateLoop();
}

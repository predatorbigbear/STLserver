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

	update();
}



bool CHECKIP::is_china_ipv4(const std::string& ip)
{
	if (ip.empty())
		return false;
	std::string::const_iterator numBegin{}, numEnd{};
	int num{};

	numBegin = ip.cbegin();
	numEnd = std::find(numBegin, ip.cend(), '.');
	if (numEnd == ip.cend())
		return false;
	num = std::accumulate(std::make_reverse_iterator(numEnd), std::make_reverse_iterator(numBegin), 0);
	if (num < 0 || num>255)
		return false;

	uint32_t target_ip = ip_to_int(ip);

	m_mutex.lock_shared();
	if (m_vec.empty())
	{
		m_mutex.unlock_shared();
		return false;
	}

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



void CHECKIP::makeRecord()
{
	bool success{ false };
	std::vector<std::vector<std::pair<unsigned int, unsigned int>>> temp;
	if (!m_cidr.empty())
	{
		size_t pos{};
		std::string net_str{};
		uint32_t mask_len{};
		uint32_t network{};
		uint32_t mask{};
		std::string::const_iterator numBegin{}, numEnd{};
		int num{};
		try
		{
			temp.resize(256, {});
			for (const auto& cidr : m_cidr)
			{
				numBegin = cidr.cbegin();
				numEnd = std::find(numBegin, cidr.cend(), '.');
				if (numEnd == cidr.cend())
					continue;
				num = std::accumulate(std::make_reverse_iterator(numEnd), std::make_reverse_iterator(numBegin), 0);
				if (num < 0 || num>255)
					continue;
				numEnd = std::find(numEnd + 1, cidr.cend(), '/');
				if (numEnd == cidr.cend())
					continue;

				pos = std::distance(cidr.cbegin(), numEnd);
				net_str = cidr.substr(0, pos);
				mask_len = std::stoi(cidr.substr(pos + 1));

				network = ip_to_int(net_str);
				mask = (0xFFFFFFFF << (32 - mask_len));

				temp[num].emplace_back(std::make_pair(mask, network & mask));
			}
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
		m_mutex.lock();
		m_vec.swap(temp);
		m_mutex.unlock();
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

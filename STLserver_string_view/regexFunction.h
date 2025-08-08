#pragma once


#include<string>
#include<algorithm>
#include<functional>
#include<numeric>
#include<iterator>
#include<vector>
#include<map>
#include<arpa/inet.h>


namespace REGEXFUNCTION
{
	//判断字符串是否是ipv4地址
	static bool isVaildIpv4(const std::string &source)
	{
		if (source.empty())return false;
		std::string::const_iterator iterBegin{ source.cbegin() }, iterEnd{ source.cend() }, lastIter{ source.cbegin() };
		long int vaildTime{}, everyDistance, index{ -1 }, num{ 1 }, totalSum{};
		while ((iterBegin = std::find_if_not(iterBegin, iterEnd, std::bind(std::logical_and<>(), std::bind(std::greater_equal<>(), std::placeholders::_1, '0'), 
			std::bind(std::less_equal<>(), std::placeholders::_1, '9')))) != iterEnd)
		{
			everyDistance = std::distance(lastIter, iterBegin);
			if (!everyDistance || everyDistance > 3 || *iterBegin != '.' ||
				std::accumulate(std::make_reverse_iterator(iterBegin), std::make_reverse_iterator(lastIter), totalSum, [&index, &num](auto &sum, auto const ch)
			{
				if (++index)num *= 10;
				return sum += (ch - '0')*num;
			}) > 255 || ++vaildTime > 3)
				return false;
			totalSum = 0;
			index = -1;
			num = 1;
			lastIter = ++iterBegin;
		}
		everyDistance = std::distance(lastIter, iterBegin);
		return !(!everyDistance || everyDistance > 3 ||
			std::accumulate(std::make_reverse_iterator(iterBegin), std::make_reverse_iterator(lastIter), totalSum, [&index, &num](auto &sum, auto const ch)
		{
			if (++index)num *= 10;
			return sum += (ch - '0')*num;
		}) > 255 || vaildTime != 3);
	}


	// 中国主要IP地址段（CIDR表示法），
	static const std::vector<std::string> CHINA_IP_RANGES
	{
	 "1.0.0.0/8",
	"14.0.0.0/8",
	"27.0.0.0/8",
	"36.0.0.0/8",
	"42.0.0.0/8",
	"49.0.0.0/8",
	"58.0.0.0/8",
	"60.0.0.0/8",
	"61.0.0.0/8",
	"106.0.0.0/8",
	"110.0.0.0/8",
	"111.0.0.0/8",
	"112.0.0.0/8",
	"113.0.0.0/8",
	"114.0.0.0/8",
	"115.0.0.0/8",
	"116.0.0.0/8",
	"117.0.0.0/8",
	"118.0.0.0/8",
	"119.0.0.0/8",
	"120.0.0.0/8",
	"121.0.0.0/8",
	"122.0.0.0/8",
	"123.0.0.0/8",
	"124.0.0.0/8",
	"125.0.0.0/8",
	"171.0.0.0/8",
	"175.0.0.0/8",
	"180.0.0.0/8",
	"182.0.0.0/8",
	"183.0.0.0/8",
	"202.0.0.0/8",
	"203.0.0.0/8",
	"210.0.0.0/8",
	"211.0.0.0/8",
	"218.0.0.0/8",
	"219.0.0.0/8",
	"220.0.0.0/8",
	"221.0.0.0/8",
	"222.0.0.0/8"
	};

	// 将IP字符串转换为32位整数
	static uint32_t ip_to_int(const std::string& ip) {
		struct in_addr addr;
		inet_pton(AF_INET, ip.c_str(), &addr);
		return ntohl(addr.s_addr);
	}

	// 检查IPV4是否属于中国公网ip地址段
	static bool is_china_ip(const std::string& ip)
	{
		uint32_t target_ip = ip_to_int(ip);

		static const std::vector<std::pair<unsigned int, unsigned int>>vec
		{ {4278190080,16777216},{4278190080,234881024},{4278190080,452984832},{4278190080,603979776},
			{4278190080,704643072},{4278190080,822083584},{4278190080,973078528},{4278190080,1006632960},
			{4278190080,1023410176},{4278190080,1778384896},{4278190080,1845493760},{4278190080,1862270976},
			{4278190080,1879048192},{4278190080,1895825408},{4278190080,1912602624},{4278190080,1929379840},
			{4278190080,1946157056},{4278190080,1962934272},{4278190080,1979711488},{4278190080,1996488704},
			{4278190080,2013265920},{4278190080,2030043136},{4278190080,2046820352},{4278190080,2063597568},
			{4278190080,2080374784},{4278190080,2097152000},{4278190080,2868903936},{4278190080,2936012800},
			{4278190080,3019898880},{4278190080,3053453312},{4278190080,3070230528},{4278190080,3388997632},
			{4278190080,3405774848},{4278190080,3523215360},{4278190080,3539992576},{4278190080,3657433088},
			{4278190080,3674210304},{4278190080,3690987520},{4278190080,3707764736},{4278190080,3724541952} };
		for (const auto pair : vec)
		{
			if ((target_ip & pair.first) == pair.second)
				return true;
		}

		return false;
	}

























}

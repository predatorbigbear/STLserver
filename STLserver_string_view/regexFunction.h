#pragma once


#include<string>
#include<algorithm>
#include<functional>
#include<numeric>
#include<string_view>
#include<iterator>
#include<vector>
#include<cctype>
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

     //是否是合法手机号
	/*
	中国移动号段
前三位包括：134、135、136、137、138、139、150、151、152、157、158、159、178、182、183、184、187、188、195、197、198。 ‌


中国联通号段
前三位包括：130、131、132、145、155、156、166、175、176、185、186、196。 ‌


中国电信号段
前三位包括：133、149、153、180、181、189、173、177、190、191、193、199。 ‌


特殊号段
‌中国广电‌：192
‌虚拟运营商‌：移动号段含165、170（3、5、6）；联通号段含167、170（4、7、8、9）；电信号段含162、170（0、1、2）。 ‌
	
	*/
	static bool isVaildPhone(const std::string_view phone)
	{
		if (phone.empty() || phone.size() != 11)
			return false;

		if(!std::all_of(phone.cbegin(), phone.cend(),std::bind(isdigit,std::placeholders::_1)))
			return false;

		//计算前三位值
		int index{ -1 }, num{ 1 };
		num = std::accumulate(std::make_reverse_iterator(phone.cbegin() + 3), std::make_reverse_iterator(phone.cbegin()), 0, [&](int sum, const char ch)
		{
			if (++index)
				num *= 10;
			return sum += (ch - '0') * num;
		});

		switch (num)
		{
		case 134:
		case 135:
		case 136:
		case 137:
		case 138:
		case 139:
		case 150:
		case 151:
		case 152:
		case 157:
		case 158:
		case 159:
		case 178:
		case 182:
		case 183:
		case 184:
		case 187:
		case 188:
		case 195:
		case 197:
		case 198:
		case 130:
		case 131:
		case 132:
		case 145:
		case 155:
		case 156:
		case 166:
		case 175:
		case 176:
		case 185:
		case 186:
		case 196:
		case 133:
		case 149:
		case 153:
		case 180:
		case 181:
		case 189:
		case 173:
		case 177:
		case 190:
		case 191:
		case 193:
		case 199:
		case 192:
		case 162:
		case 165:
		case 167:
		case 170:
			return true;
			break;

		default:
			return false;
			break;
		}

		return false;

	}
























}

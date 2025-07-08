#pragma once


#include<string>
#include<algorithm>
#include<functional>
#include<numeric>
#include<iterator>


namespace REGEXFUNCTION
{
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



























}

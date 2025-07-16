/*#include<iostream>
#include<boost/regex.hpp>
#include<string>
#include<vector>
#include<chrono>
#include<algorithm>
#include<array>
#include<functional>
#include<regex>
#include<iomanip>

using std::boolalpha;
using std::cout;
using std::array;
using std::string;
using std::bind;
using std::logical_or;
using std::logical_and;
using std::greater_equal;
using std::less_equal;
using std::find_if;
using std::find_if_not;
using std::vector;

bool isEmail(const string &source)
{
	if (source.empty())
		return false;

	static array<bool, 256>arr{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0
, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1
, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };





	int len{};
	auto iterBegin{ source.cbegin() }, iterEnd{ source.cend() }, iterTemp{ source.cend() };

	if ((iterBegin = find_if(iterBegin, iterEnd, [](auto const ch)
	{
		return !arr[static_cast<unsigned char>(ch)];
	})) == iterEnd || !distance(source.cbegin(), iterBegin) || *iterBegin != '@')
		return false;
	if ((iterTemp = find_if_not(iterBegin + 1, iterEnd, bind(logical_or<>(), bind(logical_and<>(), bind(greater_equal<>(), std::placeholders::_1, '0'), bind(less_equal<>(), std::placeholders::_1, '9')),
		bind(logical_and<>(), bind(greater_equal<>(), std::placeholders::_1, 'a'), bind(less_equal<>(), std::placeholders::_1, 'z'))))) == iterEnd || !distance(iterBegin + 1, iterTemp) || *iterTemp != '.')
		return false;
	iterBegin = iterTemp + 1;
	iterTemp = find_if_not(iterBegin, iterEnd, bind(logical_and<>(), bind(greater_equal<>(), std::placeholders::_1, 'a'), bind(less_equal<>(), std::placeholders::_1, 'z')));
	len = distance(iterBegin + 1, iterTemp);
	if (len < 2 || len>3)
		return false;
	if (iterTemp == iterEnd)
		return true;
	iterBegin = iterTemp;
	if (distance(iterBegin, iterEnd) != 3 || *iterBegin != '.' || distance(iterBegin + 1, (iterTemp = find_if_not(iterBegin + 1, iterEnd, bind(logical_and<>(), bind(greater_equal<>(), std::placeholders::_1, 'a'), bind(less_equal<>(), std::placeholders::_1, 'z'))))) != 2 || iterTemp != iterEnd)
		return false;
	return true;
}



int main()
{
	vector<string>vec{ "grdgerg6uhrthjt0.454","ap090253@163.com","test02369@163.com.cn","test02369@163.com.cn.cn.cm.df","test02369@163.com.cn.cn.cm.df.cfg","test_-063529616@163.com","test_-063529616@163.com","1075716088@qq.com","ap023652_-_-fewFFE@qq.com.cn","www.blah.com/blah.htm#blah","fwef","0","-.1","ggargregageggarg" };
	int i{};
	boost::regex r1{ "([0-9A-Za-z\\-_\\.]+)@([0-9a-z]+\\.[a-z]{2,3}(\\.[a-z]{2})?)" };
	std::regex r2{ "([0-9A-Za-z\\-_\\.]+)@([0-9a-z]+\\.[a-z]{2,3}(\\.[a-z]{2})?)" };

	isEmail(vec[3]);

	for (auto const &i : vec)
	{
		cout << "boost:" << boolalpha << boost::regex_match(i, r1) << "  "
			<< "std:" << std::regex_match(i, r2) << "  "
			<< "stl function:" << isEmail(i) << '\n';
	}


	i = 0;
	auto t1{ std::chrono::high_resolution_clock::now() };
	while (++i != 100000)
	{
		boost::regex_match(vec[0], r1);
		boost::regex_match(vec[1], r1);
		boost::regex_match(vec[2], r1);
		boost::regex_match(vec[3], r1);
		boost::regex_match(vec[4], r1);
		boost::regex_match(vec[5], r1);
		boost::regex_match(vec[6], r1);
		boost::regex_match(vec[7], r1);
		boost::regex_match(vec[8], r1);
		boost::regex_match(vec[9], r1);
		boost::regex_match(vec[10], r1);
		boost::regex_match(vec[11], r1);
		boost::regex_match(vec[12], r1);
		boost::regex_match(vec[13], r1);
	}
	auto t2{ std::chrono::high_resolution_clock::now() };

	i = 0;
	auto t3{ std::chrono::high_resolution_clock::now() };
	while (++i != 100000)
	{
		std::regex_match(vec[0], r2);
		std::regex_match(vec[1], r2);
		std::regex_match(vec[2], r2);
		std::regex_match(vec[3], r2);
		std::regex_match(vec[4], r2);
		std::regex_match(vec[5], r2);
		std::regex_match(vec[6], r2);
		std::regex_match(vec[7], r2);
		std::regex_match(vec[8], r2);
		std::regex_match(vec[9], r2);
		std::regex_match(vec[10], r2);
		std::regex_match(vec[11], r2);
		std::regex_match(vec[12], r2);
		std::regex_match(vec[13], r2);
	}
	auto t4{ std::chrono::high_resolution_clock::now() };


	i = 0;
	auto t5{ std::chrono::high_resolution_clock::now() };
	while (++i != 100000)
	{
		isEmail(vec[0]);
		isEmail(vec[1]);
		isEmail(vec[2]);
		isEmail(vec[3]);
		isEmail(vec[4]);
		isEmail(vec[6]);
		isEmail(vec[7]);
		isEmail(vec[8]);
		isEmail(vec[9]);
		isEmail(vec[10]);
		isEmail(vec[11]);
		isEmail(vec[12]);
		isEmail(vec[13]);
	}
	auto t6{ std::chrono::high_resolution_clock::now() };

	cout << "boost regex:" << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms\n"
		<< "std regex:" << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count() << " ms\n"
		<< "STL function:" << std::chrono::duration_cast<std::chrono::milliseconds>(t6 - t5).count() << " ms\n";

	return 0;
}


*/
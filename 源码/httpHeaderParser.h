#pragma once

#include<string_view>
#include<algorithm>
#include<functional>
#include<cctype>



//做个高性能http头部零拷贝解析模块
// 这个模块将实现所有http 头部解析处理，需要的可以从这里调用使用
// 分离了就不会影响默认http处理模块的测试qps了
//每一个http头部做成一个类，包含资源初始化，解析，获取状态等操作
//尽可能提供比正则更高效的实现  在一次循环内判断完毕，指针只会向前走，不会向后退，规避正则回溯问题
//因为strong版本考虑比较多，比较难写，所以开发进度会稍微慢点
//每一个类只解析一个HTTP头部，可以根据需要集成到程序中使用
//这个实现文件顺便可以练练手，加强字符串解析处理能力



//每一个解析类每次使用前先调用init
//再调用parse，并判断parse是否成功
//成功后获取string_view对象先使用对应的empty方法，因为设置bool与设置string_view性能开销比差距比较大
//再执行getView获取对应的对象
//parse可能有两种版本
//fast仅做最基本的检查
//strong做尽可能全面的检查
//将函数实现定义在类内是因为分开文件  函数不同类更难查看
//所有解析模块默认命名规则是头部名称+parse
//发现异常字符串情况可以到我的github里面提出，或者加qq 1075716088







struct HOST_PARSER
{
	HOST_PARSER() = default;

	void init()
	{
		port = defaultPort;
		hostNameEmpty = true;
	}

	//isHttp11的传值   http1.1传true    1.0传false
	//仅检查HTTP/1.1下Host字段是否为空
	//‌非法端口格式
	bool parseFast(const char* strbegin, const char* strEnd, bool isHttp11)
	{
		if (!strbegin || !strEnd || std::distance(strEnd, strbegin) > 0)
			return false;

		//HTTP/1.1强制要求请求必须包含Host字段，缺失或值为空    HTTP/1.0允许省略Host字段
		if (strbegin == strEnd)
		{
			if (isHttp11)
				return false;
			return true;
		}

		const char* hostNameBegin, * hostNameEnd, * portBegin, * portEnd;

		hostNameBegin = std::find_if(strbegin, strEnd,
			std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

		if (hostNameBegin == strEnd || *hostNameBegin == ':')
		{
			if (isHttp11)
				return false;
			else
				return true;
		}


		hostNameEnd = std::find(hostNameBegin, strEnd, ':');

		if (hostNameEnd == strEnd)
		{
			hostName = std::string_view(hostNameBegin, std::distance(hostNameBegin, hostNameEnd));
			hostNameEmpty = false;
			return true;
		}

		portBegin = hostNameEnd + 1;
		portEnd = strEnd;

		//因为不需要复用，每次都在一个函数内处理，所以这里用延迟生成的办法创建变量
		int index{ -1 }, num{ 1 }, sum{ 0 };
		//使用自定义实现函数，在判断是否全部是数字字符的情况下，同时完成数值累加计算，避免两次循环调用
		if (!std::all_of(std::make_reverse_iterator(portEnd), std::make_reverse_iterator(portBegin), [&index, &num, &sum](const char ch)
		{
			if (!std::isdigit(ch))
				return false;
			if (++index)
				num *= 10;
			sum += (ch - '0') * num;
			return true;
		}))
			return false;

		if (sum < 0 || sum>65535)
			return false;

		port = sum;

		hostName = std::string_view(hostNameBegin, std::distance(hostNameBegin, hostNameEnd));
		hostNameEmpty = false;
		return true;
	}

	//‌域名字符限制
	//英文字母（a - z，不区分大小写）
	//数字（0 - 9）
	//连字符（-）
	//这里判断域名是否合法
	//检查HTTP/1.1下Host字段是否为空
	//非法端口格式
	//仅检测非中文域名以及非ip地址
	//严格非法字符检测
	bool parseStrong(const char* strbegin, const char* strEnd, bool isHttp11)
	{
		if (!strbegin || !strEnd || std::distance(strEnd, strbegin) > 0)
			return false;

		//HTTP/1.1强制要求请求必须包含Host字段，缺失或值为空    HTTP/1.0允许省略Host字段
		if (strbegin == strEnd)
		{
			if (isHttp11)
				return false;
			return true;
		}

		const char* hostNameBegin{}, * hostNameEnd{}, * portBegin{}, * portEnd{}, * iterBegin{}, * iterEnd{}, * thisBegin{}, * thisEnd{};
		//是否出现过点号
		bool hasDot{ false };
		char ch{};

		//先找到首个不为空格的位置
		hostNameBegin = std::find_if(strbegin, strEnd,
			std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

		if (hostNameBegin == strEnd || *hostNameBegin == ':')
		{
			if (isHttp11)
				return false;
			return true;
		}


		strbegin = thisBegin = hostNameBegin;

		while (strbegin != strEnd)
		{
			if (*(strbegin - 1) == '.')
				thisBegin = iterBegin;

			iterBegin = std::find_if(strbegin, strEnd,
				[](const char ch)
			{
				return !isdigit(ch) && !isalpha(ch);
			});

			//如果这个域名一个点号都没有
			if (iterBegin == strEnd)
			{
				if (!hasDot)
					return false;
				hostName = std::string_view(hostNameBegin, std::distance(hostNameBegin, iterBegin));
				hostNameEmpty = false;
				return true;
			}

			switch (*iterBegin)
			{
				//连字符使用约束
				//禁止出现在开头或结尾（如-example.com或example-.com无效）
				//禁止连续重复（如exa--mple.com无效）
			case '-':
				if (iterBegin == hostNameBegin)
					return false;
				if (iterBegin + 1 != strEnd)
				{
					ch = *(iterBegin + 1);
					if (ch == '-' || ch == '.' || ch == ':')
						return false;
				}
				break;
			case '.':
				//单级标签（如example）长度 ≤ 63字符
				if (std::distance(thisBegin, iterBegin) > 63)
					return false;
				hasDot = true;
				break;
			case ':':
				//如果这个域名一个点号都没有
				if (!hasDot)
					return false;
				//完整域名（含点.）总长 ≤ 253字符
				if (std::distance(hostNameBegin, iterBegin) > 253)
					return false;
				break;
			default:
				return false;
				break;
			}

			if (*iterBegin != ':')
				strbegin = iterBegin + 1;
			else
			{
				hostNameEnd = iterBegin;
				++iterBegin;
				break;
			}
		}

		if (iterBegin == strEnd)
		{
			hostName = std::string_view(hostNameBegin, std::distance(hostNameBegin, hostNameEnd));
			hostNameEmpty = false;
			return true;
		}

		portBegin = iterBegin;
		portEnd = strEnd;

		//因为不需要复用，每次都在一个函数内处理，所以这里用延迟生成的办法创建变量
		int index{ -1 }, num{ 1 }, sum{ 0 };
		//使用自定义实现函数，在判断是否全部是数字字符的情况下，同时完成数值累加计算，避免两次循环调用
		if (!std::all_of(std::make_reverse_iterator(portEnd), std::make_reverse_iterator(portBegin), [&index, &num, &sum](const char ch)
		{
			if (!std::isdigit(ch))
				return false;
			if (++index)
				num *= 10;
			sum += (ch - '0') * num;
			return true;
		}))
			return false;

		if (sum < 0 || sum>65535)
			return false;

		port = sum;

		hostName = std::string_view(hostNameBegin, std::distance(hostNameBegin, hostNameEnd));
		hostNameEmpty = false;
		return true;
	}

	bool isHostNameEmpty()
	{
		return hostNameEmpty;
	}

	std::string_view getHostNameView()
	{
		return hostName;
	}


private:
	bool hostNameEmpty{ true };
	std::string_view hostName{};
	unsigned int port{};
	const unsigned int defaultPort{};     //默认端口，自己根据需要填写
};

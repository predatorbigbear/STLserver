#pragma once





#include <boost/thread.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>



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
#include<unordered_set>
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



using boost::asio::steady_timer;
using boost::thread_group;
using boost::asio::io_context;
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


#define getbit(x,y)   ((x) >> (y)&1)


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

		hostWebSite = 80,
		hostPort = 82,

		HTTPHEADERLEN = 84
	};

};








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

	testMultiRedisParseBodyReadLOT_SIZE_STRING = 8,

	testGetPublicKey = 9,

	testGetClientPublicKey = 10,

	testGetEncryptKey = 11,

	testFirstTime = 12,

	testEncryptLogin = 13,

	testBusiness = 14,

	testLogout = 15,

	testMultiPartFormData = 16,

	successUpload = 17,

	testPingPongJson =18,

	testMakeJson=19,

	testCompareWorkFlow = 20
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





//url编码规则请看   https://baike.baidu.com/item/URL%E7%BC%96%E7%A0%81/3703727?fr=aladdin&qq-pf-to=pcqq.group  

static const std::unordered_map<std::string_view, char>urlMap         
{
{"%08",'\b'} ,{"%09",'\t'},
{"%0A",'\n'},{"%0a",'\n'},{"%0D",'\r'},{"%0d",'\r'},
{"%20",' '}, { "%21",'!'},{"%22",'\"'} ,{"%23",'#'} ,{ "%24",'$'} ,{"%25",'%'} ,{"%26",'&' } ,{"%27", '\''} ,{"%28",'('}, {"%29",')'} ,
{"%2A",'*'},{"%2a",'*'},{"%2B",'+'} ,{"%2b",'+'} ,{"%2C",','},{"%2c",','},{"%2D",'-'},{"%2d",'-'},{"%2E",'.'},{"%2e",'.'},{"%2F",'/'},{"%2f",'/'},
{"%30",'0'},{"%31",'1'},{"%32",'2'},{"%33",'3'},{"%34",'4'},{"%35",'5'},{"%36",'6'},{"%37",'7'},{"%38",'8'},{"%39",'9'},
{"%3A",':'},{ "%3B",';'},{"%3C",'<'},{ "%3D",'='},{"%3E",'>'},{"%3F",'?'},
{"%3a",':'},{ "%3b",';'},{"%3c",'<'},{ "%3d",'='},{"%3e",'>'},{"%3f",'?'},
{"%40",'@'},{"%41",'A'},{"%42",'B'},{"%43",'C'},{"%44",'D'},{"%45",'E'},{"%46",'F'},{"%47",'G'},{"%48",'H'},{"%49",'I'},
{"%4A",'J'},{"%4B",'K'},{"%4C",'L'},{"%4D",'M'},{"%4E",'N'},{"%4F",'O'},
{"%4a",'J'},{"%4b",'K'},{"%4c",'L'},{"%4d",'M'},{"%4e",'N'},{"%4f",'O'},
{"%50",'P'},{"%51",'Q'},{"%52",'R'},{"%53",'S'},{"%54",'T'},{"%55",'U'},{"%56",'V'},{"%57",'W'},{"%58",'X'},{"%59",'Y'},
{"%5A",'Z'},{"%5B",'['},{"%5C",'\\'}, {"%5D",']' },{"%5E",'^'}, {"%5F",'_'},
{"%5a",'Z'},{"%5b",'['},{"%5c",'\\'}, {"%5d",']' },{"%5e",'^'}, {"%5f",'_'},
{"%60",'`'},{"%61",'a'},{"%62",'b'},{"%63",'c'},{"%64",'d'},{"%65",'e'},{"%66",'f'},{"%67",'g'},{"%68",'h'},{"%69",'i'},
{"%6A",'j'},{"%6B",'k'},{"%6C",'l'}, {"%6D",'m' },{"%6E",'n'}, {"%6F",'o'},
{"%6a",'j'},{"%6b",'k'},{"%6c",'l'}, {"%6d",'m' },{"%6e",'n'}, {"%6f",'o'},
{"%70",'p'},{"%71",'q'},{"%72",'r'},{"%73",'s'},{"%74",'t'},{"%75",'u'},{"%76",'v'},{"%77",'w'},{"%78",'x'},{"%79",'y'},
{"%7A",'z'},{"%7B",'{'}, {"%7C",'|'},{"%7D",'}'},{"%7E",'~'},
{"%7a",'z'},{"%7b",'{'}, {"%7c",'|'},{"%7d",'}'},{"%7e",'~'}
};




//中文字符，专门存储中文字符相加起来的整数值
static const std::unordered_set<int>chineseSet;



//测试版  同时处理url转码和中文转换
static bool UrlDecodeWithTransChinese(const char *source, const int len, char * des, int &desLen, const int maxDesLen)
{
	static const int pow2562{ static_cast<int>(pow(256,2)) };
	static const int pow2561{ static_cast<int>(pow(256,1)) };
	static const int pow2560{ 1 };


	static constexpr int utf8ChineseMin{ 0xce91 };
	static constexpr int utf8ChineseMax{ 0xefbfa5 };


	if (!source || maxDesLen < len)
		return false;
	desLen = 0;
	if (len > 0)
	{
		const char *iterBegin{ source }, *iterEnd{ source + len }, *iterFirst{ source }, *iterTemp{ source };
		decltype(urlMap)::const_iterator iter;
		int index{}, index1{}, index2{}, index3{}, BeginToEndLen{};
		char ch0{}, ch1{}, ch2{}, ch3{}, ch4{}, ch5{}, ch6{}, ch7{}, ch8{};

		while (iterBegin != iterEnd)
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

			BeginToEndLen = std::distance(iterBegin, iterEnd);
			if (BeginToEndLen)  //如果iterBegin不到尾部
			{

				//如果iterBegin所在位置为%，则首先判断iterBegin到尾部的剩余字符至少为3个，否则就直接存%
				//如果为3个以上，那么先截取3位，比对是否符合url编码规则中的情况，是则进行转换，
				//如果头3位不符合，则判断iterBegin到尾部剩余字符是否至少为9个，如果不是，则直接存%
				//如果是9个，则尝试判断是否为中文，如果不是，则直接存%
				//如果为+，则替换为‘ ’
				if (*iterBegin == '%')
				{
					if (BeginToEndLen > 2)
					{
						std::string_view temp(iterBegin, 3);
						iter = urlMap.find(temp);
						if (iter != urlMap.cend())
						{
							*(des + desLen++) = iter->second;
							iterFirst = (iterBegin += 2);
						}
						else
						{
							//最终中文选择unicode  \u0391-\uffe5  范围，此范围可以连同中文标点一起识别
							//utf8下为   0xce91   0xefbfa5
							//将unicode字符拆开填入utf8中计算得到中文范围为 0xe4b880   0xe9bea5，改良原中文判断办法
									//  11100100     10111000      10000000       4e00   228*65536  + 184*256   +  128*1    111001001011100010000000
									// 14942208     47104           14989440
									//  11101001     10111110      10100101       9fa5                                      111010011011111010100101
									//  15269888     48640         165     15318693
									//   https://www.cnblogs.com/sosoft/p/3456631.html
									//  [ \u2E80-\uFE4F]      0xe2ba80    0xefb98f
									//  11100010     10111010    10000000             111000101011101010000000          e2ba80
									//  11101111     10111001    10001111             111011111011100110001111          efb98f
									//https://blog.csdn.net/tenpage/article/details/8729851?utm_term=unicode%E8%8C%83%E5%9B%B4%E4%B8%AD%E6%96%87%E6%A0%87%E7%82%B9%E7%AC%A6%E5%8F%B7&utm_medium=distribute.pc_aggpage_search_result.none-task-blog-2~all~sobaiduweb~default-0-8729851&spm=3001.4430
									//   汉字范围 \u0391-\uffe5 (中文)
									//  11001110  10010001                            1100111010010001                  ce91
									//  11101111     10111111    10100101             111011111011111110100101          efbfa5
									//先用位判断是两位utf8还是三位utf8处理

									//判断是否是utf8中的中文
							//先考虑两字节的，再考虑三字节的
							if (BeginToEndLen > 5)
							{
								ch0 = *iterBegin, ch1 = *(iterBegin + 1), ch2 = *(iterBegin + 2),
									ch3 = *(iterBegin + 3), ch4 = *(iterBegin + 4), ch5 = *(iterBegin + 5);

								if (ch0 == '%' && isalnum(ch1) && isalnum(ch2) &&
									ch3 == '%' && isalnum(ch4) && isalnum(ch5)
									)
								{
									index1 = (isdigit(ch1) ? ch1 - '0' : islower(ch1) ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10)) * 16;
									index1 += (isdigit(ch2) ? ch2 - '0' : islower(ch2) ? (ch2 - 'a' + 10) : (ch2 - 'A' + 10));

									index2 = (isdigit(ch4) ? ch4 - '0' : islower(ch4) ? (ch4 - 'a' + 10) : (ch4 - 'A' + 10)) * 16;
									index2 += (isdigit(ch5) ? ch5 - '0' : islower(ch5) ? (ch5 - 'a' + 10) : (ch5 - 'A' + 10));

									// 双字节情况  110开头       10开头
									if (getbit(index1, 7) && getbit(index1, 6) && !getbit(index1, 5) && getbit(index2, 7) && !getbit(index2, 6))
									{
										index = index1 * pow2561 + index2 * pow2560;

										//
										if (index >= utf8ChineseMin || chineseSet.find(index)!= chineseSet.cend())
										{
											*(des + desLen++) = static_cast<char>(index1);
											*(des + desLen++) = static_cast<char>(index2);
											iterFirst = (iterBegin += 5);
										}
										else
											*(des + desLen++) = '%';
									}
									else
									{
										//判断是否是三字节情况
										if (BeginToEndLen > 8)
										{
											if (ch6 == '%' && isalnum(ch7) && isalnum(ch8))
											{
												index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
												index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));


												// 三字节情况  1110开头    10开头  10开头
												if (getbit(index1, 7) && getbit(index1, 6) && getbit(index1, 5) && !getbit(index1, 4) && getbit(index2, 7) && !getbit(index2, 6)
													&& getbit(index3, 7) && !getbit(index3, 6))
												{
													index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
													index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));

													index = index1 * pow2562 + index2 * pow2561 + index3 * pow2560;

													if (index <= utf8ChineseMax || chineseSet.find(index)!= chineseSet.cend())
													{
														*(des + desLen++) = static_cast<char>(index1);
														*(des + desLen++) = static_cast<char>(index2);
														*(des + desLen++) = static_cast<char>(index3);
														iterFirst = (iterBegin += 8);
													}
													else
														*(des + desLen++) = '%';
												}
												else
													*(des + desLen++) = '%';
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
	return true;
}




//测试版  同时处理url转码和中文转换
static bool UrlDecodeWithTransChinese(const char *source, const int len, int &desLen)
{
	static const int pow2562{ static_cast<int>(pow(256,2)) };
	static const int pow2561{ static_cast<int>(pow(256,1)) };
	static const int pow2560{ 1 };

	static constexpr int utf8ChineseMin{ 0xce91 };
	static constexpr int utf8ChineseMax{ 0xefbfa5 };


	if (!source)
		return false;
	desLen = 0;
	if (len > 0)
	{
		const char *iterBegin{ source }, *iterEnd{ source + len }, *iterFirst{ source }, *iterTemp{ source };
		desLen = len;
		decltype(urlMap)::const_iterator iter;
		int index{}, index1{}, index2{}, index3{}, BeginToEndLen{};
		char *des{ const_cast<char*>(source) };
		char ch0{}, ch1{}, ch2{}, ch3{}, ch4{}, ch5{}, ch6{}, ch7{}, ch8{};


		while (iterBegin != iterEnd)
		{
			iterBegin = std::find_if(iterBegin, iterEnd, std::bind(std::logical_or<bool>(), std::bind(std::equal_to<char>(), std::placeholders::_1, '%'),
				std::bind(std::equal_to<char>(), std::placeholders::_1, '+')));
			//查找%或+

			BeginToEndLen = std::distance(iterBegin, iterEnd);
			if (BeginToEndLen)  //如果iterBegin不到尾部
			{

				//如果iterBegin所在位置为%，则首先判断iterBegin到尾部的剩余字符至少为3个，否则就直接存%
				//如果为3个以上，那么先截取3位，比对是否符合url编码规则中的情况，是则进行转换，
				//如果头3位不符合，则判断iterBegin到尾部剩余字符是否至少为6个，如果不是，则直接存%
				//如果是6个，则尝试判断是否为中文，如果不是，则直接存%
				//如果为+，则替换为‘ ’
				if (*iterBegin == '%')
				{
					if (BeginToEndLen > 2)
					{
						std::string_view temp(iterBegin, 3);
						iter = urlMap.find(temp);
						if (iter != urlMap.cend())
						{
							desLen = std::distance(source, iterBegin);
							*(des + desLen++) = iter->second;
							iterBegin += 3;
							break;
						}
						else
						{
							//最终中文选择unicode  \u0391-\uffe5  范围，此范围可以连同中文标点一起识别
							//utf8下为   0xce91   0xefbfa5
							//先考虑两字节的，再考虑三字节的
							if (BeginToEndLen > 5)
							{
								ch0 = *iterBegin, ch1 = *(iterBegin + 1), ch2 = *(iterBegin + 2),
									ch3 = *(iterBegin + 3), ch4 = *(iterBegin + 4), ch5 = *(iterBegin + 5);

								if (ch0 == '%' && isalnum(ch1) && isalnum(ch2) &&
									ch3 == '%' && isalnum(ch4) && isalnum(ch5)
									)
								{
									index1 = (isdigit(ch1) ? ch1 - '0' : islower(ch1) ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10)) * 16;
									index1 += (isdigit(ch2) ? ch2 - '0' : islower(ch2) ? (ch2 - 'a' + 10) : (ch2 - 'A' + 10));

									index2 = (isdigit(ch4) ? ch4 - '0' : islower(ch4) ? (ch4 - 'a' + 10) : (ch4 - 'A' + 10)) * 16;
									index2 += (isdigit(ch5) ? ch5 - '0' : islower(ch5) ? (ch5 - 'a' + 10) : (ch5 - 'A' + 10));

									// 双字节情况  110开头       10开头
									if (getbit(index1, 7) && getbit(index1, 6) && !getbit(index1, 5) && getbit(index2, 7) && !getbit(index2, 6))
									{
										index = index1 * pow2561 + index2 * pow2560;

										//
										if (index >= utf8ChineseMin || chineseSet.find(index)!= chineseSet.cend())
										{
											desLen = std::distance(source, iterBegin);
											*(des + desLen++) = static_cast<char>(index1);
											*(des + desLen++) = static_cast<char>(index2);
											iterBegin += 6;
											break;
										}
									}
									else
									{
										//判断是否是三字节情况
										if (BeginToEndLen > 8)
										{
											ch6 = *(iterBegin + 6), ch7 = *(iterBegin + 7), ch8 = *(iterBegin + 8);

											if (ch6 == '%' && isalnum(ch7) && isalnum(ch8))
											{
												index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
												index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));


												// 三字节情况  1110开头    10开头  10开头
												if (getbit(index1, 7) && getbit(index1, 6) && getbit(index1, 5) && !getbit(index1, 4) && getbit(index2, 7) && !getbit(index2, 6)
													&& getbit(index3, 7) && !getbit(index3, 6))
												{
													index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
													index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));

													index = index1 * pow2562 + index2 * pow2561 + index3 * pow2560;

													if (index <= utf8ChineseMax || chineseSet.find(index) != chineseSet.cend())
													{
														desLen = std::distance(source, iterBegin);
														*(des + desLen++) = static_cast<char>(index1);
														*(des + desLen++) = static_cast<char>(index2);
														*(des + desLen++) = static_cast<char>(index3);
														iterBegin += 9;
														break;
													}
												}
											}
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
					++iterBegin;
					break;
				}
				++iterBegin;
			}
		}

		iterFirst = iterBegin;

		while (iterBegin != iterEnd)
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

			BeginToEndLen = std::distance(iterBegin, iterEnd);
			if (BeginToEndLen)  //如果iterBegin不到尾部
			{

				//如果iterBegin所在位置为%，则首先判断iterBegin到尾部的剩余字符至少为3个，否则就直接存%
				//如果为3个以上，那么先截取3位，比对是否符合url编码规则中的情况，是则进行转换，
				//如果头3位不符合，则判断iterBegin到尾部剩余字符是否至少为9个，如果不是，则直接存%
				//如果是9个，则尝试判断是否为中文，如果不是，则直接存%
				//如果为+，则替换为‘ ’
				if (*iterBegin == '%')
				{
					if (BeginToEndLen > 2)
					{
						std::string_view temp(iterBegin, 3);
						iter = urlMap.find(temp);
						if (iter != urlMap.cend())
						{
							*(des + desLen++) = iter->second;
							iterFirst = (iterBegin += 2);
						}
						else
						{
							//最终中文选择unicode  \u0391-\uffe5  范围，此范围可以连同中文标点一起识别
							//utf8下为   0xce91   0xefbfa5
							if (BeginToEndLen > 5)
							{
								ch0 = *iterBegin, ch1 = *(iterBegin + 1), ch2 = *(iterBegin + 2),
									ch3 = *(iterBegin + 3), ch4 = *(iterBegin + 4), ch5 = *(iterBegin + 5);

								if (ch0 == '%' && isalnum(ch1) && isalnum(ch2) &&
									ch3 == '%' && isalnum(ch4) && isalnum(ch5)
									)
								{
									index1 = (isdigit(ch1) ? ch1 - '0' : islower(ch1) ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10)) * 16;
									index1 += (isdigit(ch2) ? ch2 - '0' : islower(ch2) ? (ch2 - 'a' + 10) : (ch2 - 'A' + 10));

									index2 = (isdigit(ch4) ? ch4 - '0' : islower(ch4) ? (ch4 - 'a' + 10) : (ch4 - 'A' + 10)) * 16;
									index2 += (isdigit(ch5) ? ch5 - '0' : islower(ch5) ? (ch5 - 'a' + 10) : (ch5 - 'A' + 10));

									// 双字节情况  110开头       10开头
									if (getbit(index1, 7) && getbit(index1, 6) && !getbit(index1, 5) && getbit(index2, 7) && !getbit(index2, 6))
									{
										index = index1 * pow2561 + index2 * pow2560;

										//
										if (index >= utf8ChineseMin || chineseSet.find(index) != chineseSet.cend())
										{
											*(des + desLen++) = static_cast<char>(index1);
											*(des + desLen++) = static_cast<char>(index2);
											iterFirst = (iterBegin += 5);
										}
										else
											*(des + desLen++) = '%';
									}
									else
									{
										//判断是否是三字节情况
										if (BeginToEndLen > 8)
										{
											ch6 = *(iterBegin + 6), ch7 = *(iterBegin + 7), ch8 = *(iterBegin + 8);

											if (ch6 == '%' && isalnum(ch7) && isalnum(ch8))
											{
												index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
												index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));


												// 三字节情况  1110开头    10开头  10开头
												if (getbit(index1, 7) && getbit(index1, 6) && getbit(index1, 5) && !getbit(index1, 4) && getbit(index2, 7) && !getbit(index2, 6)
													&& getbit(index3, 7) && !getbit(index3, 6))
												{
													index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
													index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));

													index = index1 * pow2562 + index2 * pow2561 + index3 * pow2560;

													if (index <= utf8ChineseMax || chineseSet.find(index) != chineseSet.cend())
													{
														*(des + desLen++) = static_cast<char>(index1);
														*(des + desLen++) = static_cast<char>(index2);
														*(des + desLen++) = static_cast<char>(index3);
														iterFirst = (iterBegin += 8);
													}
													else
														*(des + desLen++) = '%';
												}
												else
													*(des + desLen++) = '%';
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
	return true;
}



/*
static bool UrlDecodeWithTransChinese(const char* source, const int len, int& desLen)
{
	static const int pow2562{ static_cast<int>(pow(256,2)) };
	static const int pow2561{ static_cast<int>(pow(256,1)) };
	static const int pow2560{ 1 };

	static constexpr int utf8ChineseMin{ 0xce91 };
	static constexpr int utf8ChineseMax{ 0xefbfa5 };


	if (!source)
		return false;
	desLen = 0;
	if (len > 0)
	{
		const char* iterBegin{ source }, * iterEnd{ source + len }, * iterFirst{ source }, * iterTemp{ source };
		desLen = len;
		decltype(urlMap)::const_iterator iter;
		int index{}, index1{}, index2{}, index3{}, BeginToEndLen{};
		char* des{ const_cast<char*>(source) };
		char ch0{}, ch1{}, ch2{}, ch3{}, ch4{}, ch5{}, ch6{}, ch7{}, ch8{};
		bool result{};
		char urlCh{};

		while (iterBegin != iterEnd)
		{
			iterBegin = std::find_if(iterBegin, iterEnd, std::bind(std::logical_or<bool>(), std::bind(std::equal_to<char>(), std::placeholders::_1, '%'),
				std::bind(std::equal_to<char>(), std::placeholders::_1, '+')));
			//查找%或+

			BeginToEndLen = std::distance(iterBegin, iterEnd);
			if (BeginToEndLen)  //如果iterBegin不到尾部
			{

				//如果iterBegin所在位置为%，则首先判断iterBegin到尾部的剩余字符至少为3个，否则就直接存%
				//如果为3个以上，那么先截取3位，比对是否符合url编码规则中的情况，是则进行转换，
				//如果头3位不符合，则判断iterBegin到尾部剩余字符是否至少为6个，如果不是，则直接存%
				//如果是6个，则尝试判断是否为中文，如果不是，则直接存%
				//如果为+，则替换为‘ ’
				if (*iterBegin == '%')
				{
					if (BeginToEndLen > 2)
					{

						result = findUrlChar(iterBegin + 1, urlCh);
						if (result)
						{
							desLen = std::distance(source, iterBegin);
							*(des + desLen++) = urlCh;
							iterBegin += 3;
							break;
						}
						else
						{
							//最终中文选择unicode  \u0391-\uffe5  范围，此范围可以连同中文标点一起识别
							//utf8下为   0xce91   0xefbfa5
							//先考虑两字节的，再考虑三字节的
							if (BeginToEndLen > 5)
							{
								ch0 = *iterBegin, ch1 = *(iterBegin + 1), ch2 = *(iterBegin + 2),
									ch3 = *(iterBegin + 3), ch4 = *(iterBegin + 4), ch5 = *(iterBegin + 5);

								if (ch0 == '%' && isalnum(ch1) && isalnum(ch2) &&
									ch3 == '%' && isalnum(ch4) && isalnum(ch5)
									)
								{
									index1 = (isdigit(ch1) ? ch1 - '0' : islower(ch1) ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10)) * 16;
									index1 += (isdigit(ch2) ? ch2 - '0' : islower(ch2) ? (ch2 - 'a' + 10) : (ch2 - 'A' + 10));

									index2 = (isdigit(ch4) ? ch4 - '0' : islower(ch4) ? (ch4 - 'a' + 10) : (ch4 - 'A' + 10)) * 16;
									index2 += (isdigit(ch5) ? ch5 - '0' : islower(ch5) ? (ch5 - 'a' + 10) : (ch5 - 'A' + 10));

									// 双字节情况  110开头       10开头
									if (getbit(index1, 7) && getbit(index1, 6) && !getbit(index1, 5) && getbit(index2, 7) && !getbit(index2, 6))
									{
										index = index1 * pow2561 + index2 * pow2560;

										//
										if (index >= utf8ChineseMin || chineseSet.find(index) != chineseSet.cend())
										{
											desLen = std::distance(source, iterBegin);
											*(des + desLen++) = static_cast<char>(index1);
											*(des + desLen++) = static_cast<char>(index2);
											iterBegin += 6;
											break;
										}
									}
									else
									{
										//判断是否是三字节情况
										if (BeginToEndLen > 8)
										{
											ch6 = *(iterBegin + 6), ch7 = *(iterBegin + 7), ch8 = *(iterBegin + 8);

											if (ch6 == '%' && isalnum(ch7) && isalnum(ch8))
											{
												index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
												index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));


												// 三字节情况  1110开头    10开头  10开头
												if (getbit(index1, 7) && getbit(index1, 6) && getbit(index1, 5) && !getbit(index1, 4) && getbit(index2, 7) && !getbit(index2, 6)
													&& getbit(index3, 7) && !getbit(index3, 6))
												{
													index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
													index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));

													index = index1 * pow2562 + index2 * pow2561 + index3 * pow2560;

													if (index <= utf8ChineseMax || chineseSet.find(index) != chineseSet.cend())
													{
														desLen = std::distance(source, iterBegin);
														*(des + desLen++) = static_cast<char>(index1);
														*(des + desLen++) = static_cast<char>(index2);
														*(des + desLen++) = static_cast<char>(index3);
														iterBegin += 9;
														break;
													}
												}
											}
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
					++iterBegin;
					break;
				}
				++iterBegin;
			}
		}

		iterFirst = iterBegin;

		while (iterBegin != iterEnd)
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

			BeginToEndLen = std::distance(iterBegin, iterEnd);
			if (BeginToEndLen)  //如果iterBegin不到尾部
			{

				//如果iterBegin所在位置为%，则首先判断iterBegin到尾部的剩余字符至少为3个，否则就直接存%
				//如果为3个以上，那么先截取3位，比对是否符合url编码规则中的情况，是则进行转换，
				//如果头3位不符合，则判断iterBegin到尾部剩余字符是否至少为9个，如果不是，则直接存%
				//如果是9个，则尝试判断是否为中文，如果不是，则直接存%
				//如果为+，则替换为‘ ’
				if (*iterBegin == '%')
				{
					if (BeginToEndLen > 2)
					{
						result = findUrlChar(iterBegin + 1, urlCh);
						if (result)
						{
							*(des + desLen++) = urlCh;
							iterFirst = (iterBegin += 2);
						}
						else
						{
							//最终中文选择unicode  \u0391-\uffe5  范围，此范围可以连同中文标点一起识别
							//utf8下为   0xce91   0xefbfa5
							if (BeginToEndLen > 5)
							{
								ch0 = *iterBegin, ch1 = *(iterBegin + 1), ch2 = *(iterBegin + 2),
									ch3 = *(iterBegin + 3), ch4 = *(iterBegin + 4), ch5 = *(iterBegin + 5);

								if (ch0 == '%' && isalnum(ch1) && isalnum(ch2) &&
									ch3 == '%' && isalnum(ch4) && isalnum(ch5)
									)
								{
									index1 = (isdigit(ch1) ? ch1 - '0' : islower(ch1) ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10)) * 16;
									index1 += (isdigit(ch2) ? ch2 - '0' : islower(ch2) ? (ch2 - 'a' + 10) : (ch2 - 'A' + 10));

									index2 = (isdigit(ch4) ? ch4 - '0' : islower(ch4) ? (ch4 - 'a' + 10) : (ch4 - 'A' + 10)) * 16;
									index2 += (isdigit(ch5) ? ch5 - '0' : islower(ch5) ? (ch5 - 'a' + 10) : (ch5 - 'A' + 10));

									// 双字节情况  110开头       10开头
									if (getbit(index1, 7) && getbit(index1, 6) && !getbit(index1, 5) && getbit(index2, 7) && !getbit(index2, 6))
									{
										index = index1 * pow2561 + index2 * pow2560;

										//
										if (index >= utf8ChineseMin || chineseSet.find(index) != chineseSet.cend())
										{
											*(des + desLen++) = static_cast<char>(index1);
											*(des + desLen++) = static_cast<char>(index2);
											iterFirst = (iterBegin += 5);
										}
										else
											*(des + desLen++) = '%';
									}
									else
									{
										//判断是否是三字节情况
										if (BeginToEndLen > 8)
										{
											ch6 = *(iterBegin + 6), ch7 = *(iterBegin + 7), ch8 = *(iterBegin + 8);

											if (ch6 == '%' && isalnum(ch7) && isalnum(ch8))
											{
												index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
												index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));


												// 三字节情况  1110开头    10开头  10开头
												if (getbit(index1, 7) && getbit(index1, 6) && getbit(index1, 5) && !getbit(index1, 4) && getbit(index2, 7) && !getbit(index2, 6)
													&& getbit(index3, 7) && !getbit(index3, 6))
												{
													index3 = (isdigit(ch7) ? ch7 - '0' : islower(ch7) ? (ch7 - 'a' + 10) : (ch7 - 'A' + 10)) * 16;
													index3 += (isdigit(ch8) ? ch8 - '0' : islower(ch8) ? (ch8 - 'a' + 10) : (ch8 - 'A' + 10));

													index = index1 * pow2562 + index2 * pow2561 + index3 * pow2560;

													if (index <= utf8ChineseMax || chineseSet.find(index) != chineseSet.cend())
													{
														*(des + desLen++) = static_cast<char>(index1);
														*(des + desLen++) = static_cast<char>(index2);
														*(des + desLen++) = static_cast<char>(index3);
														iterFirst = (iterBegin += 8);
													}
													else
														*(des + desLen++) = '%';
												}
												else
													*(des + desLen++) = '%';
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
	return true;
}
*/




template<typename T>
static T stringLen(const T num)
{
	if (num <= static_cast<T>(0))
		return static_cast<T>(1);
	T testNum{}, result{};
	static T ten{ 10 };
	testNum = num, result = 1;
	while (testNum /= ten)
		++result;
	return result;
}



template<typename T>
static char* NumToString(const T num, int &needLen, MEMORYPOOL<> &m_charMemoryPool)
{
	if (num <= static_cast<T>(0))
		return nullptr;
	needLen = stringLen(num);

	char *buffer{ m_charMemoryPool.getMemory<char*>(needLen) };
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

				int len{};
				//对body进行url转码和中文转换，此种中文转换只在charset=UTF-8下有用  可以用postman设置
				UrlDecodeWithTransChinese(iterBegin, std::distance(iterBegin, iterEnd), len);

				*des = iterBegin;
				*(++des) = iterBegin + len;
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

			int len{};
			//对body进行url转码和中文转换，此种中文转换只在charset=UTF-8下有用  可以用postman设置
			UrlDecodeWithTransChinese(iterBegin, std::distance(iterBegin, iterEnd), len);

			*des = iterBegin;
			*(++des) = iterBegin + len;
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









namespace HTTPRESPONSEREADY
{
	static const char *http11bodyToLong{ "HTTP/1.1 400 bad request\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:23\r\n\r\nBody length is too long" };
	static size_t http11bodyToLongLen{ strlen(http11bodyToLong) };

	////////////////////////////////////////////////

	static const char *http11OK{ "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:18\r\n\r\nHTTP request is OK" };
	static size_t http11OKLen{ strlen(http11OK) };

	//////////////////////////////////////////////////////////

	static const char *http11OKNoBodyJson{ "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:13\r\n\r\n{\"result\":\"\"}" };
	static size_t http11OKNoBodyJsonLen{ strlen(http11OKNoBodyJson) };


	static const char *http11OKNoBody{ "HTTP/1.1 400 bad request\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:0\r\n\r\n" };
	static size_t http11OKNoBodyLen{ strlen(http11OKNoBody) };

	static const char *http11OKConyentLength{ "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:" };
	static size_t http11OKConyentLengthLen{ strlen(http11OKConyentLength) };


	//////////////////////////////////////////////////

	static const char *http11invaild{ "HTTP/1.1 400 bad request\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:23\r\n\r\nHTTP request is invaild" };
	static size_t http11invaildLen{ strlen(http11invaild) };


	//////////////////////////////////////////////////////////////////////////////////////////////////

	static const char *http11sqlRow0{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:15\r\n\r\nRow number is 0" };
	static size_t http11sqlRow0Len{ strlen(http11sqlRow0) };


	///////////////////////////////////////////////////////////////////////////////

	static const char *http11sqlField0{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:17\r\n\r\nField number is 0" };
	static size_t http11sqlField0Len{ strlen(http11sqlField0) };


	///////////////////////////////////////////////////////////////////////////////

	static const char *http11sqlError{ "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:9\r\n\r\nSql error" };
	static size_t http11sqlErrorLen{ strlen(http11sqlError) };


	//////////////////////////////////////////////////////////////////////////////////

	static const char *http11sqlSizeTooBig{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:19\r\n\r\nSql size is too big" };
	static size_t http11sqlSizeTooBigLen{ strlen(http11sqlSizeTooBig) };


	///////////////////////////////////////////////////////////////////////////////////


	static const char *http11Pong{ "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:4\r\n\r\nPong" };
	static size_t http11PongLen{ strlen(http11Pong) };


	static const char *http11SqlQueryError{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:15\r\n\r\nSQL query error" };
	static size_t http11SqlQueryErrorLen{ strlen(http11SqlQueryError) };


	static const char *http11SqlNetAsyncError{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:19\r\n\r\nSQL net async error" };
	static size_t http11SqlNetAsyncErrorLen{ strlen(http11SqlNetAsyncError) };


	static const char *http11SqlResNull{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:15\r\n\r\nSQL res is null" };
	static size_t http11SqlResNullLen{ strlen(http11SqlResNull) };


	static const char *http11SqlQueryRowZero{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:18\r\n\r\nSQL query row zero" };
	static size_t http11SqlQueryRowZeroLen{ strlen(http11SqlQueryRowZero) };


	static const char *http11SqlQueryFieldZero{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:20\r\n\r\nSQL query field zero" };
	static size_t http11SqlQueryFieldZeroLen{ strlen(http11SqlQueryFieldZero) };


	static const char *httpUnknownError{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:13\r\n\r\nUnknown error" };
	static size_t httpUnknownErrorLen{ strlen(httpUnknownError) };


	static const char *httpFailToInsertSql{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:18\r\n\r\nFail to insert Sql" };
	static size_t httpFailToInsertSqlLen{ strlen(httpFailToInsertSql) };

	static const char *httpFailToInsertRedis{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:20\r\n\r\nFail to insert Redis" };
	static size_t httpFailToInsertRedisLen{ strlen(httpFailToInsertRedis) };


	static const char *httpInsertSqlWrite{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:31\r\n\r\nSuccess to insert sqlWrite list" };
	static size_t httpInsertSqlWriteLen{ strlen(httpInsertSqlWrite) };

	static const char *httpSTDException{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:13\r\n\r\nSTD exception" };
	static size_t httpSTDExceptionLen{ strlen(httpSTDException) };


	static const char *httpPasswordIsWrong{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:17\r\n\r\nPassword is wrong" };
	static size_t httpPasswordIsWrongLen{ strlen(httpPasswordIsWrong) };

	
	static const char *httpREDIS_ASYNC_WRITE_ERROR{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:23\r\n\r\nRedis async write error" };
	static size_t httpREDIS_ASYNC_WRITE_ERRORLen{ strlen(httpREDIS_ASYNC_WRITE_ERROR) };


	static const char *httpREDIS_ASYNC_READ_ERROR{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:22\r\n\r\nRedis async read error" };
	static size_t httpREDIS_ASYNC_READ_ERRORLen{ strlen(httpREDIS_ASYNC_READ_ERROR) };


	static const char *httpCHECK_REDIS_MESSAGE_ERROR{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:25\r\n\r\nCheck redis message error" };
	static size_t httpCHECK_REDIS_MESSAGE_ERRORLen{ strlen(httpCHECK_REDIS_MESSAGE_ERROR) };


	static const char *httpREDIS_READY_QUERY_ERROR{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:23\r\n\r\nRedis ready query error" };
	static size_t httpREDIS_READY_QUERY_ERRORLen{ strlen(httpREDIS_READY_QUERY_ERROR) };

	static const char *httpNO_KEY{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:6\r\n\r\nNo key" };
	static size_t httpNO_KEYLen{ strlen(httpNO_KEY) };


	static const char *httpNoMessage{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:10\r\n\r\nNo message" };
	static size_t httpNoMessageLen{ strlen(httpNoMessage) };

	static const char *httpFileGetError{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:14\r\n\r\nFile get error" };
	static size_t httpFileGetErrorLen{ strlen(httpFileGetError) };


	


	static const char *http404Nofile{ "HTTP/1.1 404 NOFILE\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:0\r\n\r\n" };
	static size_t http404NofileLen{ strlen(http404Nofile) };


	static const char *httpNoRecordInSql{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:16\r\n\r\nNo record in sql" };
	static size_t httpNoRecordInSqlLen{ strlen(httpNoRecordInSql) };


	static const char *http11WrongPublicKey{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:15\r\n\r\nWrong publicKey" };
	static size_t http11WrongPublicKeyLen{ strlen(http11WrongPublicKey) };


	static const char *http11InvaildHash{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:12\r\n\r\nInvaild hash" };
	static size_t http11InvaildHashLen{ strlen(http11InvaildHash) };


	static const char *httpRSAencryptFail{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:16\r\n\r\nRSA encrypt fail" };
	static size_t httpRSAencryptFailLen{ strlen(httpRSAencryptFail) };


	static const char *httpRSAdecryptFail{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:16\r\n\r\nRSA decrypt fail" };
	static size_t httpRSAdecryptFailLen{ strlen(httpRSAdecryptFail) };


	static const char *httpAESsetKeyFail{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:19\r\n\r\nFail to set AES key" };
	static size_t httpAESsetKeyFailLen{ strlen(httpAESsetKeyFail) };


	static const char *httpFailToVerify{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:14\r\n\r\nFail to verify" };
	static size_t httpFailToVerifyLen{ strlen(httpFailToVerify) };

	static const char *httpFailToMakeFilePath{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:21\r\n\r\nFail to make filePath" };
	static size_t httpFailToMakeFilePathLen{ strlen(httpFailToMakeFilePath) };

	static const char *httpFailToOpenFile{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:17\r\n\r\nFail to open file" };
	static size_t httpFailToOpenFileLen{ strlen(httpFailToOpenFile) };


	static const char *httpFailToGetFileSize{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:21\r\n\r\nFail to get file size" };
	static size_t httpFailToGetFileSizeLen{ strlen(httpFailToGetFileSize) };

	static const char *httpFailToMakeHttpFront{ "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:22\r\n\r\nFail to make httpFront" };
	static size_t httpFailToMakeHttpFrontLen{ strlen(httpFailToMakeHttpFront) };




	//用于对100-continue做出处理
	//https://blog.csdn.net/taoshihan/article/details/104273017?utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7Edefault-4.no_search_link&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7Edefault-4.no_search_link
	static const char *http100Continue{ "HTTP/1.1 100 Continue\r\n\r\n" };
	static size_t http100ContinueLen{ strlen(http100Continue) };
}





struct REDISNOKEY
{

};



struct READFROMDISK
{

};


struct READFROMREDIS
{

};

static const char *randomString{"1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" };
static size_t randomStringLen{ strlen(randomString) };


static std::mt19937 rdEngine{ std::chrono::high_resolution_clock::now().time_since_epoch().count() };


static size_t maxAESKeyLen{ 32 };




#pragma once



#include "IOcontextPool.h"
#include "ASYNCLOG.h"
#include "STLTimeWheel.h"

#include <boost/asio/steady_timer.hpp>

#include<fstream>
#include<string>
#include<vector>
#include<map>
#include<algorithm>
#include<functional>
#include<numeric>
#include<arpa/inet.h>
#include<cstdlib>
#include<memory>
#include<shared_mutex>

//检查ip模块，定期从APNIC获取更新信息
struct CHECKIP
{
	CHECKIP(const std::shared_ptr<IOcontextPool> &ioPool, const std::shared_ptr<ASYNCLOG> &log,
		const std::shared_ptr<STLTimeWheel> &timeWheel,
		const std::string& host,
		const std::string& port, const std::string& country,
		const std::string& saveFile,
		const unsigned int checkTime = 3600 * 24);      //构造函数

	bool is_china_ipv4(const std::string& ip);               // 检查IPV4是否属于中国公网ipv4地址段
	


private:
	const std::shared_ptr<ASYNCLOG>m_log{};                                 //日志模块

	const unsigned int m_checkTime{};                                       //更新时间间隔

	std::unique_ptr<boost::asio::steady_timer>m_timer{};            //定时器

	std::vector<std::vector<std::pair<unsigned int, unsigned int>>>m_vec;        //快速查询ip匹配vector

	std::vector<std::vector<std::pair<unsigned int, unsigned int>>>m_temp;        //解析查询记录临时vector

	std::shared_mutex m_mutex;                                 //C++17  读写锁

	std::shared_ptr<boost::asio::io_context>m_ioc{};

	const std::string m_host{};

	const std::string m_port{};

	const std::string m_country{};

	const std::string m_saveFile{};

	std::ofstream m_file;

	const std::string m_msg{ "GET /stats/apnic/delegated-apnic-latest HTTP/1.1\r\nHost:ftp.apnic.net\r\nConnection:close\r\n\r\n" };

	std::unique_ptr<boost::asio::ip::tcp::resolver>m_resolver{};

	std::unique_ptr<boost::asio::ip::tcp::socket>m_socket{};

	bool m_result{};                                               //本次解析结果

	bool m_parse{};                                               //是否需要解析

	const unsigned int m_bufLen{ 4096 };

	std::unique_ptr<char[]>m_buf{ new char[m_bufLen] };

	unsigned int m_readLen{};

	const std::shared_ptr<STLTimeWheel> m_timeWheel{};

	boost::system::error_code m_ec{};

	const int m_strBufLen{ 100 };

	std::unique_ptr<char[]>m_strBuf{ new char[m_strBufLen] };

private:

	unsigned int ip_to_int(const std::string& ip);                // 将IP字符串转为32位整数

	unsigned int ip_to_int_char(const char *ip);                   // 将IP字符串转为32位整数

	void updateLoop();                                        //定时更新程序

	void update();                                            //更新函数

	void resetResult();

	void logResult();

	void resetResolver();

	void resetSocket();

	void resetReadLen();

	void resetVector();

	void startResolver();

	void handle_resolve(const boost::system::error_code& err,
		boost::asio::ip::tcp::resolver::results_type endpoints);

	void handle_connect(const boost::system::error_code& err);

	void sendCommand();

	void handle_write(const boost::system::error_code& err, size_t len);

	void readMessage();

	void handle_read(const boost::system::error_code& err, size_t len);

	void parseMessage(const size_t len);

	int FastLog2(const unsigned int num);

	void resetFile();

	void resetParse();

	void shutdownSocket();

	void cancelSocket();

	void closeSocket();
};




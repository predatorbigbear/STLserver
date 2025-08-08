#pragma once



#include "IOcontextPool.h"
#include "ASYNCLOG.h"

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
#include<shared_mutex>

//检查ip模块，定期从APNIC获取更新信息
struct CHECKIP
{
	CHECKIP(const char* ipFileName, std::shared_ptr<IOcontextPool> ioPool, const char* command, std::shared_ptr<ASYNCLOG>log,
		const unsigned int checkTime = 3600 * 24);      //构造函数

	bool is_china_ipv4(const std::string& ip);               // 检查IPV4是否属于中国公网ipv4地址段
	


private:
	std::shared_ptr<ASYNCLOG>m_log{};                                 //日志模块

	unsigned int m_checkTime{};                                       //更新时间间隔

	std::unique_ptr<boost::asio::steady_timer>m_timer{};            //定时器

	std::vector<std::vector<std::pair<unsigned int, unsigned int>>>m_vec;        //快速查询ip匹配vector

	std::vector<std::string>m_cidr{};                              //存储文件中的ip记录

	std::shared_mutex m_mutex;                                   //C++17  读写锁                                 

	const char* m_ipFileName{};                                  //从APNIC获取更新信息后保存的日志文件

	std::string m_command{};                                     //执行命令

private:
	void executeCommand();                                     //从APNIC更新

	void readFile();                                           //读取文件并存储到中间vector中

	void makeRecord();                                         //生成快速查询记录

	uint32_t ip_to_int(const std::string& ip);                // 将IP字符串转为32位整数

	void updateLoop();                                        //定时更新程序

	void update();                                            //更新函数
};




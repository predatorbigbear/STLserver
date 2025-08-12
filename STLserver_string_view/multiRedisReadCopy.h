#pragma once


#include "ASYNCLOG.h"
#include "concurrentqueue.h"
#include "regexFunction.h"
#include "httpResponse.h"
#include "errorMessage.h"
#include "staticString.h"
#include "STLTimeWheel.h"
#include "memoryPool.h"


#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include<numeric>
#include<forward_list>
#include<atomic>


//将原有redis 零拷贝实现的客户端实现拷贝数据返回有三种方式：
//第一种是每次返回数据时根据bool值判断，这种在高并发时候会消耗一定资源进行判断
//第二种是在原有的redis客户端中添加另一套一模一样的变量，实现拷贝版逻辑，但容易出问题
//第三种是另起一个同样的类实现，仅在解析函数内实现拷贝逻辑更替
//目前采用第三种实现
//而且确保目前实现的请求类型与原redis 零拷贝中的请求类型一致


struct MULTIREDISREADCOPY
{
	/*
	0  执行命令string_view集
	1  执行命令个数
	2  每条命令的词语个数（方便根据redis RESP进行拼接）
	3  获取结果次数  （因为比如一些事务操作可能不一定有结果返回）

	4  返回结果string_view
	5  每个结果的词语个数

	6  回调函数
	7  拷贝内存用的内存池
	*/
	// 
	using redisResultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, 
		std::reference_wrapper<std::vector<unsigned int>>, unsigned int,
		std::reference_wrapper<std::vector<std::string_view>>, std::reference_wrapper<std::vector<unsigned int>>,
		std::function<void(bool, enum ERRORMESSAGE)>, MEMORYPOOL<>*>;


	MULTIREDISREADCOPY(std::shared_ptr<boost::asio::io_context> ioc, std::shared_ptr<ASYNCLOG> log, std::shared_ptr<std::function<void()>>unlockFun,
		std::shared_ptr<STLTimeWheel> timeWheel,
		const std::string &redisIP, const unsigned int redisPort,
		const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize);


	//插入请求，首先判断是否连接redis服务器成功，
	//如果没有连接，插入直接返回错误
	//连接成功的情况下，检查请求是否符合要求
	bool insertRedisRequest(std::shared_ptr<redisResultTypeSW> &redisRequest);









private:
	std::shared_ptr<boost::asio::io_context> m_ioc{};
	boost::system::error_code m_err;
	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endPoint{};
	std::unique_ptr<boost::asio::ip::tcp::socket>m_sock{};

	std::shared_ptr<ASYNCLOG> m_log{};
	std::shared_ptr<STLTimeWheel> m_timeWheel{};

	std::shared_ptr<std::function<void()>> m_unlockFun{};
	std::string m_redisIP;
	unsigned int m_redisPort{};
	

	int m_connectStatus{};

	std::unique_ptr<char[]>m_receiveBuffer{};               //接收消息的缓存
	unsigned int m_receiveBufferMaxSize{};                //接收消息内存块总大小
	unsigned int m_receiveBufferNowSize{};                //每次检查的块大小


	unsigned int m_thisReceivePos{};              //计算每次receive pos开始处
	unsigned int m_thisReceiveLen{};              //计算每次receive长度


	unsigned int m_commandTotalSize{};                //本次节点结果数总计
	unsigned int m_commandCurrentSize{};              //本次节点结果数当前计数

	bool m_jumpNode{ false };
	unsigned int m_lastPrasePos{  };     //  m_lastReceivePos在每次解析后才去变更，记录当前解析到的位置
	//每次解析开始时检测当前节点位置，当前节点最大命令，已经获取到的命令数（以及该命令是否已经返回报错处理，而激活了跳过标志）
	// $-1\r\n$11\r\nhello_world\r\n               REDISPARSETYPE::LOT_SIZE_STRING
	// :11\r\n:0\r\n                               REDISPARSETYPE::INTERGER
	// -ERR unknown command `getx`, with args beginning with: `foo`, \r\n-ERR unknown command `getg`, with args beginning with: `dan`, \r\n                REDISPARSETYPE::ERROR
	// +OK\r\n+OK\r\n                              REDISPARSETYPE::SIMPLE_STRING
	// *2\r\n$3\r\nfoo\r\n$3\r\nfoo\r\n*2\r\n$3\r\nfoo\r\n$-1\r\n         REDISPARSETYPE::ARRAY


	std::unique_ptr<char[]>m_outRangeBuffer{};              //保存边界位置的字符串，如果越界的字符串长度超过m_outRangeMaxSize，直接返回0个，避免后面使用时出现空指针问题，数据错乱问题可以通过扩大buffer大小解决
	unsigned int m_outRangeMaxSize{};                     //
	unsigned int m_outRangeNowSize{};


	std::atomic<bool>m_connect{ false };                  //判断是否已经建立与redis端的连接
	std::atomic<bool>m_queryStatus{ false };
	


	//待投递队列，未/等待拼凑消息的队列
	//使用开源无所队列进一步提升qps ，这是github地址  https://github.com/cameron314/concurrentqueue
	moodycamel::ConcurrentQueue<std::shared_ptr<redisResultTypeSW>>m_messageList;


/////////////////////////////////////////////////////////
	std::unique_ptr<std::shared_ptr<redisResultTypeSW>[]>m_waitMessageList{};

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};

	std::shared_ptr<redisResultTypeSW> *m_waitMessageListBegin{};

	std::shared_ptr<redisResultTypeSW> *m_waitMessageListEnd{};

	std::shared_ptr<redisResultTypeSW> *m_waitMessageListBeforeParse{};      //用来判断解析后位置有没有发生变化

	std::shared_ptr<redisResultTypeSW> *m_waitMessageListAfterParse{};       //用来判断解析后位置有没有发生变化


	//一次性发送最大的命令个数
	unsigned int m_commandMaxSize{};

	//本次发送的命令个数
	unsigned int m_commandNowSize{};


	////////////////////////////////////////////////////////
	                    //联合发送命令字符串

	std::unique_ptr<char[]>m_messageBuffer{};

	unsigned int m_messageBufferMaxSize{};

	unsigned int m_messageBufferNowSize{};


	const char *m_sendMessage{};

	unsigned int m_sendLen{};

	std::vector<std::string_view>m_arrayResult;             //临时存储数组结果的vector
	//////////////////////////////////////////////////////////////////////////////

	boost::system::error_code ec;

private:
	bool readyEndPoint();


	bool readySocket();


	void firstConnect();

	//redis客户端和服务器失去连接后重连函数
	void reconnect();


	void setConnectSuccess();


	void setConnectFail();


	void query();
	

	void receive();


	void resetReceiveBuffer();

	//redis返回消息解析算法
	bool parse(const int size);


	void handlelRead(const boost::system::error_code &err, const std::size_t size);

	//发送给redis-server的命令拼装函数
	void readyMessage();


	void shutdownLoop();


	void cancelLoop();


	void closeLoop();


	void resetSocket();

	//拷贝数据保存函数
	void makeCopy(redisResultTypeSW& thisRequest, const char* source, const unsigned int sourceLen);

	//拷贝数据保存函数
	void makeCopy(redisResultTypeSW& thisRequest, const char* source1, const unsigned int sourceLen1, 
		const char* source2, const unsigned int sourceLen2);

	//拷贝数据保存函数
	void makeCopy(std::vector<std::string_view>& arrayResult, redisResultTypeSW& thisRequest, const char* source, const unsigned int sourceLen);

	//拷贝数据保存函数
	void makeCopy(std::vector<std::string_view>& arrayResult, redisResultTypeSW& thisRequest, const char* source1, const unsigned int sourceLen1,
		const char* source2, const unsigned int sourceLen2);
};

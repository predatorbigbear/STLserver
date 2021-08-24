#pragma once

#include "publicHeader.h"
#include "LOG.h"
#include "STLtreeFast.h"


struct MULTIREDISWRITE
{
	/*
	执行命令string_view集
	执行命令个数
	每条命令的词语个数（方便根据redis RESP进行拼接）
	加锁直接拼接字符串到总string中，发送前加锁取出数据进行发送

	*/
	// 
	using redisWriteTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>>;


	MULTIREDISWRITE(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &redisIP, const unsigned int redisPort,
		const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize);


	/*
	//插入请求，首先判断是否连接redis服务器成功，
	//如果没有连接，插入直接返回错误
	//连接成功的情况下，检查请求是否符合要求
	*/
	bool insertRedisRequest(std::shared_ptr<redisWriteTypeSW> redisRequest);







private:
	std::shared_ptr<io_context> m_ioc{};
	std::unique_ptr<boost::asio::steady_timer>m_timer{};
	boost::system::error_code m_err;
	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endPoint{};
	std::unique_ptr<boost::asio::ip::tcp::socket>m_sock{};



	std::shared_ptr<std::function<void()>> m_unlockFun{};
	std::string m_redisIP;
	unsigned int m_redisPort{};
	const char **m_data{};



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


	



	std::mutex m_mutex;
	bool m_connect{ false };                  //判断是否已经建立与redis端的连接
	bool m_queryStatus{ false };                         //判断是否已经启动处理流程，这里的queryStatus不仅仅是请求那么简单，而是包括请求，接收消息，将目前请求全部消化完毕的标志


	//待投递队列，未/等待拼凑消息的队列
	SAFELIST<std::shared_ptr<redisWriteTypeSW>>m_messageList;



	/////////////////////////////////////////////////////////
	std::unique_ptr<std::shared_ptr<redisWriteTypeSW>[]>m_waitMessageList{};

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};

	std::shared_ptr<redisWriteTypeSW> *m_waitMessageListBegin{};

	std::shared_ptr<redisWriteTypeSW> *m_waitMessageListEnd{};

	std::shared_ptr<redisWriteTypeSW> *m_waitMessageListBeforeParse{};      //用来判断解析后位置有没有发生变化

	std::shared_ptr<redisWriteTypeSW> *m_waitMessageListAfterParse{};       //用来判断解析后位置有没有发生变化



	//首先获取判断所需要的空间，尝试进行一次性分配空间，看是否成功
	//成功的话生成，然后进行发送写入请求

	//在最初的时候，曾经想过，用string_view指引业务层的数据进来插入，在拼装前进行一次copy即可。
	//但最后考虑过之后，还是选择了在write处进行一次copy，理由是：
	//使用redis场景下，读多写少，如果源数据在业务层，则业务层每次使用该buffer都需要进行加锁处理（但写可能很少很少发生，长远来看代价不值得）
	//其次是，源数据可能存在于多个地方的buffer中，这就使得最后所有buffer都可能要加锁使用
	MEMORYPOOL<char> m_charMemoryPool;

	//


	



	//一次性发送最大的命令个数
	unsigned int m_commandMaxSize{};

	//本次发送的命令个数
	unsigned int m_commandNowSize{};


	////////////////////////////////////////////////////////
	//std::string m_message;                     //联合发送命令字符串

	std::unique_ptr<char[]>m_messageBuffer{};

	unsigned int m_messageBufferMaxSize{};

	unsigned int m_messageBufferNowSize{};


	const char *m_sendMessage{};

	unsigned int m_sendLen{};

	std::vector<std::string_view>m_arrayResult;             //临时存储数组结果的vector
	//////////////////////////////////////////////////////////////////////////////


private:


	
	bool readyEndPoint();


	bool readySocket();

	
	void firstConnect();


	void setConnectSuccess();


	void setConnectFail();


	
	void query();

	
	void receive();


	void resetReceiveBuffer();


	void handlelRead(const boost::system::error_code &err, const std::size_t size);


	bool prase(const int size);


	void readyMessage();
	


};

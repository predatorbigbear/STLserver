#pragma once




#include "mysql/mysql.h"
#include "mysql/mysqld_error.h"
#include "ASYNCLOG.h"
#include "STLtreeFast.h"
#include "memoryPool.h"
#include "safeList.h"


#include<numeric>


struct MULTISQLWRITESW
{
	/*
	执行命令string_view集
	执行命令个数
	加锁直接拼接字符串到总string中，发送前加锁取出数据进行发送

	*/
	// 

	using SQLWriteTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int>;


	MULTISQLWRITESW(std::shared_ptr<boost::asio::io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const unsigned int nodeMaxSize ,std::shared_ptr<ASYNCLOG> log);


	/*
	//插入请求，首先判断是否连接redis服务器成功，
	//如果没有连接，插入直接返回错误
	//连接成功的情况下，检查请求是否符合要求
	*/
	bool insertSQLRequest(std::shared_ptr<SQLWriteTypeSW> sqlRequest);






private:



	MYSQL* m_mysql{nullptr};

	std::string m_host{};
	std::string m_user{};
	std::string m_passwd{};
	std::string m_db{};
	unsigned int m_port{};

	const char *m_unix_socket{};




	std::shared_ptr<std::function<void()>>m_unlockFun{};




	unsigned int m_commandMaxSize{};        // 命令个数最大大小（长度）

	unsigned int m_commandNowSize{};        // 命令个数实际大小（长度）



	std::unique_ptr<boost::asio::steady_timer> m_timer{};


	std::mutex m_mutex;
	bool m_hasConnect{ false };         //是否已经处于连接状态


	net_async_status m_status;


	bool resetConnect{ false };



	////////////////////////////////////////////////////////
	//std::string m_message;                     //联合发送命令字符串

	std::unique_ptr<char[]>m_messageBuffer{};

	unsigned int m_messageBufferMaxSize{};

	unsigned int m_messageBufferNowSize{};


	const char *m_sendMessage{};

	unsigned int m_sendLen{};

	std::vector<std::string_view>m_arrayResult;             //临时存储数组结果的vector
	//////////////////////////////////////////////////////////////////////////////

	bool m_queryStatus{ false };                         //判断是否已经启动处理流程，这里的queryStatus不仅仅是请求那么简单，而是包括请求，接收消息，将目前请求全部消化完毕的标志



	std::unique_ptr<char[]>m_receiveBuffer{};               //接收消息的缓存
	unsigned int m_receiveBufferMaxSize{};                //接收消息内存块总大小
	unsigned int m_receiveBufferNowSize{};                //每次检查的块大小


	unsigned int m_thisReceivePos{};              //计算每次receive pos开始处
	unsigned int m_thisReceiveLen{};              //计算每次receive长度


	unsigned int m_commandTotalSize{};                //本次节点结果数总计
	unsigned int m_commandCurrentSize{};              //本次节点结果数当前计数

	bool m_jumpNode{ false };
	unsigned int m_lastPrasePos{  };     //  m_lastReceivePos在每次解析后才去变更，记录当前解析到的位置


	//待获取结果队列，已经发送请求的队列，处理时使用m_waitMessageList进行处理，减少使用m_messageList的锁的机会
	std::unique_ptr<std::shared_ptr<SQLWriteTypeSW>[]>m_waitMessageList{};

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};


	//  根据m_waitMessageListBegin获取每次的RES
	std::shared_ptr<SQLWriteTypeSW> *m_waitMessageListBegin{};

	std::shared_ptr<SQLWriteTypeSW> *m_waitMessageListEnd{};


	MEMORYPOOL<> m_charMemoryPool;


	//待投递队列，未/等待拼凑消息的队列  C++17    临时sql buffer  buffer长度   命令个数
	//检查m_messageList是否为空，
	//为空的置标志位退出
	//不为空的尝试获取comnmandMaxSize 的节点数据，统计需要的buffer空间，
	//尝试根据上面的buffer空间进行分配
	//分配成功的话进行copy，然后进行下一次query
	//待投递队列，未/等待拼凑消息的队列
	SAFELIST<std::shared_ptr<SQLWriteTypeSW>>m_messageList;

	std::shared_ptr<ASYNCLOG> m_log{};

	 



private:

	void ConnectDatabase();


	void ConnectDatabaseLoop();


	void setConnectSuccess();


	void setConnectFail();


	//批量发送命令
	void query();


	void readyMessage();
	
};

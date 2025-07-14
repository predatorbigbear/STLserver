#pragma once




#include "mysql/mysql.h"
#include "mysql/mysqld_error.h"
#include "LOG.h"
#include "fastSafeList.h"
#include "errorMessage.h"

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>


#include<numeric>
#include<stdexcept>
#include<exception>
#include<atomic>
#include<memory>


//查找多个时的返回结果
//业务层使用vector<const char**>来装载存储指针，省却自动管理烦恼

//框架层继续传参，但使用string来管理数据，关键点采用指针返回操作


//  设置标志位，记录sql执行状态


//  插入待投递消息队列时，判断sql执行状态，如果sql没有执行，则置位执行（如果本次执行的条数超过限定条数，则直接报错返回），否则插入待投递字符串中，累加条数（记得检查累加条数不能超过限定的条数）


//  如果累加后的条数超过限定条数则不进行累加操作


//  



//



struct MULTISQLREADSW
{
	/*
	执行语句
	执行语句数量
	结果集
	结果行数 列数int
	结果数据
	回调函数
	本意是为了避免在插入sql查询前命令进行一次无谓的拷贝，在sql发送最终批量命令前才进行真正的copy，之前通过string_view传入，复用预定义字符串 + body里面的已有字符串，如果有多个命令，那么中间需要加;
	*/

	// 

	using resultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<MYSQL_RES*>>,
		std::reference_wrapper<std::vector<unsigned int>>, std::reference_wrapper<std::vector<std::string_view>>, std::function<void(bool, enum ERRORMESSAGE)>>;


	MULTISQLREADSW(std::shared_ptr<boost::asio::io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const unsigned int commandMaxSize, std::shared_ptr<LOG> log,const unsigned int bufferSize);


	//默认开启释放MYSQL_RES操作，需要多次查询mysql时可以置为false，复用结果
	bool insertSqlRequest(std::shared_ptr<resultTypeSW>& sqlRequest, const bool freeRes = true);



	

	~MULTISQLREADSW();





private:



	MYSQL *m_mysql{nullptr};

	std::string m_host{};
	std::string m_user{};
	std::string m_passwd{};
	std::string m_db{};
	unsigned int m_port{};

	const char *m_unix_socket{};


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	 //联合发送命令字符串

	std::unique_ptr<char[]>m_messageBuffer{};

	unsigned int m_messageBufferMaxSize{};

	unsigned int m_messageBufferNowSize{};




	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	const char *m_sendMessage{};

	unsigned int m_sendLen{};

	

	bool resetConnect{ false };

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::atomic<bool> m_hasConnect{ false };         //是否已经处于连接状态
	std::atomic<bool> m_queryStatus{ false };


	bool m_jumpThisRes{ false };                     //是否跳过本次请求的标志


	net_async_status m_status;
	net_async_status m_cleanAsyncStatus;   
	std::unique_ptr<boost::asio::steady_timer> m_timer{};
	std::unique_ptr<boost::asio::steady_timer> m_cleanTimer{};
	std::shared_ptr<std::function<void()>>m_unlockFun{};


	//等待释放结果集的指针，结果集个数
	std::shared_ptr<std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>>m_freeSqlResultFun{};




	unsigned int m_commandMaxSize{};        // 命令个数最大大小（长度）

	unsigned int m_commandNowSize{};        // 命令个数实际大小（长度）

	unsigned int m_thisCommandToalSize{};       // 本条节点的总请求个数

	unsigned int m_currentCommandSize{};        // 当前命令的已处理个数


	///////////////////////////////////////////////////////////////////
	//待投递队列，未/等待拼凑消息的队列
	FASTSAFELIST<std::shared_ptr<resultTypeSW>>m_messageList;



	//待获取结果队列，已经发送请求的队列，处理时使用m_waitMessageList进行处理，减少使用m_messageList的锁的机会
	std::unique_ptr<std::shared_ptr<resultTypeSW>[]>m_waitMessageList{};

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};


	//  根据m_waitMessageListBegin获取每次的RES
	std::shared_ptr<resultTypeSW> *m_waitMessageListBegin{};

	std::shared_ptr<resultTypeSW> *m_waitMessageListEnd{};

	///////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////
	/*
	执行语句指针
	指针个数 （实际个数=指针个数/2  因为包含首尾位置，如果是多句中间需要包含;）
	执行语句数量
	结果集指针
	结果行数 列数int*    每2个存储一个结果集的行数列数
	结果行数 列数buffer大小
	结果指针，存储每一个的结果指针
	结果指针buffer大小
	回调函数
	*/


	std::shared_ptr<LOG> m_log{};
	/////////////////////////////////////////////////////////

	MYSQL_FIELD *fd;    //字段列数组
	MYSQL_RES *m_result{ };    //行的一个查询结果集
	MYSQL_ROW m_row;   //数据行的列

	unsigned int m_rowNum{};
	int m_rowCount{};
	unsigned int m_fieldNum{};
	int m_fieldCount{};
	const char **m_data{};
	unsigned long *m_lengths{};


	unsigned int m_temp{};

private:
	void FreeResult(std::reference_wrapper<std::vector<MYSQL_RES*>> vecwrapper);


	std::shared_ptr<std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>> sqlRelease();


	void FreeConnect();


	void ConnectDatabase();


	void ConnectDatabaseLoop();


	void firstQuery();


	void handleAsyncError();


	void store();


	void readyQuery();


	bool readyNextQuery();


	void next_result();

	//检查m_messageList是否有需要拼装处理的消息
	void makeMessage();


	//  每次重新组装消息时候，重置复用的变量  
	void readyMakeMessage();


	//检查待请求队列中是否有消息存在
	void checkMakeMessage();


	void fetch_row();


	//同步清理MYSQL_RES*
	void freeResult(std::shared_ptr<resultTypeSW> *m_waitMessageListBegin, std::shared_ptr<resultTypeSW> *m_waitMessageListEnd);


	//处理sql执行过程中插入错误问题
	void handleSqlMemoryError();
};

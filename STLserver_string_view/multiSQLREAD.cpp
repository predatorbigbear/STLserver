#include "multiSQLREAD.h"

MULTISQLREAD::MULTISQLREAD(const std::shared_ptr<boost::asio::io_context>& ioc, const std::shared_ptr<std::function<void()>>& unlockFun,
	const std::shared_ptr<STLTimeWheel>& timeWheel, const std::shared_ptr<ASYNCLOG>& log, 
	const std::string& SQLHOST, const std::string& SQLUSER, const std::string& SQLPASSWORD,
	const std::string& SQLDB, const unsigned int& SQLPORT, 
	const unsigned int commandMaxSize, bool& success, const unsigned int bufferSize):m_ioc(ioc),m_unlockFun(unlockFun),
	m_timeWheel(timeWheel),m_log(log), m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB),
	m_port(SQLPORT), m_commandMaxSize(commandMaxSize), m_msgBufMaxSize(bufferSize), m_success(&success)
{
	//在外层进行数据检查

	m_msgBuf.reset(new unsigned char[m_msgBufMaxSize]);

	m_endPoint.reset(new boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(m_host), m_port));

	m_sock.reset(new boost::asio::ip::tcp::socket(*m_ioc));


}



void MULTISQLREAD::connectMysql()
{
	m_sock->async_connect(*m_endPoint, [this](const boost::system::error_code& err)
	{
		if (err)
		{
			*m_success = false;
			(*m_unlockFun)();
		}
		else
		{
			//开始接收mysql 握手包
			m_readLen = 0;
			recvHandShake();
		}
	});

}



void MULTISQLREAD::recvHandShake()
{
	m_sock->async_read_some(boost::asio::buffer(m_msgBuf.get() + m_readLen, m_msgBufMaxSize - m_readLen),
		[this](const boost::system::error_code& err, const std::size_t bytes_transferred)
	{
		if (err)
		{
			*m_success = false;
			(*m_unlockFun)();
		}
		else
		{
			m_readLen += bytes_transferred;
			//解析握手包
			if (m_readLen < 4)
				return recvHandShake();

			//解析本次数据包消息长度
			m_messageLen = static_cast<unsigned int>(*(m_msgBuf.get())) + static_cast<unsigned int>(*(m_msgBuf.get() + 1)) * 256 +
				static_cast<unsigned int>(*(m_msgBuf.get() + 2)) * 65536;

			if (m_readLen < m_messageLen + 4)
				return recvHandShake();

			//解析本次数据包消息序列号
			m_seqID = static_cast<unsigned int>(*(m_msgBuf.get() + 3));

		}
	});
}





#include "multiSQLREAD.h"

MULTISQLREAD::MULTISQLREAD(const std::shared_ptr<boost::asio::io_context>& ioc, const std::shared_ptr<std::function<void()>>& unlockFun,
	const std::shared_ptr<STLTimeWheel>& timeWheel, const std::shared_ptr<ASYNCLOG>& log, 
	const std::string& SQLHOST, const std::string& SQLUSER, const std::string& SQLPASSWORD,
	const std::string& SQLDB, const unsigned int SQLPORT, 
    bool& success,
    const unsigned int commandMaxSize, const unsigned int bufferSize) :m_ioc(ioc), m_unlockFun(unlockFun),
    m_timeWheel(timeWheel), m_log(log), m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB),
    m_port(SQLPORT), m_commandMaxSize(commandMaxSize), m_msgBufMaxSize(bufferSize * 6), m_clientBufMaxSize(bufferSize),
    m_success(&success),m_messageList(commandMaxSize * 6)
{
	//在外层进行数据检查

	m_msgBuf.reset(new unsigned char[m_msgBufMaxSize]);

	m_endPoint.reset(new boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(m_host), m_port));

	m_sock.reset(new boost::asio::ip::tcp::socket(*m_ioc));

    m_waitMessageList.reset(new std::shared_ptr<MYSQLResultTypeSW>[m_commandMaxSize]);

    m_waitMessageListMaxSize = m_commandMaxSize;


    m_commandBuf.reset(new unsigned char*[commandMaxSize * commandMaxSize]);

    m_colLenArr.reset(new unsigned int[m_colLenMax]);

	connectMysql();
}



/*
    0执行命令string_view集
    1执行命令个数
    2每条命令的string_view个数（方便进行拼接）

    3 first 每条命令获取结果个数，second 是否为事务语句  （因为比如一些事务操作可能不一定有结果返回，利用second
    可以在事务中发生错误时快速跳转指针指向）


    4返回结果string_view
    5每个结果的string_view个数

    6回调函数
    */
bool MULTISQLREAD::insertMysqlRequest(std::shared_ptr<MYSQLResultTypeSW>& mysqlRequest)
{
    if (!mysqlRequest)
        return false;

    MYSQLResultTypeSW& thisRequest{ *mysqlRequest };

    
    if (std::get<0>(thisRequest).get().empty() || !std::get<1>(thisRequest) || std::get<1>(thisRequest) > m_commandMaxSize
        || std::get<3>(thisRequest).get().size() != std::get<1>(thisRequest)
        || std::get<1>(thisRequest) != std::get<2>(thisRequest).get().size() ||
        std::accumulate(std::get<2>(thisRequest).get().cbegin(), std::get<2>(thisRequest).get().cend(), 0) != std::get<0>(thisRequest).get().size()
         || std::find_if(std::get<3>(thisRequest).get().cbegin(), std::get<3>(thisRequest).get().cend(), [](const std::pair<unsigned int, bool>& everyPair)
    {
        return everyPair.first;
    })== std::get<3>(thisRequest).get().cend())
        return false;

    int status{ m_queryStatus.load(std::memory_order_acquire) };
    if (!status)
    {
        return false;
    }
    else if (status == 1)
    {
        m_queryStatus.store(2, std::memory_order_release);
    
    // 
        //////////////////////////////////////////////////////////////////////
        clientSendLen = std::accumulate(std::get<0>(thisRequest).get().cbegin(), std::get<0>(thisRequest).get().cend(), 0, [](auto& sum, auto const sw)
        {
            return sum += sw.size();
        });
        clientSendLen += std::get<1>(thisRequest);

        if (clientSendLen > (m_clientBufMaxSize - 5))
        {
            m_queryStatus.store(1, std::memory_order_release);
            return false;
        }


        try
        {
            //其实这里是不可能走到的，因为m_msgBufMaxSize大小是m_clientBufMaxSize的6倍
            if (m_msgBufMaxSize < (clientSendLen + 5))
            {
                m_msgBuf.reset(new unsigned char[clientSendLen + 5]);
                m_msgBufMaxSize = clientSendLen + 5;
            }
        }
        catch (const std::bad_alloc& e)
        {
            m_queryStatus.store(1, std::memory_order_release);
            m_msgBufMaxSize = 0;
            return false;
        }


        unsigned char* buffer{ m_msgBuf.get() + 4 };
        int index{ 0 };
        std::vector<unsigned int>::const_iterator sqlNumIter{ std::get<2>(thisRequest).get().cbegin() };
        
		*buffer++ = 0x03; //com_query

        m_commandBufBegin = m_commandBuf.get();
        *m_commandBufBegin++ = buffer;
        for (auto sw : std::get<0>(thisRequest).get())
        {
            std::copy(sw.cbegin(), sw.cend(), buffer);
            buffer += sw.size();
            if (++index == *sqlNumIter)
            {
                index = 0;
                ++sqlNumIter;
                *buffer++ = ';';
                *m_commandBufBegin++ = buffer;
            }
        }

        m_commandBufEnd = m_commandBufBegin;
        m_commandBufBegin = m_commandBuf.get();

        m_VecBegin = std::get<3>(thisRequest).get().cbegin();
        m_VecEnd = std::get<3>(thisRequest).get().cend();

        do
        {
            if (m_VecBegin->first)
                break;
            ++m_commandBufBegin;
        }while (++m_VecBegin != m_VecEnd);

        //加上0x03
        clientSendLen += 1;
        clientBegin = m_msgBuf.get();
        *clientBegin = clientSendLen % 256;
        *(clientBegin + 1) = clientSendLen / 256 % 256;
        *(clientBegin + 2) = clientSendLen / 65536 % 256;

        if (firstQuery)
        {
            firstQuery = false;
            *(clientBegin + 3) = m_seqID = 0;
        }
        else
            *(clientBegin + 3) = ++m_seqID;

        clientSendLen += 4;

        *(m_waitMessageList.get()) = mysqlRequest;

        m_waitMessageListBegin = m_waitMessageList.get();

        m_waitMessageListEnd = m_waitMessageListBegin + 1;

        m_commandTotalSize = m_VecBegin->first;

        m_commandCurrentSize = 0;

		m_sendBuf = m_msgBuf.get();

        m_recvBuf = m_msgBuf.get() + clientSendLen;


        //开始接收mysql查询结果
            //是否跳过本次请求的标志
        m_jumpNode = false;

        //检查次数
        m_checkTime = false;

        //是否开始获取结果
        m_getResult = false;

        m_everyCommandResultSum = 0;

		//发送sql请求
        mysqlQuery();       
        

        return true;
    }
    else
    {
        if (!m_messageList.try_enqueue(std::shared_ptr<MYSQLResultTypeSW>(mysqlRequest)))
            return false;

        return true;
    }
    
}



void MULTISQLREAD::connectMysql()
{
	m_sock->async_connect(*m_endPoint, [this](const boost::system::error_code& err)
	{
		if (err)
		{
            if (firstConnect)
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            m_log->writeLog("MULTISQLREAD::connectMysql", err.value(), err.message());
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
            if (firstConnect)
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            m_log->writeLog("MULTISQLREAD::recvHandShake", err.value(), err.message());
		}
		else
		{
			m_readLen += bytes_transferred;
			//解析握手包
			if (m_readLen < 4)
				return recvHandShake();

			//解析本次数据包消息长度
			m_messageLen = parseMessageLen(m_msgBuf.get());

			if (m_readLen < m_messageLen + 4)
				return recvHandShake();

			//解析本次数据包消息序列号
			m_seqID = *(m_msgBuf.get() + 3);

            if (!parseHandShake())
            {
                if (firstConnect)
                {
                    *m_success = false;
                    (*m_unlockFun)();
                }
                m_log->writeLog("MULTISQLREAD::recvHandShake", "parseHandShake");
            }
            else
            {
                //组装握手认证包
                //可以参考
                // https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets_protocol_handshake_response.html
                if (!makeHandshakeResponse41())
                {
                    if (firstConnect)
                    {
                        *m_success = false;
                        (*m_unlockFun)();
                    }
                    m_log->writeLog("MULTISQLREAD::recvHandShake", "makeHandshakeResponse41");
                }
                else
                {
                    sendHandshakeResponse41();
                }
            }
		}
	});
}



bool MULTISQLREAD::parseHandShake()
{
	strBegin = m_msgBuf.get() + 4;
	strEnd = strBegin + m_messageLen;

    // 握手包参数参考 https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets_protocol_handshake_v10.html
    unsigned int protocolVersion{};

    const unsigned char* serverVersionBegin{}, * serverVersionEnd{};

    unsigned int threadID{};

    const unsigned char* autuPluginDataPart1Begin{}, * autuPluginDataPart1End{};

    int filler{};

    const unsigned char* capabilityFlags1Begin{}, * capabilityFlags1End{};

    unsigned int characterSet{};

    const unsigned char* statusFlagsBegin{}, * statusFlagsEnd{};

    const unsigned char* capabilityFlags2Begin{}, * capabilityFlags2End{};

    unsigned int capabilityFlags1{};
    unsigned int capabilityFlags2{};
    unsigned int capabilityFlags{};

    unsigned int authPluginDataLen{};
    unsigned int authPluginDataLeftLen{};

    const unsigned char* autuPluginDataPart2Begin{}, * autuPluginDataPart2End{};

    const unsigned char* authPluginNameBegin{}, * authPluginNameEnd{};


    authPluginDataLen = 0;
    protocolVersion = *strBegin;
    if (protocolVersion != 10)
        return false;


    serverVersionBegin = strBegin + 1;
    serverVersionEnd = std::find(serverVersionBegin, strEnd, 0);
    if (serverVersionEnd == strEnd)
        return false;

    if (std::distance(serverVersionEnd + 1, strEnd) < 4)
        return false;


    threadID = static_cast<unsigned int>(*(serverVersionEnd + 1)) + static_cast<unsigned int>(*(serverVersionEnd + 2)) * 256 +
        static_cast<unsigned int>(*(serverVersionEnd + 3)) * 65536 + static_cast<unsigned int>(*(serverVersionEnd + 4)) * 16777216;

    if (std::distance(serverVersionEnd + 5, strEnd) < 8)
        return false;

    autuPluginDataPart1Begin = serverVersionEnd + 5;
    autuPluginDataPart1End = autuPluginDataPart1Begin + 8;


    if (autuPluginDataPart1End == strEnd)
        return false;

    filler = *(autuPluginDataPart1End);

    if (filler)
        return false;

    if (std::distance(autuPluginDataPart1End + 1, strEnd) < 2)
        return false;

    capabilityFlags1Begin = autuPluginDataPart1End + 1;
    capabilityFlags1End = capabilityFlags1Begin + 2;


    if (capabilityFlags1End == strEnd)
        return false;

    characterSet = static_cast<unsigned int>(*capabilityFlags1End);


    if (std::distance(capabilityFlags1End + 1, strEnd) < 2)
        return false;

    statusFlagsBegin = capabilityFlags1End + 1;
    statusFlagsEnd = statusFlagsBegin + 2;


    if (std::distance(statusFlagsEnd, strEnd) < 2)
        return false;

    capabilityFlags2Begin = statusFlagsEnd;
    capabilityFlags2End = capabilityFlags2Begin + 2;


    capabilityFlags1 = static_cast<unsigned int>(*(capabilityFlags1Begin)) +
        static_cast<unsigned int>(*(capabilityFlags1Begin + 1)) * 256;

    capabilityFlags2 = static_cast<unsigned int>(*(capabilityFlags2Begin)) * 65536 +
        static_cast<unsigned int>(*(capabilityFlags2Begin + 1)) * 16777216;

    capabilityFlags = capabilityFlags1 + capabilityFlags2;


    if (capabilityFlags2End == strEnd)
        return false;

    if (capabilityFlags & MYSQL_CLIENT_PLUGIN_AUTH)
    {
        authPluginDataLen = static_cast<unsigned int>(*capabilityFlags2End);
    }
    else
    {
        if (*capabilityFlags2End)
            return false;
    }

    if (std::distance(capabilityFlags2End + 1, strEnd) < 10)
        return false;

    if (!std::all_of(capabilityFlags2End + 1, capabilityFlags2End + 11, std::bind(std::equal_to<>(), std::placeholders::_1, 0)))
        return false;

    if (!authPluginDataLen || authPluginDataLen < 8)
        authPluginDataLeftLen = 13;
    else
        authPluginDataLeftLen = std::max(static_cast<unsigned int>(13), authPluginDataLen - 8);

    if (std::distance(capabilityFlags2End + 11, strEnd) < authPluginDataLeftLen)
        return false;

    autuPluginDataPart2Begin = capabilityFlags2End + 11;
    autuPluginDataPart2End = autuPluginDataPart2Begin + authPluginDataLeftLen;


    if (capabilityFlags & MYSQL_CLIENT_PLUGIN_AUTH)
    {
        if (autuPluginDataPart2End == strEnd)
        {
            authPluginNameBegin = nullptr;
            authPluginNameEnd = nullptr;
            return false;
        }
        else
        {
            authPluginNameBegin = autuPluginDataPart2End;
            authPluginNameEnd = std::find(authPluginNameBegin, strEnd, 0);
            if(!std::equal(authPluginNameBegin, authPluginNameEnd, plugin_name.cbegin(), plugin_name.cend()))
                return false;
        }
    }
    else
    {
        authPluginNameBegin = nullptr;
        authPluginNameEnd = nullptr;
        return false;
    }

    if (std::distance(autuPluginDataPart1Begin, autuPluginDataPart1End) +
        std::distance(autuPluginDataPart2Begin, autuPluginDataPart2End) != 21)
        return false;

	//保存服务器能力标志位
    serverCapabilityFlags = capabilityFlags;

	//保存认证插件数据前20位
    try
    {
        autuPluginData.assign(autuPluginDataPart1Begin, autuPluginDataPart1End);
        autuPluginData.append(autuPluginDataPart2Begin, autuPluginDataPart2End - 1);
    }
    catch (const std::exception& e)
    {
        return false;
	}


	return true;
}



bool MULTISQLREAD::makeHandshakeResponse41()
{
    handShakeLen = 4 + 4 + 1 + 23 + m_user.size() + 1 + 32 + 1 + m_db.size() + 1 + plugin_name.size() + 1 + 4;

    if (m_msgBufMaxSize < handShakeLen)
        return false;


    //状态标志参考
    //https://dev.mysql.com/doc/dev/mysql-server/latest/group__group__cs__capabilities__flags.html
    clientFlag = MYSQL_CLIENT_PROTOCOL_41 | MYSQL_CLIENT_LONG_PASSWORD | MYSQL_CLIENT_FOUND_ROWS |
        MYSQL_CLIENT_SECURE_CONNECTION | MYSQL_CLIENT_LOCAL_FILES | MYSQL_CLIENT_LONG_FLAG |
        MYSQL_CLIENT_CONNECT_WITH_DB |
        MYSQL_CLIENT_MULTI_RESULTS |
        MYSQL_CLIENT_MULTI_STATEMENTS |
        MYSQL_CLIENT_PS_MULTI_RESULTS |
        MYSQL_CLIENT_PLUGIN_AUTH;

    clientBegin = m_msgBuf.get();
    clientIter = clientBegin + 4;

    *clientIter++ = clientFlag % 256;
    *clientIter++ = clientFlag / 256 % 256;
    *clientIter++ = clientFlag / 65536 % 256;
    *clientIter++ = clientFlag / 16777216;


    *clientIter++ = m_clientBufMaxSize % 256;
    *clientIter++ = m_clientBufMaxSize / 256 % 256;
    *clientIter++ = m_clientBufMaxSize / 65536 % 256;
    *clientIter++ = m_clientBufMaxSize / 16777216;

    //utf8mb4_unicode_ci 
    *clientIter++ = utf8mb4_0900_ai_ci;

    std::fill(clientIter, clientIter + 23, 0);
    clientIter += 23;

    std::copy(m_user.cbegin(), m_user.cend(), clientIter);
    clientIter += m_user.size();
    *clientIter++ = 0;


    unsigned char buf1[52]{};

    *clientIter++ = 32;

    //密码加密实现参考了搜狗workflow，在workflow基础上进行了进一步优化
    // workflow-0.11.10/src/protocol/MySQLMessage.cc    
    // static std::string __caching_sha2_password_encrypt(const std::string& password,unsigned char seed[20])
    //https://dev.mysql.com/doc/dev/mysql-server/latest/page_caching_sha2_authentication_exchanges.html
     // 第一轮：对原始密码进行SHA256哈希
    SHA256((unsigned char*)m_passwd.data(), m_passwd.size(), clientIter);

    // 第二轮：对hash1再次进行SHA256哈希
    SHA256(clientIter, 32, buf1);

    // 第三轮：SHA256(hash2 + challenge)
    std::copy(autuPluginData.cbegin(), autuPluginData.cend(), buf1 + 32);
    SHA256(buf1, 52, buf1);
    for (int i = 0; i != 32; ++i)
        *(clientIter + i) ^= buf1[i];

    clientIter += 32;

    std::copy(m_db.cbegin(), m_db.cend(), clientIter);
    clientIter += m_db.size();
    *clientIter++ = 0;

    std::copy(plugin_name.cbegin(), plugin_name.cend(), clientIter);
    clientIter += plugin_name.size();
    *clientIter++ = 0;

    clientSendLen = std::distance(clientBegin, clientIter) - 4;

    *clientBegin = clientSendLen % 256;
    *(clientBegin + 1) = clientSendLen / 256 % 256;
    *(clientBegin + 2) = clientSendLen / 65536 % 256;
    *(clientBegin + 3) = ++m_seqID;

    clientSendLen += 4;

    return true;

}



void MULTISQLREAD::sendHandshakeResponse41()
{
    boost::asio::async_write(*m_sock, boost::asio::buffer(m_msgBuf.get(), clientSendLen),
        [this](const boost::system::error_code& err, const std::size_t size)
    {
        if (err)  
        {
            if (firstConnect)
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            m_log->writeLog("MULTISQLREAD::sendHandshakeResponse41", err.value(), err.message());
        }
        else
        {
			//开始接收认证结果  判断快速认证成功或者需要进行完整认证
            //当重启服务器或者重启mysql或者没有登陆过mysql时，触发完整认证
            //当服务器运行期间登陆过mysql，则触发快速认证
            m_readLen = 0;
            recvAuthResult1();
        }
    });

}



unsigned int MULTISQLREAD::parseMessageLen(const unsigned char* messageIter)
{
    if (!messageIter)
        return 0;
    return *(messageIter)+*(messageIter + 1) * 256 + *(messageIter + 2) * 65536;
}



void MULTISQLREAD::recvAuthResult1()
{
    m_sock->async_read_some(boost::asio::buffer(m_msgBuf.get() + m_readLen, m_msgBufMaxSize - m_readLen),
        [this](const boost::system::error_code& err, const std::size_t bytes_transferred)
    {
        if (err)
        {
            if (firstConnect)
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            m_log->writeLog("MULTISQLREAD::recvAuthResult1", err.value(), err.message());
        }
        else
        {
            m_readLen += bytes_transferred;
            
            if (m_readLen < 4)
                return recvAuthResult1();

            //解析本次数据包消息长度
            m_messageLen = parseMessageLen(m_msgBuf.get());

            if (m_readLen < m_messageLen + 4)
                return recvAuthResult1();

            //解析本次数据包消息序列号
            m_seqID = *(m_msgBuf.get() + 3);

            int status = parseAuthResult1();

            if (!status)
            {
                m_log->writeLog("MULTISQLREAD::parseAuthResult1", PRINTHEX(strBegin, m_messageLen));
            }
            else if (status == 1)
            {
                //快速认证成功
                //准备接收OK包
				//快速认证成功后会紧随一个OK包（0x00 0x
                //https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_basic_ok_packet.html
                int thisMessageLen = m_messageLen + 4;
                if (m_readLen - thisMessageLen < 4)
                {
                    std::copy(m_msgBuf.get() + thisMessageLen, m_msgBuf.get() + m_readLen, m_msgBuf.get());
                    m_readLen = std::distance(m_msgBuf.get() + thisMessageLen, m_msgBuf.get() + m_readLen);

                    //接收OK包
                    return recvAuthOKPacket();
                }

                m_messageLen = parseMessageLen(m_msgBuf.get() + thisMessageLen);
    
                if ((m_readLen - thisMessageLen) < (m_messageLen + 4))
                {
                    std::copy(m_msgBuf.get() + thisMessageLen, m_msgBuf.get() + m_readLen, m_msgBuf.get());
                    m_readLen = std::distance(m_msgBuf.get() + thisMessageLen, m_msgBuf.get() + m_readLen);
                    //接收OK包
                    return recvAuthOKPacket();
                }

                strBegin = m_msgBuf.get() + thisMessageLen + 4;
                strEnd = strBegin + m_messageLen;
                if (!std::equal(strBegin, strEnd, OKPacket, OKPacket + 7))
                {
                    m_log->writeLog("MULTISQLREAD::parseAuthResult1OK", PRINTHEX(strBegin, m_messageLen));
                    if (firstConnect)
                    {
                        *m_success = false;
                        (*m_unlockFun)();
                    }
                }
                else
                {
                    if (firstConnect)
                    {
                        std::cout << "new mysql module auth success\n";
                        *m_success = true;
                        (*m_unlockFun)();
                    }
                    m_seqID = 0;
                    firstQuery = true;
                    m_queryStatus.store(1, std::memory_order_release);
                    m_log->writeLog("connect mysql success");
                }
            }
            else
            {
                //需要完整认证
                //发送请求公钥数据包

                // 步骤 请求服务器公钥

                clientIter = m_msgBuf.get();

                std::copy(reqPubkey, reqPubkey + 5, clientIter);
                clientIter += 5;

                *(m_msgBuf.get() + 3) = ++m_seqID;
                clientSendLen = 5;
                sendReqPubkey();
            }
        }
    });
}



int MULTISQLREAD::parseAuthResult1()
{
    strBegin = m_msgBuf.get() + 4;
    strEnd = strBegin + m_messageLen;

    //检查是否是认证流程中的中间响应类型（AuthMoreData包）
    //‌首字节0x01‌
    //标识该数据包为认证流程中的中间响应类型（AuthMoreData包），用于通知客户端需要进一步交互完成认证

    //‌次字节0x03‌
    //表示当前处于‌快速认证（fast authentication）‌模式，说明服务器已缓存用户密码且验证通过。此时会紧随一个正常的OK包（0x00 0x00 0x00格式），
    // 客户端可直接进入命令执行阶段
    //若次字节为0x04则需完整认证（full authentication），要求客户端发送加密后的密码
    //https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets_protocol_auth_more_data.html

    //非AuthMoreData包
    if (m_messageLen != 2 || *strBegin != 0x01)
    {

        return 0;
    }

    if (*(strBegin + 1) == 0x03)
    {
        //快速认证成功
        return 1;
    }
    else if (*(strBegin + 1) == 0x04)
    {
        //需要完整认证
        
        return 2;
    }
    else
    {
      
        return 0;
	}


    return 0;
}



void MULTISQLREAD::recvAuthOKPacket()
{
    m_sock->async_read_some(boost::asio::buffer(m_msgBuf.get() + m_readLen, m_msgBufMaxSize - m_readLen),
        [this](const boost::system::error_code& err, const std::size_t bytes_transferred)
    {
        if (err)
        {
            if (firstConnect)
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            m_log->writeLog("MULTISQLREAD::recvAuthResult1", err.value(), err.message());
        }
        else
        {
            m_readLen += bytes_transferred;
            
            if (m_readLen < 4)
                return recvAuthOKPacket();

            //解析本次数据包消息长度
            m_messageLen = parseMessageLen(m_msgBuf.get());

            if (m_readLen < m_messageLen + 4)
                return recvAuthOKPacket();

            //解析本次数据包消息序列号
            m_seqID = *(m_msgBuf.get() + 3);

            strBegin = m_msgBuf.get() + 4;
            strEnd = strBegin + m_messageLen;

            if (!std::equal(strBegin, strEnd, OKPacket, OKPacket + 7))
            {
                m_log->writeLog("MULTISQLREAD::parseAuthResult1OK", PRINTHEX(strBegin, m_messageLen));
                if (firstConnect)
                {
                    *m_success = false;
                    (*m_unlockFun)();
                }
            }
            else
            {
                if (firstConnect)
                {
                    std::cout << "new mysql module auth success\n";
                    *m_success = true;
                    (*m_unlockFun)();
                }
                m_seqID = 0;
                firstQuery = true;
                m_queryStatus.store(1, std::memory_order_release);
                m_log->writeLog("connect mysql success");
            }
        }
    });
}




void MULTISQLREAD::sendReqPubkey()
{
    boost::asio::async_write(*m_sock, boost::asio::buffer(m_msgBuf.get(), clientSendLen),
        [this](const boost::system::error_code& err, const std::size_t size)
    {
        if (err)
        {
            if (firstConnect)
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            m_log->writeLog("MULTISQLREAD::sendHandshakeResponse41", err.value(), err.message());
        }
        else
        {
            //开始接收公钥
            
            m_readLen = 0;
            recvPubkey();
        }
    });

}




void MULTISQLREAD::recvPubkey()
{
    m_sock->async_read_some(boost::asio::buffer(m_msgBuf.get() + m_readLen, m_msgBufMaxSize - m_readLen),
        [this](const boost::system::error_code& err, const std::size_t bytes_transferred)
    {
        if (err)
        {
            if (firstConnect)
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            m_log->writeLog("MULTISQLREAD::recvAuthResult1", err.value(), err.message());
        }
        else
        {
            m_readLen += bytes_transferred;

            if (m_readLen < 4)
                return recvPubkey();

            //解析本次数据包消息长度
            m_messageLen = parseMessageLen(m_msgBuf.get());

            if (m_messageLen < 1 + publicKey1.size() + publicKey2.size())
            {
                if (firstConnect)
                {
                    *m_success = false;
                    (*m_unlockFun)();
                }
                return m_log->writeLog("MULTISQLREAD::recvAuthResult1", err.value(), err.message());
            }

            if (m_readLen < m_messageLen + 4)
                return recvPubkey();

            //解析本次数据包消息序列号
            m_seqID = *(m_msgBuf.get() + 3);

           //解析公钥数据包
            if (!parsePubkey())
            {
                if (firstConnect)
                {
                    *m_success = false;
                    (*m_unlockFun)();
                }
				return m_log->writeLog("MULTISQLREAD::parsePubkey", PRINTHEX(strBegin, m_messageLen));
            }


            //组装新的公钥加密密码认证包
            if (!makePubkeyAuth())
            {
                if (firstConnect)
                {
                    *m_success = false;
                    (*m_unlockFun)();
                }
                return m_log->writeLog("MULTISQLREAD::makePubkeyAuth");
            }

            //发送公钥加密密码认证包进行认证
            sendPubkeyAuth();
        }
    });

}




bool MULTISQLREAD::parsePubkey()
{
    strBegin = m_msgBuf.get() + 4;
    strEnd = strBegin + m_messageLen;

    if (*strBegin != 1)
        return false;

    if (!std::equal(strBegin + 1, strBegin + 1 + publicKey1.size(), publicKey1.cbegin(), publicKey1.cend()))
        return false;

    publicKeyBegin = strBegin + 1;

    publicKeyEnd = std::search(publicKeyBegin + publicKey1.size(), strEnd, publicKey2.cbegin(), publicKey2.cend());
    if (publicKeyEnd == strEnd)
        return false;

    publicKeyEnd += publicKey2.size();


    return true;
}





bool MULTISQLREAD::makePubkeyAuth()
{
    BIO* bio = BIO_new_mem_buf(publicKeyBegin, std::distance(publicKeyBegin, publicKeyEnd));
    RSA* rsa = PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);

    if (!rsa)
    {
        ERR_print_errors_fp(stderr);
        BIO_free(bio);
        return false;
    }

    int rsa_len = RSA_size(rsa);
    if ((m_msgBufMaxSize - 4) < rsa_len)
    {
        RSA_free(rsa);
        BIO_free(bio);
        return false;
    }

    // 确保密码长度不超过RSA密钥长度减去11（PKCS1填充）末尾需要填充0
    if (m_passwd.size() > rsa_len - 10)
    {
        RSA_free(rsa);
        BIO_free(bio);
        return false;
    }

    m_encryptPass = m_passwd;
    m_encryptPass += '\0';

    for (int i = 0; i < m_encryptPass.size(); ++i)
        m_encryptPass[i] ^= autuPluginData[i % autuPluginData.size()];

    int result = RSA_public_encrypt(
        m_encryptPass.size(),
        (const unsigned char*)m_encryptPass.c_str(),
        m_msgBuf.get() + 4,
        rsa,
        RSA_PKCS1_OAEP_PADDING
    );

    RSA_free(rsa);
    BIO_free(bio);

    if (result == -1)
        return false;
    

    ///////////////////////////////// 封装加密后的密码认证数据包并发送给服务器 //////////////////////

    clientSendLen = result;

    clientBegin = m_msgBuf.get();
    *clientBegin = clientSendLen % 256;
    *(clientBegin + 1) = clientSendLen / 256 % 256;
    *(clientBegin + 2) = clientSendLen / 65536 % 256;
    *(clientBegin + 3) = ++m_seqID;

    clientSendLen += 4;


    return true;
}



void MULTISQLREAD::sendPubkeyAuth()
{
    boost::asio::async_write(*m_sock, boost::asio::buffer(m_msgBuf.get(), clientSendLen),
        [this](const boost::system::error_code& err, const std::size_t size)
    {
        if (err)
        {
            if (firstConnect)
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            m_log->writeLog("MULTISQLREAD::sendHandshakeResponse41", err.value(), err.message());
        }
        else
        {
            //开始接收公钥

            m_readLen = 0;
            recvPubkeyAuth();
        }
    });
}



void MULTISQLREAD::recvPubkeyAuth()
{
    m_sock->async_read_some(boost::asio::buffer(m_msgBuf.get() + m_readLen, m_msgBufMaxSize - m_readLen),
        [this](const boost::system::error_code& err, const std::size_t bytes_transferred)
    {
        if (err)
        {
            if (firstConnect)
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            m_log->writeLog("MULTISQLREAD::recvHandShake", err.value(), err.message());
        }
        else
        {
            m_readLen += bytes_transferred;
            //解析握手包
            if (m_readLen < 4)
                return recvPubkeyAuth();

            //解析本次数据包消息长度
            m_messageLen = parseMessageLen(m_msgBuf.get());


            if (m_readLen < m_messageLen + 4)
                return recvPubkeyAuth();

            //解析本次数据包消息序列号
            m_seqID = *(m_msgBuf.get() + 3);

            //判断是否是OK包，不是就报错
           

            strBegin = m_msgBuf.get() + 4;
            strEnd = strBegin + m_messageLen;
            if (!std::equal(strBegin, strEnd, OKPacket, OKPacket + 7))
            {
                m_log->writeLog("MULTISQLREAD::recvPubkeyAuth", PRINTHEX(strBegin, m_messageLen));
                if (firstConnect)
                {
                    *m_success = false;
                    (*m_unlockFun)();
                }
            }
            else
            {
                if (firstConnect)
                {
                    *m_success = true;
                    (*m_unlockFun)();
                }
                m_seqID = 0;
                firstQuery = true;
                m_queryStatus.store(1, std::memory_order_release);
                m_log->writeLog("connect mysql success");
            }
           
        }
    });
}




void MULTISQLREAD::mysqlQuery()
{
    boost::asio::async_write(*m_sock, boost::asio::buffer(m_sendBuf, clientSendLen),
        [this](const boost::system::error_code& err, const std::size_t size)
    {
        if (err)
        {
           //重连  通知所有请求发生错误
            m_queryStatus.store(0, std::memory_order_release);
        }
        else
        {

            m_readLen = 0;

            recvMysqlResult();
        }
    });

}



void MULTISQLREAD::recvMysqlResult()
{
    m_sock->async_read_some(boost::asio::buffer(m_recvBuf + m_readLen , m_msgBufMaxSize - m_readLen - std::distance(m_msgBuf.get(), m_recvBuf)),
        [this](const boost::system::error_code& err, const std::size_t bytes_transferred)
    {
        if (err)
        {
            //重连  通知所有请求发生错误
            m_queryStatus.store(0, std::memory_order_release);
        }
        else
        {

            //解析mysql查询结果   0 解析协议出错    1  成功(全部解析处理完毕)   2  执行命令出错     3结果集未完毕，需要继续接收 
            switch(parseMysqlResult(bytes_transferred))
            {
            case 0:
                std::cout << "parseMysqlResult   0\n";

                break;
            case 1:
                readyMysqlMessage();

                break;
            case 2:

                // m_commandBufEnd -1 指向最末尾的 ; ,加上一位0x03 ，所以加1，最后一位的;发不发意义不大 
                clientSendLen = std::distance(*m_commandBufBegin, *(m_commandBufEnd - 1)) + 1;

                //倒退一位设置0x03 ，再倒退4位设置长度和序列号
                //然后复用现有字符串继续发送查询请求
                *(*m_commandBufBegin - 1) = 0x03;

                *m_commandBufBegin -= 4;


                **m_commandBufBegin = clientSendLen % 256;
                *(*m_commandBufBegin + 1) = clientSendLen / 256 % 256;
                *(*m_commandBufBegin + 2) = clientSendLen / 65536 % 256;

                firstQuery = false;
                *(*m_commandBufBegin + 3) = m_seqID = 0;

                clientSendLen += 4;

                m_sendBuf = *m_commandBufBegin;
                mysqlQuery();

                break;
            case 3:
                recvMysqlResult();
                break;
           
            default:
                std::cout << "parseMysqlResult   unknown\n";

                break;
            }
        }
    });
}



//解析mysql查询结果   0 解析协议出错    1  成功(全部解析处理完毕)   2  执行命令出错     3结果集未完毕，需要继续接收  
int MULTISQLREAD::parseMysqlResult(const std::size_t bytes_transferred)
{
#define 	NOT_NULL_FLAG   1

    enum  	enum_field_types
    {
        MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY, MYSQL_TYPE_SHORT, MYSQL_TYPE_LONG,
        MYSQL_TYPE_FLOAT, MYSQL_TYPE_DOUBLE, MYSQL_TYPE_NULL, MYSQL_TYPE_TIMESTAMP,
        MYSQL_TYPE_LONGLONG, MYSQL_TYPE_INT24, MYSQL_TYPE_DATE, MYSQL_TYPE_TIME,
        MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR, MYSQL_TYPE_NEWDATE, MYSQL_TYPE_VARCHAR,
        MYSQL_TYPE_BIT, MYSQL_TYPE_TIMESTAMP2, MYSQL_TYPE_DATETIME2, MYSQL_TYPE_TIME2,
        MYSQL_TYPE_TYPED_ARRAY, MYSQL_TYPE_VECTOR = 242, MYSQL_TYPE_INVALID = 243, MYSQL_TYPE_BOOL = 244,
        MYSQL_TYPE_JSON = 245, MYSQL_TYPE_NEWDECIMAL = 246, MYSQL_TYPE_ENUM = 247, MYSQL_TYPE_SET = 248,
        MYSQL_TYPE_TINY_BLOB = 249, MYSQL_TYPE_MEDIUM_BLOB = 250, MYSQL_TYPE_LONG_BLOB = 251, MYSQL_TYPE_BLOB = 252,
        MYSQL_TYPE_VAR_STRING = 253, MYSQL_TYPE_STRING = 254, MYSQL_TYPE_GEOMETRY = 255
    };

    if (!bytes_transferred)
    {
        m_readLen = 0;
        return 3;
    }

    //总接收数据   修复当上次数据未接收完时sourceEnd位置错误问题
    const unsigned char* sourceBegin{ m_recvBuf }, * sourceEnd{ m_recvBuf + bytes_transferred + m_readLen };

    //单条数据的首尾位置
    const unsigned char* strBegin{}, * strEnd{}, * thisStr{};

    ///////////////////////////////////////////////////
    std::shared_ptr<MYSQLResultTypeSW>* waitMessageListBegin{ m_waitMessageListBegin };
    std::shared_ptr<MYSQLResultTypeSW>* waitMessageListEnd{ m_waitMessageListEnd };

    //本次节点结果数总计                      本次节点结果数当前计数
    unsigned int commandTotalSize{ m_commandTotalSize }, commandCurrentSize{ m_commandCurrentSize };

    //是否开始执行检查
    bool checkTime{ m_checkTime };

	//是否开始获取结果
    bool getResult{ m_getResult };

	//是否跳过本次请求的标志
	bool jumpNode{ m_jumpNode };

    //存储命令起始位置
    unsigned char** commandBufBegin{ m_commandBufBegin };


    //指向MYSQLResultTypeSW 第3位vector的迭代器
    std::vector<std::pair<unsigned int, bool>>::const_iterator vecBegin{ m_VecBegin }, vecEnd{ m_VecEnd };
    ///////////////////////////////////////////////////////////////////////////////////////


    //本次数据包消息长度
    unsigned int messageLen{};

    //本次数据包序列号
    unsigned char seqID{};

    //每个字段字符串长度
    int papaLen{};

    //上次执行命令是否成功  true 成功  false  失败
    // 失败时需要根据当前命令指针位置 重新构建请求包进行处理  
    bool queryOK{ true };

    //每条查询结果总string_view个数
    int everyCommandResultSum{m_everyCommandResultSum };

    //执行成功           执行失败
    static const std::string_view success{ "success" }, fail{ "fail" }, emptyView{};

    const unsigned char* lengthOfFixed{};

    //column_length 数组，直接分配足够大空间   
    // 每四位  第一位表示 column_length   第二位表示enumType   第三位表示flags    第四位表示decimals 
    unsigned int *colLenArr{ m_colLenArr.get()};

    unsigned int* colLenBegin{}, *colLenMax{ colLenArr + m_colLenMax };

    unsigned char enumType{};

    unsigned int len{};

	//是否是事务语句
    bool isTransaction{ };

    int i{};

    std::string str;

    /*
    
    MySQL 8.0中不同编码类型的长度值及占用空间如下：

数值类型
整数类型‌：

TINYINT：1字节，范围-128~127（有符号）或0~255（无符号）
1
SMALLINT：2字节，范围-32768~32767（有符号）或0~65535（无符号）
1
MEDIUMINT：3字节，范围-8388608~8388607（有符号）或0~16777215（无符号）
1
INT/INTEGER：4字节，范围-2147483648~2147483647（有符号）或0~4294967295（无符号）
1
3
BIGINT：8字节，范围-2~2-1（有符号）或0~2-1（无符号）
1
3
浮点类型‌：

FLOAT：4字节，单精度浮点数
1
3
DOUBLE：8字节，双精度浮点数
1
3
DECIMAL(M,D)：可变长度，M为总位数，D为小数位数，占用空间为(M+2)字节
3
14
日期/时间类型
DATE：3字节（格式yyyy-mm-dd）
1
TIME：3~6字节（格式HH:MM）
1
YEAR：1字节（格式yyyy）
1
DATETIME：5~8字节（格式yyyy-mm-dd HH:MM）
1
TIMESTAMP：4~7字节（范围1970-2038年）
1
字符串类型
定长字符串‌：

CHAR(M)：固定占用M×字符集字节数（UTF8MB4为4字节/字符）
8
9
BINARY(M)：固定占用M字节
9
变长字符串‌：

VARCHAR(M)：
占用空间 = 实际字符数×字符集字节数 + 长度头（1或2字节）
长度≤255字符时用1字节记录长度，否则用2字节
7
9
VARBINARY(M)：同VARCHAR但存储二进制数据
9
大文本类型‌：

TINYTEXT/TINYBLOB：最大255字节，1字节长度头
9
TEXT/BLOB：最大65,535字节，2字节长度头
9
MEDIUMTEXT/MEDIUMBLOB：最大16MB，3字节长度头
9
LONGTEXT/LONGBLOB：最大4GB，4字节长度头
9
特殊类型
BIT(M)：占用(M+7)/8字节
1
ENUM/SET：根据选项数量占用1~8字节
9
JSON：类似LONGTEXT存储，额外支持JSON验证
4
编码影响
UTF8MB4编码下，每个字符最多占4字节，VARCHAR最大可定义16,383字符（因行限制65,535字节）
7
GBK编码下中文占2字节，UTF8编码下中文占3字节
    
    */
    while (sourceBegin != sourceEnd)
    {
        if (std::distance(sourceBegin, sourceEnd) < 4)
        {
            m_commandBufBegin = commandBufBegin;
            m_waitMessageListBegin = waitMessageListBegin;
            m_commandTotalSize = commandTotalSize;
            m_commandCurrentSize = commandCurrentSize;
           
            m_checkTime = checkTime;
            m_getResult = getResult;
            m_jumpNode = jumpNode;
            m_VecBegin = vecBegin;
            m_VecEnd = vecEnd;
            m_everyCommandResultSum = everyCommandResultSum;
            if (sourceBegin != m_recvBuf)
            {
                std::copy(sourceBegin, sourceEnd, m_recvBuf);
                m_readLen = std::distance(sourceBegin, sourceEnd);
            }
            else
            {
                //修复sourceBegin==m_recvBuf但可能在第二次接收接收不全问题
                m_readLen = std::distance(sourceBegin, sourceEnd);
            }
            return 3;
        }

        messageLen = static_cast<unsigned int>(*(sourceBegin)) + static_cast<unsigned int>(*(sourceBegin + 1)) * 256 +
            static_cast<unsigned int>(*(sourceBegin + 2)) * 65536;

        if (!messageLen)
        {
            //空包
            sourceBegin += 4;
            continue;
        }

        if (std::distance(sourceBegin, sourceEnd) < messageLen + 4)
        {
            m_commandBufBegin = commandBufBegin;
            m_waitMessageListBegin = waitMessageListBegin;
            m_commandTotalSize = commandTotalSize;
            m_commandCurrentSize = commandCurrentSize;
          
            m_checkTime = checkTime;
            m_getResult = getResult;
            m_jumpNode = jumpNode;
            m_VecBegin = vecBegin;
            m_VecEnd = vecEnd;
            m_everyCommandResultSum = everyCommandResultSum;
            if (sourceBegin != m_recvBuf)
            {
                std::copy(sourceBegin, sourceEnd, m_recvBuf);
                m_readLen = std::distance(sourceBegin, sourceEnd);
            }
            else
            {
                //修复sourceBegin==m_recvBuf但可能在第二次接收接收不全问题
                m_readLen = std::distance(sourceBegin, sourceEnd);
            }
            return 3;
        }

        seqID = static_cast<unsigned int>(*(sourceBegin + 3));

        strBegin = sourceBegin + 4;
        strEnd = strBegin + messageLen;
       
        //目前仅对int  varchar短字符串类型进行处理  其他情况待测试    (update  insert  delete select等已经支持)
        
        /*
    0执行命令string_view集
    1执行命令个数
    2每条命令的string_view个数（方便进行拼接）

    3 first 每条命令获取结果个数，second 是否为事务语句  （因为比如一些事务操作可能不一定有结果返回，利用second
    可以在事务中发生错误时快速跳转指针指向）、

    4返回结果string_view
    5每个结果的string_view个数

    6回调函数
    */
        MYSQLResultTypeSW& thisRequest{**waitMessageListBegin };
        if (!checkTime)
        {    
            switch (*strBegin)
            {
            case 0x00:
                //update  insert  delete语句执行成功
                queryOK = true;

                try
                {
                    if (!jumpNode)
                    {
                        std::get<4>(thisRequest).get().emplace_back(success);
                    }

                }
                catch (const std::exception& e)
                {
                    //出错处理，内存不足，不再往里面插入数据
                    jumpNode = true;
                }

                if (++commandCurrentSize == commandTotalSize)
                {
                    try
                    {
                        if (!jumpNode)
                        {
                            std::get<5>(thisRequest).get().emplace_back(commandCurrentSize);
                        }
                    }
                    catch (const std::exception& e)
                    {
                        //出错处理，内存不足，不再往里面插入数据
                        jumpNode = true;
                    }


                    //优化处理，先处理本次waitMessageListBegin内的情况，
                    // 不满足条件才进入循环处理
                    //遍历查找定位有结果的命令
                    while (++vecBegin != vecEnd)
                    {
                        ++commandBufBegin;
                        if (vecBegin->first)
                            break;
                    }
                    if (vecBegin != vecEnd)
                    {
                        commandTotalSize = vecBegin->first;
                        commandCurrentSize = 0;
                        sourceBegin = strEnd;
                        continue;
                    }

                    //根据是否发生过内存不足问题进行回调
                    if (!jumpNode)
                    {
                        std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
                    }
                    else
                    {
                        std::get<6>(thisRequest)(false, ERRORMESSAGE::STD_BADALLOC);
                    }

                    //将内存不足标志位复位
                    jumpNode = false;
                    //进入循环处理
                    do
                    {
                        if (++waitMessageListBegin == waitMessageListEnd)
                            break;
                        vecBegin = std::get<3>(**waitMessageListBegin).get().cbegin();
                        vecEnd = std::get<3>(**waitMessageListBegin).get().cend();
                        do
                        {
                            ++commandBufBegin;
                            if (vecBegin->first)
                                break;
                        } while (++vecBegin != vecEnd);

                        if (vecBegin != vecEnd)
                        {
                            commandTotalSize = vecBegin->first;
                            commandCurrentSize = 0;
                            break;
                        }
                        std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
                    } while (true);
                }

                
                sourceBegin = strEnd;
                continue;
                break;
            case 0xff:
                //update  insert  delete  select语句执行失败
                queryOK = false;


                try
                {
                    if (!jumpNode)
                    {
                        std::get<4>(thisRequest).get().emplace_back(fail);
                    }

                }
                catch (const std::exception& e)
                {
                    //出错处理，内存不足，不再往里面插入数据
                    jumpNode = true;
                }

                if (++commandCurrentSize == commandTotalSize)
                {
                    try
                    {
                        if (!jumpNode)
                        {
                            std::get<5>(thisRequest).get().emplace_back(commandCurrentSize);
                        }
                    }
                    catch (const std::exception& e)
                    {
                        //出错处理，内存不足，不再往里面插入数据
                        jumpNode = true;
                    }

                    //失败时检查是否是事务语句
                    //优化处理，先处理本次waitMessageListBegin内的情况，
                    // 不满足条件才进入循环处理
                    isTransaction = vecBegin->second;

                    while (++vecBegin != vecEnd)
                    {
                        ++commandBufBegin;
                        if (vecBegin->first)
                        {
                            //如果是事务语句，则需要将中间跳过的语句全部返回失败
                            if (isTransaction && vecBegin->second)
                            {
                                if (!jumpNode)
                                {
                                    try
                                    {
                                        for (i = 0; i != vecBegin->first; ++i)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(fail);

                                        }
                                        std::get<5>(thisRequest).get().emplace_back(vecBegin->first);
                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                }
                            }
                            else
                            {
                                isTransaction = false;
                                break;
                            }
                        }
                    }
                    if (vecBegin != vecEnd)
                    {
                        commandTotalSize = vecBegin->first;
                        commandCurrentSize = 0;
                        sourceBegin = strEnd;
                        continue;
                    }
                    //根据是否发生过内存不足问题进行回调
                    if (!jumpNode)
                    {
                        std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
                    }
                    else
                    {
                        std::get<6>(thisRequest)(false, ERRORMESSAGE::STD_BADALLOC);
                    }
                    isTransaction = false;


                    //将内存不足标志位复位
                    jumpNode = false;
                    //进入循环处理
                    do
                    {
                        if (++waitMessageListBegin == waitMessageListEnd)
                            break;
                        vecBegin = std::get<3>(**waitMessageListBegin).get().cbegin();
                        vecEnd = std::get<3>(**waitMessageListBegin).get().cend();
                        do
                        {
                            ++commandBufBegin;
                            if (vecBegin->first)
                                break;
                        } while (++vecBegin != vecEnd);

                        if (vecBegin != vecEnd)
                        {
                            commandTotalSize = vecBegin->first;
                            commandCurrentSize = 0;
                            break;
                        }
                        std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
                    } while (true);
                }

               
                sourceBegin = strEnd;
                continue;
                break;

   



            default:
                //执行select，成功时第一个包返回查询参数个数
                //select语句执行成功

                queryOK = true;

                colLenBegin = colLenArr;
                checkTime = true;
                break;
            }
            
        }
        else
        {
            if (!getResult)
            {
                if (*strBegin != 0xfe)
                {
                    //https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_com_query_response_text_resultset_column_definition.html
                    len = *strBegin;
                    //catalog
                    lengthOfFixed = strBegin + len + 1;

                    len = *lengthOfFixed;
                    //schema
                    lengthOfFixed += len + 1;

                    len = *lengthOfFixed;
                    //table
                    lengthOfFixed += len + 1;

                    len = *lengthOfFixed;
                    //org_table
                    lengthOfFixed += len + 1;

                    len = *lengthOfFixed;
                    //name
                    lengthOfFixed += len + 1;

                    len = *lengthOfFixed;
                    //org_name
                    lengthOfFixed += len + 1;


                    //	length of fixed length fields
                    ++lengthOfFixed;
                    //character_set  暂时不是很需要，跳过
                    lengthOfFixed += 2;

                    //column_length

                    if (colLenBegin == colLenMax)
                    {
                        //出错处理
                    }

                    *colLenBegin++ = static_cast<unsigned int>(*(lengthOfFixed)) + static_cast<unsigned int>(*(lengthOfFixed + 1)) * 256 +
                        static_cast<unsigned int>(*(lengthOfFixed + 2)) * 65536 + static_cast<unsigned int>(*(lengthOfFixed + 3)) * 16777216;

                    //
                    lengthOfFixed += 4;

                    enumType = *lengthOfFixed;

                    //enumType
                    *colLenBegin++ = enumType;

                    ++lengthOfFixed;

                    //flags
                    *colLenBegin++ = static_cast<unsigned int>(*(lengthOfFixed)) + static_cast<unsigned int>(*(lengthOfFixed + 1)) * 256;

                    //flags
                    lengthOfFixed += 2;


                    //decimals
                    *colLenBegin++ = *lengthOfFixed;

                    ++lengthOfFixed;


                    //reserved
                    lengthOfFixed += 2;

                    if (std::distance(lengthOfFixed, strEnd))
                    {
                        len = *lengthOfFixed;
                        //lengthOfFixed + 1,strEnd   default value
                    }

                }
                else
                {
                    getResult = true;
                    everyCommandResultSum = std::get<4>(thisRequest).get().size();
                }
            }
            else
            {
                if (*strBegin == 0xfe)
                {
                    getResult = false;
                    //将everyCommandResultSum 存储起来
                    checkTime = false;
                    ++commandBufBegin;
                    //colLenBegin = colLenArr;

                    if (++commandCurrentSize == commandTotalSize)
                    {
                        try
                        {
                            if (!jumpNode)
                            {
                                //返回本次select结果总string_view个数，自己根据查询参数用+=获取不同的结果
                                std::get<5>(thisRequest).get().emplace_back(std::get<4>(thisRequest).get().size() - everyCommandResultSum);
                            }
                        }
                        catch (const std::exception& e)
                        {
                            //出错处理，内存不足，不再往里面插入数据
                            jumpNode = true;//是九二喝是证明品牌子的
                        }


                        //优化处理，先处理本次waitMessageListBegin内的情况，
                        // 不满足条件才进入循环处理
                        //遍历查找定位有结果的命令
                        while (++vecBegin != vecEnd)
                        {
                            ++commandBufBegin;
                            if (vecBegin->first)
                                break;
                        }
                        if (vecBegin != vecEnd)
                        {
                            commandTotalSize = vecBegin->first;
                            commandCurrentSize = 0;
                            sourceBegin = strEnd;
                            continue;
                        }

                        //根据是否发生过内存不足问题进行回调
                        if (!jumpNode)
                        {
                            std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
                        }
                        else
                        {
                            std::get<6>(thisRequest)(false, ERRORMESSAGE::STD_BADALLOC);
                        }

                        //将内存不足标志位复位
                        jumpNode = false;
                        //进入循环处理
                        do
                        {
                            if (++waitMessageListBegin == waitMessageListEnd)
                                break;
                            vecBegin = std::get<3>(**waitMessageListBegin).get().cbegin();
                            vecEnd = std::get<3>(**waitMessageListBegin).get().cend();
                            do
                            {
                                ++commandBufBegin;
                                if (vecBegin->first)
                                    break;
                            } while (++vecBegin != vecEnd);

                            if (vecBegin != vecEnd)
                            {
                                commandTotalSize = vecBegin->first;
                                commandCurrentSize = 0;
                                break;
                            }
                            std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
                        } while (true);
                    }
                }
                else
                {
                    colLenBegin = colLenArr;

                    while (strBegin != strEnd)
                    {
                        switch (*(colLenBegin + 1))
                        {
                            //在 MySQL 8.0 中，TINYINT 类型支持 NULL 值。
                            // 根据官方文档，TINYINT 属于数值类型，其存储范围为 -128 至 127（有符号）或 0 至 255（无符号），
                            // 且其定义允许 NULL 值
                        case MYSQL_TYPE_TINY:
                            //SMALLINT: 小的整数，有符号范围是-32768到32767，无符号范围是0到65535  支持 NULL 值
                        case MYSQL_TYPE_SHORT:
                            //MEDIUMINT: 中等大小的整数，有符号范围是-8388608到8388607，无符号范围是0到16777215。 支持 NULL 值
                        case MYSQL_TYPE_INT24:
                            //INT 或 INTEGER: 标准整数，有符号范围是-2147483648到2147483647，无符号范围是0到4294967295。  支持 NULL 值
                        case MYSQL_TYPE_LONG:
                            //BIGINT: 大的整数， 支持 NULL 值
                        case MYSQL_TYPE_LONGLONG:
                            //关于负数支持
                            //默认行为‌：FLOAT类型遵循IEEE 754标准，默认包含有符号范围（ - 3.402823466E+38至3.402823466E+38），无需特殊声明即可存储负值。
                            //无符号选项‌：可通过UNSIGNED修饰符限制为非负数，此时取值范围变为0至3.402823466E+38。
                            //从MySQL 8.0.17开始，FLOAT(M,D)语法已被弃用
                        case MYSQL_TYPE_FLOAT:
                            //在MySQL 8.0中，DOUBLE类型的默认小数点位数、支持NULL：
                        case MYSQL_TYPE_DOUBLE:
                           //MySQL 8.0的（DECIMAL类型）取值范围和精度由定义时的参数决定：
                           // 精度与取值范围
                           // ‌总位数（M）‌：最大65位，默认值10位。
                           // ‌小数位数（D）‌：最大30位，默认0位。
                        case MYSQL_TYPE_NEWDECIMAL:
                            //MySQL的  DATE  类型取值范围为‌1000年1月1日至9999年12月31日‌，支持NULL值。
                        case MYSQL_TYPE_DATE:
                            //MySQL 的 ‌TIME 类型‌取值范围为 ‌-838 小时 59 分钟 59 秒至 838 小时 59 分钟 59 秒‌，支持存储 NULL 值
                            //‌标准格式‌：以 ‌HH:MM:SS‌（小时:分钟:秒）表示，支持包含秒的小数部分（默认精度为秒）。 ‌
                            //扩展格式‌：允许使用 ‌天数‌（如 D HH:MM:SS）表示超过 24 小时的时间段
                        case MYSQL_TYPE_TIME:
                            //MySQL 8.0中YEAR类型的取值范围为‌1901至2155年‌，占用1个字节   YEAR字段默认允许存储NULL值
                        case MYSQL_TYPE_YEAR:
                            //DATETIME 类型在 MySQL 中用来存储日期和时间值，格式为 YYYY-MM-DD HH:MM:SS。
                            // 其取值范围是从 '1000-01-01 00:00:00' 到 '9999-12-31 23:59:59
                            // 支持 NULL 值
                        case MYSQL_TYPE_DATETIME:
                            //MySQL 8.0 中 TIMESTAMP 数据类型的取值范围为 '1970-01-01 00:00:01.000000' UTC 到 '2038-01-19 03:14:07.999999' UTC
                            //可通过显式声明 NULL 约束来允许 NULL 值
                        case MYSQL_TYPE_TIMESTAMP:
                           

                            papaLen = *strBegin;

                            if (papaLen != 251)
                            {
                                try
                                {
                                    if (!jumpNode)
                                    {
                                        std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen));
                                    }

                                }
                                catch (const std::exception& e)
                                {
                                    //出错处理，内存不足，不再往里面插入数据
                                    jumpNode = true;
                                }
                                strBegin += papaLen + 1;
                            }
                            else
                            {
                                //NULL值
                                //存储空string_view
                                try
                                {
                                    if (!jumpNode)
                                    {
                                        std::get<4>(thisRequest).get().emplace_back(emptyView);
                                    }

                                }
                                catch (const std::exception& e)
                                {
                                    //出错处理，内存不足，不再往里面插入数据
                                    jumpNode = true;
                                }
                                ++strBegin;
                            }

                            break;

                            //MySQL 8.0中的POINT类型属于空间数据类型，其取值范围由坐标系的数学定义决定，
                           // 通常支持双精度浮点数表示的坐标值（经度范围-180到180，纬度范围-90到90）
                           //该类型默认支持NULL值，除非显式定义为NOT NULL约束
                            //POINT类型需要进一步解析处理，原始返回数据不是直接数据

                            //LINESTRING类型
                            // POLYGON类型等


                            //在 MySQL 8.0 中，ENUM 类型的范围和 NULL 支持情况如下：
                            //范围
                            // 最大值数量‌：ENUM 类型最多支持 65, 535 个不同的元素（即 65, 535 个预定义值）。
                            // 单个值长度限制‌：每个 ENUM 元素的最大支持长度为 255 个字符（字节）。
                            // NULL 支持
                            // 默认行为‌：如果 ENUM 列允许 NULL（这是默认设置），NULL 是该列的有效值，并且默认值为 NULL。
                            // NOT NULL 约束‌：如果 ENUM 列被声明为 NOT NULL，则默认值为允许值列表的第一个元素。
                        case MYSQL_TYPE_GEOMETRY:


                          

                            papaLen = *strBegin;
                            


                            if (papaLen < 251)
                            {
                                try
                                {
                                    if (!jumpNode)
                                    {
                                        std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen));
                                    }

                                }
                                catch (const std::exception& e)
                                {
                                    //出错处理，内存不足，不再往里面插入数据
                                    jumpNode = true;
                                }
                                strBegin += papaLen + 1;
                            }
                            else
                            {
                                //NULL值
                                //存储空string_view
                                if (papaLen == 251)
                                {
                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(emptyView);
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }

                                    ++strBegin;
                                }
                                else
                                {
                                    //252
                                    //当长度为252时  strBegin+1为正确长度   strBegin+2 为 0x00  strBegin+3为数据开始位置 
                                    papaLen = *(strBegin + 1);

                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 3)), papaLen));
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    strBegin += papaLen + 3;


                                }

                                
                            }

                            break;



                            //MySQL 的 CHAR 类型有效长度范围为 ‌0 到 255 个字符‌  NULL 支持‌：默认允许，可通过约束限制。

                            //在MySQL 8.0中，BINARY类型是定长二进制字符串类型，其取值范围由创建表时指定的长度参数M决定，最大允许长度为255字节
                            //关于NULL值的支持，BINARY类型默认允许存储NULL值，除非在列定义时显式指定NOT NULL约束

                            //CHAR  BINARY共用标志
                        case MYSQL_TYPE_STRING:
                            papaLen = *strBegin;


                            if (papaLen < 251)
                            {
                                try
                                {
                                    if (!jumpNode)
                                    {
                                        std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen));
                                    }

                                }
                                catch (const std::exception& e)
                                {
                                    //出错处理，内存不足，不再往里面插入数据
                                    jumpNode = true;
                                }
                                strBegin += papaLen + 1;
                            }
                            else
                            {
                                //NULL值
                                if (papaLen == 251)
                                {
                                    //存储空string_view
                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(emptyView);
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    ++strBegin;

                                }
                                else
                                {
                                    //252
                                    //当长度为252时  strBegin+1为正确长度   strBegin+2 为 0x00  strBegin+3为数据开始位置 
                                    papaLen = *(strBegin + 1);

                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 3)), papaLen));
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    strBegin += papaLen + 3;

                                }
                            }

                            break;




                            //TINYTEXT在MySQL 8.0中最大支持‌255个字符‌（不是字节，如果是UTF-8编码，最多存储85个汉字），支持NULL值

                            //MySQL 8.0中TEXT类型的取值范围为最大长度65,535个字符（2-1），属于可变长度字符串类型，
                            // 适用于存储大段文本内容如文章或博客正文
                            //3。该类型在UTF8MB4字符集下可存储约2万个中文字符，但大JSON串（尤其是含中文较多时）可能超出其容量限制
                            //关于NULL值支持，TEXT字段默认允许为NULL，表示该字段值未知或不存在。用户可通过显式设置NOT NULL约束来禁止空值存储

                             //MEDIUMTEXT的最大长度为‌16,777,215个字符
                            //MEDIUMTEXT字段支持NULL值，即可以存储空值

                            //在MySQL 8.0中，LONGTEXT类型的取值范围为最大长度4294967295个字符
                            //LONGTEXT列默认允许存储NULL值，除非显式定义为NOT NULL
                            

                            //MySQL 8.0中TINYBLOB类型的取值范围为0到255字节，支持存储NULL值，且‌不支持‌显式定义长度（长度固定为255字节）


                            //MySQL 8.0中BLOB类型的取值范围为0到65535字节，支持存储NULL值，且‌不支持‌显式定义长度（长度固定为65535字节）

                            //MySQL 8.0 中，MEDIUMBLOB 类型的取值范围是 ‌0 到 16,777,215 字节（约 16MB）‌，
                            // 适用于存储中等大小的二进制数据，如图片或文档等


                            //MySQL 8.0中LONGBLOB类型的取值范围为最大4GB（即4,294,967,295字节），适用于存储大量二进制数据
                            //该类型支持NULL值，与其他MySQL数据类型一样，若未显式声明NOT NULL约束，则默认允许NULL值



                            //TEXT 和 TINYTEXT 和 MEDIUMTEXT 和  LONGTEXT  共用标志
                        case MYSQL_TYPE_BLOB:
                            //MySQL 8.0中的JSON类型取值范围受系统变量max_allowed_packet限制，
                            // 其存储空间与LONGBLOB或LONGTEXT类似，最大长度不超过该变量设置的值
                            //默认值限制‌：在MySQL 8.0.13之前，JSON列不允许设置非NULL默认值；从8.0.13版本开始，允许默认值为NULL
                            //MySQL 8.0 默认值‌：64MB（即 67108864 字节）‌
                            //最小值‌：通常为 1KB（1024 字节）‌
                            //最大值‌：1GB（1073741824 字节
                        case MYSQL_TYPE_JSON:

                   

                            papaLen = *strBegin;
                            

                            if (papaLen < 251)
                            {
                                try
                                {
                                    if (!jumpNode)
                                    {
                                        std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen));
                                    }

                                }
                                catch (const std::exception& e)
                                {
                                    //出错处理，内存不足，不再往里面插入数据
                                    jumpNode = true;
                                }
                                strBegin += papaLen + 1;
                            }
                            else
                            {
                                switch (papaLen)
                                {
                                case 251:
                                    //存储空string_view
                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(emptyView);
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    ++strBegin;
                                    break;

                                case 252:
                                    //252
                                    //当长度为252时  strBegin+1   strBegin+2为正确长度   strBegin+3为数据开始位置 
                                    papaLen = *(strBegin + 1) + *(strBegin + 2) * 256;

                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 3)), papaLen));
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    strBegin += papaLen + 3;
                                    break;

                                case 253:

                                    //253
                                    //当长度为253时  strBegin+1 strBegin+2 strBegin+3 为正确长度  strBegin+4为数据开始位置 
                                    papaLen = *(strBegin + 1) + *(strBegin + 2) * 256 + *(strBegin + 3) * 65536;

                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 4)), papaLen));
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    strBegin += papaLen + 4;
                                    break;

                                case 254:

                                    //254
                                    //当长度为254时  strBegin+1 strBegin+2 strBegin+3 strBegin+4 为正确长度  strBegin+5为数据开始位置 
                                    papaLen = *(strBegin + 1) + *(strBegin + 2) * 256 + *(strBegin + 3) * 65536 + *(strBegin + 4) * 16777216;

                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 5)), papaLen));
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    strBegin += papaLen + 5;
                                    break;

                                default:

                                    break;
                                };
                            }

                            break;

                            //VARCHAR的最大长度为‌65,535字节‌（对应65,535个字符），
                          // 但实际存储的字符数量可能因字符集而异。例如，使用UTF-8字符集时，
                          // 每个字符可能占用1-4字节，因此实际存储的字符数会减少
                          //VARCHAR字段占用1或2个额外字节记录长度（取决于字符集），加上实际数据长度。
                          // 例如，存储Latin1字符集时，每个字符占用1字节；UTF-8字符集则可能占用1-4字节


                           // MySQL 8.0中VARBINARY类型的取值范围为0到65,535字节，具体长度可通过显式定义指定（如VARBINARY(255)），且支持NULL值存储
                           //对比BINARY‌：与固定长度的BINARY不同，VARBINARY不会用0x00填充剩余空间

                        case MYSQL_TYPE_VAR_STRING:

                           
                            papaLen = *strBegin;

                            if (papaLen < 251)
                            {
                                try
                                {
                                    if (!jumpNode)
                                    {
                                        std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen));
                                    }

                                }
                                catch (const std::exception& e)
                                {
                                    //出错处理，内存不足，不再往里面插入数据
                                    jumpNode = true;
                                }
                                strBegin += papaLen + 1;
                            }
                            else
                            {
                                //NULL值
                                if (papaLen == 251)
                                {
                                    //存储空string_view
                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(emptyView);
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    ++strBegin;

                                }
                                else
                                {
                                    //252
                                    //当长度为252时  strBegin+1为正确长度   strBegin+2 为 0x00  strBegin+3为数据开始位置 
                                    papaLen = *(strBegin + 1) + *(strBegin + 2) * 256;

                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 3)), papaLen));
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    strBegin += papaLen + 3;

                                }                               
                            }

                            break;


                            

                        default:
                            std::cout << "enum: " << *(colLenBegin + 1) << "  len: " << *colLenBegin << '\n';

                            if (*colLenBegin < 256)
                            {
                                papaLen = *strBegin;

                                if (papaLen != 251)
                                {
                                    //存储结果string_view  std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen)
                                    try
                                    {
                                        if (!jumpNode)
                                        {
                                            std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen));
                                        }

                                    }
                                    catch (const std::exception& e)
                                    {
                                        //出错处理，内存不足，不再往里面插入数据
                                        jumpNode = true;
                                    }
                                    strBegin += papaLen + 1;
                                }
                                else
                                {
                                    if (*(colLenBegin + 2) & NOT_NULL_FLAG)
                                    {
                                        //存储结果string_view  std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen)
                                        try
                                        {
                                            if (!jumpNode)
                                            {
                                                std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen));
                                            }

                                        }
                                        catch (const std::exception& e)
                                        {
                                            //出错处理，内存不足，不再往里面插入数据
                                            jumpNode = true;
                                        }
                                        strBegin += papaLen + 1;
                                    }
                                    else
                                    {
                                        if (std::distance(strBegin + 1, strEnd) >= 251)
                                        {
                                            //存储结果string_view  std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen)
                                            try
                                            {
                                                if (!jumpNode)
                                                {
                                                    std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 1)), papaLen));
                                                }

                                            }
                                            catch (const std::exception& e)
                                            {
                                                //出错处理，内存不足，不再往里面插入数据
                                                jumpNode = true;
                                            }
                                            strBegin += papaLen + 1;
                                        }
                                        else
                                        {
                                            //存储空string_view
                                            try
                                            {
                                                if (!jumpNode)
                                                {
                                                    std::get<4>(thisRequest).get().emplace_back(emptyView);
                                                }

                                            }
                                            catch (const std::exception& e)
                                            {
                                                //出错处理，内存不足，不再往里面插入数据
                                                jumpNode = true;
                                            }
                                            ++strBegin;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                //大型字符串待处理   目前仅对int短值和varchar 两种类型进行处理，其他类型后续添加，先跑通全程看看

                                papaLen = static_cast<unsigned int>(*(strBegin)) + static_cast<unsigned int>(*(strBegin + 1)) * 256;

                                try
                                {
                                    if (!jumpNode)
                                    {
                                        std::get<4>(thisRequest).get().emplace_back(std::string_view(reinterpret_cast<char*>(const_cast<unsigned char*>(strBegin + 2)), papaLen));
                                    }

                                }
                                catch (const std::exception& e)
                                {
                                    //出错处理，内存不足，不再往里面插入数据
                                    jumpNode = true;
                                }
                                strBegin += papaLen + 2;

                            }


                            break;
                        }

                        //std::cout << "enum: " << *(colLenBegin + 1) << "  len: " << *colLenBegin << '\n';


                  
                        colLenBegin += 4;
                    }

                    
                }
            }
        }



        sourceBegin = strEnd;
    }


    if (!queryOK)
    {
        //检查出错的命令是否已经是最后一个命令，否则需要构建请求包发送
        if (waitMessageListBegin == waitMessageListEnd)
        {
            firstQuery = true;
            m_seqID = 0;
            return 1;
        }
        else
        {
            m_commandBufBegin = commandBufBegin;
            m_waitMessageListBegin = waitMessageListBegin;
            m_commandTotalSize = commandTotalSize;
            m_commandCurrentSize = commandCurrentSize;
           
            m_checkTime = checkTime;
            m_getResult = getResult;
            m_jumpNode = jumpNode;
            m_VecBegin = vecBegin;
            m_VecEnd = vecEnd;
            m_everyCommandResultSum = everyCommandResultSum;
            return 2;
        }

    }
    else
    {
        //如果处理完已经达到最后一个处理节点
        if (waitMessageListBegin == waitMessageListEnd)
        {
            //全部处理完毕后序列号归0
            firstQuery = true;
            m_seqID = 0;
            return 1;
        }
        else
        {
            m_commandBufBegin = commandBufBegin;
            m_waitMessageListBegin = waitMessageListBegin;
            m_commandTotalSize = commandTotalSize;
            m_commandCurrentSize = commandCurrentSize;
           
            m_checkTime = checkTime;
            m_getResult = getResult;
            m_jumpNode = jumpNode;
            m_VecBegin = vecBegin;
            m_VecEnd = vecEnd;
            m_everyCommandResultSum = everyCommandResultSum;
            m_readLen = 0;
            return 3;
        }
    }

    m_seqID = seqID;
    return 1;

}




void MULTISQLREAD::readyMysqlMessage()
{
    //每个MYSQLResultTypeSW请求内容的字符串总长度
    unsigned int everyTotalLen{};


    //客户端接收消息单个数据包最大大小
    const unsigned int clientBufMaxSize{ m_clientBufMaxSize - 4 };

    std::shared_ptr<MYSQLResultTypeSW>* waitMessageListBegin{};

    std::shared_ptr<MYSQLResultTypeSW>* waitMessageListEnd{};

    unsigned int thisClientSendLen{};


    //////////////////////////////////


    unsigned char* buffer{ m_msgBuf.get() + 4 };
    int index{ 0 };
    std::vector<unsigned int>::const_iterator sqlNumIter{ };

    *buffer++ = 0x03; //com_query

    //存储命令起始位置
    unsigned char** commandBufBegin{};



    ////////////////////////////////////


    while (true)
    {
        thisClientSendLen = 0;
        waitMessageListBegin = m_waitMessageList.get();
        waitMessageListEnd = waitMessageListBegin + m_commandMaxSize;


        //从m_messageList中获取元素到m_waitMessageList中

        do
        {
            if (!m_messageList.try_dequeue(*waitMessageListBegin))
                break;
            ++waitMessageListBegin;
        } while (waitMessageListBegin != waitMessageListEnd);
        if (waitMessageListBegin == m_waitMessageList.get())
        {
            m_queryStatus.store(1, std::memory_order_release);
            break;
        }


        //尝试计算总命令个数  命令字符串所需要总长度,不用检查非空问题
        waitMessageListEnd = waitMessageListBegin;
        waitMessageListBegin = m_waitMessageList.get();
        everyTotalLen = 0;


        //这里采用根据redis模块改进的写法，先计算每个MYSQLResultTypeSW内的字符串总长度
        //再判断clientSendLen + everyTotalLen 是否大于 m_clientBufMaxSize - 4
        //改进之后可以直接在一次循环内实现判断字符串长度是否超出缓冲区大小并同时组装字符串
        //后面测试好之后再去改进redis模块实现，让性能更上一层楼


        buffer = m_msgBuf.get() + 4 ;
       
        *buffer++ = 0x03; //com_query

        commandBufBegin = m_commandBuf.get();

        m_waitMessageListStart = waitMessageListBegin;

        *commandBufBegin++ = buffer;

        do
        {
            MYSQLResultTypeSW& thisRequest{ **waitMessageListBegin };

            everyTotalLen = std::accumulate(std::get<0>(thisRequest).get().cbegin(), std::get<0>(thisRequest).get().cend(), 0, [](auto& sum, auto const sw)
            {
                return sum += sw.size();
            });
            everyTotalLen += std::get<1>(thisRequest);

            //检查waitMessageListStart是否等于waitMessageListBegin
            //如果等于，返回错误，跳转到下一个节点
            if (thisClientSendLen + everyTotalLen > clientBufMaxSize)
            {
                if (waitMessageListBegin == m_waitMessageListStart)
                {
                    std::get<6>(**waitMessageListBegin)(false, ERRORMESSAGE::MYSQL_QUERY_LEN_TOO_LONG);
                    if (++waitMessageListBegin != waitMessageListEnd)
                    {
                        m_waitMessageListStart = waitMessageListBegin;
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            thisClientSendLen += everyTotalLen;


            ///////////////////////////////////////////////////////////////////

            sqlNumIter = std::get<2>(thisRequest).get().cbegin();
            index = 0;


            for (auto sw : std::get<0>(thisRequest).get())
            {
                std::copy(sw.cbegin(), sw.cend(), buffer);
                buffer += sw.size();
                if (++index == *sqlNumIter)
                {
                    index = 0;
                    ++sqlNumIter;
                    *buffer++ = ';';
                    *commandBufBegin++ = buffer;
                }
            }

        } while (++waitMessageListBegin != waitMessageListEnd);


        //所有节点均长度超长的情况，跳转到循环开始处重新处理
        if (m_waitMessageListStart == waitMessageListEnd)
        {
            continue;
        }


        m_commandBufEnd = commandBufBegin;
        m_commandBufBegin = m_commandBuf.get();

        MYSQLResultTypeSW& thisRequest{ **m_waitMessageListStart };

        m_VecBegin = std::get<3>(thisRequest).get().cbegin();
        m_VecEnd = std::get<3>(thisRequest).get().cend();

        do
        {
            if (m_VecBegin->first)
                break;
            ++m_commandBufBegin;
        } while (++m_VecBegin != m_VecEnd);

       
        //0x03
        thisClientSendLen += 1;
        clientBegin = m_msgBuf.get();
        *clientBegin = thisClientSendLen % 256;
        *(clientBegin + 1) = thisClientSendLen / 256 % 256;
        *(clientBegin + 2) = thisClientSendLen / 65536 % 256;

        if (firstQuery)
        {
            firstQuery = false;
            *(clientBegin + 3) = m_seqID = 0;
        }
        else
            *(clientBegin + 3) = ++m_seqID;

        thisClientSendLen += 4;

        clientSendLen = thisClientSendLen;

        m_waitMessageListEnd = waitMessageListBegin;

        m_waitMessageListBegin = m_waitMessageListStart;

        m_commandTotalSize = m_VecBegin->first;

        m_commandCurrentSize = 0;

        m_sendBuf = m_msgBuf.get();

        m_recvBuf = m_msgBuf.get() + clientSendLen;



        //开始接收mysql查询结果
           //是否跳过本次请求的标志
        m_jumpNode = false;

        //检查次数
        m_checkTime = false;

        //是否开始获取结果
        m_getResult = false;

        m_everyCommandResultSum = 0;

        //发送sql请求
        mysqlQuery();

        break;

    }

}





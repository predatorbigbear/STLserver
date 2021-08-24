#include "cloudHttp.h"

cloudHttp::cloudHttp(
	shared_ptr<io_context>ioc,
	shared_ptr<boost::asio::ip::tcp::socket> socket,
	std::shared_ptr<std::string const> const doc_root,
	shared_ptr<string>fileBuf,
	shared_ptr<mutex>m1
)
{
	m_ioc = ioc;
	m_sock = socket;
	m_timer.reset(new steady_timer(*m_ioc));
	m_strand.reset(new boost::asio::io_context::strand(*m_ioc));
	m_sendStrand.reset(new boost::asio::io_context::strand(*m_ioc));
	//m_requestInvaild.store(false);
	m_startRead.store(false);
	m_startWrite.store(false);
	m_startCheck.store(false);

	m_readBuf.reset(new char[m_maxReadLen]);

	m_doc_root = doc_root;
	m_fileBuf = fileBuf;
	m_m1 = m1;

	run();
}




void cloudHttp::run()
{
	checkTimeOut();
	do_read();
}




void cloudHttp::checkTimeOut()
{
	m_timer->expires_from_now(seconds(1800));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)
		{
			if (err == boost::asio::error::operation_aborted)
			{
				if(!m_startCheck.load())
					checkTimeOut();
			}
			else
			{
				if (!m_startCheck.load())
					checkOnDelete();
			}
			return;
		}
		else
		{
			if (!m_startCheck.load())
				checkOnDelete();
		}
	});
}




void cloudHttp::do_read()
{

	if (!m_startCheck.load())
	{
		m_timer->cancel();
		m_sock->async_read_some(boost::asio::buffer(m_readBuf.get(), m_maxReadLen), [this](const boost::system::error_code &err, std::size_t size)
		{
			string readStr{ m_readBuf.get(),m_readBuf.get() + size };
			cout << '\n' << readStr;
			do_read();
			m_strand->post([this, readStr ,err]
			{
				m_readStr.append(readStr);
				on_read(err);
			});
		});
	}
}




void cloudHttp::on_read(const boost::system::error_code & ec)
{
	if (ec)
	{
		if (ec != boost::asio::error::eof)
		{

		}
		else if (ec == boost::asio::error::connection_reset)
		{

		}
		else if (ec == boost::asio::error::operation_aborted)
		{
				
		}
		else
		{
			cout << ec.value() << ":" << ec.message() << '\n';
		}
	}
	else
	{
		if (!m_readStr.empty())
		{
			parseHttp();

			if (m_requestInvaild.load())
			{
				getReq();
			}
			else
			{
				
			}
		}
	}
}



void cloudHttp::parseHttp()
{
	if (!m_readStr.empty())    //如果本次socket接收内容有长度
	{
		static string httpStr{ "HTTP/" };
		static string finalStr{ "\r\n" };
		static string ultimateStr{ "\r\n\r\n" };
		static string ContentLength{ "content-length:" };
		m_requestInvaild.store(true);            //标记接收内容请求有效的atomic


		m_readLen = m_readStr.size();
		//m_readLen += bytes_transferred;         //将上次没有处理的socket内容长度加上本次长度  一般http一个一个请求发  那么就是0  + X
		bool success{ false };                      //标记成功解析的bool
		string::iterator iterBegin, iterEnd, iterThisUltimate, iterthisFinal;   //截取内容的指针  表示截取内容首尾   /r/n位置   本次内容的/r位置
		string::iterator iterLast;                                                 //本次读取内容的最终位置
		size_t totalCheckLen{};                                         //已经处理过的数据长度

		int Length{};                                                   //表示target的长度

		string header, temp, thisTarget;

		do
		{
			m_req.clear();

			iterBegin = m_readStr.begin() + totalCheckLen;
			iterLast = m_readStr.end();

			Length = 0;

			m_success = false;
			do
			{
				if ((iterBegin = find_if(iterBegin, iterLast, std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))) == iterLast)    //查找首个不为空格字符
					break;

				if ((iterThisUltimate = search(iterBegin, iterLast, ultimateStr.begin(), ultimateStr.end())) == iterLast)     //查找本次http的\r\n\r\n位置
					break;

				if ((iterthisFinal = search(iterBegin, iterThisUltimate, finalStr.begin(), finalStr.end())) == iterThisUltimate)     //查找本次\r\n位置
				{
					m_requestInvaild.store(false);
					break;
				}

				if ((iterEnd = find(iterBegin, iterthisFinal, ' ')) == iterthisFinal)       //查找从首个不为空的字符开始向前走为空格的字符
				{
					m_requestInvaild.store(false);
					break;
				}

				m_req.setMethod(iterBegin, iterEnd);                                    //截取方法字符串
				if (m_req.method() != "GET" && m_req.method() != "POST")                //判断方法是否为GET  或  POST
				{
					m_requestInvaild.store(false);
					break;
				}

				if ((iterBegin = find_if(iterEnd, iterthisFinal, std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))) == iterthisFinal)   //从刚才的空格出发找不为空格的字符
				{
					m_requestInvaild.store(false);
					break;
				}

				if ((iterEnd = find(iterBegin, iterthisFinal, ' ')) == iterthisFinal)        //然后找为空格的字符位置
				{
					m_requestInvaild.store(false);
					break;
				}


				auto targetBegin = find(iterBegin, iterEnd, '?');            // 如果中间有  ？  表示target后面跟着body，反之没有  ，根据情况设置
				if (targetBegin != iterEnd)
				{
					m_req.setBody(targetBegin + 1, iterEnd);
					m_req.setTarget(iterBegin, targetBegin);
				}
				else
				{
					thisTarget.assign(iterBegin, targetBegin);
					thisTarget = UrlDecodeWithTransChinese(thisTarget);
					m_req.setTarget(thisTarget.begin(), thisTarget.end());
				}


				if ((iterBegin = search(iterEnd, iterthisFinal, httpStr.begin(), httpStr.end())) == iterthisFinal)     //从刚才target后的空格位置开始查找http的位置
				{
					m_requestInvaild.store(false);
					break;
				}

				iterBegin += httpStr.size();

				m_req.setVersion(iterBegin, iterthisFinal);

				if (m_req.version() != "1.0" && m_req.version() != "1.1")                    //截取http后面的版本字符串，判断是1.0或者1.1
				{
					m_requestInvaild.store(false);
					break;
				}

				iterBegin = iterthisFinal + finalStr.size();

				do              //  这里是反复截取http里面的 header 和内容
				{

					iterthisFinal = search(iterthisFinal + finalStr.size(), iterThisUltimate, finalStr.begin(), finalStr.end());     //查找本次\r\n位置

					if ((iterBegin = find_if(iterBegin, iterthisFinal, std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))) == iterthisFinal)    //查找首个不为空的位置，header
					{
						m_requestInvaild.store(false);
						break;
					}

					if ((iterEnd = find(iterBegin, iterthisFinal, ':')) == iterthisFinal)            //查找header跟着的：
					{
						m_requestInvaild.store(false);
						break;
					}

					if (!distance(iterEnd, iterthisFinal))                    // 如果header没有任何一个字符  header与：之间，则认为是错误（反正我没见过为空的）
					{
						m_requestInvaild.store(false);
						break;
					}

					++iterEnd;  //尾位置向后走一位截取header

					header.assign(iterBegin, iterEnd);
					std::transform(header.begin(), header.end(), header.begin(), [](const int elem)    //将header字符转为小写，后面要处理，http里面的header是大小写不敏感的
					{
						if (elem >= 'A' && elem <= 'Z')return tolower(elem);
						return elem;
					});

					//查找：后一位开始不为空的字符
					if ((iterBegin = find_if(iterEnd, iterthisFinal, std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))) == iterthisFinal)
					{
						m_requestInvaild.store(false);
						break;
					}

					temp.assign(iterBegin, iterthisFinal);    //一直到\r\n位置就是 header： 后面的内容啦  保存起来

					m_req.insertHeader(header, temp);

					iterBegin = iterthisFinal + finalStr.size();     //位置加上从\r\n加上4位  跳到下一次开始

				} while (iterthisFinal != iterThisUltimate);

				if (!m_requestInvaild.load())
					break;

				//跳过所有为\r\n，找寻不为空格的位置  定位到可能有body的位置
				iterBegin = find_if(iterthisFinal, iterLast, std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\r'),
					std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\n'), 
						std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))));


				if (m_req.method() == "GET")      //如果是get，那么跳过\r\n找寻不为空格的位置，进入下一条http请求起始位置
				{
					m_success = true;

					iterBegin = find_if(iterBegin, iterLast, std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\r'),
						std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\n'),
							bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))));
				}
				else if (m_req.method() == "POST")   //如果是post，继续进行处理
				{
					auto iter = m_req.headerMap().find(ContentLength);    //查找是否有content-length:的header ，并检查其对应内容是否都是数字  
					if (iter != m_req.headerMap().end() && all_of(iter->second.begin(), iter->second.end(), std::bind(std::logical_and<bool>(),
						std::bind(greater_equal<char>(), std::placeholders::_1, '0'), std::bind(less_equal<char>(), std::placeholders::_1, '9'))))
					{
						Length = stoi(iter->second);      //获取长度

						if (distance(iterBegin, iterLast) < Length)   //检查到本次读取内容之间长度是否足够
						{
							m_requestInvaild.store(false);
							break;
						}

						iterEnd = iterBegin + Length;

						if (Length && m_req.body().empty())            //如果有长度并且之前body为空，则存储body
							m_req.setBody(iterBegin, iterEnd);

						m_success = true;

						if (!distance(iterEnd, iterLast))
						{
							iterBegin = iterEnd;
							break;
						}

						//跳到下一个http请求起始位置
						iterBegin = find_if(iterEnd, iterLast, std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\r'), 
							std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\n'),
								std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))));
					}
				}
			} while (0);
			if (m_success)
			{
				totalCheckLen = distance(m_readStr.begin(), iterBegin);         //本次检查长度加上
				m_MYREQVec.emplace_back(m_req);                               //将解析到的m_req存进vector里面稍后处理
			}
			else
			{
				
			}
		} while (m_success && m_readLen - totalCheckLen);
		if (totalCheckLen)
		{
			std::copy(m_readStr.begin() + totalCheckLen, m_readStr.begin() + m_readLen, m_readStr.begin());          //将处理过的内容向前挪
			m_readStr.resize(m_readLen - totalCheckLen);                                                             //总读取的字符数减去目前处理过的长度
		}
	}
}




void cloudHttp::send_bad_response(const std::string & error)
{
	m_sstr.str("");
	m_sstr.clear();
	m_sstr << "HTTP/1.1 400 Bad Request\r\n";
	//m_sstr << "HTTP/" << m_req.version() << " 400 Bad Request\r\n";
	m_sstr << "Content-Type: text/plain\r\n";
	m_sstr << "Content-Length:" << error.size() << "\r\n\r\n";
	m_sstr << error;

	string temp{ m_sstr.str() };

	m_sendStrand->post([this, temp]
	{
		m_sendMessage.push_back(temp);
		if (!m_startWrite.load())
		{
			sendLoop();
		}
	});
}



void cloudHttp::getReq()
{
	if (!m_MYREQVec.empty())
	{
		for (auto const &req : m_MYREQVec)
		{
			handle_request(req);
		}
		m_MYREQVec.clear();
	}
}



void cloudHttp::handle_request(const MYREQ &req)
{
	if (req.method() == "GET")
	{
		send_file(req.target());
	}
	else if (req.method() == "POST")
	{
		process_interface();
	}
	else
	{
		send_bad_response("Invalid request-method '" + std::string(req.method()) + "'\r\n");
	}
}



void cloudHttp::checkOnDelete()
{
	if (!m_startCheck.load())
	{
		m_startCheck.store(true);
		m_timer->cancel();
		waitForSocket();
	}
}



void cloudHttp::waitForSocket()
{
	boost::system::error_code err;
	m_sock->cancel(err);

	m_timer->expires_from_now(std::chrono::seconds(1));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (!m_startRead.load() && !m_startWrite.load())
		{
			delete this;
		}
		else
		{
			waitForSocket();
		}
	});
}




void cloudHttp::send_file(const std::string & target)
{
		string targetTemp, full_path;
		targetTemp.assign(UrlDecodeWithTransChinese(target));
		if (openUrl)                                                                                              //是否允许url后面带参数，如果为true则允许，否则不允许
			targetTemp.assign(targetTemp.begin(), find(targetTemp.begin(), targetTemp.end(), '?'));

		if (targetTemp.empty() || targetTemp[0] != '/' || targetTemp.find("..") != std::string::npos)
		{
			send_bad_response("File not found\r\n");
			return;
		}

		full_path.assign(*m_doc_root);
		full_path.append(targetTemp);

		if (full_path.back() == '/')full_path.append("login.html");
		m_sstr.str("");
		m_sstr.clear();

		//m_sendStrand->post([this, target]
		//{
			//string full_path;
		{
			std::lock_guard<mutex>l1{ *m_m1 };
			static ifstream file;
			file.open(full_path, std::ios::binary);
			if (!file)
			{
				send_bad_response("File not found\r\n");
				return;
			}
			else
			{
				file.unsetf(std::ios::skipws);
				static uintmax_t size;
				size = fs::file_size(full_path);
				static int readLen;
				readLen = 0;

				m_sstr << "HTTP/1.1 200 OK\r\n";
				//m_sstr << "HTTP/" << m_req.version() << " 200 OK\r\n";
				m_sstr << "Access-Control-Allow-Origin:*\r\n";
				m_sstr << "Content-Type: " << mime_type(full_path) << "\r\n";
				m_sstr << "Content-Length:" << size << "\r\n\r\n";

				while ((size - readLen) >= 4096)
				{
					file.read(reinterpret_cast<char*>(m_fileBuf->data()), 4096);
					m_sstr << *m_fileBuf;
					readLen += 4096;
				}
				readLen = size - readLen;
				if (readLen)
				{
					response.resize(readLen);
					file.read(reinterpret_cast<char*>(response.data()), readLen);
					m_sstr << response;
				}
				file.close();
			}
			string temp{ m_sstr.str() };

			m_sendStrand->post([this, temp]
			{
				m_sendMessage.push_back(temp);
				if (!m_startWrite.load())
				{
					sendLoop();
				}
			});
		}
}



void cloudHttp::process_interface()
{
	test();


	//sendResponse("hello world");
	//send_bad_response("unknown method\r\n");
}




void cloudHttp::setErrorMessage(const char * ch)
{
	if (ch)
		errorMessage = ch;
}





void cloudHttp::sendResponse(const string & sendMessage, int sendMode)
{
	m_sstr.str("");
	m_sstr.clear();

	m_sstr << "HTTP/1.1 200 OK\r\n";
	//m_sstr << "HTTP/" << m_req.version() << " 200 OK\r\n";
	m_sstr << "Access-Control-Allow-Origin:*\r\n";
	m_sstr << "Content-Length:" << sendMessage.size() << "\r\n\r\n";
	m_sstr << sendMessage;

	string temp{ m_sstr.str() };

	m_sendStrand->post([this, temp]
	{
		m_sendMessage.push_back(temp);
		if (!m_startWrite.load())
		{
			sendLoop();
		}
	});
}




void cloudHttp::sendLoop()
{
	m_startWrite.store(true);
	if (!m_startCheck.load())
	{
		innerSendLoop();
	}
}



void cloudHttp::innerSendLoop()
{
	m_sendStrand->post([this]
	{
		if (!m_sendMessage.empty())
		{
			response = m_sendMessage.front();
			boost::asio::async_write(*m_sock, boost::asio::buffer(response), [this](const boost::system::error_code &err, std::size_t size)
			{
				if (!err)
				{
					m_sendStrand->post([this]
					{
						m_sendMessage.pop_front();
						innerSendLoop();
					});
				}
				else
				{
					if (err == boost::asio::error::operation_aborted)
					{
						checkOnDelete();
					}
					else
					{
						checkOnDelete();
					}
				}
			});
		}
		else
			m_startWrite.store(false);
	});
}

void cloudHttp::test()
{
	/*
	ptree ptJspon, pt1, pt2, pt3;
	
	pt2.push_back(make_pair("", pt1));

	pt3.put_child("list", pt2);
	pt2.clear();

	pt3.put("alltotal", 0);
	pt3.put("total", 0);
	pt2.put_child("resultBody", pt3);
	pt2.put("result", "success");
	

	stringstream sstr;
	write_json(sstr, pt2);
	response.assign(sstr.str());

	sendResponse(response);
	*/
	STLtree stJson, st1, st2, st3;

	st2.push_back(st1);

	st3.put_child("list", st2);
	st2.clear();
	st3.put("alltotal", 0);
	st3.put("total", 0);
	st2.push_back("resultBody", st3);
	st2.put("result", "success");

	sendResponse(st2.make_json());
	
}



/*
{
	"resultBody": {
		"list": [
			""
		],
			"alltotal" : "0",
				"total" : "0"
	},
		"result": "success"
}
*/





/*
{
	"resultBody": {
		"list": "",
			"alltotal" : "0",
			"total" : "0"
	},
		"result" : "success"
}
*/




/*
{
	"resultBody": {
		"list": "",
		"alltotal": "0",
		"total": "0"
	},
	"result": "success"
}




*/
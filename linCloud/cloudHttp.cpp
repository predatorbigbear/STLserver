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
	if (!m_readStr.empty())    //�������socket���������г���
	{
		static string httpStr{ "HTTP/" };
		static string finalStr{ "\r\n" };
		static string ultimateStr{ "\r\n\r\n" };
		static string ContentLength{ "content-length:" };
		m_requestInvaild.store(true);            //��ǽ�������������Ч��atomic


		m_readLen = m_readStr.size();
		//m_readLen += bytes_transferred;         //���ϴ�û�д����socket���ݳ��ȼ��ϱ��γ���  һ��httpһ��һ������  ��ô����0  + X
		bool success{ false };                      //��ǳɹ�������bool
		string::iterator iterBegin, iterEnd, iterThisUltimate, iterthisFinal;   //��ȡ���ݵ�ָ��  ��ʾ��ȡ������β   /r/nλ��   �������ݵ�/rλ��
		string::iterator iterLast;                                                 //���ζ�ȡ���ݵ�����λ��
		size_t totalCheckLen{};                                         //�Ѿ�����������ݳ���

		int Length{};                                                   //��ʾtarget�ĳ���

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
				if ((iterBegin = find_if(iterBegin, iterLast, std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))) == iterLast)    //�����׸���Ϊ�ո��ַ�
					break;

				if ((iterThisUltimate = search(iterBegin, iterLast, ultimateStr.begin(), ultimateStr.end())) == iterLast)     //���ұ���http��\r\n\r\nλ��
					break;

				if ((iterthisFinal = search(iterBegin, iterThisUltimate, finalStr.begin(), finalStr.end())) == iterThisUltimate)     //���ұ���\r\nλ��
				{
					m_requestInvaild.store(false);
					break;
				}

				if ((iterEnd = find(iterBegin, iterthisFinal, ' ')) == iterthisFinal)       //���Ҵ��׸���Ϊ�յ��ַ���ʼ��ǰ��Ϊ�ո���ַ�
				{
					m_requestInvaild.store(false);
					break;
				}

				m_req.setMethod(iterBegin, iterEnd);                                    //��ȡ�����ַ���
				if (m_req.method() != "GET" && m_req.method() != "POST")                //�жϷ����Ƿ�ΪGET  ��  POST
				{
					m_requestInvaild.store(false);
					break;
				}

				if ((iterBegin = find_if(iterEnd, iterthisFinal, std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))) == iterthisFinal)   //�ӸղŵĿո�����Ҳ�Ϊ�ո���ַ�
				{
					m_requestInvaild.store(false);
					break;
				}

				if ((iterEnd = find(iterBegin, iterthisFinal, ' ')) == iterthisFinal)        //Ȼ����Ϊ�ո���ַ�λ��
				{
					m_requestInvaild.store(false);
					break;
				}


				auto targetBegin = find(iterBegin, iterEnd, '?');            // ����м���  ��  ��ʾtarget�������body����֮û��  �������������
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


				if ((iterBegin = search(iterEnd, iterthisFinal, httpStr.begin(), httpStr.end())) == iterthisFinal)     //�Ӹղ�target��Ŀո�λ�ÿ�ʼ����http��λ��
				{
					m_requestInvaild.store(false);
					break;
				}

				iterBegin += httpStr.size();

				m_req.setVersion(iterBegin, iterthisFinal);

				if (m_req.version() != "1.0" && m_req.version() != "1.1")                    //��ȡhttp����İ汾�ַ������ж���1.0����1.1
				{
					m_requestInvaild.store(false);
					break;
				}

				iterBegin = iterthisFinal + finalStr.size();

				do              //  �����Ƿ�����ȡhttp����� header ������
				{

					iterthisFinal = search(iterthisFinal + finalStr.size(), iterThisUltimate, finalStr.begin(), finalStr.end());     //���ұ���\r\nλ��

					if ((iterBegin = find_if(iterBegin, iterthisFinal, std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))) == iterthisFinal)    //�����׸���Ϊ�յ�λ�ã�header
					{
						m_requestInvaild.store(false);
						break;
					}

					if ((iterEnd = find(iterBegin, iterthisFinal, ':')) == iterthisFinal)            //����header���ŵģ�
					{
						m_requestInvaild.store(false);
						break;
					}

					if (!distance(iterEnd, iterthisFinal))                    // ���headerû���κ�һ���ַ�  header�룺֮�䣬����Ϊ�Ǵ��󣨷�����û����Ϊ�յģ�
					{
						m_requestInvaild.store(false);
						break;
					}

					++iterEnd;  //βλ�������һλ��ȡheader

					header.assign(iterBegin, iterEnd);
					std::transform(header.begin(), header.end(), header.begin(), [](const int elem)    //��header�ַ�תΪСд������Ҫ����http�����header�Ǵ�Сд�����е�
					{
						if (elem >= 'A' && elem <= 'Z')return tolower(elem);
						return elem;
					});

					//���ң���һλ��ʼ��Ϊ�յ��ַ�
					if ((iterBegin = find_if(iterEnd, iterthisFinal, std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))) == iterthisFinal)
					{
						m_requestInvaild.store(false);
						break;
					}

					temp.assign(iterBegin, iterthisFinal);    //һֱ��\r\nλ�þ��� header�� �����������  ��������

					m_req.insertHeader(header, temp);

					iterBegin = iterthisFinal + finalStr.size();     //λ�ü��ϴ�\r\n����4λ  ������һ�ο�ʼ

				} while (iterthisFinal != iterThisUltimate);

				if (!m_requestInvaild.load())
					break;

				//��������Ϊ\r\n����Ѱ��Ϊ�ո��λ��  ��λ��������body��λ��
				iterBegin = find_if(iterthisFinal, iterLast, std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\r'),
					std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\n'), 
						std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))));


				if (m_req.method() == "GET")      //�����get����ô����\r\n��Ѱ��Ϊ�ո��λ�ã�������һ��http������ʼλ��
				{
					m_success = true;

					iterBegin = find_if(iterBegin, iterLast, std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\r'),
						std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\n'),
							bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))));
				}
				else if (m_req.method() == "POST")   //�����post���������д���
				{
					auto iter = m_req.headerMap().find(ContentLength);    //�����Ƿ���content-length:��header ����������Ӧ�����Ƿ�������  
					if (iter != m_req.headerMap().end() && all_of(iter->second.begin(), iter->second.end(), std::bind(std::logical_and<bool>(),
						std::bind(greater_equal<char>(), std::placeholders::_1, '0'), std::bind(less_equal<char>(), std::placeholders::_1, '9'))))
					{
						Length = stoi(iter->second);      //��ȡ����

						if (distance(iterBegin, iterLast) < Length)   //��鵽���ζ�ȡ����֮�䳤���Ƿ��㹻
						{
							m_requestInvaild.store(false);
							break;
						}

						iterEnd = iterBegin + Length;

						if (Length && m_req.body().empty())            //����г��Ȳ���֮ǰbodyΪ�գ���洢body
							m_req.setBody(iterBegin, iterEnd);

						m_success = true;

						if (!distance(iterEnd, iterLast))
						{
							iterBegin = iterEnd;
							break;
						}

						//������һ��http������ʼλ��
						iterBegin = find_if(iterEnd, iterLast, std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\r'), 
							std::bind(std::logical_and<bool>(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, '\n'),
								std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))));
					}
				}
			} while (0);
			if (m_success)
			{
				totalCheckLen = distance(m_readStr.begin(), iterBegin);         //���μ�鳤�ȼ���
				m_MYREQVec.emplace_back(m_req);                               //����������m_req���vector�����Ժ���
			}
			else
			{
				
			}
		} while (m_success && m_readLen - totalCheckLen);
		if (totalCheckLen)
		{
			std::copy(m_readStr.begin() + totalCheckLen, m_readStr.begin() + m_readLen, m_readStr.begin());          //���������������ǰŲ
			m_readStr.resize(m_readLen - totalCheckLen);                                                             //�ܶ�ȡ���ַ�����ȥĿǰ������ĳ���
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
		if (openUrl)                                                                                              //�Ƿ�����url��������������Ϊtrue��������������
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
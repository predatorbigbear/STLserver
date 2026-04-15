
#include <iostream>
#include <string>
#include <algorithm>
#include <functional>
#include <cctype>

void parseSPICE(std::string source)
{
	//解释字段名称
	std::string::const_iterator funBegin, funEnd;
	std::string::const_iterator versionBegin, versionEnd;
	std::string::const_iterator powerBegin, powerEnd;

	std::string::const_iterator renzhengBegin, renzhengEnd;
	int renzheng;
	std::string::const_iterator nameBegin, nameEnd;
	int nameLen;
	std::string::const_iterator passBegin, passEnd;
	int passLen;

	std::string::const_iterator idBegin, idEnd;
	int id;
	std::string::const_iterator configBegin, configEnd;

	std::string::const_iterator thingBegin, thingEnd;
	int thing;

	std::string::const_iterator maBegin, maEnd;
	std::string::const_iterator statusBegin, statusEnd;
	int status;

	std::string::const_iterator errorTypeBegin, errorTypeEnd;
	int errorType;
	std::string::const_iterator errorLenBegin, errorLenEnd;
	int errorLen;

	std::string::const_iterator timeStampBegin, timeStampEnd;

	std::string::const_iterator dataLenBegin, dataLenEnd;
	int dataLen;

	std::string::const_iterator controlTypeBegin, controlTypeEnd;
	int controlType;
	std::string::const_iterator soundBegin, soundEnd;
	int sound;
	std::string::const_iterator fileLenBegin, fileLenEnd;
	int fileLen;


	funBegin = std::find_if(source.cbegin(), source.cend(), std::bind(std::isupper, std::placeholders::_1));

	if (funBegin == source.cend())
		std::cout << "解析错误，没有字段名称\n";

	funEnd= std::find_if_not(source.cbegin(), source.cend(), std::bind(std::isupper, std::placeholders::_1));

	if (funEnd == source.cend())
		std::cout << "解析错误，没有字段名称\n";

	//FILE
	//PING

	//SPICE
	//INPUT
	//ERROR
	//AUDIO

	//CONNECT
	//DISPLAY
	
	//CLIPBOARD
	switch (std::distance(funBegin, funEnd))
	{
	case 4:
		switch (*funBegin)
		{
		case 'P':
			std::cout << "PING\n";

			if (std::distance(funEnd, source.cend()) < 4)
				std::cout << "解析错误\n";

			timeStampBegin = funEnd;
			timeStampEnd = funEnd + 4;

			//时间戳 timeStampBegin timeStampEnd

			break;


		case 'F':
			std::cout << "FILE\n";

			if (std::distance(funEnd, source.cend()) < 8)
				std::cout << "解析错误\n";

			controlTypeBegin = funEnd;
			controlTypeEnd = controlTypeBegin + 4;

			controlType = *controlTypeBegin + *(controlTypeBegin + 1) * 256 + *(controlTypeBegin + 2)* (256 * 256) + *(controlTypeBegin + 3)* (256 * 256 * 256);

			std::cout << "操作类型:" << controlType << '\n';

			//操作类型03表示上传请求，文件名长度01 00 00 00，数据块大小08 00 00 00

			if (controlType == 3)
			{

				fileLenBegin = controlTypeEnd + 4;
				fileLenEnd = fileLenBegin + 4;

				fileLen = *fileLenBegin + *(fileLenBegin + 1) * 256 + *(fileLenBegin + 2)* (256 * 256) + *(fileLenBegin + 3)* (256 * 256 * 256);

				std::cout << "文件名长度：" << fileLen << '\n';

				dataLenBegin = fileLenEnd;
				dataLenEnd = dataLenBegin + 4;

				dataLen = *dataLenBegin + *(dataLenBegin + 1) * 256 + *(dataLenBegin + 2)* (256 * 256) + *(dataLenBegin + 3)* (256 * 256 * 256);

				std::cout << "数据块大小：" << dataLen << '\n';

			}

			break;



		}



		break;
	case 5:
		switch (*funBegin)
		{
		case 'S':
			std::cout << "SPICE\n";

			if (std::distance(funEnd, source.cend()) < 8)
				std::cout << "解析错误\n";

			versionBegin = funEnd;
			versionEnd = versionBegin + 4;

			//协议版本号:versionBegin  versionEnd
			
			powerBegin = versionEnd;
			powerEnd = powerBegin + 4;

			//客户端能力标志位 powerBegin  powerEnd

			break;


		case 'I':
			std::cout << "INPUT\n";

			if (std::distance(funEnd, source.cend()) < 4)
				std::cout << "解析错误\n";

			thingBegin = funEnd;
			thingEnd = thingBegin + 4;

			thing = *thingBegin + *(thingBegin + 1) * 256 + *(thingBegin + 2)* (256 * 256) + *(thingBegin + 3)* (256 * 256 * 256);

			std::cout << "输入事件类型:" << thing << '\n';

			//事件类型03表示键盘或鼠标操作
			if (thing == 3)
			{
				//按键码
				maBegin = thingEnd;
				maEnd = maBegin + 4;

				//状态信息
				statusBegin = maEnd;
				statusEnd = statusBegin + 4;
			}

			break;


		case 'E':
			std::cout << "ERROR\n";

			if (std::distance(funEnd, source.cend()) < 8)
				std::cout << "解析错误\n";

			errorTypeBegin = funEnd;
			errorTypeEnd = errorTypeBegin + 4;

			errorType = *errorTypeBegin + *(errorTypeBegin + 1) * 256 + *(errorTypeBegin + 2)* (256 * 256) + *(errorTypeBegin + 3)* (256 * 256 * 256);

			std::cout << "错误类型:" << errorType << '\n';

			errorLenBegin = errorTypeEnd;
			errorLenEnd = errorLenBegin + 4;

			errorLen = *errorLenBegin + *(errorLenBegin + 1) * 256 + *(errorLenBegin + 2)* (256 * 256) + *(errorLenBegin + 3)* (256 * 256 * 256);

			std::cout << "错误字符串长度:" << errorLen << '\n';

			std::cout << "错误码字符串内容:" << std::string(errorLenEnd, source.cend()) << '\n';


			break;

		case 'A':
			std::cout << "AUDIO\n";

			if (std::distance(funEnd, source.cend()) < 8)
				std::cout << "解析错误\n";

			controlTypeBegin = funEnd;
			controlTypeEnd = controlTypeBegin + 4;

			controlType = *controlTypeBegin + *(controlTypeBegin + 1) * 256 + *(controlTypeBegin + 2)* (256 * 256) + *(controlTypeBegin + 3)* (256 * 256 * 256);

			std::cout << "控制类型:" << controlType << '\n';

			soundBegin = controlTypeEnd;
			soundEnd = soundBegin + 4;

			sound = *soundBegin + *(soundBegin + 1) * 256 + *(soundBegin + 2)* (256 * 256) + *(soundBegin + 3)* (256 * 256 * 256);

			std::cout << "音量设置:" << sound << '\n';

			//控制类型02表示音量调节，参数值01表示音量设置

			break;

		}

		break;
	case 7:
		switch (*funBegin)
		{
		case 'C':
			std::cout << "CONNECT\n";

			if (std::distance(funEnd, source.cend()) < 16)
				std::cout << "解析错误\n";

			renzhengBegin = funEnd;
			renzhengEnd = funEnd + 4;

			renzheng = *renzhengBegin + *(renzhengBegin + 1) * 256 + *(renzhengBegin + 2)* (256 * 256) + *(renzhengBegin + 3)* (256 * 256 * 256);

			std::cout << "认证状态类型:" << renzheng << '\n';

			//认证状态类型 renzhengBegin   renzhengEnd

			idBegin = funEnd + 4;
			idEnd = idBegin + 4;

			id = *idBegin + *(idBegin + 1) * 256 + *(idBegin + 2)* (256 * 256) + *(idBegin + 3)* (256 * 256 * 256);

			std::cout << "连接id：" << id << '\n';

			//2  连接认证请求
			if (renzheng == 2)
			{
				nameBegin = funEnd + 8;
				nameEnd = nameBegin + 4;

				//用户名长度  
				nameLen = *nameBegin + *(nameBegin + 1) * 256 + *(nameBegin + 2)* (256 * 256) + *(nameBegin + 3)* (256 * 256 * 256);

				std::cout << "用户名长度:" << nameLen << '\n';


				passBegin = nameEnd;
				passEnd = passBegin + 4;

				//密码长度  
				passLen = *passBegin + *(passBegin + 1) * 256 + *(passBegin + 2)* (256 * 256) + *(passBegin + 3)* (256 * 256 * 256);

				std::cout << "密码长度:" << passLen << '\n';
			}
			//3 认证响应
			else if (renzheng == 3)
			{
				statusBegin = idEnd;
				statusEnd = statusBegin + 4;

				status = *statusBegin + *(statusBegin + 1) * 256 + *(statusBegin + 2)* (256 * 256) + *(statusBegin + 3)* (256 * 256 * 256);

				std::cout << "认证状态(0表示成功，1表示失败)" << status << '\n';

				dataLenBegin = statusEnd;
				dataLenEnd = dataLenBegin + 4;

				dataLen = *dataLenBegin + *(dataLenBegin + 1) * 256 + *(dataLenBegin + 2)* (256 * 256) + *(dataLenBegin + 3)* (256 * 256 * 256);

				std::cout << "附加数据长度:" << dataLen << '\n';

			}


			break;

		case 'D':
			std::cout << "DISPLAY\n";

			if (std::distance(funEnd, source.cend()) < 8)
				std::cout << "解析错误\n";

			idBegin = funEnd;
			idEnd = idBegin + 4;

			id= *idBegin + *(idBegin + 1) * 256 + *(idBegin + 2)* (256 * 256) + *(idBegin + 3)* (256 * 256 * 256);

			std::cout << "通道ID号码:" << id << '\n';
			
			configBegin = idEnd + 4;
			configEnd = source.cend();

			//显示参数配置信息 configBegin  configEnd



			break;
		}

		break;


	case 9:
		std::cout << "CLIPBOARD\n";

		if (std::distance(funEnd, source.cend()) < 8)
			std::cout << "解析错误\n";

		dataLenBegin = funEnd + 8;
		dataLenEnd = dataLenBegin + 4;

		dataLen = *dataLenBegin + *(dataLenBegin + 1) * 256 + *(dataLenBegin + 2)* (256 * 256) + *(dataLenBegin + 3)* (256 * 256 * 256);

		std::cout << "剪贴板操作数据长度:" << dataLen << '\n';

		std::cout << "剪贴板操作数据:" << std::string(dataLenEnd, dataLenEnd + dataLen) << '\n';



		break;

	default:

		break;
	}

	std::cout << "SPICE 解析完毕\n\n";
}


int main()
{
	int spice1[]{ 83, 80, 73, 67, 69, 01, 00, 00, 00, 01, 00, 00, 00, 00, 00, 00, 00 };

	int spice2[]{ 67, 79, 78, 78, 69, 67, 84, 02, 00, 00, 00, 00, 00, 00, 00, 01, 00, 00, 00, 8,0,0,0 };

	int spice3[]{ 68, 73, 83, 80, 76, 65, 89, 01, 00, 00, 00, 00, 00, 00, 00, 01, 00, 00, 00 };

	int spice4[]{ 73, 78, 80, 85, 84, 03, 00, 00, 00, 01, 00, 00, 00, 00, 01, 00, 00 };

	int spice5[]{ 69, 82, 82, 79, 82, 01, 00, 00, 00, 12, 00, 00, 00, 's', 'o', 'c', 'k', 'e', 't', ' ', 'e', 'r', 'r', 'o', 'r' };

	int spice6[]{ 80, 73, 78, 71, 00, 00, 00, 00 };

	int spice7[]{ 'C', 'L', 'I', 'P', 'B', 'O', 'A', 'R', 'D', 01, 00, 00, 00, 00, 00, 00, 00, 05, 00, 00, 00, 74, 65, 73, 74, 00 };

	int spice8[]{ 65, 85, 68, 73, 79, 02, 00, 00, 00, 01, 00, 00, 00, 00, 00, 00, 00 };

	int spice9[]{ 70, 73, 72, 65, 03, 00, 00, 00, 00, 00, 00, 00, 01, 00, 00, 00, 8, 00, 00, 00 };

	int spice10[]{ 67, 79, 78, 78, 69, 67, 84, 03, 00, 00, 00, 00, 00, 00, 00, 01, 00, 00, 00, 00, 00, 00, 00 };


	std::string str;
	str.assign(spice1, spice1 + 17);

	parseSPICE(str);

	str.assign(spice2, spice2 + 23);
	parseSPICE(str);

	str.assign(spice3, spice3 + 19);
	parseSPICE(str);

	str.assign(spice4, spice4 + 17);
	parseSPICE(str);

	str.assign(spice5, spice5 + 25);
	parseSPICE(str);

	str.assign(spice6, spice6 + 8);
	parseSPICE(str);

	str.assign(spice7, spice7 + 26);
	parseSPICE(str);

	str.assign(spice8, spice8 + 17);
	parseSPICE(str);

	str.assign(spice9, spice9 + 20);
	parseSPICE(str);

	str.assign(spice10, spice10 + 23);
	parseSPICE(str);

	return 0;
}

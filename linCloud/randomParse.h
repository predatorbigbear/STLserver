#pragma once

#include<algorithm>
#include<functional>
#include<cctype>


namespace HTTPBODYKEY
{
	namespace TESTRANDOMBODY
	{
		static const char *test{ "test" };
		static const char *parameter{ "parameter" };
		static const char *number{ "number" };

		static constexpr int testLen{ 4 };
		static constexpr int parameterLen{ 9 };
		static constexpr int numberLen{ 6 };
	};


	namespace TESTLOGINBODY
	{
		static const char *username{ "username" };
		static const char *password{ "password" };

		static constexpr int usernameLen{ 8 };
		static constexpr int passwordLen{ 8 };
	};


	namespace TESTMAKEJSON
	{
		static const char *jsonType{ "jsonType" };

		static constexpr int jsonTypeLen{ 8 };
	};
};




namespace HTTPINTERFACENUM
{
	enum TESTRANDOMBODY
	{
		test = 0,
		parameter = 2,
		number = 4,
		max=6
	};


	enum TESTLOGIN
	{
		username = 0,
	    password = 2
	};

	enum TESTGETCLIENTPUBLICKEY
	{
		publicKey = 0,
		hashValue = 2,
		randomString = 4
	};

	enum TESTGETENCRYPTKEY
	{
		encryptKey = 0
	};
};






template<typename INTERFACE, unsigned int INTERFACEPARANUM>
static bool randomParseBody(const char *source, const unsigned int sourceLen, const char **des, const unsigned int desLen)
{
	if (!source || !sourceLen || !des || !desLen || !INTERFACEPARANUM || desLen < (INTERFACEPARANUM * 2))
		return false;

	std::fill(des, des + INTERFACEPARANUM, nullptr);


	const char *iterBegin{ source }, *iterEnd{ source + sourceLen }, *iterStrBegin{}, *iterStrEnd{}, *iterWordBegin{}, *iterWordEnd{};
	unsigned int paraNum{};
	unsigned int keyNum{ INTERFACEPARANUM / 2 };

	while (iterBegin != iterEnd)
	{
		iterStrBegin = iterBegin;

		iterStrEnd = std::find_if(iterStrBegin + 1, iterEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, '='),
			std::bind(std::equal_to<>(), std::placeholders::_1, '&')));


		if (iterStrEnd == iterEnd || *iterStrEnd == '&')
		{
			if (++paraNum > keyNum)
				return false;


			if constexpr (std::is_same<INTERFACE, HTTPINTERFACENUM::TESTRANDOMBODY>::value)
			{
				switch (std::distance(iterStrBegin, iterStrEnd))
				{
				case HTTPBODYKEY::TESTRANDOMBODY::testLen:
					if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTRANDOMBODY::test, HTTPBODYKEY::TESTRANDOMBODY::test + HTTPBODYKEY::TESTRANDOMBODY::testLen,
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
						return false;

					/*这里没有判断同一项参数有没有重复，如果要判断的话，加上这句  
					if(*(des + HTTPINTERFACENUM::TESTRANDOMBODY::test))
					return false;

					*/

					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::test) = iterStrBegin;
					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::test + 1) = iterStrBegin;

					break;
				case HTTPBODYKEY::TESTRANDOMBODY::parameterLen:
					if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTRANDOMBODY::parameter, HTTPBODYKEY::TESTRANDOMBODY::parameter + HTTPBODYKEY::TESTRANDOMBODY::parameterLen,
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
						return false;

					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::parameter) = iterStrBegin;
					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::parameter + 1) = iterStrBegin;

					break;
				case HTTPBODYKEY::TESTRANDOMBODY::numberLen:
					if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTRANDOMBODY::number, HTTPBODYKEY::TESTRANDOMBODY::number + HTTPBODYKEY::TESTRANDOMBODY::numberLen,
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
						return false;

					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::number) = iterStrBegin;
					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::number + 1) = iterStrBegin;

					break;


				default:

					return false;
					break;
				}
			}

			//////////////////////////////////////////////////////////////////////////////////////////////
			else if constexpr (std::is_same<INTERFACE, HTTPINTERFACENUM::TESTLOGIN>::value)
			{
				switch (std::distance(iterStrBegin, iterStrEnd))
				{
				case HTTPBODYKEY::TESTLOGINBODY::usernameLen:
					switch (*iterStrBegin)
					{
					case 'u':
					case 'U':
						if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTLOGINBODY::username, HTTPBODYKEY::TESTLOGINBODY::username + HTTPBODYKEY::TESTLOGINBODY::usernameLen,
							std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TESTLOGIN::username) = iterStrBegin;
						*(des + HTTPINTERFACENUM::TESTLOGIN::username + 1) = iterStrBegin;

						break;

					case 'p':
					case 'P':
						if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTLOGINBODY::password, HTTPBODYKEY::TESTLOGINBODY::password + HTTPBODYKEY::TESTLOGINBODY::passwordLen,
							std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TESTLOGIN::password) = iterStrBegin;
						*(des + HTTPINTERFACENUM::TESTLOGIN::password + 1) = iterStrBegin;

						break;


					default:
						return false;
						break;
					}

					break;

				default:

					return false;
					break;
				}
			}



			iterBegin = iterStrEnd;
		}
		else     // '='
		{
			iterWordBegin = iterStrEnd + 1;
			iterWordEnd = std::find(iterWordBegin, iterEnd, '&');

			if (++paraNum > keyNum)
				return false;


			if constexpr (std::is_same<INTERFACE, HTTPINTERFACENUM::TESTRANDOMBODY>::value)
			{
				switch (std::distance(iterStrBegin, iterStrEnd))
				{
				case HTTPBODYKEY::TESTRANDOMBODY::testLen:
					if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTRANDOMBODY::test, HTTPBODYKEY::TESTRANDOMBODY::test + HTTPBODYKEY::TESTRANDOMBODY::testLen,
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
						return false;

					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::test) = iterWordBegin;
					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::test + 1) = iterWordEnd;

					break;
				case HTTPBODYKEY::TESTRANDOMBODY::parameterLen:
					if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTRANDOMBODY::parameter, HTTPBODYKEY::TESTRANDOMBODY::parameter + HTTPBODYKEY::TESTRANDOMBODY::parameterLen,
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
						return false;

					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::parameter) = iterWordBegin;
					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::parameter + 1) = iterWordEnd;

					break;
				case HTTPBODYKEY::TESTRANDOMBODY::numberLen:
					if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTRANDOMBODY::number, HTTPBODYKEY::TESTRANDOMBODY::number + HTTPBODYKEY::TESTRANDOMBODY::numberLen,
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
						return false;

					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::number) = iterWordBegin;
					*(des + HTTPINTERFACENUM::TESTRANDOMBODY::number + 1) = iterWordEnd;
					
					break;


				default:

					return false;
					break;
				}
			}

			//////////////////////////////////////////////////////////////////////////////////////////////
			else if constexpr (std::is_same<INTERFACE, HTTPINTERFACENUM::TESTLOGIN>::value)
			{
				switch (std::distance(iterStrBegin, iterStrEnd))
				{
				case HTTPBODYKEY::TESTLOGINBODY::usernameLen:
					switch (*iterStrBegin)
					{
					case 'u':
					case 'U':
						if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTLOGINBODY::username, HTTPBODYKEY::TESTLOGINBODY::username + HTTPBODYKEY::TESTLOGINBODY::usernameLen,
							std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TESTLOGIN::username) = iterStrBegin;
						*(des + HTTPINTERFACENUM::TESTLOGIN::username + 1) = iterStrBegin;

						break;

					case 'p':
					case 'P':
						if (!std::equal(iterStrBegin, iterStrEnd, HTTPBODYKEY::TESTLOGINBODY::password, HTTPBODYKEY::TESTLOGINBODY::password + HTTPBODYKEY::TESTLOGINBODY::passwordLen,
							std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TESTLOGIN::password) = iterStrBegin;
						*(des + HTTPINTERFACENUM::TESTLOGIN::password + 1) = iterStrBegin;

						break;


					default:
						return false;
						break;
					}

					break;

				default:

					return false;
					break;
				}
			}



			iterBegin = iterWordEnd;
		}

		if (iterBegin != iterEnd)
			++iterBegin;
	}

	return true;
}
#include "randomCodeGenerator.h"

RandomCodeGenerator::RandomCodeGenerator() :m_rng(std::chrono::system_clock::now().time_since_epoch().count()),
m_dist(0, m_charset.size() - 1)
{

}

const bool RandomCodeGenerator::generate(char*& verifyCodeBuf, const unsigned int length)
{
	if (!verifyCodeBuf)
		return false;

	char* ptr{ verifyCodeBuf };
	for (int i = 0; i != length; ++i)
	{
		*ptr++ = m_charset[m_dist(m_rng)];
	}

	return true;
}





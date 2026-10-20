// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

using namespace crt;
namespace pong_super
{
	class INameInputPageListener
	{
	public:
		virtual void nameInputPageHidden() = 0;
	};
};// end namespace pong_super

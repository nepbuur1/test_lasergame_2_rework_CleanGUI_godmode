// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class INameInputPageListener
	{
	public:
		virtual void nameInputPageHidden() = 0;
	};
};// end namespace pong_cleangui_dg_adv

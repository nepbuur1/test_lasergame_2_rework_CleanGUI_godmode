// by Marius Versteegen, 2024

#pragma once
#include <crt_IKeyboardListener.h>

namespace crt
{
	class IKeyboardListener;

	class IKeyboard
	{
	public:
		virtual void addKeyboardListener(IKeyboardListener* pButtonListener) = 0;
	};
};
// by Marius Versteegen, 2024

#pragma once
#include "crt_IInputField.h"

namespace crt
{
	// Note: Forward referencing with IButton because of mutual dependency could have 
	// been avoided if not IButton* but an enum value would be passed to identify the button. 
	// But I like to avoid the hassle of bookkeeping enums.
	// Mutual dependency of pure abstract interfaces is perfectly sound.
	class IInputfield;

	class IInputfieldListener
	{
	public:
		virtual void inputNewKey(IInputfield* pInputfield,const char* newChar) = 0;
		virtual void inputfieldPressed(IInputfield* pInputfield) = 0;
		virtual void outsideInputfieldPressed(IInputfield* pInputfield) = 0;
	};
};
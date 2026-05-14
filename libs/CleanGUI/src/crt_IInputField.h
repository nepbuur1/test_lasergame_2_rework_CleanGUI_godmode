// by Marius Versteegen, 2024

#pragma once
#include <crt_IInputFieldListener.h>
#include <crt_string.h>

namespace crt
{
	class IInputfieldListener;

	class IInputfield
	{
	public:
		virtual void addInputfieldListener(IInputfieldListener* pInputfieldListener) = 0;
		virtual void getIntValue(int& nValue) = 0;
		virtual void getFloatValue(float& fValue) = 0;
		virtual void getTextValue(std::string_base& strValue) = 0;
	};
};
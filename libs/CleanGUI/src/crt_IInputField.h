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

		// Het veld in een keer leegmaken, van buitenaf. Dit is geen toetsaanslag:
		// er gaat geen inputNewKey naar de luisteraars, want er is niemand die
		// iets heeft ingetypt. Wie wel wil dat er iets van merkt, tekent zelf
		// opnieuw - het veld zelf staat na afloop leeg in beeld.
		virtual void clearInput() = 0;

		virtual void getIntValue(int& nValue) = 0;
		virtual void getFloatValue(float& fValue) = 0;
		virtual void getTextValue(std::string_base& strValue) = 0;
	};
};
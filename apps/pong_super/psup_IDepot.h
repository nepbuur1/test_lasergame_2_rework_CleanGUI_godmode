// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>

// Het stukje Depot dat een uitgegeven exemplaar zelf nodig heeft: zichzelf weer
// vrijgeven. release() laat het concrete Depot ook reset() op het item aanroepen,
// zodat het na teruggave weer in zijn herbruikbare ruststand staat.
//
// Waarom een interface en niet gewoon Depot<T,N>*: dat zou een cirkel opleveren.
// Depot<Bal,50> bevat Bal-objecten, en een Bal die zichzelf wil vrijgeven zou dan
// het volledige Depot<Bal,50> moeten kennen - inclusief zichzelf. Via IDepot kent
// de Bal alleen de ene functie die hij gebruikt, en weet hij verder niets van het
// depot waar hij in ligt.

using namespace crt;
namespace pong_super
{
	class IDepot
	{
	public:
		virtual void release(int index) = 0;
	};
};// end namespace pong_super

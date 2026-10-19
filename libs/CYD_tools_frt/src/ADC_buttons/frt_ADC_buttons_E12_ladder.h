// by Marius Versteegen, 2026

// De weerstandsladder voor zes knoppen met twaalf E12-weerstanden (+/-1%).
// Ontwerp: e12_definitief.json en knoppen_e12_drempels.h in
// Handleidingen/Hardware Development/Analoge Electronica/Weerstandsladder_voor_knoppen.
//
// Tak i (S1..S6): Uuit --B[i]-- knooppunt --P[i]-- Vref. Een ingedrukte Si trekt het
// knooppunt naar massa. Daarnaast zit er een belastingsweerstand RL van Uuit naar massa.
// Die hoort bij de ladder: zonder RL komt "geen knop" op Vref = 3,3 V, boven de bovengrens
// van de ESP32-ADC (~3,1 V). Met 68k ligt het hoogste niveau op ~2,40 V, binnen het
// nauwkeurige bereik van de ADC bij 12 dB demping (tot ~2450 mV).
//
// De code (0..63) heeft bit 5 = S1 ... bit 0 = S6, 1 = ingedrukt. Hoe hoger de code,
// hoe lager Uuit: code 0 (geen knop) is het hoogste niveau, code 63 (alle knoppen) is 0 V.
// Dat blijft zo met RL erbij (nagerekend voor RL = 47k..150k).
//
// Contactweerstand van een knop (of draad) houdt een ingedrukt knooppunt net boven 0 V
// en tilt Uuit dus altijd iets óp, richting een code met meer knoppen. De buren van een
// code met weinig knoppen zijn vaak codes met veel knoppen (S1+S2 = 110000 ligt naast
// 101111 = S1+S3+S4+S5+S6). Met maxSimultaneousButtons decodeer je alleen naar codes met
// hooguit zoveel ingedrukte knoppen; de drempels liggen dan midden tussen de overgebleven
// niveaus, en de marges worden veel groter.

#pragma once
#include <stdint.h>

namespace frt
{
	class ADC_buttons_E12_ladder
	{
	public:
		static const int NOF_BUTTONS = 6;
		static const int NOF_CODES   = 64;

		// Weerstandswaarden van het ontwerp (ohm).
		static constexpr float B_ohm[NOF_BUTTONS] = {  33000.0f,  68000.0f, 150000.0f, 330000.0f, 680000.0f, 1200000.0f };
		static constexpr float P_ohm[NOF_BUTTONS] = {  18000.0f,  33000.0f,  47000.0f,  47000.0f,  56000.0f,  390000.0f };
		static constexpr float RL_ohm = 68000.0f;   // Van Uuit naar massa.

	private:
		float   levels[NOF_CODES];             // Uuit/Vref per code, nominaal.
		uint8_t allowedCodes[NOF_CODES];       // De codes waarnaar gedecodeerd mag worden, oplopend (dus dalend niveau).
		int     nofAllowedCodes;
		float   thresholds[NOF_CODES - 1];     // thresholds[k] ligt midden tussen de niveaus van allowedCodes[k] en [k+1].

	public:
		// maxSimultaneousButtons: hoeveel knoppen er hooguit tegelijk ingedrukt worden (1..6).
		ADC_buttons_E12_ladder(int maxSimultaneousButtons)
		{
			for (int code = 0; code < NOF_CODES; code++)
			{
				levels[code] = computeLevel(code);
			}
			nofAllowedCodes = 0;
			for (int code = 0; code < NOF_CODES; code++)
			{
				if (__builtin_popcount(code) <= maxSimultaneousButtons)
				{
					allowedCodes[nofAllowedCodes++] = (uint8_t)code;
				}
			}
			for (int k = 0; k < nofAllowedCodes - 1; k++)
			{
				thresholds[k] = 0.5f * (levels[allowedCodes[k]] + levels[allowedCodes[k + 1]]);
			}
		}

		// Uuit/Vref bij een gegeven code. Knooppuntsvergelijking op Uuit.
		static float computeLevel(int code)
		{
			float gToVref = 0.0f;   // Geleiding van Uuit naar Vref.
			float gTotal  = 0.0f;   // Totale geleiding vanuit Uuit.
			for (int i = 0; i < NOF_BUTTONS; i++)
			{
				bool bPressed = (code & (1 << (NOF_BUTTONS - 1 - i))) != 0;
				if (bPressed)
				{
					gTotal += 1.0f / B_ohm[i];                 // Via B naar massa.
				}
				else
				{
					float g = 1.0f / (B_ohm[i] + P_ohm[i]);    // Via B en P naar Vref.
					gToVref += g;
					gTotal  += g;
				}
			}
			gTotal += 1.0f / RL_ohm;                           // Via RL naar massa.
			return gToVref / gTotal;
		}

		float getLevel(int code) const { return levels[code]; }

		// Geeft de toegestane code met het dichtstbijzijnde niveau bij een gemeten Uuit/Vref.
		// Binair zoeken in de dalende drempeltabel.
		uint8_t decode(float uuitOverVref) const
		{
			int low = 0, high = nofAllowedCodes - 1;
			while (low < high)
			{
				int mid = (low + high) / 2;
				if (uuitOverVref >= thresholds[mid]) high = mid;
				else low = mid + 1;
			}
			return allowedCodes[low];
		}

		// Code -> bitmasker met bit i = knop i (knop 0 = S1 ... knop 5 = S6).
		// In de code zelf is het precies andersom (S1 = bit 5), vandaar het omdraaien.
		static uint8_t codeToButtonMask(uint8_t code)
		{
			uint8_t mask = 0;
			for (int i = 0; i < NOF_BUTTONS; i++)
			{
				if (code & (1 << (NOF_BUTTONS - 1 - i))) mask |= (1 << i);
			}
			return mask;
		}
	};
};

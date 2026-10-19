// by Marius Versteegen, 2026

#pragma once
#include <math.h>
#include <string.h>
#include <algorithm>
#include <crt_CleanRTOS.h>
#include <crt_ADC_buttons.h>

namespace crt
{
	// Meet hoe ver de gemeten spanning van een ingedrukte knoppencombinatie van de dichtstbijzijnde
	// beslisgrens (drempel naar een andere combinatie) af ligt, in mV. Per combinatie wordt de
	// kleinste afstand over alle drukken bijgehouden: de worst case.
	//
	// Werkwijze:
	// - Elke pollPeriodMs een snapshot van ADC_buttons: gedebouncede code, gemeten mV en
	//   gekalibreerde Vref, uit dezelfde toestand.
	// - Een "druk" is een periode waarin de gedebouncede code gelijk blijft. Alle metingen van de
	//   druk gaan in een buffer. De eerste SKIP_START_POLLS (contact zet zich nog) en de laatste
	//   TRIM_END_POLLS (de spanning verandert al, terwijl de debounce de code nog vasthoudt) tellen
	//   niet mee. Is er dan minder dan MIN_HOLD_SAMPLES over, dan was het een tussenstand
	//   (bijvoorbeeld onderweg van S1 naar S1+S2) en telt de druk niet mee.
	// - De mediaan van de rest bepaalt welke combinatie het was: de combinatie in wier band de
	//   mediaan valt. Dat hoeft niet de gedebouncede code te zijn. Ligt een combinatie precies op
	//   een grens, dan springt de gedebouncede code soms niet mee, en dan zou je anders de
	//   spanning van de ene combinatie aan de andere toerekenen.
	// - Metingen die meer dan STEADY_WINDOW_MV van de mediaan af liggen, zijn dender of een
	//   overgang. Ze tellen niet mee voor de afstand, maar worden wel als uitschieters geteld.
	// - De afstand van een meting is die tot de dichtstbijzijnde grens van de combinatie. Negatief
	//   betekent: aan de verkeerde kant van een grens.
	//
	// Let op: de test weet niet wat je bedoelde in te drukken. Valt de mediaan in de band van een
	// andere combinatie dan die je indrukte, dan telt de druk mee bij die andere, met een kleine
	// afstand. Kijk in de log of elke combinatie herkend wordt als wat je indrukte.
	template<unsigned int MAXLISTENERCOUNT> class WorstCaseBorderDistanceTester : public Task
	{
	private:
		static const int NOF_CODES        = ADC_buttons_E12_ladder::NOF_CODES;
		static const int NOF_BUTTONS      = ADC_buttons_E12_ladder::NOF_BUTTONS;
		static const int SKIP_START_POLLS = 5;      // 50 ms bij 10 ms per meting.
		static const int TRIM_END_POLLS   = 8;      // 80 ms.
		static const int MIN_HOLD_SAMPLES = 5;      // Na het bijsnijden; korter is een tussenstand.
		static const int MAX_HOLD_SAMPLES = 1000;   // 10 s; wat langer duurt, telt vanaf daar niet meer mee.
		static constexpr float STEADY_WINDOW_MV = 10.0f;
		static const int TABLE_AFTER_IDLE_POLLS = 1500;   // 15 s zonder nieuwe druk: tabel.
		static const uint32_t TODO_EVERY_MS     = 5000;   // Elke 5 s: wat nog niet gedrukt is.
		static const int TODO_COUNT             = 5;      // Zoveel combinaties tegelijk voorstellen.

		struct CodeStats
		{
			uint32_t nofHolds;
			uint32_t nofSamples;         // Alleen de steady metingen.
			uint32_t nofOutliers;
			uint16_t minMillivolts, maxMillivolts;
			float    minDistanceMv;      // Kleinste afstand van een steady meting tot een grens.
			int8_t   minDistanceSide;    // +1: de grens erboven, -1: de grens eronder.
			float    minMedianDistanceMv;// Kleinste afstand van de mediaan van een druk tot een grens.
		};

		ADC_buttons<MAXLISTENERCOUNT>& adcButtons;
		const ADC_buttons_E12_ladder& ladder;
		uint32_t pollPeriodMs;

		CodeStats stats[NOF_CODES];

		// Lopende druk.
		uint8_t  holdDebouncedCode;
		int      holdCount;
		uint16_t holdBuffer[MAX_HOLD_SAMPLES];
		uint16_t sortBuffer[MAX_HOLD_SAMPLES];

		uint32_t pollsSinceLastHold;
		bool     bNewSinceTable;
		TickType_t lastTodoTick;

	public:
		WorstCaseBorderDistanceTester(const char* taskName, unsigned int taskPriority, unsigned int taskStackSizeBytes, unsigned int taskCoreNumber,
		                              ADC_buttons<MAXLISTENERCOUNT>& adcButtons, uint32_t pollPeriodMs = 10) :
			Task(taskName, taskPriority, taskStackSizeBytes, taskCoreNumber),
			adcButtons(adcButtons), ladder(adcButtons.getLadder()), pollPeriodMs(pollPeriodMs),
			holdDebouncedCode(0), holdCount(0), pollsSinceLastHold(0), bNewSinceTable(false), lastTodoTick(0)
		{
			for (int c = 0; c < NOF_CODES; c++)
			{
				stats[c].nofHolds = 0; stats[c].nofSamples = 0; stats[c].nofOutliers = 0;
				stats[c].minMillivolts = 0xFFFF; stats[c].maxMillivolts = 0;
				stats[c].minDistanceMv = INFINITY; stats[c].minDistanceSide = 0;
				stats[c].minMedianDistanceMv = INFINITY;
			}
			start();
		}

	private:
		static void codeName(uint8_t code, char* buf)   // buf: minstens 20 tekens
		{
			if (code == 0) { strcpy(buf, "geen"); return; }
			char* p = buf;
			for (int i = 0; i < NOF_BUTTONS; i++)
			{
				if (code & (1 << (NOF_BUTTONS - 1 - i)))
				{
					if (p != buf) *p++ = '+';
					*p++ = 'S'; *p++ = (char)('1' + i);
				}
			}
			*p = '\0';
		}

		// Afstand (mV) van een spanning tot de dichtstbijzijnde grens van code; side +1 = erboven.
		float distanceMv(uint8_t code, float uuitOverVref, float vrefMillivolts, int8_t& side) const
		{
			float up = ladder.getUpperBorder(code);
			float lo = ladder.getLowerBorder(code);
			float dUp = isnan(up) ? INFINITY : (up - uuitOverVref) * vrefMillivolts;
			float dLo = isnan(lo) ? INFINITY : (uuitOverVref - lo) * vrefMillivolts;
			side = (dUp <= dLo) ? +1 : -1;
			return (dUp <= dLo) ? dUp : dLo;
		}

		// Nominale afstand volgens de berekening, bij de Vref van nu.
		float nominalDistanceMv(uint8_t code, float vrefMillivolts) const
		{
			int8_t side;
			return distanceMv(code, ladder.getLevel(code), vrefMillivolts, side);
		}

		uint8_t worstCode() const
		{
			int worst = -1;
			for (int c = 1; c < NOF_CODES; c++)   // "geen knop" telt niet mee: de kalibratie zet hem op zijn nominale niveau.
			{
				if (stats[c].nofHolds == 0) continue;
				if (worst < 0 || stats[c].minDistanceMv < stats[worst].minDistanceMv) worst = c;
			}
			return (worst < 0) ? 0 : (uint8_t)worst;
		}

		// Het aantal combinaties dat de ladder kan herkennen, "geen knop" niet meegeteld.
		int nofCodesToTest() const
		{
			return ladder.getNofAllowedCodes() - 1;
		}

		int nofCodesSeen() const
		{
			int n = 0;
			for (int c = 1; c < NOF_CODES; c++) if (stats[c].nofHolds > 0) n++;
			return n;
		}

		void finishHold(float vrefMillivolts)
		{
			int first = SKIP_START_POLLS;
			int last  = holdCount - TRIM_END_POLLS;   // exclusief
			int n     = last - first;
			if (holdDebouncedCode == 0 || n < MIN_HOLD_SAMPLES) return;

			// Mediaan van de bruikbare metingen.
			memcpy(sortBuffer, holdBuffer + first, n * sizeof(uint16_t));
			::std::sort(sortBuffer, sortBuffer + n);   // :: omdat CleanRTOS een eigen crt::std heeft.
			float medianMv = (n % 2) ? sortBuffer[n / 2] : 0.5f * (sortBuffer[n / 2 - 1] + sortBuffer[n / 2]);

			// De combinatie is die in wier band de mediaan valt.
			uint8_t code = ladder.decode(medianMv / vrefMillivolts);
			int8_t side;
			float medianDistance = distanceMv(code, medianMv / vrefMillivolts, vrefMillivolts, side);

			// Afstand van elke steady meting; uitschieters tellen.
			float    minDistance = INFINITY;
			int8_t   minSide = 0;
			uint32_t nofSteady = 0, nofOutliers = 0;
			uint16_t minMv = 0xFFFF, maxMv = 0;
			for (int i = first; i < last; i++)
			{
				uint16_t mv = holdBuffer[i];
				if (fabsf(mv - medianMv) > STEADY_WINDOW_MV) { nofOutliers++; continue; }
				nofSteady++;
				if (mv < minMv) minMv = mv;
				if (mv > maxMv) maxMv = mv;
				float d = distanceMv(code, mv / vrefMillivolts, vrefMillivolts, side);
				if (d < minDistance) { minDistance = d; minSide = side; }
			}

			CodeStats& st = stats[code];
			st.nofHolds++;
			st.nofSamples  += nofSteady;
			st.nofOutliers += nofOutliers;
			if (minMv < st.minMillivolts) st.minMillivolts = minMv;
			if (maxMv > st.maxMillivolts) st.maxMillivolts = maxMv;
			if (minDistance < st.minDistanceMv) { st.minDistanceMv = minDistance; st.minDistanceSide = minSide; }
			if (medianDistance < st.minMedianDistanceMv) st.minMedianDistanceMv = medianDistance;

			char name[20], debouncedName[20], worstName[20];
			codeName(code, name);
			codeName(holdDebouncedCode, debouncedName);
			uint8_t w = worstCode();
			codeName(w, worstName);
			ESP_LOGI(taskName, "%-14s code %2u: %3u metingen (+%u uitschieters), mediaan %4.0f mV, afstand mediaan %5.1f / kleinste %5.1f mV tot de grens %s (nominaal %4.1f)%s%s  |  gezien %2d/%d, slechtste: %.1f mV bij %s",
			         name, code, (unsigned)nofSteady, (unsigned)nofOutliers, medianMv, medianDistance, minDistance,
			         (minSide > 0) ? "erboven" : "eronder", nominalDistanceMv(code, vrefMillivolts),
			         (code != holdDebouncedCode) ? "  LET OP: herkend als " : "", (code != holdDebouncedCode) ? debouncedName : "",
			         nofCodesSeen(), nofCodesToTest(), stats[w].minDistanceMv, worstName);
			pollsSinceLastHold = 0;
			bNewSinceTable = true;
		}

		// Stelt TODO_COUNT combinaties voor die nog niet gedrukt zijn: eerst die met de minste
		// knoppen, en binnen een aantal op volgorde van code.
		void printTodo()
		{
			char line[TODO_COUNT * 20 + 1] = "";
			int nofListed = 0, nofLeft = 0;
			for (int buttons = 1; buttons <= NOF_BUTTONS; buttons++)
			{
				for (int c = 1; c < NOF_CODES; c++)
				{
					if (__builtin_popcount(c) != buttons || !ladder.isAllowed((uint8_t)c) || stats[c].nofHolds > 0) continue;
					nofLeft++;
					if (nofListed < TODO_COUNT)
					{
						char name[20];
						codeName((uint8_t)c, name);
						if (nofListed > 0) strcat(line, ",  ");
						strcat(line, name);
						nofListed++;
					}
				}
			}
			if (nofLeft == 0)
			{
				ESP_LOGI(taskName, ">>> Alle %d combinaties gehad. Laat 15 s los voor de tabel.", nofCodesToTest());
			}
			else
			{
				ESP_LOGI(taskName, ">>> Nog %d te gaan. Druk:  %s", nofLeft, line);
			}
		}

		void printTable(float vrefMillivolts)
		{
			bool printed[NOF_CODES] = {};
			ESP_LOGI(taskName, "==== Worst case afstand tot een beslisgrens, slechtste eerst (Vref %.0f mV, %d/%d combinaties gezien) ====",
			         vrefMillivolts, nofCodesSeen(), nofCodesToTest());
			ESP_LOGI(taskName, "combinatie      code  keer  metingen  uitsch.   min..max mV   kleinste afstand    slechtste mediaan   nominaal");
			for (int row = 0; row < NOF_CODES; row++)
			{
				int best = -1;
				for (int c = 1; c < NOF_CODES; c++)
				{
					if (printed[c] || stats[c].nofHolds == 0) continue;
					if (best < 0 || stats[c].minDistanceMv < stats[best].minDistanceMv) best = c;
				}
				if (best < 0) break;
				printed[best] = true;
				const CodeStats& st = stats[best];
				char name[20];
				codeName((uint8_t)best, name);
				ESP_LOGI(taskName, "%-14s  %2d   %3u   %6u   %5u   %4u..%4u   %6.1f mV %s      %6.1f mV      %5.1f mV",
				         name, best, (unsigned)st.nofHolds, (unsigned)st.nofSamples, (unsigned)st.nofOutliers,
				         st.minMillivolts, st.maxMillivolts,
				         st.minDistanceMv, (st.minDistanceSide > 0) ? "erboven" : "eronder",
				         st.minMedianDistanceMv, nominalDistanceMv((uint8_t)best, vrefMillivolts));
				vTaskDelay(1);   // De log even laten wegschrijven.
			}
			ESP_LOGI(taskName, "==== einde tabel ====");
		}

		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(pdMS_TO_TICKS(500));   // Wacht tot ADC_buttons gekalibreerd is.
			ESP_LOGI(taskName, "Klaar. Druk knoppencombinaties in en houd ze elk ~1 s vast. Na 15 s zonder drukken volgt een tabel.");
			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); // This function call takes about 0.25ms! It should be called while debugging only.

				uint8_t code; uint16_t millivolts; float vrefMillivolts;
				adcButtons.readSnapshot(code, millivolts, vrefMillivolts);

				if (code != holdDebouncedCode)
				{
					finishHold(vrefMillivolts);
					holdDebouncedCode = code;
					holdCount = 0;
				}
				if (code != 0 && holdCount < MAX_HOLD_SAMPLES)
				{
					holdBuffer[holdCount++] = millivolts;
				}

				if (code == 0)
				{
					pollsSinceLastHold++;
					if (bNewSinceTable && pollsSinceLastHold >= TABLE_AFTER_IDLE_POLLS)
					{
						printTable(vrefMillivolts);
						bNewSinceTable = false;
					}
				}
				if (xTaskGetTickCount() - lastTodoTick >= pdMS_TO_TICKS(TODO_EVERY_MS))
				{
					lastTodoTick = xTaskGetTickCount();
					printTodo();
				}
				vTaskDelay(pdMS_TO_TICKS(pollPeriodMs));
			}
		}
	}; // end class WorstCaseBorderDistanceTester
};// end namespace crt

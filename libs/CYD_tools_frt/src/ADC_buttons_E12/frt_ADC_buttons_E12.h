// by Marius Versteegen, 2026

// frt::ADC_buttons_E12
//
// FreeRTOS-variant van crt::ADC_buttons_E12 (libs/CYD_tools_crt), zonder CleanRTOS.
// Vertaald volgens de regels in ModelAnalysis/FastModelFeedback/Config/StdCleanRtosNaarStdFreeRtos:
//   - crt::Task       -> een eigen FreeRTOS-taak (xTaskCreatePinnedToCore met een statische taskWrapper).
//   - Pool<State>     -> stateQueue, een queue met één plek: schrijven met xQueueOverwrite,
//                        lezen met xQueuePeek. Hij krijgt in de constructor een beginwaarde,
//                        omdat isButtonPressed() al aangeroepen kan worden voordat de taak er iets in zet.
//
// Leest zes knoppen uit via één ADC-pin, met de weerstandsladder van twaalf
// E12-weerstanden (zie docs/ADC_buttons_E12/ADC_buttons_E12.md).
//
// Het object heeft een eigen taak. Die leest periodiek de ADC uit (gemiddeld over
// een aantal samples), zet de spanning om in een knoppencombinatie, en stuurt
// buttonPressed/buttonReleased naar de listeners zodra een nieuwe combinatie een
// paar metingen achter elkaar stabiel is (dat is meteen de debounce).
//
// isButtonPressed() mag vanuit elke taak worden aangeroepen: de toestand staat in stateQueue.
//
// Zelfkalibratie: de absolute spanning van de 3,3 V-regelaar en de versterking van de
// ADC liggen allebei maar op een procent of twee vast, en dat is meer dan de halve
// afstand tussen twee niveaus. Daarom meet de taak bij het opstarten het niveau
// "geen knop ingedrukt" en leidt daar de effectieve Vref uit af. Daarna volgt hij die
// langzaam zolang er geen knop is ingedrukt. Wie bij het opstarten een knop vasthoudt,
// ziet die knop pas na het loslaten: het niveau "geen knop" ligt boven alle andere
// niveaus, dus de eerste keer dat alles los is, trekt de kalibratie zichzelf recht.

#pragma once
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include "frt_IADC_buttons_E12.h"
#include "frt_ADC_buttons_E12_ladder.h"

namespace frt
{
	// Don't select the MAXLISTENERCOUNT too high, or you may suffer from stack overflow troubles.
	template<unsigned int MAXLISTENERCOUNT> class ADC_buttons_E12 : public IADC_buttons_E12
	{
	public:
		static const int NOF_BUTTONS = ADC_buttons_E12_ladder::NOF_BUTTONS;

	private:
		// De toestand, zoals die in stateQueue staat.
		struct State
		{
			uint8_t  pressedMask;   // Gedebounced. Bit i = knop i ingedrukt.
			uint8_t  code;          // Gedebounced. Ladder-code 0..63 (bit 5 = S1).
			uint16_t millivolts;    // Laatste meting (gemiddeld), na ADC-kalibratie.
			State() : pressedMask(0), code(0), millivolts(0) {}
		};

		static const int   NOF_SAMPLES_PER_POLL = 32;
		static const int   NOF_BOOT_POLLS       = 8;       // Zoveel metingen middelen voor de eerste kalibratie.
		static constexpr float VREF_TRACKING    = 0.01f;   // IIR-factor per meting voor het volgen van Vref.
		static constexpr float VREF_BOOT_WINDOW = 0.08f;   // Eerste kalibratie alleen binnen +/-8% van de opgegeven Vref.

		const char*  taskName;
		TaskHandle_t taskHandle;

		uint8_t  pinAdc;
		uint32_t vrefMillivolts;     // Opgegeven (nominale) Vref.
		uint32_t pollPeriodMs;
		uint32_t nofStablePolls;
		float    vrefEstimateMv;     // Gekalibreerde Vref zoals de ADC hem ziet.

		ADC_buttons_E12_ladder ladder;

		IADC_buttons_E12Listener* arListeners[MAXLISTENERCOUNT] = {};
		uint16_t nofListeners;

		QueueHandle_t stateQueue;    // Pool: een queue met één plek, overschreven en gepeekt.

		adc_oneshot_unit_handle_t hAdcUnit;
		adc_channel_t             adcChannel;
		adc_cali_handle_t         hAdcCali;
		bool                      bCalibrated;

	public:
		// pinAdc:         de ADC-pin waar Uuit van de ladder op zit. Op de CYD is dat GPIO35 (connector P3).
		// vrefMillivolts: de spanning waarop de ladder (Vref in het schema) is aangesloten.
		//                 Alleen een startwaarde: de taak kalibreert hem bij het opstarten.
		// pollPeriodMs:   hoe vaak de ADC wordt uitgelezen.
		// nofStablePolls: zoveel metingen achter elkaar moet een nieuwe combinatie gelijk blijven
		//                 voordat hij geldt (debounce: nofStablePolls * pollPeriodMs).
		// maxSimultaneousButtons: hoeveel knoppen er hooguit tegelijk ingedrukt worden (1..6).
		//                 Hoe kleiner, hoe groter de marges (zie frt_ADC_buttons_E12_ladder.h).
		//                 Worden er toch meer ingedrukt, dan meldt de klasse de dichtstbijzijnde
		//                 combinatie die wel mag - en dat is dan een verkeerde.
		ADC_buttons_E12(const char* taskName, UBaseType_t taskPriority, uint32_t taskStackSizeBytes, BaseType_t taskCoreNumber,
		                uint8_t pinAdc = 35, uint32_t vrefMillivolts = 3300,
		                uint32_t pollPeriodMs = 10, uint32_t nofStablePolls = 3, int maxSimultaneousButtons = 3) :
			taskName(taskName), taskHandle(nullptr),
			pinAdc(pinAdc), vrefMillivolts(vrefMillivolts), pollPeriodMs(pollPeriodMs), nofStablePolls(nofStablePolls),
			vrefEstimateMv((float)vrefMillivolts), ladder(maxSimultaneousButtons),
			nofListeners(0), hAdcUnit(nullptr), adcChannel(ADC_CHANNEL_0), hAdcCali(nullptr), bCalibrated(false)
		{
			stateQueue = xQueueCreate(1, sizeof(State));
			configASSERT(stateQueue != NULL);
			const State initialState;
			xQueueOverwrite(stateQueue, &initialState);

			xTaskCreatePinnedToCore(taskWrapper, taskName, taskStackSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
		}

		// Voeg listeners toe vóórdat de taak meet, dus vanuit een constructor of setup().
		void addButtonListener(IADC_buttons_E12Listener* pListener) /*override*/
		{
			if (!isAlreadyPresent(pListener))
			{
				assert(nofListeners < MAXLISTENERCOUNT);
				arListeners[nofListeners++] = pListener;
			}
		}

		// Thread-safe. buttonIndex 0..5 (knop 0 = S1).
		bool isButtonPressed(int buttonIndex) /*override*/
		{
			if (buttonIndex < 0 || buttonIndex >= NOF_BUTTONS) return false;
			State state;
			xQueuePeek(stateQueue, &state, 0);
			return (state.pressedMask & (1 << buttonIndex)) != 0;
		}

		// Thread-safe. Bit i = knop i ingedrukt.
		uint8_t getPressedMask() /*override*/
		{
			State state;
			xQueuePeek(stateQueue, &state, 0);
			return state.pressedMask;
		}

		// Thread-safe. Handig om de ladder na te meten.
		uint16_t getMillivolts() /*override*/
		{
			State state;
			xQueuePeek(stateQueue, &state, 0);
			return state.millivolts;
		}

	private:
		static void taskWrapper(void* arg)
		{
			static_cast<ADC_buttons_E12*>(arg)->main();
		}

		bool isAlreadyPresent(IADC_buttons_E12Listener* pListener)
		{
			for (int i = 0; i < nofListeners; i++)
			{
				if (arListeners[i] == pListener) return true;
			}
			return false;
		}

		void initAdc()
		{
			adc_unit_t adcUnit;
			ESP_ERROR_CHECK(adc_oneshot_io_to_channel(pinAdc, &adcUnit, &adcChannel));

			adc_oneshot_unit_init_cfg_t unitConfig = {};
			unitConfig.unit_id  = adcUnit;
			unitConfig.ulp_mode = ADC_ULP_MODE_DISABLE;
			ESP_ERROR_CHECK(adc_oneshot_new_unit(&unitConfig, &hAdcUnit));

			adc_oneshot_chan_cfg_t channelConfig = {};
			channelConfig.atten    = ADC_ATTEN_DB_12;   // Bereik tot ~3,1 V.
			channelConfig.bitwidth = ADC_BITWIDTH_12;
			ESP_ERROR_CHECK(adc_oneshot_config_channel(hAdcUnit, adcChannel, &channelConfig));

			// De ESP32 kent alleen line fitting; nieuwere chips curve fitting.
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
			adc_cali_curve_fitting_config_t caliConfig = {};
			caliConfig.unit_id  = adcUnit;
			caliConfig.chan     = adcChannel;
			caliConfig.atten    = ADC_ATTEN_DB_12;
			caliConfig.bitwidth = ADC_BITWIDTH_12;
			bCalibrated = (adc_cali_create_scheme_curve_fitting(&caliConfig, &hAdcCali) == ESP_OK);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
			adc_cali_line_fitting_config_t caliConfig = {};
			caliConfig.unit_id  = adcUnit;
			caliConfig.atten    = ADC_ATTEN_DB_12;
			caliConfig.bitwidth = ADC_BITWIDTH_12;
	#if CONFIG_IDF_TARGET_ESP32
			caliConfig.default_vref = 1100;              // Alleen gebruikt als de eFuse geen Vref bevat.
	#endif
			bCalibrated = (adc_cali_create_scheme_line_fitting(&caliConfig, &hAdcCali) == ESP_OK);
#endif
			if (!bCalibrated)
			{
				ESP_LOGW(taskName, "Geen ADC-kalibratie beschikbaar; de knoppen worden onbetrouwbaar herkend.");
			}
		}

		// Meet één keer (gemiddeld over NOF_SAMPLES_PER_POLL samples).
		// Geeft false als de ADC niet gelezen kon worden.
		bool measure(int& millivolts)
		{
			int32_t sum = 0;
			for (int i = 0; i < NOF_SAMPLES_PER_POLL; i++)
			{
				int raw = 0;
				if (adc_oneshot_read(hAdcUnit, adcChannel, &raw) != ESP_OK) return false;
				sum += raw;
			}
			int rawAverage = (sum + NOF_SAMPLES_PER_POLL / 2) / NOF_SAMPLES_PER_POLL;

			if (!bCalibrated || adc_cali_raw_to_voltage(hAdcCali, rawAverage, &millivolts) != ESP_OK)
			{
				millivolts = (rawAverage * 3100) / 4095;   // Grove schatting zonder kalibratie.
			}
			return true;
		}

		// Ga ervan uit dat er bij het opstarten niets is ingedrukt, en leid Vref af uit
		// het niveau "geen knop". Ligt de meting te ver van wat er bij de opgegeven Vref
		// hoort, dan klopt die aanname (of de bedrading) niet, en blijft de opgegeven Vref staan.
		void calibrateAtBoot()
		{
			int32_t sum = 0;
			int n = 0;
			for (int i = 0; i < NOF_BOOT_POLLS; i++)
			{
				int millivolts = 0;
				if (measure(millivolts))
				{
					sum += millivolts;
					n++;
				}
				vTaskDelay(pdMS_TO_TICKS(pollPeriodMs));
			}
			if (n == 0) return;

			float idleMv        = (float)sum / (float)n;
			float vrefFromIdle  = idleMv / ladder.getLevel(0);
			float relativeError = vrefFromIdle / (float)vrefMillivolts - 1.0f;
			if (relativeError > -VREF_BOOT_WINDOW && relativeError < VREF_BOOT_WINDOW)
			{
				vrefEstimateMv = vrefFromIdle;
				ESP_LOGI(taskName, "Kalibratie: geen knop = %d mV, Vref = %d mV (verwacht %d mV, niveau 'geen knop' %d mV)",
				         (int)idleMv, (int)vrefEstimateMv, (int)vrefMillivolts, (int)(ladder.getLevel(0) * vrefMillivolts));
			}
			else
			{
				ESP_LOGW(taskName, "Kalibratie overgeslagen: geen knop = %d mV, verwacht rond %d mV. Zit RL (zie frt_ADC_buttons_E12_ladder.h) erin, en is er bij het opstarten niets ingedrukt?",
				         (int)idleMv, (int)(ladder.getLevel(0) * vrefMillivolts));
			}
		}

		void notifyChanges(uint8_t prevMask, uint8_t newMask)
		{
			uint8_t changed = prevMask ^ newMask;
			// Eerst de losgelaten knoppen, dan de ingedrukte.
			for (int b = 0; b < NOF_BUTTONS; b++)
			{
				if ((changed & (1 << b)) && !(newMask & (1 << b)))
				{
					for (int i = 0; i < nofListeners; i++) arListeners[i]->buttonReleased(b);
				}
			}
			for (int b = 0; b < NOF_BUTTONS; b++)
			{
				if ((changed & (1 << b)) && (newMask & (1 << b)))
				{
					for (int i = 0; i < nofListeners; i++) arListeners[i]->buttonPressed(b);
				}
			}
		}

		void main()
		{
			initAdc();
			calibrateAtBoot();   // Duurt NOF_BOOT_POLLS * pollPeriodMs; geeft de listeners ook de tijd zich aan te melden.

			State state;
			uint8_t  candidateCode  = 0;   // Laatst gemeten code.
			uint32_t candidateCount = 0;   // Zoveel metingen achter elkaar is hij al gelijk.

			while (true)
			{
				int millivolts = 0;
				if (measure(millivolts))
				{
					uint8_t code = ladder.decode((float)millivolts / vrefEstimateMv);

					if (code == candidateCode)
					{
						if (candidateCount < nofStablePolls) candidateCount++;
					}
					else
					{
						candidateCode  = code;
						candidateCount = 1;
					}
					bool bStable = (candidateCount >= nofStablePolls);

					// Vref volgen zolang er stabiel niets is ingedrukt.
					if (bStable && candidateCode == 0)
					{
						float vrefFromIdle = (float)millivolts / ladder.getLevel(0);
						vrefEstimateMv += VREF_TRACKING * (vrefFromIdle - vrefEstimateMv);
					}

					state.millivolts = (uint16_t)millivolts;

					uint8_t prevMask = state.pressedMask;
					bool bNewStableCode = bStable && (candidateCode != state.code);
					if (bNewStableCode)
					{
						state.code        = candidateCode;
						state.pressedMask = ADC_buttons_E12_ladder::codeToButtonMask(candidateCode);
					}

					// Eerst stateQueue bijwerken, dan pas de listeners: wie in buttonPressed
					// isButtonPressed() aanroept, ziet zo al de nieuwe toestand.
					xQueueOverwrite(stateQueue, &state);

					if (bNewStableCode)
					{
						notifyChanges(prevMask, state.pressedMask);
					}
				}
				vTaskDelay(pdMS_TO_TICKS(pollPeriodMs));
			}
		}
	}; // end class ADC_buttons_E12
}; // end namespace frt

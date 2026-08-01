// by Marius Versteegen, 2023

#pragma once
#include "crt_CleanRTOS.h"
#include "crt_IButton.h"
#include "crt_ILogger.h"
#include <crt_ITouchListener.h>

// Expliciet, en niet leunen op wie ons includeert: hieronder hangt van
// TOUCH_THRESHOLD uit User_Setup.h af hoe gevoelig het scherm reageert. Was die
// hier niet zichtbaar, dan koos de #ifdef stilzwijgend de standaard van 600 en
// zou de instelling alsnog niet aankomen.
#include <TFT_eSPI.h>

// for testing only (requires presence of a global logger object):
// #define TEST_TOUCHDETECTOR

namespace crt
{
	//extern ILogger& logger;

	// Don't select the MaxTouchListenerCount too high, or you may suffer from stack overflow troubles.
	template<size_t MaxTouchListenerCount> class TFT_eSPI_TouchDetector
	{
	private:
		// De drempel uit update(). Komt uit User_Setup.h als die er staat, met de
		// standaard van TFT_eSPI als terugval.
#ifdef TOUCH_THRESHOLD
		static const uint16_t PressThreshold = TOUCH_THRESHOLD;
#else
		static const uint16_t PressThreshold = 600;
#endif
		// Bewust GEEN lagere drempel voor het vasthouden. Dat heeft hier gestaan,
		// als hysterese tegen een vinger die in druk varieert, en het was een
		// fout: in rust leest Z op dit paneel 25 tot 80, en af en toe levert die
		// ruis twee identieke uitlezingen op die ook de positiecontrole halen.
		// Met een lagere drempel voor "nog steeds ingedrukt" bleef een aanraking
		// daardoor hangen op ruis - geen nieuwe press-events meer, tot de ruis
		// toevallig twee keer op rij onder de drempel zakte. Dat is het "soms een
		// halve minuut lang geen reactie".
		//
		// Een drempel voor vasthouden moet dus net zo goed boven de ruisvloer
		// liggen als die voor aanraken, en dan is er niets meer te winnen: de
		// druk was nooit het knelpunt (gemeten 1700-2500 bij elke echte
		// aanraking). Wat wel helpt tegen losse uitvallers is de misser-teller
		// hieronder, en die kijkt niet naar druk.

		// Hoeveel mislukte metingen op rij nodig zijn voordat een aanraking als
		// losgelaten geldt. Zie de uitleg in update(); twee is genoeg voor de
		// losse uitvallers die daar gemeten zijn.
		static const int NofMissesBeforeRelease = 2;

		const uint64_t infiniteBatchSizeUs = 1000000000000; // 1e6 s means: infinite: no limitation in batchsize.

		TFT_eSPI& _tft;
		bool bPrevPressed;
		int nofConsecutiveMisses;		// zie NofMissesBeforeRelease
		ITouchListener* arTouchListeners[MaxTouchListenerCount] = {}; // This intializes all to 0.
		uint16_t nofTouchListeners;
		uint16_t t_x, t_y; // To store the touch coordinates
		Vec2 posLastPress;
		uint64_t batchSizeUs;

	public:
		// Make sure that the update period of the handler that calls update() is about 70ms:
		// Slow enough for debounce, fast enough for response.
		// By default, allow an infinite batch size.
		TFT_eSPI_TouchDetector(TFT_eSPI& tft) :
			_tft(tft), bPrevPressed(false), nofConsecutiveMisses(0), nofTouchListeners(0), t_x(9999), t_y(9999), batchSizeUs(1000000000000)
		{
		}

		// Typically, if many buttons are directly assigned to the TouchDetector,
		// it may be a good idea to limit batchSizeUs, such that tasks of equal priority
		// also get their turns without waiting very long.
		TFT_eSPI_TouchDetector(TFT_eSPI& tft, uint64_t batchSizeUs) :
			_tft(tft), bPrevPressed(false), nofConsecutiveMisses(0), nofTouchListeners(0), t_x(9999), t_y(9999), batchSizeUs(batchSizeUs)
		{
		}

		/*override*/
		void addTouchListener(ITouchListener* pTouchListener)
		{
			if (!isAlreadyPresent(pTouchListener))
			{
				assert(nofTouchListeners < MaxTouchListenerCount);
				arTouchListeners[nofTouchListeners++] = pTouchListener;
			}
		}

		/*override*/
		bool isTouched()
		{
			return bPrevPressed;
		}

		// It is assumed that this update function is called by a handler once in about 70ms: slow enough for debounce, fast enough for response.
		/*override keyword not supported in current compiler*/
		void update(void* pCallingTask)
		{
			// De drempel is de druk (Z) waarboven een aanraking meetelt. Met de
			// waarde uit User_Setup.h, net als de kalibratie in
			// crt_TFT_eSPI_DisplayAdapter.h - zonder die parameter valt TFT_eSPI
			// terug op 600, terwijl dit bord er 200 opgeeft, en dan reageert de
			// kalibratie op een tikje terwijl de knoppen daarna drie keer zo hard
			// geduwd willen worden.
			//
			// Een aparte, lagere drempel voor het vasthouden staat hier bewust
			// niet meer; zie de uitleg bij PressThreshold.
			bool bMeasuredPressed = _tft.getTouch(&t_x, &t_y, PressThreshold);

			// Een enkele mislukte meting is nog geen losgelaten vinger. Gemeten
			// tijdens vijf seconden stil vasthouden: tussen metingen van z=1435,
			// 1545, 1585 en 1647 zit er ongeveer een op de vijf met z=6 en een
			// ruwe x van 0 - de aanraking is dan niet zwakker, de uitlezing is
			// simpelweg niet gelukt. Geen drempel vangt dat, en dat is precies
			// waarom draaien aan TOUCH_THRESHOLD (200, 100, 10) niets uithaalde.
			//
			// Vandaar loslaten pas na een paar missers op rij. Dat kost bij het
			// echte loslaten NofMissesBeforeRelease keer de pollperiode extra,
			// dus houd het klein: bij 50 ms polling is twee missers 100 ms, wat
			// je bij een knop niet merkt.
			if (bMeasuredPressed)
			{
				nofConsecutiveMisses = 0;
			}
			else if (bPrevPressed)
			{
				nofConsecutiveMisses++;
			}

			// Zolang de missers op zijn, blijven we geloven dat de vinger er ligt.
			bool bPressed = bMeasuredPressed
			             || (bPrevPressed && (nofConsecutiveMisses < NofMissesBeforeRelease));

			if (bPressed)
			{
				if (bMeasuredPressed)
				{
					// Alleen een geslaagde meting levert een positie op; die van een
					// misser is rommel (ruwe x van 0) en zou de knop verspringen.
					posLastPress.set(t_x, t_y);
				}
				if (!bPrevPressed)
				{
					doUpdatePressedOrReleased(true, pCallingTask); // true means (just-) pressed.
				}
			}
			else if (bPrevPressed)
			{
				nofConsecutiveMisses = 0;
				doUpdatePressedOrReleased(false, pCallingTask); // false means (just-) released.
			}
			bPrevPressed = bPressed;
		}

	private:
		void doUpdatePressedOrReleased(bool bPressed, void* pCallingTask_in)
		{
#ifdef CleanRTOS_INCLUDED
			Task* pCallingTask = (Task*)pCallingTask_in;
#else
			void* pCallingTask = pCallingTask_in;
#endif

#ifdef TEST_TOUCHDETECTOR
			uint64_t before2 = esp_timer_get_time();	// For measuring of actual complete period.
			uint64_t after2 = 0;
			uint64_t diff2 = 0;
			int32_t  nofYieldsDone = 0;
			uint64_t yieldTimeSpent = 0;
			uint64_t beforeBatches = 0;
			uint64_t spentOnBatches = 0;
			uint64_t beforeAll = esp_timer_get_time();
			uint64_t afterAll = 0;	// gross Time spent by Handler, including yields to other threads.
#endif
			uint64_t beforeBatch = 0;
			uint64_t batchTimeSpent = 0;

			beforeBatch = esp_timer_get_time();

#ifdef TEST_TOUCHDETECTOR
			beforeAll = esp_timer_get_time();
			uint32_t diffAll = 0;
			yieldTimeSpent = 0;
			nofYieldsDone = 0;
			beforeBatches = beforeBatch;
#endif				

			for (int i = 0; i < nofTouchListeners; i++)
			{
				// Do the prober update:
				if (bPressed)
				{
					arTouchListeners[i]->touchPressed(posLastPress);
				}
				else
				{
					arTouchListeners[i]->touchReleased(posLastPress);
				}

				if (batchSizeUs != infiniteBatchSizeUs)
				{
					uint64_t nowUs = esp_timer_get_time();
					batchTimeSpent = (nowUs - beforeBatch);

					if (batchTimeSpent > batchSizeUs)
					{
						taskYIELD();

#ifdef TEST_TOUCHDETECTOR
						yieldTimeSpent += (esp_timer_get_time() - nowUs);
						nofYieldsDone++;
#endif
						beforeBatch = esp_timer_get_time();
					}
				}
			}
			
			// This is the time spent on batches,
			// yields and optionally the test code
			// within the #defines below.
			//if (diffAll >= periodUs)
			//{
			//	ESP_LOGI("Handler Delay too long:", "%d us", diffAll);
			//}

#ifdef TEST_TOUCHDETECTOR
			afterAll = esp_timer_get_time();     // time in micro seconds.
			diffAll = (afterAll - beforeAll);    // difference in milliseconds
			after2 = esp_timer_get_time();
			diff2 = (uint32_t)(after2 - before2);
			before2 = after2;

			spentOnBatches = afterAll - beforeBatches;

			logger.logText("-----------------------------------------------------");
			logger.logText(Task::taskName);
			logger.logText("Diagnostics:");
			logger.logText("nofYieldsDone:");
			logger.logUint32(nofYieldsDone);
			logger.logText("gross time spent on batches in microsecs, including yields:");
			logger.logUint32((int32_t)spentOnBatches);
			logger.logText("nett time spent on batches in microsecs:");
			logger.logUint32((int32_t)(spentOnBatches - yieldTimeSpent));
			logger.logText("handler interval microsecs");
			logger.logUint32((int32_t)diff2);
#endif

			if (pCallingTask != nullptr)
			{
				pCallingTask->dumpStackHighWaterMarkIfIncreased();
			}
		}

		bool isAlreadyPresent(ITouchListener* pTouchListener)
		{
			for (int i = 0; i < nofTouchListeners; i++)
			{
				//ESP_LOGI("adasdffasfsadf", "%s", i);
				if (arTouchListeners[i] == pTouchListener)
				{
					return true;
				}
			}
			return false;
		}
	}; // end class Button
}; // end namespace CleanRTOS

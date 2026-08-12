// by Marius Versteegen, 2026

#pragma once
#include <Arduino.h>
#include <crt_CleanRTOS.h>
#include <crt_TFT_eSPI_DisplayAdapter.h>
#include <crt_TFT_eSPI_FreeFonts.h>

// Kalibreert het aanraakscherm voor alle vier de schermrotaties achter elkaar.
//
// Waarom dit voorbeeld bestaat: elke app en elk voorbeeld kiest zijn eigen
// rotatie - portret, landschap, of een van beide op zijn kop. De kalibratie
// werd tot nu toe per rotatie bewaard maar er paste er maar een in het
// bestand, dus wie een app in een andere rotatie startte moest eerst weer vier
// hoeken aantikken, en gooide daarmee de kalibratie van de vorige rotatie weg.
// Achter elkaar door de voorbeelden lopen was daardoor niet te doen.
//
// Draai dit voorbeeld een keer, tik viermaal vier hoeken aan, en daarna vindt
// elke app zijn eigen rotatie kant en klaar in het bestand terug. Het overleeft
// een gewone flash; alleen een `idf.py erase_flash` wist de spiffs-partitie
// waar het in leeft.
//
// Bewust zonder AsyncDisplay en zonder widgets: de kalibratie tekent
// rechtstreeks op het paneel en draait voordat er een widgetboom is. Daarom ook
// de RGB565-adapter en niet de PAL8-variant - die zou per rotatie een
// backbuffer van een andere maat nodig hebben, en er valt hier niets te bufferen.

namespace crt
{
	class TestTouchCalibrateAll : public Task
	{
		static const int MaxNofTouchListeners = 4;	// er luistert er hier geen een

		using DISPLAYADAPTER = TFT_eSPI_DisplayAdapter_RGB565<MaxNofTouchListeners>;

	private:
		TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;	// niet gebruikt; de adapter wil er wel een
		DISPLAYADAPTER tft_eSPI_DisplayAdapter;

	public:
		TestTouchCalibrateAll(const char* taskName, unsigned int taskPriority, unsigned int taskCoreNumber) :
			Task(taskName, taskPriority,
				10000 + sizeof(freeFonts) + sizeof(tft_eSPI_DisplayAdapter), taskCoreNumber),

			// bRepeatCalibration doet er hier niet toe: touchCalibrateAllRotations
			// kalibreert sowieso alle vier, dat is de hele opzet van dit voorbeeld.
			tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, true/*bRepeatCalibration*/)
		{
			start(); // For simplicity, the task is started right away in it's constructor.
		}

	private:
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			ESP_LOGI(taskName, "kalibratie van alle vier de schermrotaties - tik de hoeken aan zoals aangegeven");

			tft_eSPI_DisplayAdapter.touchCalibrateAllRotations(2/*font*/);

			ESP_LOGI(taskName, "klaar. Alle vier de rotaties staan nu in /TouchCalData1.");
			ESP_LOGI(taskName, "Flash nu je eigen app; die vraagt niet opnieuw om kalibratie.");

			while (true)
			{
				vTaskDelay(1000);
			}
		}
	}; // end class TestTouchCalibrateAll
};// end namespace crt

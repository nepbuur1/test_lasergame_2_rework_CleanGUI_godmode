// by Marius Versteegen, 2026

#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <crt_CleanGUI.h>

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

#include "images/_pal8/_allImages.h"
#include "psupf_StartPageDisplayControl.h"	// voor PlayButton

// De DisplayControl van de knoppen-infopagina: de allereerste pagina na het opstarten.
// Hij lijkt op de startpagina (titel bovenaan, knop onderaan), maar zonder fakkels, met
// een paar regels tekst over het externe toetsenbord (de weerstandsladder op GPIO35), en
// met "Ga verder" op de knop. Eigen PageRoot en eigen taak, net als de andere pagina's;
// zie psupf_GameDisplayControl.h voor waarom het verbergen niets tekent.

using namespace crt;
namespace pong_super_frt
{
	class ButtonInfoDisplayControl
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitShowButtonInfoPage = (1 << 0);
		static constexpr EventBits_t eventBitHideButtonInfoPage = (1 << 1);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()
		TaskHandle_t taskHandle;

		enum State
		{
			STATE_HIDDEN,
			STATE_SHOWN
		};

		static const int NofInfoLines       = 4;
		static const int InfoFont           = 2;		// 16 pixels hoog
		static const int InfoTopPix         = 100;		// onder de titel, die op y=16 begint en 72 hoog is
		static const int InfoLineHeightPix  = 20;

		static const int ContinueButtonWidthPix  = 160;
		static const int ContinueButtonHeightPix = 44;
		static const int ContinueButtonFont      = 4;	// 26 pixels hoog

		// De tekst per geval, al in regels gebroken: een Text is een enkele regel, en de
		// zinnen zijn te lang voor de 320 pixels van het scherm.
		static constexpr const char* arConnectedLines[NofInfoLines] =
		{
			"Extern 6 button ADC_Buttons",
			"keyboard detected.",
			"You may use its first two buttons",
			"for moving left and right."
		};
		static constexpr const char* arNotConnectedLines[NofInfoLines] =
		{
			"For optimal framerate, connect",
			"an extern 6 button ADC_Buttons",
			"keyboard and reboot.",
			""
		};

	private:
		IDisplay& display;
		TFT_eSPI_IFreeFonts& freeFonts;

		int screenWidth;
		int screenHeight;

		// Schermvullend, net als bij de andere pagina's: wie in beeld komt wist wat de
		// vorige achterliet.
		Panel<0> backgroundPanel;
		Panel<0> titlePanel;
		Text<0> arInfoLines[NofInfoLines];
		PlayButton buttonContinue;

		// Welke tekst er moet staan. Wordt door een andere taak gezet (via
		// showButtonInfoPage), en alleen door deze taak in de Texts gezet.
		QueueHandle_t connectedQueue;		// lengte 1: was Pool<bool> poolConnected


		State state;

	public:
		ButtonInfoDisplayControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber,
							IDisplay& display, TFT_eSPI_IFreeFonts& freeFonts) :
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			taskHandle(nullptr),
			display(display),
			freeFonts(freeFonts),
			screenWidth(0),
			screenHeight(0),
			state(STATE_HIDDEN)
		{
			connectedQueue = xQueueCreate(1, sizeof(bool));
			{ bool initValue{}; xQueueOverwrite(connectedQueue, &initValue); }
			Vec2 screenSize = display.getScreenSize();
			screenWidth  = screenSize.x;
			screenHeight = screenSize.y;

			{ bool poolValue = false; xQueueOverwrite(connectedQueue, &poolValue); }
			display.addTouchListener(&buttonContinue);

			xTaskCreatePinnedToCore(staticMain, taskName, taskSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
		}

	IButton* addButtonInfoContinueButtonListener(IButtonListener* pButtonListener)
	{
		buttonContinue.addButtonListener(pButtonListener);
		return &buttonContinue;
	}

	// bConnected: of het externe toetsenbord (de weerstandsladder) is aangesloten.
	// Bepaalt welke tekst er op de pagina staat.
	void showButtonInfoPage(bool bConnected)
	{
		{ bool poolValue = bConnected; xQueueOverwrite(connectedQueue, &poolValue); }
		xEventGroupClearBits(eventGroup, eventBitHideButtonInfoPage);	// alleen de laatste wens telt.
		xEventGroupSetBits(eventGroup, eventBitShowButtonInfoPage);
	}

	void hideButtonInfoPage()
	{
		xEventGroupClearBits(eventGroup, eventBitShowButtonInfoPage);
		xEventGroupSetBits(eventGroup, eventBitHideButtonInfoPage);
	}

	private:
		void initPanels()
		{
			backgroundPanel.setPanelProps("buttonInfoBackground", /*locPos*/Vec2(0, 0), CoordType::Pixels,
				/*size*/Vec2(screenWidth, screenHeight), /*cornerRadius*/0, CoordType::Pixels,
				Alignment::TopLeft, /*colFg*/0x00000000, /*colBg*/0x00000000);

			// Dezelfde titel als op de startpagina, maar hoger: de tekst eronder heeft
			// meer ruimte nodig dan het regeltje van de startpagina.
			Vec2 titleSizePix(imageTitleSuperPong.width, imageTitleSuperPong.height);
			titlePanel.setPanelProps("buttonInfoTitlePanel",
				/*locPos*/Vec2((screenWidth - titleSizePix.x)/2, 16), CoordType::Pixels,
				/*size*/titleSizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colFg*/0x0000FFFF, /*colBg*/0x00000000);
			titlePanel.setImage(&imageTitleSuperPong);

			// TopMid: met locPos (0, y) staat de regel horizontaal gecentreerd op hoogte y.
			// Zie Text::calcOriginPix.
			for (int i = 0; i < NofInfoLines; i++)
			{
				arInfoLines[i].setTextProps
					(/*locPos*/Vec2(0, InfoTopPix + i*InfoLineHeightPix), CoordType::Pixels, Alignment::TopMid,
					"", InfoFont, /*color*/0x0000FFFF, /*fontScale*/1);
			}

			initContinueButton();
		}

		// Een gewone knop met tekst, zonder plaatje: "Ga verder" staat niet in een van de
		// knopplaatjes. Alles in pixels, zie StartPageDisplayControl::initPlayButton.
		void initContinueButton()
		{
			Vec2 sizePix(ContinueButtonWidthPix, ContinueButtonHeightPix);
			int32_t topPix = screenHeight - ContinueButtonHeightPix - 12;

			buttonContinue.setTouchScreenButtonProps
				("continue",/*locPos*/Vec2((screenWidth - sizePix.x)/2, topPix), CoordType::Pixels,
				/*size*/sizePix, /*cornerRadius*/0, CoordType::Pixels,
				Alignment::MidMid, /*colPanel*/0x00FF0000, /*colBg*/0x00000000,
				/*bInvertedArea*/false, "Ga verder", /*buttonFont*/ ContinueButtonFont, /*colFont*/ 0x00FFFFFF,
				/*fontScale*/ 1, Alignment::MidMid,
				/*lowerCaseOffsetY*/0, /*upperCaseOffsetY*/0);
		}

		void fillInfoLines()
		{
			bool bConnected = false;
			xQueuePeek(connectedQueue, &bConnected, 0);
			const char* const* arLines = bConnected ? arConnectedLines : arNotConnectedLines;
			for (int i = 0; i < NofInfoLines; i++)
			{
				arInfoLines[i].setText(arLines[i]);
			}
		}

		// Zie StartPageDisplayControl::showPage/hidePage: dezelfde afspraak, voor alle pagina's.
		void showPage(IWidget& page)
		{
			page.makeDrawable(true);
			page.redraw(true);
			page.enable(true);
		}

		void hidePage(IWidget& page)
		{
			page.makeNonDrawable(true);
			page.disable(true);
		}

		void defineDisplayHierarchy(IWidget& pageRoot)
		{
			pageRoot.addChildWidget(backgroundPanel);	// eerst, want alles hierna tekent erop.
			pageRoot.addChildWidget(titlePanel);
			for (int i = 0; i < NofInfoLines; i++)
			{
				pageRoot.addChildWidget(arInfoLines[i]);
			}
			pageRoot.addChildWidget(buttonContinue);
		}


		// --- FreeRTOS-hulpfuncties ---
		static void staticMain(void* pvParameters)
		{
			static_cast<ButtonInfoDisplayControl*>(pvParameters)->main();
		}

		// Zoals CleanRTOS' hasFired(): kijkt of het bit bij de laatste wachtoproep
		// gevuurd had, en wist alleen dat bit.
		bool hasFired(EventBits_t eventBit)
		{
			bool bFired = ((firedBits & eventBit) != 0);
			if (bFired)
			{
				xEventGroupClearBits(eventGroup, eventBit);
			}
			return bFired;
		}


		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			initPanels();

			PageRoot<10> pageButtonInfo("rootButtonInfoPageDisplay", display);
			defineDisplayHierarchy(pageButtonInfo);

			hidePage(pageButtonInfo);	// ButtonInfoControl vraagt hem zo meteen zelf op.

			while (true)
			{

				switch(state)
				{
					case STATE_HIDDEN:
						firedBits = xEventGroupWaitBits(eventGroup, eventBitShowButtonInfoPage, pdTRUE, pdFALSE, portMAX_DELAY);
						fillInfoLines();
						showPage(pageButtonInfo);
						state = STATE_SHOWN;
						break;

					case STATE_SHOWN:
						firedBits = xEventGroupWaitBits(eventGroup, eventBitHideButtonInfoPage, pdTRUE, pdFALSE, portMAX_DELAY);
						hidePage(pageButtonInfo);
						state = STATE_HIDDEN;
						break;

					default:
						assert(false);
				}
			}
		}
	}; // end class ButtonInfoDisplayControl

};// end namespace pong_super_frt

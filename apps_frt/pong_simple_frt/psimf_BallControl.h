// by Marius Versteegen, 2024

#pragma once
#include <stdlib.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <crt_Button.h>

#include "psimf_DisplayControl.h"
#include "psimf_PaddleControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

// Testschakelaar, normaal uit. Aan: de bal begint vlak boven de doellijn en is
// binnen een halve seconde weg, zodat de overgang naar het game-over-scherm
// honderden keren per minuut te beproeven is in plaats van twintig keer per
// kwartier. Zie initialiseRandomStartPos().
//#define PONG_FAST_GAME_OVER

using namespace crt;
namespace pong_simple_frt
{
	// Loggen gaat in deze FreeRTOS-variant met ESP_LOGI, niet met crt::logger.

    class IBallControlListener
    {

    public:
        virtual void ballMissedByPaddle() = 0;
    };

	class BallControl
	{

		// --- FreeRTOS (was CleanRTOS) ---
		static constexpr EventBits_t eventBitStart = (1 << 0);
		EventGroupHandle_t eventGroup;
		EventBits_t firedBits;		// wat de laatste wachtoproep op meer tegelijk opleverde; zie hasFired()
		TaskHandle_t taskHandle;
        enum State
        {
            STATE_STOPPED,
            STATE_RUNNING
        };

	private:
        DisplayControl& displayControl;
        PaddleControl& paddleControl;
        IBallControlListener* pBallControlListener;
        int screenWidth;
        int screenHeigth;
        int posX;
        int posY;
        int vX;
        int vY;
        State state;

	public:
		BallControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, DisplayControl& displayControl, PaddleControl& paddleControl) :
			eventGroup(xEventGroupCreate()),
			firedBits(0),
			taskHandle(nullptr),
            displayControl(displayControl),
            paddleControl(paddleControl),
            pBallControlListener(nullptr),
			screenWidth(displayControl.getWidth()),
            screenHeigth(displayControl.getHeight()),
            posX(screenWidth/2),
            posY(screenHeigth/2),
            vX(1),
            vY(1),
            state(STATE_STOPPED)
		{
            srand((unsigned int)esp_timer_get_time());  // anders elke boot dezelfde "willekeurige" richting.
			xTaskCreatePinnedToCore(staticMain, taskName, taskSizeBytes, this, taskPriority, &taskHandle, taskCoreNumber);
		}

        void setBallControlListener(IBallControlListener* pBallControlListener)
        {
            this->pBallControlListener = pBallControlListener;
        }

        void startBall()
        {
            xEventGroupSetBits(eventGroup, eventBitStart);
        }

	private:
        // posX/posY zijn het MIDDEN van de bal, in speelveldpixels met de
        // oorsprong linksonder. Alles hieronder rekent met de straal erbij, want
        // een bal van 12 pixels kaatst met zijn rand tegen de wand, niet met zijn
        // middelpunt.
        static const int ballRadius = BallSizePix/2;

        void initialiseRandomStartPos()
        {
#ifdef PONG_FAST_GAME_OVER
            // De bal begint vlak boven de doellijn, helemaal links, en gaat
            // meteen naar beneden. De peddel begint in het midden en
            // haalt dat niet, dus is het na een handvol beelden game over.
            //
            // Bedoeld voor het beproeven van die overgang, waar drie taken
            // tegelijk gaan tekenen en het dus stuk kan gaan.
            //
            // vX = 0 houdt hem bij de linkerwand vandaan: bij een botsing tegen
            // die wand zou hij naar rechts wegdraaien en alsnog bij de peddel
            // kunnen uitkomen.
            posX = WallThicknessPix + ballRadius;
            posY = PaddleHeightPix + ballRadius + 2*BallSpeedPixPerFrame;
            vX = 0;
            vY = -BallSpeedPixPerFrame;
#else
            posX = screenWidth/2;
            posY = screenHeigth/2;
            vX = ((rand()%2)*2-1) * BallSpeedPixPerFrame;
            vY = BallSpeedPixPerFrame;
#endif

            // Meteen doorgeven, anders staat de bal het eerste beeld van een
            // nieuw spel nog op de plek waar het vorige spel verloren ging.
            displayControl.updateBallPosition(posX, posY);
        }

        void updateBallPosition()
        {
            posX += vX;
            posY += vY;

            // Tegen een wand aan: terugzetten op de rand en de richting omkeren.
            // Terugzetten hoort erbij - zonder dat kan de bal een stap in de wand
            // eindigen en er bij de volgende stap nog dieper in lopen, want de
            // snelheid is groter dan een pixel.
            if (posX - ballRadius <= WallThicknessPix)
            {
                posX = WallThicknessPix + ballRadius;
                vX = (vX < 0) ? -vX : vX;
            }
            else if (posX + ballRadius >= screenWidth - WallThicknessPix)
            {
                posX = screenWidth - WallThicknessPix - ballRadius;
                vX = (vX > 0) ? -vX : vX;
            }

            if (posY + ballRadius >= screenHeigth - WallThicknessPix)
            {
                posY = screenHeigth - WallThicknessPix - ballRadius;
                vY = (vY > 0) ? -vY : vY;
            }

            // De peddel: alleen als de bal daalt, en alleen als hij binnen de
            // breedte van de peddel valt.
            int paddlePosition = paddleControl.getPaddlePos();
            
            if ((vY < 0) && (posY - ballRadius <= PaddleHeightPix)
                && (posX >= paddlePosition - PaddleWidthPix/2 - ballRadius)
                && (posX <= paddlePosition + PaddleWidthPix/2 + ballRadius))
            {
                posY = PaddleHeightPix + ballRadius;
                vY = -vY;

                // Waar op de peddel hij landt bepaalt de hoek: bij de rand loopt
                // hij scherper weg dan in het midden. Zonder dit blijft de bal
                // eeuwig onder 45 graden heen en weer gaan en valt er niets te
                // spelen.
                int offsetFromCentre = posX - paddlePosition;
                vX = (offsetFromCentre * 2 * BallSpeedPixPerFrame) / (PaddleWidthPix/2);
                if (vX == 0)
                {
                    vX = (offsetFromCentre < 0) ? -1 : 1;	// nooit helemaal recht omhoog.
                }
            }

            displayControl.updateBallPosition(posX, posY);
        }


		// --- FreeRTOS-hulpfuncties ---
		static void staticMain(void* pvParameters)
		{
			static_cast<BallControl*>(pvParameters)->main();
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

			while (true)
			{

				switch (state)
                {
                case STATE_STOPPED:
                    firedBits = xEventGroupWaitBits(eventGroup, eventBitStart, pdTRUE, pdFALSE, portMAX_DELAY);
                    initialiseRandomStartPos();
                    state = STATE_RUNNING;
                    break;

                case STATE_RUNNING:
                    if (posY + ballRadius <= 0)		// helemaal onder de doellijn door.
                    {
                        assert(pBallControlListener!=nullptr);
                        pBallControlListener->ballMissedByPaddle();
                        state = STATE_STOPPED;
                    }
                    else
                    {
                        updateBallPosition();
                        vTaskDelay(FrameIntervalMs);	// de beeldklok van het spel: 25 keer per seconde
                                                        // een stap van BallSpeedPixPerFrame pixels.
                    }
                    break;
                
                default:
                    break;
                }
			}
		}
	}; // end class BallControl

};// end namespace crt

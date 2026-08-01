// by Marius Versteegen, 2024

#pragma once
#include <stdlib.h>
#include <esp_timer.h>
#include <crt_CleanRTOS.h>
#include <crt_Button.h>
#include "crt_SceneDisplayControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

namespace crt
{
	extern ILogger& logger;

    class IBallControlListener
    {
    public:
        virtual void ballMissedByPaddle() = 0;
    };

	class BallControl : public Task
	{
        enum State
        {
            STATE_STOPPED,
            STATE_RUNNING
        };

	private:
        Flag flagStart;
        Pool<int> poolPaddlePosition;
        ISceneDisplayControl& sceneDisplayControl;
        IBallControlListener* pBallControlListener;
        int screenWidth;
        int screenHeigth;
        int posX;
        int posY;
        int vX;
        int vY;
        State state;

	public:
		BallControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, ISceneDisplayControl& sceneDisplayControl) :	
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber), 
            flagStart(this),
            sceneDisplayControl(sceneDisplayControl),
            pBallControlListener(nullptr),
			screenWidth(sceneDisplayControl.getWidth()),
            screenHeigth(sceneDisplayControl.getHeight()),
            posX(screenWidth/2),
            posY(screenHeigth/2),
            vX(1),
            vY(1),
            state(STATE_STOPPED)
		{
            // Zonder deze regel denkt BallControl dat de peddel op 0 staat tot de
            // speler voor het eerst links of rechts aanraakt - en is de eerste bal
            // dus altijd mis. De peddel begint in het midden, net als in
            // PaddleControl en SceneDisplayControl.
            poolPaddlePosition.write(screenWidth/2);

            srand((unsigned int)esp_timer_get_time());  // anders elke boot dezelfde "willekeurige" richting.
			start();
		}

        void setBallControlListener(IBallControlListener* pBallControlListener)
        {
            this->pBallControlListener = pBallControlListener;
        }

        void startBall()
        {
            flagStart.set();
        }

        void updatePaddlePosition(int x)
        {
            poolPaddlePosition.write(x);
        }

	private:
        // posX/posY zijn het MIDDEN van de bal, in speelveldpixels met de
        // oorsprong linksonder. Alles hieronder rekent met de straal erbij, want
        // een bal van 12 pixels kaatst met zijn rand tegen de wand, niet met zijn
        // middelpunt.
        static const int ballRadius = BallSizePix/2;

        void initialiseRandomStartPos()
        {
            posX = screenWidth/2;
            posY = screenHeigth/2;
            vX = ((rand()%2)*2-1) * BallSpeedPixPerFrame;
            vY = BallSpeedPixPerFrame;

            // Meteen doorgeven, anders staat de bal het eerste beeld van een
            // nieuw spel nog op de plek waar het vorige spel verloren ging.
            sceneDisplayControl.updateBallPosition(posX, posY);
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
            int paddlePosition(0);
            poolPaddlePosition.read(paddlePosition);
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

            sceneDisplayControl.updateBallPosition(posX, posY);
        }

		/*override keyword not supported*/
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				switch (state)
                {
                case STATE_STOPPED:
                    wait(flagStart);
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
                    }
                    break;
                
                default:
                    break;
                }

                vTaskDelay(FrameIntervalMs);	// de beeldklok van het spel: 25 keer per seconde
                                                // een stap van BallSpeedPixPerFrame pixels.
			}
		}
	}; // end class BallControl

};// end namespace crt

// by Marius Versteegen, 2024

#pragma once
#include <stdlib.h>
#include <esp_timer.h>
#include <crt_CleanRTOS.h>
#include <crt_Button.h>

#include "pdga_GameDisplayControl.h"
#include "pdga_PaddleControl.h"

// This file contains the code of multiple tasks that run concurrently and notify eachother using flags.

// Testschakelaar, normaal uit. Aan: de bal begint vlak boven de doellijn en is
// binnen een halve seconde weg, zodat de overgang naar het game-over-scherm
// honderden keren per minuut te beproeven is in plaats van twintig keer per
// kwartier. Zie initialiseRandomStartPos().
//#define PONG_FAST_GAME_OVER

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Geen eigen extern-declaratie van logger: die zou pong_cleangui_dg_adv::logger
	// aankondigen, en dat object bestaat niet. De enige logger is crt::logger uit
	// main.cpp, en die is hier al bereikbaar via het using namespace crt hierboven.

    class IBallControlListener
    {
    public:
        // Vroeger heette dit ballMissedByPaddle, en dat klopte toen ook: de bal
        // was het spel uit op het moment dat hij de doellijn passeerde. Sinds
        // UC05_BallHandling stap 1b vliegt hij daarna brandend door, en is het
        // spel pas afgelopen als hij het scherm heeft verlaten. De naam volgt dat
        // moment, want een naam die het verkeerde moment aanwijst is erger dan
        // geen naam.
        virtual void ballLeftTheScreen() = 0;
    };

	class BallControl : public Task
	{
        enum State
        {
            STATE_STOPPED,
            STATE_RUNNING,

            // Onder de doellijn door: de bal staat in brand en valt door tot hij
            // het scherm uit is. Zie UC05_BallHandling stap 1b. Een eigen
            // toestand en geen vlaggetje in STATE_RUNNING, omdat er hier niets
            // meer te botsen valt - er staan onder de doellijn geen wanden, en de
            // peddel ligt erboven.
            STATE_BURNING
        };

	private:
        Flag flagStart;
        IGameDisplayControl& gameDisplayControl;
        PaddleControl& paddleControl;
        IBallControlListener* pBallControlListener;
        int screenWidth;
        int screenHeigth;
        int posX;
        int posY;
        int vX;
        int vY;
        State state;

        // De beeldklok van het spel.
        //
        // vTaskDelay(40) is een pauze VAN 40 ms en geen periode van 40 ms: de tijd
        // die de lus zelf kost komt er bovenop. Gemeten kwam dat op 43,6 ms per
        // stap uit - 9% te traag - en hoeveel het is hangt af van hoe druk de
        // tekentaak het heeft, dus het is ook nog eens niet constant.
        // xTaskDelayUntil rekent vanaf het vorige wekmoment en haalt dat eruit.
        //
        // Let op bij hervatten: staat timeOfNextStep nog op een moment van
        // seconden geleden - en dat is zo na de game-over-pagina - dan haalt
        // xTaskDelayUntil die achterstand in met een reeks stappen zonder pauze.
        // Precies de schok die er net is uitgehaald. Vandaar dat hij bij het
        // starten van een bal opnieuw wordt gezet.
        TickType_t timeOfNextStep;

        // Doorlopend nummer van elke stap, zodat GameDisplayControl kan zien of er
        // een aan zijn neus voorbij is gegaan. De Pool bewaart alleen de laatste
        // stand: loopt het spel voor op het tekenen, dan wordt een positie
        // overschreven voordat hij ooit op het scherm stond, en springt de bal.
        // Zonder nummer is dat niet te onderscheiden van een bal die gewoon snel
        // gaat.
        int32_t indexStep;

        // Alleen voor de meting in STATE_BURNING; zie daar.
        int64_t timeOfIgnitionUs;
        int posYAtIgnition;
        int32_t nofBurningSteps;

	public:
		BallControl(const char *taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, IGameDisplayControl& gameDisplayControl, PaddleControl& paddleControl) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber), 
            flagStart(this),
            gameDisplayControl(gameDisplayControl),
            paddleControl(paddleControl),
            pBallControlListener(nullptr),
			screenWidth(gameDisplayControl.getWidth()),
            screenHeigth(gameDisplayControl.getHeight()),
            posX(screenWidth/2),
            posY(screenHeigth/2),
            vX(1),
            vY(1),
            state(STATE_STOPPED),
            timeOfNextStep(0),
            indexStep(0),
            timeOfIgnitionUs(0),
            posYAtIgnition(0),
            nofBurningSteps(0)
		{
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
            indexStep = 0;
            gameDisplayControl.updateBallPosition(posX, posY, false, indexStep);

            // De klok begint hier, en niet ergens in het vorige spel: zie de
            // toelichting bij timeOfNextStep.
            timeOfNextStep = xTaskGetTickCount();

            // En dan een beeld wachten voordat STATE_RUNNING zijn eerste stap
            // doet. Zonder dit volgen de beginstand en de eerste stap elkaar
            // binnen enkele milliseconden op - dezelfde schok als bij het
            // ontbranden, alleen bij het opstarten van een spel. Zichtbaar in de
            // log als een beeldtijd van 1 ms tussen twee beelden die 40 ms uit
            // elkaar horen te liggen.
            waitForNextStep();
        }

        // Wachten tot het volgende beeldmoment op het vaste raster.
        void waitForNextStep()
        {
            xTaskDelayUntil(&timeOfNextStep, pdMS_TO_TICKS(FrameIntervalMs));
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

            indexStep++;
            gameDisplayControl.updateBallPosition(posX, posY, false, indexStep);
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
                        // Nog niet afgelopen: hij vat vlam en valt door.
                        //
                        // Hier bewust GEEN updateBallPosition(): dat stuurde de
                        // ongestapte positie door, waarna de eerstvolgende ronde
                        // van STATE_BURNING op hetzelfde moment de gestapte stuurde.
                        // Het scherm tekende dan twee beelden binnen enkele
                        // milliseconden en de bal versprong zijn hele stap in een
                        // oogwenk - een schok die er precies bij het ontbranden uit
                        // zag als een versnelling. Gemeten als een beeldtijd van
                        // 1..3 ms tussen twee beelden die 40 ms uit elkaar horen te
                        // liggen. De vlammen verschijnen nu een beeld later, en dat
                        // is precies goed: hij vat vlam terwijl hij de lijn passeert.
                        //
                        // Meting: klopt het dat de bal tijdens het branden even
                        // snel valt als tijdens het spel? Verwacht wordt
                        // BallSpeedPixPerFrame pixels per FrameIntervalMs. Wijkt
                        // de gemeten pixels-per-seconde daarvan af, dan is het
                        // geen gezichtsbedrog.
                        timeOfIgnitionUs = esp_timer_get_time();
                        posYAtIgnition = posY;
                        nofBurningSteps = 0;

                        state = STATE_BURNING;
                    }
                    else
                    {
                        updateBallPosition();
                        waitForNextStep();	// de beeldklok van het spel: 25 keer per seconde
                                            // een stap van BallSpeedPixPerFrame pixels.
                    }
                    break;

                case STATE_BURNING:
                    if (posY <= gameDisplayControl.getBallOffScreenY())
                    {
                        int32_t elapsedMs = (int32_t)((esp_timer_get_time() - timeOfIgnitionUs)/1000);
                        int32_t droppedPix = posYAtIgnition - posY;
                        ESP_LOGI("pong", "brandend: %d stappen, %d px in %d ms = %d px/s "
                            "(verwacht %d px/s)",
                            (int)nofBurningSteps, (int)droppedPix, (int)elapsedMs,
                            (elapsedMs > 0) ? (int)(droppedPix*1000/elapsedMs) : 0,
                            (int)(BallSpeedPixPerFrame*1000/FrameIntervalMs));

                        assert(pBallControlListener!=nullptr);
                        pBallControlListener->ballLeftTheScreen();
                        state = STATE_STOPPED;
                    }
                    else
                    {
                        // Kaal doorvliegen: geen wanden, geen peddel. Wel op
                        // dezelfde beeldklok, zodat het vallen even snel gaat als
                        // het spelen en de vlamcyclus meeloopt.
                        posX += vX;
                        posY += vY;
                        nofBurningSteps++;
                        indexStep++;
                        gameDisplayControl.updateBallPosition(posX, posY, true, indexStep);
                        waitForNextStep();
                    }
                    break;

                default:
                    break;
                }
			}
		}
	}; // end class BallControl

};// end namespace crt

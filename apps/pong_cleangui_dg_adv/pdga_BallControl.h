// by Marius Versteegen, 2024

#pragma once
#include <stdlib.h>
#include <esp_timer.h>
#include <crt_CleanRTOS.h>
#include "pdga_IDepot.h"
#include "pdga_IDepotItem.h"

#include "pdga_GameDisplayControl.h"
#include "pdga_IBallCollisionCheck.h"
#include "pdga_IBallListener.h"

// Testschakelaar, normaal uit. Aan: de bal begint vlak boven de doellijn en is
// binnen een halve seconde weg, zodat de overgang naar het game-over-scherm
// honderden keren per minuut te beproeven is in plaats van twintig keer per
// kwartier. Zie spawn().
//#define PONG_FAST_GAME_OVER

using namespace crt;
namespace pong_cleangui_dg_adv
{
	class BallControl : public IDepotItem
	{
        enum State
        {
            STATE_IDLE,		// ligt in het depot, wordt niet ge-update
            STATE_RUNNING,

            // Onder de doellijn door en dus verloren: hij staat in brand en valt
            // kaal door tot het scherm uit. Zie UC05_BallHandling stap 1b.
            STATE_FALLING_OUT,

            // Vlam gevat aan een vuurkorf (UC05 stap 1d), maar nog volop in het
            // spel: hij kaatst gewoon tegen wanden, peddel en crates, en brandt
            // ondertussen in BallBurnUpMs op.
            STATE_BURNING_UP
        };

	private:
        GameDisplayControl* pGameDisplayControl;

        // Waar de bal vraagt of hij ergens tegenaan zit, en waar de peddel staat.
        IBallCollisionCheck* pBallCollisionCheck;

        // Voor het melden van een score update.
        IBallListener* pBallListener;

        // Het depot waar deze bal in ligt. Hij geeft zichzelf
        // vrij zodra hij het scherm uit is. Via IDepot en niet via Depot<...>:
        // anders zou een bal het depot moeten kennen dat hemzelf bevat.
        IDepot* pDepot;
        int indexSlot;

        int fieldWidth;
        int fieldHeight;
        int posX;
        int posY;
        int vX;
        int vY;
        State state;

        // Twee tellers voor hetzelfde branden, want ze hebben verschillende
        // eigenaren: de klok houdt de levensduur bij, de animatiestap wordt gebruikt
        // voor het animeren van het sprite-frame.
        int32_t nofBurnUpClockStepsLeft;
        int32_t indexBurnAnimationStep;

        // Gewone booleans en geen crt::Flag: een Flag kost een event-bit en een
        // taak om op te wachten, en beide kanten zitten hier in dezelfde taak. Het
        // gaat hier om de scheiding tussen melden en uitvoeren, niet om taaksynchronisatie.
        bool bFlagSpawnInTheMiddle;
        bool bFlagSpawnAt;
        int spawnAtPosX;
        int spawnAtPosY;

#ifdef PONG_MEASURE
        // Alleen voor de meting bij het verlaten van het scherm; zie daar.
        int64_t timeOfIgnitionUs;
        int posYAtIgnition;
        int32_t nofBurningSteps;
#endif

        // posX,posY staan voor het MIDDEN van de bal, in speelveldpixels met de
        // oorsprong linksonder. Alles hieronder rekent met de straal erbij, want
        // een bal van 12 pixels kaatst met zijn rand tegen de wand, niet met zijn
        // middelpunt.
        static const int ballRadius = BallSizePix/2;

	public:
        // Default constructor, want Depot<BallControl,N> bouwt alle N exemplaren
        // in een keer. Alles wat een bal verder nodig heeft komt via init().
		BallControl() :
            pGameDisplayControl(nullptr),
            pBallCollisionCheck(nullptr),
            pBallListener(nullptr),
            pDepot(nullptr),
            indexSlot(-1),
            fieldWidth(0),
            fieldHeight(0),
            posX(0),
            posY(0),
            vX(0),
            vY(0),
            state(STATE_IDLE),
            nofBurnUpClockStepsLeft(0),
            indexBurnAnimationStep(0),
            bFlagSpawnInTheMiddle(false),
            bFlagSpawnAt(false),
            spawnAtPosX(0),
            spawnAtPosY(0)
#ifdef PONG_MEASURE
            , timeOfIgnitionUs(0)
            , posYAtIgnition(0)
            , nofBurningSteps(0)
#endif
		{
		}

        void init(GameDisplayControl& gameDisplayControl,
                  IBallCollisionCheck* pBallCollisionCheck, IDepot* pDepot, int indexSlot)
        {
            this->pGameDisplayControl = &gameDisplayControl;
            this->pBallCollisionCheck = pBallCollisionCheck;
            this->pDepot = pDepot;
            this->indexSlot = indexSlot;

            fieldWidth  = gameDisplayControl.getWidth();
            fieldHeight = gameDisplayControl.getHeight();

            posX = fieldWidth/2;
            posY = fieldHeight/2;
        }

        // Apart van init(), want het spel bestaat nog niet als de depots worden
        // gebouwd. Zie Spawnables::setBallListener().
        void setBallListener(IBallListener* pBallListener)
        {
            this->pBallListener = pBallListener;
        }

        // Op een opgegeven plek, met een willekeurige richting. Zo komen de
        // ballen uit een geopende present crate (UC07 stap 3) vandaan.
        void spawnAt(int posX, int posY)
        {
            spawnAtPosX = posX;
            spawnAtPosY = posY;
            bFlagSpawnAt = true;
        }

        void spawnInTheMiddle()
        {
            bFlagSpawnInTheMiddle = true;
        }

        // Wat de display van deze bal moet weten. De bal schrijft nergens naartoe;
        // hij wordt uitgelezen. 
        BallState getBallState() const
        {
            if (state == STATE_IDLE)
            {
                return BallState();
            }

            return BallState(posX, posY,
                             (state == STATE_FALLING_OUT) || (state == STATE_BURNING_UP),
                             indexBurnAnimationStep);
        }

        // Een update stap. Aangeroepen door het depot, dat alleen uitgegeven exemplaren
        // afloopt.
        // Deze functie mag niet ergens op wachten. Hij moet meteen terugkeren: hij draait 
        // in de wachtlus van GameControl::doGame(), samen met de negenenveertig andere ballen.
        /*override*/ void update()
        {
            switch (state)
            {
            case STATE_IDLE:
                if (bFlagSpawnInTheMiddle)
                {
                    bFlagSpawnInTheMiddle = false;
                    startInTheMiddle();
                }
                else if (bFlagSpawnAt)
                {
                    bFlagSpawnAt = false;
                    startAt(spawnAtPosX, spawnAtPosY);
                }
                break;

            case STATE_RUNNING:
                if (posY + ballRadius <= 0)		// helemaal onder de doellijn door.
                {
                    // Nog niet afgelopen: hij vat vlam en valt door.
#ifdef PONG_MEASURE
                    nofBurningSteps = 0;
#endif
                    state = STATE_FALLING_OUT;
                }
                else
                {
                    moveStep();
                }
                break;

            case STATE_BURNING_UP:
                if (nofBurnUpClockStepsLeft <= 0)
                {
                    // Opgebrand. Hij is nooit bij de doellijn gekomen; hij is
                    // gewoon op.
                    assert(pDepot != nullptr);
                    pDepot->release(indexSlot);
                }
                else if (posY + ballRadius <= 0)
                {
                    // Toch nog onder de doellijn door voordat hij op was. Dan
                    // geldt UC05 stap 1b: hij valt brandend het scherm uit, en de
                    // opbrandklok doet er niet meer toe.
#ifdef PONG_MEASURE
                    nofBurningSteps = 0;
#endif
                    state = STATE_FALLING_OUT;
                }
                else
                {
                    nofBurnUpClockStepsLeft--;
                    indexBurnAnimationStep++;
                    moveStep();
                }
                break;

            case STATE_FALLING_OUT:
                if (posY <= pGameDisplayControl->getBallOffScreenY())
                {
#ifdef PONG_MEASURE
                    int32_t elapsedMs = (int32_t)((esp_timer_get_time() - timeOfIgnitionUs)/1000);
                    int32_t droppedPix = posYAtIgnition - posY;
                    ESP_LOGI("pong", "brandend: %d stappen, %d px in %d ms = %d px/s "
                        "(verwacht %d px/s)",
                        (int)nofBurningSteps, (int)droppedPix, (int)elapsedMs,
                        (elapsedMs > 0) ? (int)(droppedPix*1000/elapsedMs) : 0,
                        (int)(BallSpeedPixPerFrame*1000/FrameIntervalMs));
#endif

                    // Zichzelf weer vrijgeven bij zijn Depot.
                    assert(pDepot != nullptr);
                    pDepot->release(indexSlot);
                }
                else
                {
#ifdef PONG_MEASURE
                    // Meting: klopt het dat de bal tijdens het branden even snel
                    // valt als tijdens het spel? Verwacht wordt
                    // BallSpeedPixPerFrame pixels per FrameIntervalMs.
                    //
                    // De klok begint bij de EERSTE stap en niet bij het ontbranden.
                    // Dat scheelde: tussen die twee zit een volle handler-periode,
                    // dus met het ontbranden als nulpunt liep het venster over 18
                    // perioden terwijl er 17 stappen in vielen - en dan meet je een
                    // te lage snelheid die er als een echte vertraging uitziet.
                    // Vanaf hier beslaan venster en stappen precies hetzelfde.
                    if (nofBurningSteps == 0)
                    {
                        timeOfIgnitionUs = esp_timer_get_time();
                        posYAtIgnition = posY;
                    }
                    nofBurningSteps++;
#endif

                    // Kaal doorvliegen: geen wanden, geen peddel. Wel op dezelfde
                    // beeldklok, zodat het vallen even snel gaat als het spelen en
                    // de vlamcyclus meeloopt.
                    posX += vX;
                    posY += vY;
                    indexBurnAnimationStep++;
                }
                break;

            default:
                break;
            }
        }

        // Door Depot aangeroepen bij release() en releaseAll().
        /*override*/ void reset()
        {
            posX = fieldWidth/2;
            posY = fieldHeight/2;
            vX = 0;
            vY = 0;
            state = STATE_IDLE;
            nofBurnUpClockStepsLeft = 0;
            indexBurnAnimationStep = 0;
            bFlagSpawnInTheMiddle = false;
            bFlagSpawnAt = false;
            spawnAtPosX = 0;
            spawnAtPosY = 0;
#ifdef PONG_MEASURE
            timeOfIgnitionUs = 0;
            posYAtIgnition = 0;
            nofBurningSteps = 0;
#endif
        }

	private:
        // Het werk achter de twee spawn-berichten.
        void startAt(int posX, int posY)
        {
            this->posX = posX;
            this->posY = posY;

            // Een spreiding in de horizontale snelheid, zodat tien ballen uit
            // dezelfde crate niet als een blok bewegen. De verticale houdt zijn
            // vaste grootte - daar hangt de valsnelheid aan (en dus de moeilijkheids-
            // graad om op de bal te reageren met de paddle)
            vX = (int)(rand() % (4*BallSpeedPixPerFrame + 1)) - 2*BallSpeedPixPerFrame;
            vY = ((rand()%2) == 0) ? BallSpeedPixPerFrame : -BallSpeedPixPerFrame;

            indexBurnAnimationStep = 0;
            state = STATE_RUNNING;
        }

        void startInTheMiddle()
        {
#ifdef PONG_FAST_GAME_OVER
            // De bal begint vlak boven de doellijn, helemaal links, en gaat
            // meteen naar beneden. De peddel begint in het midden en
            // haalt dat niet, dus is het na een handvol beelden game over.
            //
            // Bedoeld voor het beproeven van die overgang, waar meerdere taken
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
            posX = fieldWidth/2;
            posY = fieldHeight/2;
            vX = ((rand()%2)*2-1) * BallSpeedPixPerFrame;
            vY = BallSpeedPixPerFrame;
#endif
            indexBurnAnimationStep = 0;
            state = STATE_RUNNING;
        }

        void bounceOffWallsIfHit()
        {
            // Tegen een wand aan: terugzetten op de rand en de richting omkeren.
            // Terugzetten hoort erbij - zonder dat kan de bal een stap in de wand
            // eindigen en er bij de volgende stap nog dieper in lopen, want de
            // snelheid is groter dan een pixel.
            if (posX - ballRadius <= WallThicknessPix)
            {
                posX = WallThicknessPix + ballRadius;
                vX = (vX < 0) ? -vX : vX;
            }
            else if (posX + ballRadius >= fieldWidth - WallThicknessPix)
            {
                posX = fieldWidth - WallThicknessPix - ballRadius;
                vX = (vX > 0) ? -vX : vX;
            }

            if (posY + ballRadius >= fieldHeight - WallThicknessPix)
            {
                posY = fieldHeight - WallThicknessPix - ballRadius;
                vY = (vY > 0) ? -vY : vY;
            }
        }

        bool bouncePaddleIfHit(int paddlePosition)
        {
            bool bBouncedPaddle = false;

            // De peddel: alleen als de bal daalt, en alleen als hij binnen de
            // breedte van de peddel valt.
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
                bBouncedPaddle = true;
            }
            return bBouncedPaddle;
        }

        void moveStep()
        {
            posX += vX;
            posY += vY;

            bounceOffWallsIfHit();

            int paddlePosition = (pBallCollisionCheck != nullptr) ? pBallCollisionCheck->getPaddlePosition() : 0;
            bool bBouncedPaddle = bouncePaddleIfHit(paddlePosition);
            if (bBouncedPaddle && (pBallListener != nullptr))
            {
                // De score gaat omhoog bij een kaatsing op de
                // peddel. Daarto de bounce doorgeven.
                pBallListener->ballBouncedOnPaddle();
            }

            bounceOffCrateIfHit();
            catchFireIfInFireBin();
        }

        // Een bal die een vuurkorf raakt vliegt in brand en brandt
        // op in BallBurnUpMs. Geen kaatsing - een korf is vuur en geen kist, dus
        // hij vliegt er doorheen.
        void catchFireIfInFireBin()
        {
            // Alleen een bal die nog niet brandt kan vlam vatten. 
            if (state != STATE_RUNNING)
            {
                return;
            }

            if ((pBallCollisionCheck == nullptr) || !pBallCollisionCheck->isFireBinTouchedAtPos(posX, posY, ballRadius))
            {
                return;
            }

            nofBurnUpClockStepsLeft = NofBallBurnUpSteps;
            indexBurnAnimationStep = 0;
            state = STATE_BURNING_UP;
        }

        // De hit komt van buiten binnen als parameter, zodat hier alleen rekenwerk
        // staat. Zie bounceOffCrateIfHit() voor de externe kant.
        void bounceOffCrate(const CrateHit& hit)
        {
            // Terugduwen langs de as waar hij het minst diep in zit: dat is de
            // kant waar hij binnenkwam. Zou je altijd verticaal kaatsen, dan
            // schiet een bal die van opzij komt dwars door het kistje heen.
            int halfSize = hit.sizePix/2;
            int dx = posX - hit.posX;
            int dy = posY - hit.posY;

            int overlapX = (halfSize + ballRadius) - ((dx < 0) ? -dx : dx);
            int overlapY = (halfSize + ballRadius) - ((dy < 0) ? -dy : dy);

            if (overlapX < overlapY)
            {
                posX += (dx >= 0) ? overlapX : -overlapX;
                vX = (dx >= 0) ? ((vX < 0) ? -vX : vX) : ((vX > 0) ? -vX : vX);
            }
            else
            {
                posY += (dy >= 0) ? overlapY : -overlapY;
                vY = (dy >= 0) ? ((vY < 0) ? -vY : vY) : ((vY > 0) ? -vY : vY);
            }
        }

        // Raakt de bal een present crate, dan geeft hij dat door aan dat object,
        // en kaatst hij ervan weg. 
        void bounceOffCrateIfHit()
        {
            if (pBallCollisionCheck != nullptr)
            {
                CrateHit hit = pBallCollisionCheck->getCrateHitAtPos(posX, posY, ballRadius);
                if (hit.bHit)
                {
                    // Eerst melden, dan kaatsen. De plek en de maat zitten al in hit, dus
                    // dat de crate hierdoor kan opengaan en verdwijnen maakt voor de
                    // berekening hieronder niets uit.
                    pBallCollisionCheck->crateWasHitAtPos(hit);
                    bounceOffCrate(hit);
                }
            }
        }
	}; // end class BallControl

};// end namespace pong_cleangui_dg_adv

// by Marius Versteegen, 2024

#pragma once
#include <stdlib.h>
#include <esp_timer.h>
#include <crt_CleanRTOS.h>
#include "pdga_IDepot.h"
#include <crt_Button.h>

#include "pdga_GameDisplayControl.h"
#include "pdga_PaddleControl.h"
#include "pdga_PresentCrateControl.h"

// Testschakelaar, normaal uit. Aan: de bal begint vlak boven de doellijn en is
// binnen een halve seconde weg, zodat de overgang naar het game-over-scherm
// honderden keren per minuut te beproeven is in plaats van twintig keer per
// kwartier. Zie spawn().
//#define PONG_FAST_GAME_OVER

using namespace crt;
namespace pong_cleangui_dg_adv
{
	// Geen eigen extern-declaratie van logger: die zou pong_cleangui_dg_adv::logger
	// aankondigen, en dat object bestaat niet. De enige logger is crt::logger uit
	// main.cpp, en die is hier al bereikbaar via het using namespace crt hierboven.

	// Een bal is GEEN taak.
	//
	// Er kunnen er tot MaxNofBalls tegelijk in de lucht zijn (UC07 spawnt er tien
	// ineens), en vijftig taken van 5000 bytes stack is 250 KB - meer DRAM dan dit
	// bord heeft, zeker met een backbuffer van 76.800 bytes erbij. Daarom ligt hij
	// in een Depot (zie pdga_Depot.h) en doet hij per beeld een stap in update(),
	// aangedreven door een crt::Handler die de klok bezit.
	//
	// Dat past ook gewoon in deze codebase: GameControl, GameOverControl en
	// StartPageControl zijn al kale control-objecten die in de taak van een ander
	// meeliften.
	class BallControl
	{
        enum State
        {
            STATE_IDLE,		// ligt in het depot, wordt niet ge-update
            STATE_RUNNING,

            // Onder de doellijn door: de bal staat in brand en valt door tot hij
            // het scherm uit is. Zie UC05_BallHandling stap 1b. Een eigen
            // toestand en geen vlaggetje in STATE_RUNNING, omdat er hier niets
            // meer te botsen valt - er staan onder de doellijn geen wanden, en de
            // peddel ligt erboven.
            STATE_BURNING
        };

	private:
        IGameDisplayControl* pGameDisplayControl;

        // De peddelstand van dit beeld, eigendom van GameObjectsControl. Een
        // aanwijzer en geen PaddleControl&, en dat is geen stijlkeuze:
        // PaddleControl::getPaddlePos() gaat via een Pool, en die neemt een
        // SimpleMutex. Met vijftig ballen waren dat vijftig slotoperaties per
        // beeld op een slot dat ook door PaddleControl zelf wordt genomen - en die
        // draait op een lágere prioriteit, dus de Handler kon daar op gaan staan
        // wachten. Nu leest GameObjectsControl de stand een keer per beeld, in
        // dezelfde taak waarin deze update() draait, en is er niets meer te
        // vergrendelen. Alle ballen zien bovendien gegarandeerd dezelfde peddel.
        const int* pPaddlePosition;

        // Waar de bal vraagt of hij een present crate raakt. Hij kent de crates
        // zelf niet - zie IPresentCrateField.
        IPresentCrateField* pCrateField;

        // Het depot waar deze bal in ligt, en waar hij in ligt. Hij geeft zichzelf
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

        // Hoeveel stappen deze bal al brandt. De display maakt daar een cel van de
        // spritesheet van; hoeveel cellen die heeft is kennis van de display en
        // niet van de bal. Per bal, want met vijftig ballen kan elk zijn eigen
        // brandfase hebben.
        int32_t indexBurningStep;

        // Alleen voor de meting bij het verlaten van het scherm; zie daar.
        int64_t timeOfIgnitionUs;
        int posYAtIgnition;
        int32_t nofBurningSteps;

        // posX/posY zijn het MIDDEN van de bal, in speelveldpixels met de
        // oorsprong linksonder. Alles hieronder rekent met de straal erbij, want
        // een bal van 12 pixels kaatst met zijn rand tegen de wand, niet met zijn
        // middelpunt.
        static const int ballRadius = BallSizePix/2;

	public:
        // Default constructor, want Depot<BallControl,N> bouwt alle N exemplaren
        // in een keer. Alles wat een bal verder nodig heeft komt via init().
		BallControl() :
            pGameDisplayControl(nullptr),
            pPaddlePosition(nullptr),
            pCrateField(nullptr),
            pDepot(nullptr),
            indexSlot(-1),
            fieldWidth(0),
            fieldHeight(0),
            posX(0),
            posY(0),
            vX(0),
            vY(0),
            state(STATE_IDLE),
            indexBurningStep(0),
            timeOfIgnitionUs(0),
            posYAtIgnition(0),
            nofBurningSteps(0)
		{
		}

        // Eenmalige bedrading, vlak na het opstarten. Geen setBallProps(): die
        // vorm zegt in deze codebase dat je hem later nog eens mag aanroepen om
        // iets te wijzigen, en dat geldt hier niet - dit ligt vast zodra het spel
        // loopt.
        void init(IGameDisplayControl& gameDisplayControl, const int* pPaddlePosition,
                  IPresentCrateField* pCrateField, IDepot* pDepot, int indexSlot)
        {
            this->pGameDisplayControl = &gameDisplayControl;
            this->pPaddlePosition = pPaddlePosition;
            this->pCrateField = pCrateField;
            this->pDepot = pDepot;
            this->indexSlot = indexSlot;

            fieldWidth  = gameDisplayControl.getWidth();
            fieldHeight = gameDisplayControl.getHeight();

            posX = fieldWidth/2;
            posY = fieldHeight/2;
        }

        // In het spel zetten. Aan te roepen nadat het depot deze bal heeft
        // uitgegeven - het depot begint hem daarna vanzelf te updaten.
        void spawn()
        {
            spawnInTheMiddle();
        }

        // Op een opgegeven plek, met een willekeurige richting. Zo komen de
        // ballen uit een geopende present crate (UC07 stap 3) daar vandaan in
        // plaats van uit het midden.
        void spawnAt(int posX, int posY)
        {
            this->posX = posX;
            this->posY = posY;

            // Een spreiding in de horizontale snelheid, zodat tien ballen uit
            // dezelfde crate niet als een blok bewegen. De verticale houdt zijn
            // vaste grootte - daar hangt de valsnelheid aan.
            vX = (int)(rand() % (4*BallSpeedPixPerFrame + 1)) - 2*BallSpeedPixPerFrame;
            vY = ((rand()%2) == 0) ? BallSpeedPixPerFrame : -BallSpeedPixPerFrame;

            indexBurningStep = 0;
            state = STATE_RUNNING;
        }

        void spawnInTheMiddle()
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
            indexBurningStep = 0;
            state = STATE_RUNNING;
        }

        // Wat de display van deze bal moet weten. De bal schrijft nergens naartoe;
        // hij wordt uitgelezen. Dat scheelt vijftig Pool-schrijfacties per beeld -
        // Pool::write() neemt zijn argument op waarde aan.
        BallState getBallState() const
        {
            return BallState(posX, posY, (state == STATE_BURNING), indexBurningStep);
        }

        // Een stap. Aangeroepen door het depot, dat alleen uitgegeven exemplaren
        // afloopt - vandaar dat er geen "sta ik wel aan"-toets in staat.
        //
        // Deze functie moet meteen terugkeren: hij draait in de taak van de
        // Handler, samen met alles wat daar verder aan hangt. Vandaar geen enkele
        // wacht- of slaapoproep, waar de oude taakversie een xTaskDelayUntil had.
        void update()
        {
            switch (state)
            {
            case STATE_RUNNING:
                if (posY + ballRadius <= 0)		// helemaal onder de doellijn door.
                {
                    // Nog niet afgelopen: hij vat vlam en valt door.
                    //
                    // Hier bewust geen stap en geen melding: het eerstvolgende
                    // beeld toont hem brandend op zijn huidige plek. Toen hier nog
                    // een positie werd doorgestuurd, kwam die op hetzelfde moment
                    // aan als de gestapte van de volgende ronde, en versprong de
                    // bal zijn hele stap in een oogwenk - een schok die er precies
                    // bij het ontbranden uitzag als een versnelling.
                    //
                    nofBurningSteps = 0;
                    state = STATE_BURNING;
                }
                else
                {
                    step();
                }
                break;

            case STATE_BURNING:
                if (posY <= pGameDisplayControl->getBallOffScreenY())
                {
                    int32_t elapsedMs = (int32_t)((esp_timer_get_time() - timeOfIgnitionUs)/1000);
                    int32_t droppedPix = posYAtIgnition - posY;
                    ESP_LOGI("pong", "brandend: %d stappen, %d px in %d ms = %d px/s "
                        "(verwacht %d px/s)",
                        (int)nofBurningSteps, (int)droppedPix, (int)elapsedMs,
                        (elapsedMs > 0) ? (int)(droppedPix*1000/elapsedMs) : 0,
                        (int)(BallSpeedPixPerFrame*1000/FrameIntervalMs));

                    state = STATE_IDLE;

                    // Zichzelf teruggeven. Dit mag vanuit de eigen update():
                    // een Depot perst zijn administratie niet samen, dus de lus
                    // die ons aanroept raakt hier niet van in de war. Het panel
                    // volgt vanzelf - de display tekent per beeld alleen de slots
                    // die in gebruik zijn.
                    assert(pDepot != nullptr);
                    pDepot->release(indexSlot);
                }
                else
                {
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

                    // Kaal doorvliegen: geen wanden, geen peddel. Wel op dezelfde
                    // beeldklok, zodat het vallen even snel gaat als het spelen en
                    // de vlamcyclus meeloopt.
                    posX += vX;
                    posY += vY;
                    nofBurningSteps++;
                    indexBurningStep++;
                }
                break;

            default:
                break;
            }
        }

	private:
        void step()
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

            // De peddel: alleen als de bal daalt, en alleen als hij binnen de
            // breedte van de peddel valt.
            int paddlePosition = (pPaddlePosition != nullptr) ? *pPaddlePosition : 0;

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

            bounceOffCrateIfHit();
        }

        // UC05_BallHandling stap 1c: raakt de bal een present crate, dan geeft hij
        // dat door aan dat object. Wat er verder gebeurt - de teller omlaag, en bij
        // nul de crate open - is de zaak van de crate, niet van de bal.
        //
        // De kaatsing zelf staat NIET in het ontwerp, maar zonder is de teller
        // zinloos: een bal die er dwars doorheen vliegt, overlapt vijf beelden
        // achter elkaar en sloopt het kistje in tweehonderd milliseconde. Kaatsen
        // is bovendien wat je van een kistje verwacht.
        void bounceOffCrateIfHit()
        {
            if (pCrateField == nullptr)
            {
                return;
            }

            CrateHit hit = pCrateField->reportBallHit(posX, posY, ballRadius);
            if (!hit.bHit)
            {
                return;
            }

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
	}; // end class BallControl

};// end namespace pong_cleangui_dg_adv

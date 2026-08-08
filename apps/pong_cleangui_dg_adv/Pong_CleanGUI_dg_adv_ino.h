// by Marius Versteegen, 2023

#include <crt_CleanRTOS.h>        // This file includes crt_Config.h  You'll need to change defines there for a release build.
#include <crt_Mutex.h>            // crt_Mutex.h must be included separately.

#include <crt_CleanGUI.h>
#include <crt_AsyncDisplay.h>
#include <crt_TouchscreenButton.h>
#include <crt_TFT_eSPI_DisplayAdapter.h>
#include <crt_TFT_eSPI_DisplayAdapter_PAL8.h>

// Het gedeelde palet van alle plaatjes van deze app. Zie images/_pal8/.
#include "images/_pal8/_allImages.h"

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

// All Tasks should be created in this main file.
#include <crt_Logger.h>
#include <crt_Handler.h>
#include <crt_Button.h>
#include "pdga_StartPageDisplayControl.h"
#include "pdga_GameDisplayControl.h"
#include "pdga_GameOverDisplayControl.h"
#include "pdga_PaddleControl.h"
#include "pdga_BallControl.h"
#include "pdga_CrateControl.h"
#include "pdga_FireBinControl.h"
#include "pdga_HallOfFame.h"
#include "pdga_Spawnables.h"
#include "pdga_SpawnablesHandler.h"
#include "pdga_MainControl.h"

using namespace crt;
namespace pong_cleangui_dg_adv
{
    MainInits mainInits;            // Initialize CleanRTOS.

	// Keuzes voor buffergroottes - ruim gekozen voor deze applicatie.
	const int MaxNofTouchListeners = 20;	// Max aantal touch listeners dat het display kan hebben.
	const int CommandBufferSize = 100;		// Max aantal commando's dat het display kan bufferen.
	const int MaxNofClientTasks = 10;		// Max aantal taken dat commando's naar het display kan sturen.

	TFT_eSPI_FreeFonts<20/*MaxNofFreeFonts*/> freeFonts;		// not used when LOAD_GFXFF is not defined.

	// Typedefs, puur voor de leesbaarheid hieronder.
	//
	// PAL8 en niet RGB565: deze variant tekent in een schermvullende backbuffer
	// van 8 bits per pixel (320 x 240 = 76.800 bytes) en stuurt die bij present()
	// in een keer uit. Dat is wat de plaatjes goedkoop maakt - buffer en plaatje
	// delen hetzelfde palet, dus blitten is memcpy in plaats van een omzetting
	// per pixel. Zie de kopregel van crt_TFT_eSPI_DisplayAdapter_PAL8.h.
	//
	// Het kost wel wat, en dat is geen detail:
	// - present() stuurt altijd het hele scherm: ~31 ms, dus ~30 beelden per
	//   seconde is het plafond, ongeacht hoe weinig er verandert.
	// - Gedeeltelijk hertekenen (moveTo) is er daardoor niet meer bij; de
	//   DisplayControls tekenen hun hele boom per beeld. Zie
	//   IDisplay::isBuffered() en waar dat in pdga_GameDisplayControl.h op
	//   uitkomt.
	// - Afgeronde hoeken worden vierkant: een TFT_eSprite heeft er geen
	//   primitieve voor. Alles wat rond moet zijn, is daarom een plaatje.
	//
	// Terug naar rechtstreeks tekenen is een kwestie van deze ene regel omzetten
	// naar TFT_eSPI_DisplayAdapter_RGB565; de rest van de app kijkt naar
	// isBuffered() en past zich aan.
	typedef TFT_eSPI_DisplayAdapter_PAL8<MaxNofTouchListeners> DisplayAdapterType;
	typedef AsyncDisplay<CommandBufferSize, MaxNofClientTasks> AsyncDisplayType;

	DisplayAdapterType tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, false/*bRepeatCalibration*/);
	AsyncDisplayType asyncDisplay("AsyncDisplay", 2 /*priority*/, 20000 + CommandBufferSize*sizeof(DisplayCommand)+
				            MaxNofClientTasks*sizeof(PrintContext) /*stackBytes*/, ARDUINO_RUNNING_CORE,
				            tft_eSPI_DisplayAdapter, 1/*rotation*/, 2/*fontTouchCalibration*/);

	// Elke pagina zijn eigen DisplayControl, elk met een eigen PageRoot en een
	// eigen taak. Ze mogen naast elkaar bestaan omdat er steeds maar een van de
	// drie in beeld staat - MainControl roept de pagina's strikt na elkaar aan.
	// Zie de opmerking boven crt_PageRoot.h.
	//
	// Let op de volgorde: elk object hieronder krijgt de objecten erboven mee.
	// gameDisplayControl kent de schermmaat al in zijn constructor, dus de twee
	// controls daaronder mogen daar meteen getWidth()/getHeight() van vragen.
	// Prioriteit 2 - dezelfde als asyncDisplay, en dat is geen toeval maar een
	// eis. Zie de auto-present onderaan AsyncDisplay::main(): die neemt "de
	// commandoqueue is leeg" als "het beeld is af" en stuurt de backbuffer uit.
	//
	// Een tekenende taak die ONDER de tekentaak zit, wordt bij elke ingestuurde
	// opdracht opzij gezet. De tekentaak treft de queue dan leeg aan terwijl er
	// pas een deel van het beeld in de buffer staat, en stuurt dat halve beeld
	// uit. Bij deze app is het eerste commando van elk beeld het schermvullende
	// zwarte achtergrondpaneel, dus zo'n half beeld IS een zwart scherm: je ziet
	// het hele scherm knipperen.
	//
	// Op 2, gelijk aan de tekentaak, was dat grotendeels weg - FreeRTOS wisselt
	// niet uit zichzelf tussen taken van gelijke prioriteit - maar niet helemaal.
	// Met time slicing valt er nog steeds af en toe een klokttik middenin een
	// reeks commando's, en dan gebeurt precies hetzelfde. Op de startpagina, die
	// sinds de fakkels een keer of tien per seconde hertekent, kwam dat neer op
	// een flits per vijf a tien seconden.
	//
	// Vandaar 3, BOVEN asyncDisplay. Dan kan de tekentaak de schrijver niet meer
	// onderbreken: die maakt zijn reeks af, blokkeert, en pas dan wordt er
	// getekend en gepresenteerd. Uithongeren kan niet - loopt de commandoqueue
	// vol, dan blokkeert de schrijver daarop en komt de tekentaak alsnog aan de
	// beurt.
	//
	// Alleen de taken die niet tekenen (Ball, Paddle, Main, Handler) blijven
	// eronder.
	// De top-5 leeft in de spiffs-partitie, naast de touchkalibratie. Zie
	// pdga_HallOfFame.h.
	HallOfFame hallOfFame("/hallOfFame");

	StartPageDisplayControl startPageDisplayControl("StartPageDisplayControl", 3 /*priority*/, 10000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts);
	GameDisplayControl<20,10> gameDisplayControl("GameDisplayControl", 3 /*priority*/, 20000 /*stack size*/, ARDUINO_RUNNING_CORE, 1000 /*frameDelayMs*/, asyncDisplay, freeFonts);
	GameOverDisplayControl gameOverDisplayControl("GameOverDisplayControl", 3 /*priority*/, 10000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts);

	PaddleControl paddleControl("PaddleControl", 1 /*priority*/, 5000 /*stack size*/, ARDUINO_RUNNING_CORE, gameDisplayControl);

	// De ballen zijn geen taken maar objecten in een depot: er kunnen er
	// MaxNofBalls tegelijk zijn (UC07 spawnt er tien ineens), en vijftig taken van
	// 5000 bytes stack is 250 KB - meer DRAM dan dit bord heeft, zeker met een
	// backbuffer van 76.800 bytes erbij. Een Handler tikt ze aan; zie
	// pdga_BallsControl.h en pdga_Depot.h.
	Spawnables spawnables(gameDisplayControl);

	// De beeldklok van het spel: de volgorde waarin de depots per beeld worden
	// bijgewerkt, en het publiceren van de standen. Zie pdga_SpawnablesHandler.h.
	SpawnablesHandler spawnablesHandler(spawnables, paddleControl);

	// Prioriteit 2, gelijk aan asyncDisplay en de DisplayControls. Deze taak
	// tekent zelf niet, maar hij zet wel de balstanden klaar waar een beeld op
	// wacht; onder de tekentaak zitten zou hem per beeld opzij laten schuiven.
	//
	// De periode is de beeldklok van het spel. Hij verving de xTaskDelayUntil die
	// BallControl zelf had - Handler rekent zijn interval af vanaf het eind van de
	// vorige ronde, dus de looptijd van het werk telt niet op bij de pauze.
	Handler<4/*MaxNofListeners*/> handler("Handler", 2 /*priority*/, ARDUINO_RUNNING_CORE, FrameIntervalMs);

	MainControl mainControl("MainControl", 1 /*priority*/, 5000 /*stack size*/, ARDUINO_RUNNING_CORE,
	                        startPageDisplayControl, gameDisplayControl, gameOverDisplayControl,
	                        spawnables, spawnablesHandler, paddleControl,
	                        hallOfFame);
}

void setup()
{
	// initialize serial communication at 115200 bits per second:
	// Serial.begin(115200);    // Only needed when using Serial.print();

	vTaskDelay(10);// allow tasks to initialize.

	// Het palet van de plaatjes is ook het palet van het scherm. Zonder dit
	// wordt er geen enkel PAL8-plaatje getoond en is alles wat de code zelf
	// tekent magenta.
	//
	// Hier en niet in een van de taken: elke taak begint met vTaskDelay(1000) en
	// deze regel is er ruim binnen. Dat is de hele synchronisatie - er is geen
	// moment waarop er al getekend wordt en het palet nog niet staat.
	pong_cleangui_dg_adv::tft_eSPI_DisplayAdapter.setPalette(crt::arPalette);

	// Aanmelden bij de Handler. Dit moet binnen 500 ms na het opstarten gebeuren:
	// Handler::main() wacht zo lang voordat hij begint met tikken, juist zodat
	// zijn luisteraars zich kunnen melden. setup() draait ruim binnen die tijd.
	pong_cleangui_dg_adv::handler.addHandlerListener(&pong_cleangui_dg_adv::spawnablesHandler);

	// De top-5 uit de spiffs-partitie halen. Hier en niet in een constructor:
	// LITTLEFS moet aangekoppeld zijn, en dat gebeurt pas als er iets om vraagt.
	pong_cleangui_dg_adv::hallOfFame.load();

	ESP_LOGI("checkpoint", "start of main");vTaskDelay(1);
}

void loop()
{
	vTaskDelay(1);// Nothing to do in loop - all example code runs in the 4 threads above.
}

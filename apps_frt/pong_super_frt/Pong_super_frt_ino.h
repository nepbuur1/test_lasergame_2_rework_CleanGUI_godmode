// by Marius Versteegen, 2023

#include <crt_CleanRTOS.h>        // This file includes crt_Config.h  You'll need to change defines there for a release build.
#include <crt_Mutex.h>            // crt_Mutex.h must be included separately.

#include <crt_CleanGUI.h>
#include <crt_AsyncDisplay.h>
#include <crt_TFT_eSPI_DisplayAdapter_PAL8.h>

// Het gedeelde palet van alle plaatjes van deze app. Zie images/_pal8/.
#include "images/_pal8/_allImages.h"

#ifdef LOAD_GFXFF
#include <Free_Fonts.h>
#endif

#include <crt_TFT_eSPI_FreeFonts.h>

// All Tasks should be created in this main file.
#include <crt_Logger.h>
#include <frt_ADC_buttons.h>
#include "psupf_ButtonInfoDisplayControl.h"
#include "psupf_StartPageDisplayControl.h"
#include "psupf_GameDisplayControl.h"
#include "psupf_NameInputDisplayControl.h"
#include "psupf_HallOfFameDisplayControl.h"
#include "psupf_PaddleControl.h"
#include "psupf_HallOfFame.h"
#include "psupf_MainControl.h"

using namespace crt;
namespace pong_super_frt
{
    MainInits mainInits;            // Initialize CleanRTOS - nodig voor de bibliotheken (CleanGUI, AsyncDisplay); de eigen klassen gebruiken FreeRTOS.

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
	//   DisplayControls tekenen hun hele boom per beeld.
	// - Afgeronde hoeken worden vierkant: een TFT_eSprite heeft er geen
	//   primitieve voor. Alles wat rond moet zijn, is daarom een plaatje.
	//
	// Deze app rekent op die backbuffer: elke pagina tekent per beeld haar hele
	// boom, en niets tekent gedeeltelijk. Rechtstreeks tekenen
	// (TFT_eSPI_DisplayAdapter_RGB565) zou daardoor per beeld een zichtbare wis
	// van het hele scherm zijn. Die route heeft er wel in gezeten en werkte; hij
	// is op 2026-09-07 bewust weggehaald om de code eenvoudig te houden (zie
	// het logboek; commit 5963268 is de laatste versie die hem nog had).
	// GameDisplayControl controleert de aanname bij het opstarten.
	typedef TFT_eSPI_DisplayAdapter_PAL8<MaxNofTouchListeners> DisplayAdapterType;
	typedef AsyncDisplay<CommandBufferSize, MaxNofClientTasks> AsyncDisplayType;

	DisplayAdapterType tft_eSPI_DisplayAdapter(freeFonts, "/TouchCalData1"/*filenameCalibration*/, false/*bRepeatCalibration*/);
	AsyncDisplayType asyncDisplay("AsyncDisplay", 2 /*priority*/, 20000 + CommandBufferSize*sizeof(DisplayCommand)+
				            MaxNofClientTasks*sizeof(PrintContext) /*stackBytes*/, ARDUINO_RUNNING_CORE,
				            tft_eSPI_DisplayAdapter, 1/*rotation*/, 2/*fontTouchCalibration*/);

	// Elke pagina zijn eigen DisplayControl, elk met een eigen PageRoot en een
	// eigen taak. Ze mogen naast elkaar bestaan omdat er steeds maar een van de
	// vier in beeld staat - MainControl roept de pagina's strikt na elkaar aan.
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
	// Alleen de taken die niet tekenen (Paddle, Main) blijven eronder.
	// De top-5 leeft in de spiffs-partitie, naast de touchkalibratie. Zie
	// psupf_HallOfFame.h.
	HallOfFame hallOfFame("/hallOfFame");

	// De allereerste pagina: of het externe toetsenbord (adcButtons hieronder) is aangesloten.
	ButtonInfoDisplayControl buttonInfoDisplayControl("ButtonInfoDisplayControl", 3 /*priority*/, 10000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts);
	StartPageDisplayControl startPageDisplayControl("StartPageDisplayControl", 3 /*priority*/, 10000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts);
	GameDisplayControl gameDisplayControl("GameDisplayControl", 3 /*priority*/, 20000 /*stack size*/, ARDUINO_RUNNING_CORE, 1000 /*frameDelayMs*/, asyncDisplay, freeFonts);

	// De game-over-afhandeling is twee pagina's, en dus twee taken: eerst de
	// naaminvoer (alleen bij een top-5-score), daarna de hall of fame. Ze staan
	// nooit tegelijk in beeld - GameOverControl roept ze strikt na elkaar aan.
	NameInputDisplayControl nameInputDisplayControl("NameInputDisplayControl", 3 /*priority*/, 10000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts);
	HallOfFameDisplayControl hallOfFameDisplayControl("HallOfFameDisplayControl", 3 /*priority*/, 10000 /*stack size*/, ARDUINO_RUNNING_CORE, asyncDisplay, freeFonts);

	// Zes knoppen op een weerstandsladder aan GPIO35 (libs/CYD_tools_crt). S1 en S2 sturen de
	// peddel naar links en rechts, naast de touchscreenknoppen; zie psupf_PaddleControl.h.
	// Prioriteit 2: hij tekent niet, maar moet wel op tijd meten (elke 10 ms, ~1 ms werk).
	frt::ADC_buttons<1 /*MAXLISTENERCOUNT*/> adcButtons("ADC_buttons", 2 /*priority*/, 4096 /*stackBytes*/, ARDUINO_RUNNING_CORE);

	PaddleControl paddleControl("PaddleControl", 1 /*priority*/, 5000 /*stack size*/, ARDUINO_RUNNING_CORE, gameDisplayControl, adcButtons);

	// Hier stonden de spawnables en hun beeldklok als losse globals. Ze zitten nu in
	// GameControl: dat drijft de klok aan, dus dat hoort ze ook te bezitten. Het
	// geheugen is niet verhuisd - het staat nog steeds in BSS, alleen binnen
	// mainControl hieronder.
	//
	// De stack van die taak draagt de hele beeldstap: doGame() roept
	// updateSpawnables() aan, en daar hangt de balketen tot en met publishStates()
	// onder. De Handler-taak die dat eerder deed had daar zijn eigen ~5,5 KB voor.
	MainControl mainControl("MainControl", 1 /*priority*/, 9000 /*stack size*/, ARDUINO_RUNNING_CORE,
	                        buttonInfoDisplayControl, adcButtons,
	                        startPageDisplayControl, gameDisplayControl,
	                        nameInputDisplayControl, hallOfFameDisplayControl,
	                        paddleControl, hallOfFame);
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
	pong_super_frt::tft_eSPI_DisplayAdapter.setPalette(crt::arPalette);

	// De top-5 uit de spiffs-partitie halen. Hier en niet in een constructor:
	// LITTLEFS moet aangekoppeld zijn, en dat gebeurt pas als er iets om vraagt.
	pong_super_frt::hallOfFame.load();

	ESP_LOGI("checkpoint", "start of main");vTaskDelay(1);
}

void loop()
{
	vTaskDelay(1);// Nothing to do in loop - all example code runs in the 4 threads above.
}

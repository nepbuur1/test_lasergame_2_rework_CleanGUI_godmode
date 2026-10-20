// by Marius Versteegen, 2026

// Gereedschap: de top-5 van pong_super wissen.
//
// De lijst leeft in /hallOfFame op de spiffs-partitie, naast de
// touchkalibratie. Dit programma zet er een lege lijst voor in de plaats en
// laat hem daarna opnieuw inlezen om te laten zien dat het gelukt is. Er is
// geen scherm en geen aanraking bij nodig; alles staat in de seriele uitvoer.
//
// Wat het NIET doet, en met opzet:
// - geen LITTLEFS.format(): dat neemt /TouchCalData1 mee, en dan mag je alle
//   vier de schermrotaties opnieuw kalibreren.
// - geen LITTLEFS.remove(): een leeg bestand van de juiste versie is minder
//   bijzonder dan een ontbrekend bestand, en de app kan met allebei overweg.
//   Zo blijft dit gereedschap ook een controle op de leesroutine.
//
// Gebruik: zet in main/main.cpp deze regel aan in plaats van de app, bouw en
// flash. Daarna main.cpp weer terugzetten.
//
//     #include <ClearHallOfFame.ino>

#include <crt_CleanRTOS.h>
#include <LITTLEFS.h>

#include "psup_HallOfFame.h"

// Op 1 zetten om het gereedschap zichzelf te laten controleren: dan vult het
// eerst een lijst met verzonnen namen, bewaart die, en wist hem daarna zoals
// altijd. Zonder dat is de kans groot dat je een lijst wist die al leeg was,
// en dan zegt "GEWIST" alleen dat er niets veranderd is.
//
// Blijft op 0 in git: wie het gereedschap gebruikt wil zijn lijst kwijt, niet
// eerst een valse erin.
#define CLEAR_HALL_OF_FAME_SELFTEST 0

using namespace crt;
namespace pong_super
{
	MainInits mainInits;			// Initialize CleanRTOS.

	// Dezelfde naam als in Pong_super_ino.h. Loopt dat uit elkaar, dan
	// wist dit gereedschap een bestand dat niemand leest en blijft de echte lijst
	// staan - vandaar dat de uitvoer hieronder de naam meldt.
	const char* strHallOfFameFilename = "/hallOfFame";

	// Globaal en niet op de stack van setup(): de rest van deze codebase houdt
	// het daar ook, en zo staat de omvang van dit gereedschap in de map van de
	// linker in plaats van in een stackberekening.
	HallOfFame hallOfFameBefore(strHallOfFameFilename);
	HallOfFame hallOfFameAfter(strHallOfFameFilename);

	static const char* TAG = "clearHallOfFame";

	void printHallOfFame(const char* strWhen, const HallOfFame& hallOfFame)
	{
		for (int i = 0; i < NofHallOfFameEntries; i++)
		{
			const HallOfFameEntry& entry = hallOfFame.getEntry(i);
			bool bEmpty = (entry.strName[0] == '\0');

			ESP_LOGI(TAG, "%s  %d: %-20s %d", strWhen, i + 1,
				bEmpty ? "<leeg>" : entry.strName, (int)entry.score);
		}
	}

	// Wat "gewist" betekent, in dezelfde termen als de lijst zelf: geen naam en
	// geen score. Precies wat HallOfFame::clear() achterlaat.
	bool bIsEmpty(const HallOfFame& hallOfFame)
	{
		for (int i = 0; i < NofHallOfFameEntries; i++)
		{
			const HallOfFameEntry& entry = hallOfFame.getEntry(i);
			if ((entry.strName[0] != '\0') || (entry.score != 0))
			{
				return false;
			}
		}
		return true;
	}

#if CLEAR_HALL_OF_FAME_SELFTEST
	// Via insert() en niet door de velden zelf te vullen: dan loopt dit langs
	// dezelfde weg als het spel, en is wat er straks gewist wordt ook echt wat
	// het spel achterlaat.
	void writeTestList()
	{
		HallOfFame hallOfFameTest(strHallOfFameFilename);

		hallOfFameTest.insert("zelftest een", 300);
		hallOfFameTest.insert("zelftest twee", 200);
		hallOfFameTest.insert("zelftest drie", 100);
		hallOfFameTest.save();

		ESP_LOGW(TAG, "zelftest staat aan: er is eerst een verzonnen lijst "
			"weggeschreven. Die hoort hieronder bij 'voor' te staan.");
	}
#endif
}

void setup()
{
	using namespace pong_super;

	vTaskDelay(100);	// de seriele uitvoer op gang laten komen.

	ESP_LOGI(TAG, "=== hall of fame wissen: %s ===", strHallOfFameFilename);

	// Zelf aankoppelen in plaats van het aan load() overlaten, zodat een
	// partitie die er niet is hier al opvalt in plaats van als een lege lijst.
	// formatOnFail blijft uit (de standaard): formatteren zou de touchkalibratie
	// meenemen.
	if (!LITTLEFS.begin())
	{
		ESP_LOGE(TAG, "LITTLEFS start niet. Staat de spiffs-partitie uit "
			"partitions.csv er wel op? Er is niets gewist.");
		return;
	}

	ESP_LOGI(TAG, "bestandssysteem: %u bytes totaal, %u in gebruik.",
		(unsigned)LITTLEFS.totalBytes(), (unsigned)LITTLEFS.usedBytes());

#if CLEAR_HALL_OF_FAME_SELFTEST
	writeTestList();
#endif

	// Eerst laten zien wat er stond. Wie dit per ongeluk draait, ziet in het log
	// nog terug wat hij kwijt is.
	hallOfFameBefore.load();
	printHallOfFame("voor: ", hallOfFameBefore);

	// Nu onthouden, want clear() hieronder maakt het antwoord onbruikbaar.
	bool bWasFilled = !bIsEmpty(hallOfFameBefore);
	if (!bWasFilled)
	{
		ESP_LOGI(TAG, "de lijst was al leeg; hij wordt toch opnieuw geschreven.");
	}

	hallOfFameBefore.clear();
	hallOfFameBefore.save();

	// En nu het oordeel: opnieuw inlezen vanaf de partitie, met een tweede
	// object. Dat het geheugen leeg is bewijst niets - het gaat erom dat wat er
	// op de flash staat leeg is, en dat is precies wat de app straks leest.
	hallOfFameAfter.load();
	printHallOfFame("na:   ", hallOfFameAfter);

	if (bIsEmpty(hallOfFameAfter))
	{
		ESP_LOGI(TAG, "GEWIST. %s bevat een lege top-5.", strHallOfFameFilename);
	}
	else
	{
		ESP_LOGE(TAG, "MISLUKT. %s bevat nog steeds gegevens.", strHallOfFameFilename);
	}

#if CLEAR_HALL_OF_FAME_SELFTEST
	// Het oordeel over het gereedschap zelf, en dat is strenger: er moet ook
	// echt iets te wissen zijn geweest.
	if (bIsEmpty(hallOfFameAfter) && bWasFilled)
	{
		ESP_LOGW(TAG, "ZELFTEST GESLAAGD: een gevulde lijst is gelezen en gewist.");
	}
	else
	{
		ESP_LOGE(TAG, "ZELFTEST GEFAALD.");
	}
#endif

	ESP_LOGI(TAG, "klaar. Zet main.cpp weer op de app.");
}

void loop()
{
	vTaskDelay(1000);	// het werk is in setup() gedaan.
}

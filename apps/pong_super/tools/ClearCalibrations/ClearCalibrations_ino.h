// by Marius Versteegen, 2026

// Gereedschap: de touchkalibratie van dit bord wissen.
//
// De kalibratie leeft in /TouchCalData1 op de spiffs-partitie, naast de top-5
// van de app. Sinds formaat v2 zitten daar alle VIER de schermrotaties naast
// elkaar in, elk met een geldigheidsbit; zie de kop van
// crt_TFT_eSPI_DisplayAdapter.h. Dit programma meldt eerst welke rotaties er
// gekalibreerd waren en gooit het bestand daarna weg.
//
// LET OP - dit is niet ongedaan te maken zonder vinger. Na afloop vraagt elke
// app bij het opstarten weer om een kalibratie voor zijn eigen rotatie. Draai
// daarna het voorbeeld TouchCalibrateAll, dan zijn alle vier de rotaties in een
// keer weer klaar.
//
// Waarom weggooien en niet, zoals ClearHallOfFame, een leeg bestand
// terugschrijven: dan zou dit programma de indeling van v2 moeten kennen en
// zelf schrijven. Die indeling staat op precies een plek, en dat hoort zo. Een
// ontbrekend bestand is bovendien de weg die de adapter zelf al netjes afhandelt
// ("geen kalibratiebestand ..."), dus er valt niets te winnen met een variant
// die hij verder nergens tegenkomt. Lezen doet dit programma wel, want dat is
// wat het oordeel hieronder de moeite waard maakt.
//
// Wat het NIET doet: LITTLEFS.format(). Dat neemt /hallOfFame mee en alles wat
// er verder ooit bij komt.
//
// Gebruik: zet in main/main.cpp deze regel aan in plaats van de app, bouw en
// flash. Daarna main.cpp weer terugzetten.
//
//     #include <ClearCalibrations.ino>

#include <crt_CleanRTOS.h>
#include <LITTLEFS.h>

namespace clear_calibrations
{
	crt::MainInits mainInits;		// Initialize CleanRTOS.

	static const char* TAG = "clearCalibrations";

	// Hetzelfde pad dat elke app aan zijn DisplayAdapter meegeeft. Loopt dat uit
	// elkaar, dan wist dit gereedschap een bestand dat niemand leest - vandaar
	// dat de uitvoer de naam meldt.
	const char* strCalibrationFilename = "/TouchCalData1";

	// Overgenomen uit crt_TFT_eSPI_DisplayAdapter.h, waar de indeling thuishoort.
	// Alleen wat nodig is om te LEZEN: dit programma schrijft niets terug, dus
	// als de indeling daar verschuift levert dat hier hooguit een "niet herkend"
	// op en nooit een verminkt bestand.
	const int NofRotations      = 4;
	const int NofCalValues      = 5;
	const int CalFileVersion    = 2;
	const int CalFileSizeV1     = 1 + NofCalValues * 2;								// 11
	const int CalFileHeaderSize = 6;
	const int CalFileSizeV2     = CalFileHeaderSize + NofRotations * NofCalValues * 2;	// 46

	// Meldt wat er stond. Wie dit per ongeluk draait, ziet in het log nog terug
	// hoeveel werk hij zichzelf bezorgd heeft.
	//
	// Geeft terug of er iets bruikbaars in stond; dat is straks de helft van het
	// oordeel.
	bool bReportCalibrations()
	{
		if (!LITTLEFS.exists(strCalibrationFilename))
		{
			ESP_LOGI(TAG, "voor: er is geen %s; er valt niets te wissen.",
				strCalibrationFilename);
			return false;
		}

		fs::File f = LITTLEFS.open(strCalibrationFilename, FILE_READ);
		if (!f)
		{
			ESP_LOGE(TAG, "voor: %s bestaat maar gaat niet open.", strCalibrationFilename);
			return false;
		}

		uint8_t buf[CalFileSizeV2] = {};
		size_t nofRead = f.read(buf, sizeof(buf));
		f.close();

		if ((nofRead >= (size_t)CalFileSizeV2) &&
			(buf[0] == 'C') && (buf[1] == 'G') && (buf[2] == 'T') && (buf[3] == 'C') &&
			(buf[4] == (uint8_t)CalFileVersion))
		{
			uint8_t validMask = buf[5];
			ESP_LOGI(TAG, "voor: %s, formaat v%d, geldigheidsmasker 0x%02X.",
				strCalibrationFilename, CalFileVersion, (unsigned)validMask);

			for (int r = 0; r < NofRotations; r++)
			{
				ESP_LOGI(TAG, "voor:   rotatie %d: %s", r,
					((validMask & (1 << r)) != 0) ? "gekalibreerd" : "-");
			}
			return (validMask != 0);
		}

		// Exact 11 bytes en een eerste byte onder de 4: het oude formaat, met
		// een enkele rotatie erin. Zie dezelfde afweging in loadCalTable().
		if ((nofRead == (size_t)CalFileSizeV1) && (buf[0] < (uint8_t)NofRotations))
		{
			ESP_LOGI(TAG, "voor: %s, oud formaat v1, alleen rotatie %d.",
				strCalibrationFilename, (int)buf[0]);
			return true;
		}

		// Bijvoorbeeld het bestand van 14 bytes dat Button_demo.ino van
		// TFT_eWidget naar hetzelfde pad schrijft. Ook dat mag weg.
		ESP_LOGW(TAG, "voor: %s is %d bytes en wordt niet herkend; het gaat toch weg.",
			strCalibrationFilename, (int)nofRead);
		return true;
	}
}

void setup()
{
	using namespace clear_calibrations;

	vTaskDelay(100);	// de seriele uitvoer op gang laten komen.

	ESP_LOGI(TAG, "=== touchkalibratie wissen: %s ===", strCalibrationFilename);

	// formatOnFail blijft uit (de standaard). Formatteren zou hier extra zuur
	// zijn: dan is de top-5 ook weg terwijl daar een eigen gereedschap voor is.
	if (!LITTLEFS.begin())
	{
		ESP_LOGE(TAG, "LITTLEFS start niet. Staat de spiffs-partitie uit "
			"partitions.csv er wel op? Er is niets gewist.");
		return;
	}

	ESP_LOGI(TAG, "bestandssysteem: %u bytes totaal, %u in gebruik.",
		(unsigned)LITTLEFS.totalBytes(), (unsigned)LITTLEFS.usedBytes());

	bool bHadCalibration = bReportCalibrations();

	LITTLEFS.remove(strCalibrationFilename);

	// Het oordeel komt van een verse vraag aan het bestandssysteem, niet van de
	// terugwaarde van remove(): het gaat erom wat de volgende app aantreft.
	bool bGone = !LITTLEFS.exists(strCalibrationFilename);

	ESP_LOGI(TAG, "na:   %s bestaat %s.", strCalibrationFilename,
		bGone ? "niet meer" : "NOG STEEDS");
	ESP_LOGI(TAG, "bestandssysteem: %u bytes totaal, %u in gebruik.",
		(unsigned)LITTLEFS.totalBytes(), (unsigned)LITTLEFS.usedBytes());

	if (!bGone)
	{
		ESP_LOGE(TAG, "MISLUKT. De kalibratie staat er nog.");
	}
	else if (bHadCalibration)
	{
		ESP_LOGI(TAG, "GEWIST. Er stond een kalibratie en die is weg.");
	}
	else
	{
		ESP_LOGI(TAG, "GEWIST, maar er stond niets bruikbaars in.");
	}

	ESP_LOGI(TAG, "klaar. Draai nu het voorbeeld TouchCalibrateAll om alle vier "
		"de rotaties in een keer terug te zetten, en zet main.cpp daarna op de app.");
}

void loop()
{
	vTaskDelay(1000);	// het werk is in setup() gedaan.
}

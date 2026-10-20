// by Marius Versteegen, 2026

// Gereedschap: uitzoeken welke displaycontroller er op een CYD zit.
//
// Aanleiding: op een nieuwere CYD (merk Inim) liep de touchkalibratie mis -
// alleen de eerste pijl en de tekst stonden goed, de rest kwam als streepjes op
// verkeerde plekken, en de zwarte achtergrond was een vierkant. TFT_eSPI is hier
// gebouwd voor een ILI9341 (libs/TFT_eSPI/User_Setup.h). Vermoeden: de nieuwe
// heeft een andere controller, bijvoorbeeld een ST7789.
//
// Wat dit doet:
// 1. De identificatieregisters van de controller uitlezen en loggen. Dat gaat
//    hier met een eigen, trage leesroutine en niet met TFT_eSPI::readcommand8():
//    die stuurt eerst het ILI9341-specifieke 0xD9-indexcommando, en dat is juist
//    wat we nog niet weten.
// 2. Elke rotatie (0..3) om de beurt tekenen: een kader langs de rand, een
//    gekleurd blok in elke hoek en een regel tekst. Daarbij het MADCTL-register
//    teruglezen, zodat te zien is of de controller de rotatie overneemt.
//
// Wat je moet zien bij een goed werkend scherm, per rotatie:
// - een wit kader precies langs alle vier de randen;
// - linksboven ROOD, rechtsboven GROEN, linksonder BLAUW, rechtsonder GEEL;
// - de tekst leesbaar, niet gespiegeld.
// Wijkt het af (kleuren omgewisseld of geinverteerd, kader niet rondom,
// gespiegeld), noteer dan per rotatie wat je ziet.
//
// Gebruik: zet in main/main.cpp deze regel aan in plaats van de app, bouw en
// flash, en bewaar de seriele uitvoer. Daarna main.cpp weer terugzetten.
//
//     #include <DisplayId.ino>
//
// Raakt het bestandssysteem niet aan, dus ook de touchkalibratie niet.

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

namespace display_id
{
	static const char* TAG = "displayId";

	TFT_eSPI tft;

	// Langzaam genoeg voor elke controller; lezen mag bij beide maar ~6..15 MHz.
	const uint32_t ReadFrequency = 1000000;

	// Stuurt cmd en leest daarna 4 bytes. Geeft ze terug als 32 bit, de eerst
	// gelezen byte bovenaan. Bij de meeste leescommando's van ILI9341 en ST7789
	// zit er in 4-draads SPI een dummyklok tussen; vandaar dat de aanroeper ook
	// de een bit verschoven versie logt.
	uint32_t rawRead32(uint8_t cmd)
	{
		SPIClass& spi = TFT_eSPI::getSPIinstance();
		spi.beginTransaction(SPISettings(ReadFrequency, MSBFIRST, SPI_MODE0));
		digitalWrite(TFT_CS, LOW);
		digitalWrite(TFT_DC, LOW);
		spi.transfer(cmd);
		digitalWrite(TFT_DC, HIGH);
		uint32_t value = 0;
		for (int i = 0; i < 4; i++)
		{
			value = (value << 8) | spi.transfer(0);
		}
		digitalWrite(TFT_CS, HIGH);
		spi.endTransaction();
		return value;
	}

	void logRead(uint8_t cmd, const char* strName)
	{
		uint32_t raw = rawRead32(cmd);
		uint32_t shifted = raw << 1;	// voor als er een dummyklok voor zat
		ESP_LOGI(TAG, "0x%02X %-34s ruw %02X %02X %02X %02X   na 1 dummybit %02X %02X %02X",
			cmd, strName,
			(unsigned)(raw >> 24) & 0xFF, (unsigned)(raw >> 16) & 0xFF, (unsigned)(raw >> 8) & 0xFF, (unsigned)raw & 0xFF,
			(unsigned)(shifted >> 24) & 0xFF, (unsigned)(shifted >> 16) & 0xFF, (unsigned)(shifted >> 8) & 0xFF);
	}

	void logIds()
	{
		ESP_LOGI(TAG, "--- identificatie ---");
		logRead(0x04, "RDDID     (ST7789: 85 85 52)");
		logRead(0x09, "RDDST     status");
		logRead(0x0A, "RDDPM     power mode");
		logRead(0x0B, "RDDMADCTL rotatie/kleurvolgorde");
		logRead(0x0C, "RDDCOLMOD pixelformaat (55 = 16 bit)");
		logRead(0xDA, "RDID1     (ST7789: 85)");
		logRead(0xDB, "RDID2     (ST7789: 85)");
		logRead(0xDC, "RDID3     (ST7789: 52)");
		logRead(0xD3, "RDID4     (ILI9341: xx 00 93 41)");

		// En de ILI9341-manier, via TFT_eSPI zelf.
		uint8_t id1 = tft.readcommand8(0xD3, 1);
		uint8_t id2 = tft.readcommand8(0xD3, 2);
		uint8_t id3 = tft.readcommand8(0xD3, 3);
		ESP_LOGI(TAG, "0xD3 via TFT_eSPI::readcommand8 (ILI9341: 00 93 41): %02X %02X %02X",
			(unsigned)id1, (unsigned)id2, (unsigned)id3);

		ESP_LOGI(TAG, "TFT_eSPI: readPanelId %06X, panel-rotatie-offset %u",
			(unsigned)tft.readPanelId(), (unsigned)tft.getPanelRotationOffset());
	}

	// Eerste meting op de nieuwe CYD: met de ILI9341-waarden kwam rotatie 2 (MY,
	// zonder MV) rechtop en liggend in beeld, 320 breed. Dit paneel staat dus van
	// zichzelf liggend, waar de ILI9341 staand is. Proef: per rotatie de
	// MADCTL-waarde van de ILI9341 voor rotatie (r+1)%4 gebruiken; breedte en
	// hoogte laat TFT_eSPI gewoon bij r. Dat klopte (2026-10-20), en zit nu in
	// TFT_eSPI zelf: init() herkent het paneel aan zijn ID en zet
	// setPanelRotationOffset(1). Daarom staat de proef hier uit.
	// Op 1 zetten om de proef met de hand te herhalen, bovenop de offset van TFT_eSPI.
	#define DISPLAYID_SHIFTED_MADCTL 0

	const uint8_t arIliMadctl[4] = {
		TFT_MAD_MX | TFT_MAD_COLOR_ORDER,
		TFT_MAD_MV | TFT_MAD_COLOR_ORDER,
		TFT_MAD_MY | TFT_MAD_COLOR_ORDER,
		TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER };

	void drawTestPattern(uint8_t rotation)
	{
		tft.setRotation(rotation);
#if DISPLAYID_SHIFTED_MADCTL
		tft.writecommand(TFT_MADCTL);
		tft.writedata(arIliMadctl[(rotation + 1) % 4]);
#endif
		int w = tft.width();
		int h = tft.height();
		const int s = 40;

		tft.fillScreen(TFT_BLACK);
		tft.drawRect(0, 0, w, h, TFT_WHITE);
		tft.fillRect(1, 1, s, s, TFT_RED);					// linksboven
		tft.fillRect(w - 1 - s, 1, s, s, TFT_GREEN);		// rechtsboven
		tft.fillRect(1, h - 1 - s, s, s, TFT_BLUE);		// linksonder
		tft.fillRect(w - 1 - s, h - 1 - s, s, s, TFT_YELLOW);	// rechtsonder

		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.setTextFont(2);
		tft.setCursor(s + 10, s + 10);
		tft.printf("rotatie %d: %d x %d, offset %u%s", (int)rotation, w, h,
			(unsigned)tft.getPanelRotationOffset(), DISPLAYID_SHIFTED_MADCTL ? " (verschoven)" : "");
		tft.setCursor(s + 10, s + 30);
		tft.print("LB rood, RB groen,");
		tft.setCursor(s + 10, s + 50);
		tft.print("LO blauw, RO geel");

		// Een diagonaal, zoals de kalibratiepijlen: die tekenen met drawLine.
		tft.drawLine(s, s, w - s, h - s, TFT_MAGENTA);

		// Kleurvolgorde: elk woord in zijn eigen kleur. Bij omgewisseld rood en
		// blauw staat ROOD in blauw, CYAAN in geel, enzovoort.
		const struct { const char* strName; uint16_t color; } arBars[] = {
			{ "ROOD", TFT_RED }, { "GROEN", TFT_GREEN }, { "BLAUW", TFT_BLUE },
			{ "CYAAN", TFT_CYAN }, { "GEEL", TFT_YELLOW }, { "MAGENTA", TFT_MAGENTA } };
		tft.setTextFont(4);
		int y = s + 75;
		for (int i = 0; i < 6; i++)
		{
			tft.setTextColor(arBars[i].color, TFT_BLACK);
			tft.setCursor(s + 10 + (i % 2) * 120, y + (i / 2) * 28);
			tft.print(arBars[i].strName);
		}

		uint32_t madctl = rawRead32(0x0B);
		ESP_LOGI(TAG, "rotatie %d: %d x %d getekend; MADCTL teruggelezen ruw %08X (na 1 dummybit %02X)",
			(int)rotation, w, h, (unsigned)madctl, (unsigned)((madctl << 1) >> 24) & 0xFF);
	}
}

// ---------------------------------------------------------------------------
// Gammaproef. Op de nieuwe CYD (Inim) kwam het GAME OVER-label van pong_cg -
// rood op donkergrijs (0x202020) - als roze op middelgrijs in beeld: de
// middentonen te licht. De ILI9341-init zet een eigen gammacurve (0xE0/0xE1),
// en die past kennelijk niet op deze controller. Hier wisselen vier varianten
// elkaar om de 8 s af, elk met dezelfde testpagina; kies de variant die het
// meest op de oude CYD lijkt. Uitkomst (2026-10-20): variant B, de ST7789-curve.
// Die zet TFT_eSPI::init() nu zelf voor dit paneel; let op dat variant A
// (tft.init()) daardoor nu ook al de ST7789-curve heeft.
// Op 1 zetten voor de gammaproef, op 0 voor de gewone rotatieproef hierboven.
#define DISPLAYID_GAMMA_TEST 0

namespace display_id
{
	void writeCommandWithData(uint8_t cmd, const uint8_t* arData, int nofData)
	{
		tft.writecommand(cmd);
		for (int i = 0; i < nofData; i++)
		{
			tft.writedata(arData[i]);
		}
	}

	// Uit TFT_Drivers/ILI9341_Init.h (de huidige), ILI9341_2 en ST7789_Init.h.
	const uint8_t arIliGammaP[15]  = { 0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00 };
	const uint8_t arIliGammaN[15]  = { 0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F };
	const uint8_t arIli2GammaP[15] = { 0x0F,0x2A,0x28,0x08,0x0E,0x08,0x54,0xA9,0x43,0x0A,0x0F,0x00,0x00,0x00,0x00 };
	const uint8_t arIli2GammaN[15] = { 0x00,0x15,0x17,0x07,0x11,0x06,0x2B,0x56,0x3C,0x05,0x10,0x0F,0x3F,0x3F,0x0F };
	const uint8_t arStGammaP[14]   = { 0xD0,0x00,0x02,0x07,0x0A,0x28,0x32,0x44,0x42,0x06,0x0E,0x12,0x14,0x17 };
	const uint8_t arStGammaN[14]   = { 0xD0,0x00,0x02,0x07,0x0A,0x28,0x31,0x54,0x47,0x0E,0x1C,0x17,0x1B,0x1E };

	void drawGammaPage(const char* strVariant)
	{
		tft.setRotation(1);
		int w = tft.width();
		tft.fillScreen(TFT_BLACK);

		tft.setTextFont(2);
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.setCursor(4, 2);
		tft.print(strVariant);

		// Grijstrap en roodtrap, 16 stappen van 0 tot 255.
		const int nofSteps = 16;
		int barW = w / nofSteps;
		for (int i = 0; i < nofSteps; i++)
		{
			uint8_t v = (uint8_t)(i * 17);
			tft.fillRect(i * barW, 22, barW, 50, tft.color565(v, v, v));
			tft.fillRect(i * barW, 74, barW, 30, tft.color565(v, 0, 0));
		}

		// Zoals het GAME OVER-label in pong_cg: rood op 0x202020.
		tft.fillRoundRect(32, 120, w - 64, 90, 16, tft.color565(0x20, 0x20, 0x20));
		tft.setTextFont(4);
		tft.setTextColor(TFT_RED, tft.color565(0x20, 0x20, 0x20));
		tft.setCursor(90, 152);
		tft.print("GAME OVER");
	}

	void showGammaVariant(int variant)
	{
		switch (variant)
		{
			case 0:
				tft.init();		// alles terug naar de gewone init, inclusief de ILI9341-gamma
				drawGammaPage("A: ILI9341-gamma (zoals nu)");
				break;
			case 1:
				writeCommandWithData(0xE0, arStGammaP, 14);
				writeCommandWithData(0xE1, arStGammaN, 14);
				drawGammaPage("B: ST7789-gamma");
				break;
			case 2:
				writeCommandWithData(0xE0, arIli2GammaP, 15);
				writeCommandWithData(0xE1, arIli2GammaN, 15);
				drawGammaPage("C: ILI9341_2-gamma");
				break;
			case 3:
				// Software-reset: alle registers terug naar de fabriekswaarden van
				// de controller, en dan alleen het hoognodige.
				tft.writecommand(0x01);
				delay(150);
				tft.writecommand(0x11);		// sleep out
				delay(120);
				{
					const uint8_t colmod = 0x55;
					writeCommandWithData(0x3A, &colmod, 1);
				}
				tft.writecommand(0x29);		// display on
				drawGammaPage("D: fabrieksinstelling (reset)");
				break;
		}
		ESP_LOGI(TAG, "gammavariant %c op het scherm", 'A' + variant);
	}
}

void setup()
{
	using namespace display_id;

	vTaskDelay(100);	// de seriele uitvoer op gang laten komen.
	ESP_LOGI(TAG, "=== displaycontroller identificeren ===");

#ifdef TFT_BL
	pinMode(TFT_BL, OUTPUT);
	digitalWrite(TFT_BL, HIGH);
#endif

	tft.init();
	logIds();
}

void loop()
{
	using namespace display_id;

#if DISPLAYID_GAMMA_TEST
	static int variant = 0;
	showGammaVariant(variant);
	variant = (variant + 1) % 4;
	vTaskDelay(pdMS_TO_TICKS(8000));
	return;
#endif

	static uint8_t rotation = 0;
	drawTestPattern(rotation);
	rotation = (rotation + 1) % 4;

	if (rotation == 0)
	{
		logIds();	// na een volle ronde nog eens, voor de zekerheid.
	}
	vTaskDelay(pdMS_TO_TICKS(6000));
}

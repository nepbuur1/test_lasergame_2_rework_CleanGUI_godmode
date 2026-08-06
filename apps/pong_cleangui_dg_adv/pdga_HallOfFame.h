// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <string.h>
#include <LITTLEFS.h>

// De top-5 uit UC03_GameOverHandling, met zijn opslag.
//
// Het ontwerp zegt "spif geheugen"; in dit project draait daar LITTLEFS op - zie
// partitions.csv en crt_TFT_eSPI_DisplayAdapter.h, dat de touchkalibratie in
// dezelfde partitie bewaart. Aanhaken bij wat er al staat is hier belangrijker
// dan de letterlijke naam: een tweede bestandssysteem op dezelfde partitie gaat
// niet samen.
//
// Alles is geprealloceerd: vijf ingangen, vaste namen, geen heap.

using namespace crt;
namespace pong_cleangui_dg_adv
{
	const int NofHallOfFameEntries = 5;

	// Twintig letters. Op de opslag maakt dat vrijwel niets uit - zie de melding
	// in load() - en op het scherm past het nog net: de regel wordt
	// "1  <20 tekens>  1234", zo'n 29 tekens bij font 2, en dat is ruim binnen de
	// 320 pixels.
	const int MaxNofNameChars = 20;

	struct HallOfFameEntry
	{
		HallOfFameEntry():score(0) { strName[0] = '\0'; }

		char strName[MaxNofNameChars + 1];
		int32_t score;
	};

	// De lijst als los kopieerbaar geheel, om hem via een crt::Pool van de ene
	// taak naar de andere te krijgen: GameOverControl bezit de HallOfFame, maar
	// GameOverDisplayControl tekent hem.
	struct HallOfFameSnapshot
	{
		HallOfFameEntry arEntry[NofHallOfFameEntries];
	};

	class HallOfFame
	{
	private:
		// Het bestand draagt een versiebyte. Zonder dat leest een lijst van een
		// oudere indeling als geldige onzin, en dat is erger dan een lege lijst.
		static const uint8_t FileVersion = 1;

		const char* strFilename;
		HallOfFameEntry arEntry[NofHallOfFameEntries];

	public:
		HallOfFame(const char* strFilename) : strFilename(strFilename)
		{
		}

		const HallOfFameEntry& getEntry(int index) const
		{
			assert((index >= 0) && (index < NofHallOfFameEntries));
			return arEntry[index];
		}

		// Verdient deze score een plek? De lijst staat aflopend, dus alleen de
		// laatste telt. Een score van nul nooit - dan zou elk spel waarin je niets
		// raakt om een naam vragen.
		bool qualifies(int32_t score) const
		{
			return (score > 0) && (score > arEntry[NofHallOfFameEntries - 1].score);
		}

		// Zet de score op zijn plek en schuift de rest een op. Bij gelijke score
		// komt de nieuwe eronder: wie er eerder stond, stond er eerder.
		void insert(const char* strName, int32_t score)
		{
			int indexInsert = NofHallOfFameEntries - 1;
			while ((indexInsert > 0) && (arEntry[indexInsert - 1].score < score))
			{
				arEntry[indexInsert] = arEntry[indexInsert - 1];
				indexInsert--;
			}

			HallOfFameEntry& entry = arEntry[indexInsert];
			entry.score = score;
			strncpy(entry.strName, strName, MaxNofNameChars);
			entry.strName[MaxNofNameChars] = '\0';

			// Een lege naam zou een gat in de lijst achterlaten waar niets van te
			// zien is. Dan liever iets neutraals dat wel leest.
			if (entry.strName[0] == '\0')
			{
				strncpy(entry.strName, "anoniem", MaxNofNameChars);
				entry.strName[MaxNofNameChars] = '\0';
			}
		}

		// Uit het bestandssysteem lezen. Ontbreekt het bestand of klopt het niet,
		// dan blijft de lijst leeg - dat is geen fout maar een eerste keer.
		void load()
		{
			if (!beginFileSystem())
			{
				return;
			}

			// Een keer melden wat er aan ruimte is. De vraag "past de top-5 wel in
			// de spiffs-partitie" is met een rekensom niet hard te maken: LittleFS
			// werkt in blokken van een flash-sector, dus een bestand van 126 bytes
			// kost er net zo goed een. Dit zijn de echte getallen.
			ESP_LOGI("hallOfFame", "bestandssysteem: %u bytes totaal, %u in gebruik, "
				"%u vrij; de top-5 kost %d bytes aan gegevens.",
				(unsigned)LITTLEFS.totalBytes(), (unsigned)LITTLEFS.usedBytes(),
				(unsigned)(LITTLEFS.totalBytes() - LITTLEFS.usedBytes()),
				(int)(1 + NofHallOfFameEntries * (MaxNofNameChars + 1 + 4)));

			fs::File f = LITTLEFS.open(strFilename, "r");
			if (!f)
			{
				ESP_LOGI("hallOfFame", "geen %s; de lijst begint leeg.", strFilename);
				return;
			}

			bool bOk = (f.read() == FileVersion);
			for (int i = 0; bOk && (i < NofHallOfFameEntries); i++)
			{
				for (int c = 0; bOk && (c <= MaxNofNameChars); c++)
				{
					int byte = f.read();
					if (byte < 0) { bOk = false; break; }
					arEntry[i].strName[c] = (char)byte;
				}
				arEntry[i].strName[MaxNofNameChars] = '\0';

				int32_t score = 0;
				for (int b = 0; bOk && (b < 4); b++)
				{
					int byte = f.read();
					if (byte < 0) { bOk = false; break; }
					score |= ((int32_t)(byte & 0xFF)) << (8 * b);
				}
				arEntry[i].score = score;
			}
			f.close();

			if (!bOk)
			{
				ESP_LOGE("hallOfFame", "%s is onleesbaar of van een andere versie; "
					"de lijst begint leeg.", strFilename);
				for (int i = 0; i < NofHallOfFameEntries; i++)
				{
					arEntry[i] = HallOfFameEntry();
				}
			}
		}

		void save()
		{
			if (!beginFileSystem())
			{
				return;
			}

			fs::File f = LITTLEFS.open(strFilename, FILE_WRITE);
			if (!f)
			{
				ESP_LOGE("hallOfFame", "%s kan niet worden geschreven.", strFilename);
				return;
			}

			f.write(FileVersion);
			for (int i = 0; i < NofHallOfFameEntries; i++)
			{
				for (int c = 0; c <= MaxNofNameChars; c++)
				{
					f.write((uint8_t)arEntry[i].strName[c]);
				}
				for (int b = 0; b < 4; b++)
				{
					f.write((uint8_t)((arEntry[i].score >> (8 * b)) & 0xFF));
				}
			}
			f.close();
		}

	private:
		// LITTLEFS is normaal al aangekoppeld door de display-adapter, die de
		// touchkalibratie uit dezelfde partitie leest. Toch hier nog een keer:
		// deze klasse hoort niet stil te vallen omdat een ander object toevallig
		// eerder was.
		bool beginFileSystem()
		{
			if (LITTLEFS.begin())
			{
				return true;
			}

			ESP_LOGE("hallOfFame", "LITTLEFS start niet; de top-5 wordt niet bewaard.");
			return false;
		}
	}; // end class HallOfFame

};// end namespace pong_cleangui_dg_adv

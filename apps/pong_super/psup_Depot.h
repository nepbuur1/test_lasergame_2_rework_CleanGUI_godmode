// by Marius Versteegen, 2026

#pragma once
#include <crt_CleanRTOS.h>
#include <type_traits>
#include "psup_IDepot.h"
#include "psup_IDepotItem.h"
#include <crt_IHandlerListener.h>

// Een Depot bezit een vast aantal geprealloceerde objecten, geeft ze uit met
// obtain() en neemt ze terug met release(), en roept per ronde update() aan op
// alleen de exemplaren die in gebruik zijn.
//
// Een Depot is er voor EEN type. Depot<Bal,50> bevat alleen ballen; komen er
// mijnen bij, dan is dat een tweede Depot<Mijn,10>. Ze mogen zich alle twee bij
// dezelfde Handler aanmelden (aangenomen dat ze dezelfde update klokfrequentie hebben).
//
// Eisen aan T:
//   - afgeleid van IDepotItem. Daarmee zijn update() en reset() onderdeel van
//     het expliciete depotcontract in plaats van toevallige duck typing.
//   - een default constructor, want arItems[N] wordt in een keer gebouwd. Wat
//     een exemplaar verder nodig heeft, geef je het daarna met een eigen
//     init()-achtige functie via getItem().
//   - reset() wist alleen runtime-toestand en herstelt de toestand na init().
//
// Er wordt niets gealloceerd: geen new, geen delete, geen heap.

using namespace crt;
namespace pong_super
{
	template <typename T, unsigned int N> class Depot : public IDepot, public IHandlerListener
	{
		// ::std en niet std: CleanRTOS heeft een eigen crt::std (zie
		// internals/crt__std_Stack.h), en met het using namespace crt hierboven
		// is een kale std:: hier dubbelzinnig.
		static_assert(::std::is_base_of<IDepotItem, T>::value,
		              "Depot<T,N> vereist dat T van IDepotItem is afgeleid");

	public:
		static const int InvalidIndex = -1;

	private:
		const char* depotName;		// alleen voor de meldingen hieronder.
		T arItems[N];
		bool arInUse[N];
		int nofInUse;
		bool bExhaustionReported;	// zodat een vol depot niet elke ronde opnieuw logt.

	public:
		Depot(const char* depotName) : depotName(depotName), nofInUse(0), bExhaustionReported(false)
		{
			for (unsigned int i = 0; i < N; i++)
			{
				arInUse[i] = false;
			}
		}

		// Claimt een vrij exemplaar en geeft zijn index terug, niet een referentie.
		// De index is wat de aanroeper nodig heeft: hij wijst behalve het object
		// ook alles aan wat er in andere arrays naast ligt - het panel waarmee dit
		// object getekend wordt, bijvoorbeeld.
		//
		// Is alles uit, dan is dat geen fout maar een uitkomst: InvalidIndex.
		// Bewust geen assert, want een spel dat tien objecten ineens wil spawnen
		// komt daar vanzelf een keer aan, en dan hoort het spel door te lopen en
		// niet te stoppen.
		int obtain()
		{
			for (unsigned int i = 0; i < N; i++)
			{
				if (!arInUse[i])
				{
					arInUse[i] = true;
					nofInUse++;
					bExhaustionReported = false;
					return (int)i;
				}
			}

			if (!bExhaustionReported)
			{
				bExhaustionReported = true;
				ESP_LOGI("Depot", "%s: alle %u exemplaren zijn in gebruik.", depotName, N);
			}
			return InvalidIndex;
		}

		// Geeft een exemplaar terug aan het depot.
		//
		// Dit mag een exemplaar ook over zichzelf aanroepen, midden in zijn eigen
		// update(). Dat is veilig omdat arInUse nooit wordt samengeperst: de lus in
		// update() hieronder loopt op index en raakt niet in de war van een slot
		// dat onderweg vrijkomt.
		/*override*/ void release(int index)
		{
			if ((index < 0) || ((unsigned int)index >= N))
			{
				ESP_LOGE("Depot", "%s: release van index %d, buiten 0..%u.", depotName, index, N-1);
				return;
			}

			if (!arInUse[index])
			{
				ESP_LOGE("Depot", "%s: index %d werd al vrijgegeven.", depotName, index);
				return;
			}

			// Eerst het object zelf terugbrengen naar zijn herbruikbare ruststand.
			// reset() mag de vaste bedrading uit init() niet verwijderen.
			arItems[index].reset();
			arInUse[index] = false;
			nofInUse--;
		}

		// Alles in een keer terug. Bedoeld voor het opruimen tussen twee rondes
		// door.
		void releaseAll()
		{
			for (unsigned int i = 0; i < N; i++)
			{
				if (arInUse[i])
				{
					arItems[i].reset();
				}
				arInUse[i] = false;
			}
			nofInUse = 0;
			bExhaustionReported = false;
		}

		// Ook voor niet-uitgegeven exemplaren: bij het opstarten moet de eigenaar
		// ze alle N kunnen instellen, voordat er een is geclaimd.
		T& getItem(int index)
		{
			assert((index >= 0) && ((unsigned int)index < N));
			return arItems[index];
		}

		bool isInUse(int index)
		{
			return (index >= 0) && ((unsigned int)index < N) && arInUse[index];
		}

		int getNofInUse()
		{
			return nofInUse;
		}

		unsigned int getCapacity()
		{
			return N;
		}

		/*override*/ void update()
		{
			for (unsigned int i = 0; i < N; i++)
			{
				if (arInUse[i])
				{
					arItems[i].update();
				}
			}
		}
	}; // end class Depot
};// end namespace pong_super

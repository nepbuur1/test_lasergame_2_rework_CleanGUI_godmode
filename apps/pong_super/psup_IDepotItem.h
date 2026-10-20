// by Marius Versteegen, 2026

#pragma once

// Het contract voor objecten die door een Depot worden bezeten.
//
// update() is de niet-blokkerende stap die het Depot alleen voor uitgegeven
// exemplaren aanroept. reset() brengt uitsluitend de runtime-toestand terug naar
// de ruststand voor hergebruik; bedrading uit init(), zoals verwijzingen naar het
// Depot en andere collaborators, moet daarbij behouden blijven.
//
// Het Depot roept reset() aan bij zowel release() als releaseAll(). Een item hoeft
// dus niet zelf rekening te houden met de manier waarop zijn slot is vrijgekomen.

namespace pong_super
{
	class IDepotItem
	{
	public:
		virtual ~IDepotItem() = default;
		virtual void update() = 0;
		virtual void reset() = 0;
	};
};// end namespace pong_super


////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2003-2008 Martin Freij
// Copyright (C) 2020-2022 Rupert Carmichael
//
// This file is part of Nestopia.
//
// Nestopia is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Nestopia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Nestopia; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef NST_BOARD_UNL_MMC3BIGPRGROM_H
#define NST_BOARD_UNL_MMC3BIGPRGROM_H

#ifdef NST_PRAGMA_ONCE
#pragma once
#endif

namespace Nes
{
	namespace Core
	{
		namespace Boards
		{
			namespace Unlicensed
			{
				class NST_NO_VTABLE Mmc3BigPrgRom : public Mmc3
				{
				public:

					explicit Mmc3BigPrgRom(const Context& c)
					: Mmc3(c) {}

				private:
					void SubReset(bool);
					void SubSave(State::Saver&) const;
					void SubLoad(State::Loader&,dword);

					NES_DECL_POKE( 8001 );
				};
			}
		}
	}
}

#endif

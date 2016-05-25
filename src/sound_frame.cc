/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

    This file is part of libdcp.

    libdcp is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    libdcp is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libdcp.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file  src/sound_frame.cc
 *  @brief SoundFrame class.
 */

#include "sound_frame.h"
#include "exceptions.h"
#include "AS_DCP.h"
#include "KM_fileio.h"

using namespace std;
using namespace dcp;

SoundFrame::SoundFrame (boost::filesystem::path path, int n, ASDCP::AESDecContext* c)
{
	ASDCP::PCM::MXFReader reader;
	Kumu::Result_t r = reader.OpenRead (path.string().c_str());
	if (ASDCP_FAILURE (r)) {
		boost::throw_exception (FileError ("could not open MXF file for reading", path, r));
	}

	/* XXX: unfortunate guesswork on this buffer size */
	_buffer = new ASDCP::PCM::FrameBuffer (1 * Kumu::Megabyte);

	if (ASDCP_FAILURE (reader.ReadFrame (n, *_buffer, c))) {
		boost::throw_exception (DCPReadError ("could not read audio frame"));
	}
}

SoundFrame::~SoundFrame ()
{
	delete _buffer;
}

uint8_t const *
SoundFrame::data () const
{
	return _buffer->RoData();
}

int
SoundFrame::size () const
{
	return _buffer->Size ();
}

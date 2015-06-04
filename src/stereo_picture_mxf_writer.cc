/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "stereo_picture_mxf_writer.h"
#include "exceptions.h"
#include "dcp_assert.h"
#include "picture_mxf.h"
#include "AS_DCP.h"
#include "KM_fileio.h"

#include "picture_mxf_writer_common.cc"

using std::istream;
using std::ostream;
using std::string;
using boost::shared_ptr;
using namespace dcp;

struct StereoPictureMXFWriter::ASDCPState : public ASDCPStateBase
{
	ASDCP::JP2K::MXFSWriter mxf_writer;
};

StereoPictureMXFWriter::StereoPictureMXFWriter (PictureMXF* mxf, boost::filesystem::path file, Standard standard, bool overwrite)
	: PictureMXFWriter (mxf, file, standard, overwrite)
	, _state (new StereoPictureMXFWriter::ASDCPState)
	, _next_eye (EYE_LEFT)
{

}

void
StereoPictureMXFWriter::start (uint8_t* data, int size)
{
	dcp::start (this, _state, _standard, _picture_mxf, data, size);
	_picture_mxf->set_frame_rate (Fraction (_picture_mxf->edit_rate().numerator * 2, _picture_mxf->edit_rate().denominator));
}

/** Write a frame for one eye.  Frames must be written left, then right, then left etc.
 *  @param data JPEG2000 data.
 *  @param size Size of data.
 */
FrameInfo
StereoPictureMXFWriter::write (uint8_t* data, int size)
{
	DCP_ASSERT (!_finalized);

	if (!_started) {
		start (data, size);
	}

 	if (ASDCP_FAILURE (_state->j2k_parser.OpenReadFrame (data, size, _state->frame_buffer))) {
 		boost::throw_exception (MiscError ("could not parse J2K frame"));
 	}

	uint64_t const before_offset = _state->mxf_writer.Tell ();

	string hash;
	Kumu::Result_t r = _state->mxf_writer.WriteFrame (
		_state->frame_buffer,
		_next_eye == EYE_LEFT ? ASDCP::JP2K::SP_LEFT : ASDCP::JP2K::SP_RIGHT,
		_encryption_context,
		0,
		&hash
		);

	if (ASDCP_FAILURE (r)) {
		boost::throw_exception (MXFFileError ("error in writing video MXF", _file.string(), r));
	}

	_next_eye = _next_eye == EYE_LEFT ? EYE_RIGHT : EYE_LEFT;

	if (_next_eye == EYE_LEFT) {
		++_frames_written;
	}
			
	return FrameInfo (before_offset, _state->mxf_writer.Tell() - before_offset, hash);
}

void
StereoPictureMXFWriter::fake_write (int size)
{
	DCP_ASSERT (_started);
	DCP_ASSERT (!_finalized);

	Kumu::Result_t r = _state->mxf_writer.FakeWriteFrame (size, _next_eye == EYE_LEFT ? ASDCP::JP2K::SP_LEFT : ASDCP::JP2K::SP_RIGHT);
	if (ASDCP_FAILURE (r)) {
		boost::throw_exception (MXFFileError ("error in writing video MXF", _file.string(), r));
	}

	_next_eye = _next_eye == EYE_LEFT ? EYE_RIGHT : EYE_LEFT;
	if (_next_eye == EYE_LEFT) {
		++_frames_written;
	}
}

void
StereoPictureMXFWriter::finalize ()
{
	Kumu::Result_t r = _state->mxf_writer.Finalize();
	if (ASDCP_FAILURE (r)) {
		boost::throw_exception (MXFFileError ("error in finalizing video MXF", _file.string(), r));
	}

	_picture_mxf->_intrinsic_duration = _frames_written;
	PictureMXFWriter::finalize ();
}

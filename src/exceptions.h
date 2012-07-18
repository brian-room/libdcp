/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file  src/exceptions.h
 *  @brief Exceptions thrown by libdcp.
 */

namespace libdcp
{

/** @brief An exception related to a file */
class FileError : public std::exception
{
public:
	FileError (std::string const & message, std::string const & filename)
		: _message (message)
		, _filename (filename)
	{}
			    
	~FileError () throw () {}

	/** @return error message */
	char const * what () const throw () {
		return _message.c_str ();
	}
	
	/** @return filename of file that was involved */
	std::string filename () const {
		return _filename;
	}

private:
	/** error message */
	std::string _message;
	/** filename of file that was involved */
	std::string _filename;
};


/** @brief A miscellaneous exception */
class MiscError : public std::exception
{
public:
	MiscError (std::string const & message) : _message (message) {}
	~MiscError () throw () {}

	/** @return error message */
	char const * what () const throw () {
		return _message.c_str ();
	}

private:
	/** error message */
	std::string _message;
};

}
/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  src/signer_chain.h
 *  @brief Functions to make signer chains.
 */

#ifndef LIBDCP_CERTIFICATE_CHAIN_H
#define LIBDCP_CERTIFICATE_CHAIN_H

#include "certificate.h"
#include "types.h"
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

namespace xmlpp {
	class Node;
}

namespace dcp {

/** @class CertificateChain
 *  @brief A chain of any number of certificates, from root to leaf.
 */
class CertificateChain
{
public:
	CertificateChain () {}

	/** Create a chain of certificates for signing things.
	 *  @param openssl Name of openssl binary (if it is on the path) or full path.
	 *  @return Directory (which should be deleted by the caller) containing:
	 *    - ca.self-signed.pem      self-signed root certificate
	 *    - intermediate.signed.pem intermediate certificate
	 *    - leaf.key                leaf certificate private key
	 *    - leaf.signed.pem         leaf certificate
	 */
	CertificateChain (
		boost::filesystem::path openssl,
		std::string organisation = "example.org",
		std::string organisational_unit = "example.org",
		std::string root_common_name = ".smpte-430-2.ROOT.NOT_FOR_PRODUCTION",
		std::string intermediate_common_name = ".smpte-430-2.INTERMEDIATE.NOT_FOR_PRODUCTION",
		std::string leaf_common_name = "CS.smpte-430-2.LEAF.NOT_FOR_PRODUCTION"
		);

	void add (Certificate c);
	void remove (Certificate c);
	void remove (int);

	Certificate root () const;
	Certificate leaf () const;

	typedef std::list<Certificate> List;

	List leaf_to_root () const;
	List root_to_leaf () const;

	bool valid () const;
	bool attempt_reorder ();

	void sign (xmlpp::Element* parent, Standard standard) const;
	void add_signature_value (xmlpp::Node* parent, std::string ns) const;

	boost::optional<std::string> key () const {
		return _key;
	}

	void set_key (std::string k) {
		_key = k;
	}

private:
	friend class ::certificates;

	List _certificates;
	/** Leaf certificate's private key, if known */
	boost::optional<std::string> _key;
};

}

#endif

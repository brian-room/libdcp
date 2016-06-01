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

#include "dcp.h"
#include "exceptions.h"
#include "common.h"
#include "mxf.h"
#include <getopt.h>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <list>

using std::list;
using std::cerr;
using std::cout;
using std::string;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using namespace dcp;

static bool verbose = false;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION] <DCP> <DCP>\n"
	     << "  -V, --version                show libdcp version\n"
	     << "  -h, --help                   show this help\n"
	     << "  -v, --verbose                be verbose\n"
	     << "      --cpl-annotation-texts   allow differing CPL annotation texts\n"
	     << "      --reel-annotation-texts  allow differing reel annotation texts\n"
	     << "  -a, --annotation-texts       allow different CPL and reel annotation texts\n"
	     << "  -m, --mean-pixel             maximum allowed mean pixel error (default 5)\n"
	     << "  -s, --std-dev-pixel          maximum allowed standard deviation of pixel error (default 5)\n"
	     << "      --key                    hexadecimal key to use to decrypt MXFs\n"
	     << "  -k, --keep-going             carry on in the event of errors, if possible\n"
	     << "      --ignore-missing-assets  ignore missing asset files\n"
	     << "\n"
	     << "The <DCP>s are the DCP directories to compare.\n"
	     << "Comparison is of metadata and content, ignoring timestamps\n"
	     << "and differing UUIDs.\n";
}

void
note (NoteType t, string n)
{
	if (t == DCP_ERROR || verbose) {
		cout << " " << n << "\n";
		cout.flush ();
	}
}

DCP *
load_dcp (boost::filesystem::path path, bool keep_going, bool ignore_missing_assets, optional<string> key)
{
	DCP* dcp = 0;
	try {
		dcp = new DCP (path);
		DCP::ReadErrors errors;
		dcp->read (keep_going, &errors);
		filter_errors (errors, ignore_missing_assets);
		for (DCP::ReadErrors::const_iterator i = errors.begin(); i != errors.end(); ++i) {
			cerr << (*i)->what() << "\n";
		}

		if (key) {
			list<shared_ptr<Asset> > assets = dcp->assets ();
			for (list<shared_ptr<Asset> >::const_iterator i = assets.begin(); i != assets.end(); ++i) {
				shared_ptr<MXF> mxf = dynamic_pointer_cast<MXF> (*i);
				if (mxf) {
					mxf->set_key (Key (key.get ()));
				}
			}
		}

	} catch (FileError& e) {
		cerr << "Could not read DCP " << path.string() << "; " << e.what() << " " << e.filename() << "\n";
		exit (EXIT_FAILURE);
	}

	return dcp;
}

int
main (int argc, char* argv[])
{
	EqualityOptions options;
	options.max_mean_pixel_error = 5;
	options.max_std_dev_pixel_error = 5;
	options.reel_hashes_can_differ = true;
	options.reel_annotation_texts_can_differ = false;
	options.keep_going = false;
	bool ignore_missing_assets = false;
	optional<string> key;

	int option_index = 0;
	while (1) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 'V'},
			{ "help", no_argument, 0, 'h'},
			{ "verbose", no_argument, 0, 'v'},
			{ "mean-pixel", required_argument, 0, 'm'},
			{ "std-dev-pixel", required_argument, 0, 's'},
			{ "keep-going", no_argument, 0, 'k'},
			{ "annotation-texts", no_argument, 0, 'a'},
			/* From here we're using random capital letters for the short option */
			{ "ignore-missing-assets", no_argument, 0, 'A'},
			{ "cpl-annotation-texts", no_argument, 0, 'C'},
			{ "key", required_argument, 0, 'D'},
			{ "reel-annotation-texts", no_argument, 0, 'E'},
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "Vhvm:s:kaACD:E", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'V':
			cout << "dcpdiff version " << LIBDCP_VERSION << "\n";
			exit (EXIT_SUCCESS);
		case 'h':
			help (argv[0]);
			exit (EXIT_SUCCESS);
		case 'v':
			verbose = true;
			break;
		case 'm':
			options.max_mean_pixel_error = atof (optarg);
			break;
		case 's':
			options.max_std_dev_pixel_error = atof (optarg);
			break;
		case 'k':
			options.keep_going = true;
			break;
		case 'a':
			options.cpl_annotation_texts_can_differ = options.reel_annotation_texts_can_differ = true;
			break;
		case 'A':
			ignore_missing_assets = true;
			break;
		case 'B':
		case 'C':
			options.cpl_annotation_texts_can_differ = true;
			break;
		case 'D':
			key = string (optarg);
			break;
		case 'E':
			options.reel_annotation_texts_can_differ = true;
			break;
		}
	}

	if (argc <= optind || argc > (optind + 2)) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	if (!boost::filesystem::exists (argv[optind])) {
		cerr << argv[0] << ": DCP " << argv[optind] << " not found.\n";
		exit (EXIT_FAILURE);
	}

	if (!boost::filesystem::exists (argv[optind + 1])) {
		cerr << argv[0] << ": DCP " << argv[optind + 1] << " not found.\n";
		exit (EXIT_FAILURE);
	}

	DCP* a = load_dcp (argv[optind], options.keep_going, ignore_missing_assets, key);
	DCP* b = load_dcp (argv[optind + 1], options.keep_going, ignore_missing_assets, key);

	/* I think this is just below the LSB at 16-bits (ie the 8th most significant bit at 24-bit) */
	options.max_audio_sample_error = 255;

	bool const equals = a->equals (*b, options, boost::bind (note, _1, _2));

	exit (equals ? EXIT_SUCCESS : EXIT_FAILURE);
}

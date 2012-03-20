/**
 * Main program for running compression and decompression.
 *
 * @author jkataja
 */

#include <iostream>
#include <stdexcept>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include "pompom.hpp"

namespace po = boost::program_options;

using std::string;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::exception;
using std::flush;
using boost::str;
using boost::format;
using namespace pompom;

#define BUFSIZE 16384

int main(int argc, char** argv) {

	long len = -1;
	string model_args( str( format("%1%") % LimitDefault ) );

	// Improves iostream performance
	// http://stackoverflow.com/questions/5166263/how-to-get-iostream-to-perform-better
	setlocale(LC_ALL,"C");
	char inbuf[BUFSIZE];
	char outbuf[BUFSIZE];
	cin.rdbuf()->pubsetbuf(inbuf, BUFSIZE);
	cout.rdbuf()->pubsetbuf(outbuf, BUFSIZE);

	try {

		po::options_description desc("Usage: pompom [OPTION]");
		desc.add_options()
			("help,h", "show this help")
			("stdout,c", "compress to stdout (default)")
			("decompress,d", "decompress to stdout")
			("limit,l", po::value<string>(), 
				str( format("model memory limit in MiB (range %1%-%2%, default %3%)") 
					% LimitMin % LimitMax % LimitDefault).c_str()
			)
			;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);    

		if (vm.count("help")) {
			cerr << desc << "" << endl;
			return 1;
		}

		if (vm.count("limit")) {
			model_args = vm["limit"].as<string>();
		}

		// TODO input from file

		if (vm.count("decompress"))
			len = decompress(cin, cout, cerr);
		else
			len = compress(cin, cout, cerr, model_args);

	}
	catch (exception& e) {
		cerr << SELF << ": " << e.what() << endl << flush;
		return 1;
	}
	catch (...) {
		cerr << SELF << ": caught unknown exception" << endl << flush;
		return 1;
	}

	cout << flush;
	cerr << flush;

	return (len >= 0 ? 0 : 1);

}

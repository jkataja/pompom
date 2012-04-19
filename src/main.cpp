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

#define BUFSIZE 32768
#define USAGE "Usage: pompom [OPTION]...\n" \
	"Compress or decompress input using fixed-order PPM compression.\n" \
	"Reads from standard input and writes to standard output.\n" \
	"\n"

int main(int argc, char** argv) {

	long len = -1;

	// Should improve iostream performance
	// http://stackoverflow.com/questions/5166263/how-to-get-iostream-to-perform-better
	setlocale(LC_ALL,"C");
	char inbuf[BUFSIZE];
	char outbuf[BUFSIZE];
	cin.rdbuf()->pubsetbuf(inbuf, BUFSIZE);
	cout.rdbuf()->pubsetbuf(outbuf, BUFSIZE);

	try {
		string order_str( str( format(
			"compress: model order (range %1%-%2%, default %3%)") 
				% (int)OrderMin % (int)OrderMax % (int)OrderDefault));

		string mem_str( str( format(
			"compress: memory use MiB (range %1%-%2%, default %3%)") 
				% LimitMin % LimitMax % LimitDefault));

		po::options_description args("Options");
		args.add_options()
			( "stdout,c", "compress to stdout (default)" )
			( "decompress,d", "decompress to stdout" )
			( "help,h", "show this help" )
			( "order,o", 
				po::value<int>()->default_value((int)OrderDefault),
				order_str.c_str()
			)
			( "mem,m", 
				po::value<int>()->default_value((int)LimitDefault),
				mem_str.c_str()
			)
			;


        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).
                  options(args).run(), vm);
        po::notify(vm);

		// help
		if (vm.count("help") 
				|| (vm.count("stdout") && vm.count("decompress")) ) {
			cerr << USAGE << args << endl << flush;
			return 1;
		}

		if (vm.count("decompress"))
			len = decompress(cin, cout, cerr);
		else
			len = compress(cin, cout, cerr, vm["order"].as<int>(), 
				vm["mem"].as<int>());

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

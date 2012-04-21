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

using namespace pompom;

#define BUFSIZE 32768
#define USAGE "Usage: pompom [OPTION]...\n" \
	"Compress or decompress input using fixed-order PPM compression.\n" \
	"Reads from standard input and writes to standard output.\n" \
	"\n"

int main(int argc, char** argv) {

	long len = -1;

	// Should improve iostream performance
	// @see http://stackoverflow.com/questions/5166263/how-to-get-iostream-to-perform-better
	setlocale(LC_ALL,"C");
	char inbuf[BUFSIZE];
	char outbuf[BUFSIZE];
	std::cin.rdbuf()->pubsetbuf(inbuf, BUFSIZE);
	std::cout.rdbuf()->pubsetbuf(outbuf, BUFSIZE);

	try {
		std::string order_str( boost::str( boost::format(
			"compress: model order (range %1%-%2%, default %3%)") 
				% (int)OrderMin % (int)OrderMax % (int)OrderDefault));

		std::string mem_str( boost::str( boost::format(
			"compress: memory use MiB (range %1%-%2%, default %3%)") 
				% LimitMin % LimitMax % LimitDefault));

		po::options_description args("Options");
		args.add_options()
			( "stdout,c", "compress to stdout (default)" )
			( "decompress,d", "decompress to stdout" )
			( "help,h", "show this help" )
			( "count,n", 
				po::value<int>()->default_value((int)CountDefault),
				"compress: stop after n bytes"	
			)
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
			std::cerr << USAGE << args << std::endl << std::flush;
			return 1;
		}

		if (vm.count("decompress"))
			len = decompress(std::cin, std::cout, std::cerr);
		else
			len = compress(std::cin, std::cout, std::cerr, vm["order"].as<int>(), 
				vm["mem"].as<int>(), vm["count"].as<int>());

	}
	catch (std::exception& e) {
		std::cerr << SELF << ": " << e.what() << std::endl << std::flush;
		return 1;
	}
	catch (...) {
		std::cerr << SELF << ": caught unknown std::exception" << std::endl << std::flush;
		return 1;
	}

	std::cout << std::flush;
	std::cerr << std::flush;

	return (len >= 0 ? 0 : 1);

}

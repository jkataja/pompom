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
		std::string adapt_str( boost::str( boost::format(
			"compress: adaptation threshold in bits [%1%,%2%]") 
				% (int)AdaptMin % (int)AdaptMax));

		std::string bootstrap_str( boost::str( boost::format(
			"compress: bootstrap buffer size in KiB [%1%,%2%]") 
				% (int)BootMin % (int)BootMax));

		std::string order_str( boost::str( boost::format(
			"compress: model order [%1%,%2%]") 
				% (int)OrderMin % (int)OrderMax));

		std::string mem_str( boost::str( boost::format(
			"compress: memory use in MiB [%1%,%2%]") 
				% (int)LimitMin % (int)LimitMax));

		po::options_description args("Options");
		args.add_options()
			( "stdout,c", "compress to stdout (default)" )
			( "decompress,d", "decompress to stdout" )
			( "help,h", "show this help" )
			( "adapt,a", "compress: fast local adaptation" )
			( "adaptsize,A", 
				po::value<int>()->default_value(AdaptDefault),
				adapt_str.c_str()
			)
			( "reset,r", "compress: full reset model on memory limit" )
			( "bootsize,b", 
				po::value<int>()->default_value(BootDefault),
				bootstrap_str.c_str()
			)
			( "count,n", 
				po::value<long>()->default_value(CountDefault),
				"compress: stop after count bytes"	
			)
			( "order,o", 
				po::value<int>()->default_value(OrderDefault),
				order_str.c_str()
			)
			( "mem,m", 
				po::value<int>()->default_value(LimitDefault),
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
			len = compress(std::cin, std::cout, std::cerr, 
				vm["order"].as<int>(), 
				vm["mem"].as<int>(), 
				vm["count"].as<long>(), 
				(vm.count("reset") > 0),
				vm["bootsize"].as<int>(),
				(vm.count("adapt") > 0),
				vm["adaptsize"].as<int>()
			);

	}
	catch (std::exception& e) {
		std::cerr << SELF << ": " << e.what() << std::endl << std::flush;
		return 1;
	}
	catch (...) {
		std::cerr << SELF << ": caught unknown std::exception" 
			<< std::endl << std::flush;
		return 1;
	}

	std::cout << std::flush;
	std::cerr << std::flush;

	return (len >= 0 ? 0 : 1);

}

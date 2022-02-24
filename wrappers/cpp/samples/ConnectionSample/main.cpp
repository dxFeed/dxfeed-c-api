/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Devexperts LLC.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 */

#include <iostream>
#include <thread>
#include <vector>

#include <Connection.hpp>
#include <Error.hpp>

#include <args.hxx>
#include <nonstd/variant.hpp>

//Prevents file names globbing (converting * to all files in the current dir)
#ifdef __MINGW64_VERSION_MAJOR
int _CRT_glob = 0;
#endif

int main(int argc, char **argv) {
	args::ArgumentParser parser("This is a connection sample program.",
								"Examples:\n"
								"DXFeedCppWrapperConnectionSample demo.dxfeed.com:7300 DAX:DBI MDAX:DBI");
	args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
	args::Positional<std::string> addressParameter(parser,
													"address",
													"The DXFeed server address, e.g. demo.dxfeed.com:7300. "
													"If you want to use file instead of server data just write "
													"there path to file e.g. path\\to\\raw.bin");
	args::PositionalList<std::string> symbolsParameter(parser, "symbols",
														"The trade symbols, e.g. C, MSFT, YHOO, IBM");

	try {
		parser.ParseCLI(argc, argv);
	} catch (const args::Help&) {
		std::cout << parser;
		return 0;
	} catch (const args::ParseError &e) {
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	} catch (const args::ValidationError &e) {
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	}

	if (!addressParameter || !symbolsParameter) {
		std::cerr << parser;
		return 2;
	}

	auto address = args::get(addressParameter);
	auto symbols = args::get(symbolsParameter);

	dxf_initialize_logger("ConnectionSample.log", true, true, true);
	std::cout << "Connecting...\n";
	//auto connection = Connection::create("demo.dxfeed.com:7300");
	auto connection = Connection::create(address);

	if (!connection) {
		std::cerr << "Error:" << Error::getLast() << "\n";

		return 3;
	}

	std::cout << "Connected\n";

	auto subscription = connection->createSubscription(DXF_ET_GREEKS | DXF_ET_UNDERLYING | DXF_ET_QUOTE);

	if (!subscription) {
		std::cerr << "Error:" << Error::getLast() << "\n";

		return 4;
	}

	std::cout << "Subscribed\n";

	auto result = subscription->attachListener([](const std::string& symbol, const std::vector<Subscription::EventType>& events) {
	  struct CommonVisitor {
		  std::string operator()(const Greeks& greeks) const {
			  return greeks.toString();
		  }

		  std::string operator()(const Underlying& underlying) const {
			  return underlying.toString();
		  }

		  std::string operator()(const Quote& quote) const {
			  return quote.toString();
		  }
	  };

	  CommonVisitor visitor;

	  fmt::print("Symbol = {}:\n", symbol);

	  for (const auto& e : events) {
		  fmt::print("{}\n", nonstd::visit(visitor, e));
	  }

	  std::cout << "\n";
	});

	if (!result) {
		std::cerr << "Error:" << Error::getLast() << "\n";

		return 6;
	}

	std::cout << "Listener attached\n";

	for (const auto& symbol : symbols) {
		if (!subscription->addSymbol(symbol)) {
			std::cerr << "Error:" << Error::getLast() << "\n";

			return 5;
		}
	}

	std::cout << "Added symbols\n";

	std::this_thread::sleep_for(std::chrono::hours(1));

	std::cout << "Disconnecting...\n";

	return 0;
}
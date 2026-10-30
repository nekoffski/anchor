#include <cstdio>
#include <exception>
#include <iostream>
#include <kcore/Log.hh>

#include "Application.hh"

using namespace anchor;

int main(int argc, const char** argv) {
    kc::log::setLogLevel(kc::log::Level::off);
    try {
        cli::Client client;
        return cli::Application{client, std::cout, std::cerr}.run(argc, argv);
    } catch (const std::exception& error) {
        fmt::print(stderr, "anchor: {}\n", error.what());
        return 1;
    }
}

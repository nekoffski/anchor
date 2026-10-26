#include <cstdio>
#include <exception>
#include <iostream>

#include "Application.hh"
#include "base/Log.hh"

using namespace anchor;

int main(int argc, const char** argv) {
    log::setLogLevel(log::Level::off);
    try {
        cli::Client client;
        return cli::Application{client, std::cout, std::cerr}.run(argc, argv);
    } catch (const std::exception& error) {
        fmt::print(stderr, "anchor: {}\n", error.what());
        return 1;
    }
}

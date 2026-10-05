#include <cstdio>
#include <exception>

#include "Application.hh"
#include "base/Log.hh"

using namespace anchor;

int main(int argc, const char** argv) {
    log::setLogLevel(log::Level::off);
    try {
        return cli::Application{}.run(argc, argv);
    } catch (const std::exception& error) {
        fmt::print(stderr, "anchor: {}\n", error.what());
        return 1;
    }
}

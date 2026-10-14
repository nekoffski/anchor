#pragma once

#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>

#include <sstream>

#include "base/Log.hh"

namespace anchor::test {

class LogCapture : public testing::Test {
   protected:
    void SetUp() override {
        original = spdlog::default_logger();
        originalLevel = log::level();
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
        auto logger = std::make_shared<spdlog::logger>("test", sink);
        logger->set_pattern("%v");
        spdlog::set_default_logger(logger);
        log::setLogLevel(log::Level::trace);
    }
    void TearDown() override {
        spdlog::set_default_logger(original);
        log::setLogLevel(originalLevel);
    }

    std::ostringstream output;

   private:
    std::shared_ptr<spdlog::logger> original;
    log::Level originalLevel;
};

}  // namespace anchor::test

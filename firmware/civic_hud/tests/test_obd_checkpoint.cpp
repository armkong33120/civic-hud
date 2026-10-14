#include "ObdTask.h"
#include "OdometerEngine.h"
#include <cassert>
#include <iostream>
#include <limits>
int main() {
    uint16_t km = 0;
    uint8_t speed = 0;
    int16_t temp = 0;
    assert(ObdTask::parsePid0131("41 31 -1 00", km) == ElmParseResult::ERROR_RESP);
    assert(ObdTask::parsePid0131("41 31 +1 00", km) == ElmParseResult::ERROR_RESP);
    assert(ObdTask::parsePid010D("41 0D -1", speed) == ElmParseResult::ERROR_RESP);
    assert(ObdTask::parsePid0105("41 05 +1", temp) == ElmParseResult::ERROR_RESP);
    assert(ObdTask::parsePid0131("7e8 04 41 31 04 b0", km) == ElmParseResult::OK && km == 1200);
    OdometerEngine first;
    first.calibrate(100000);
    first.updateFromPid31(100);
    auto saved = first.snapshot();
    first.updateFromPid31(104);
    OdometerEngine rebooted;
    rebooted.restore(saved);
    rebooted.updateFromPid31(105);
    assert(rebooted.getCurrentOdo() == 100005);
    assert(rebooted.persistenceDue(5));
    auto checkpoint = rebooted.snapshot();
    rebooted.markPersisted(checkpoint);
    assert(!rebooted.persistenceDue(5));
    rebooted.updateFromSpeed(std::numeric_limits<float>::infinity(), 1.0f);
    assert(rebooted.getCurrentOdo() == 100005);
    ObdTask obd;
    assert(!obd.begin("", "1234", &rebooted));
    std::cout << "Production parser/checkpoint regressions: 10 assertions passed\n";
}

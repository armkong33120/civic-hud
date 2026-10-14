#include "../../firmware/civic_hud/ResourceMonitor.h"
#include <cassert>
#include <iostream>
int main(){MonitorRange r;r.add(50000);r.add(20000);r.add(45000);assert(r.min==20000&&r.max==50000&&r.now==45000&&r.span()==30000);assert(monitorCpuTenths(5000000,5000000)==0);assert(monitorCpuTenths(5000000,0)==1000);assert(monitorCpuTenths(5000000,4000000)==200);assert(monitorCpuTenths(5000000,5001000)==0);uint32_t old=0xfffffff0u,now=0x20;assert(uint32_t(now-old)==48);assert(monitorCpuTenths(100,uint32_t(now-old))==520);std::cout<<"Monitor min/max, CPU idle ratio, clamping and counter wrap passed\n";}

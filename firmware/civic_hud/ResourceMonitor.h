#pragma once
#include <stdint.h>
#include <algorithm>
struct MonitorRange {
 uint32_t now=0,min=0,max=0; bool valid=false;
 void add(uint32_t v){now=v;if(!valid){min=max=v;valid=true;}else{min=std::min(min,v);max=std::max(max,v);}}
 uint32_t span()const{return max-min;}
};
inline uint32_t monitorCpuTenths(uint64_t elapsed,uint64_t idle){return elapsed?uint32_t(1000-std::min<uint64_t>(1000,idle*1000/elapsed)):0;}
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/idf_additions.h>
// Fixed-size state only; no task, file writes, task enumeration or allocations.
class ResourceMonitor {
 uint32_t lastRamMs=0; uint64_t lastCpuUs=0;
 configRUN_TIME_COUNTER_TYPE previousIdle[2]={};
public:
 MonitorRange freeRam,ramUsedTenths,cpu[2];uint32_t total=0,largest=0,minEver=0,samples=0;
 void begin(){lastCpuUs=esp_timer_get_time();for(int i=0;i<2;++i)previousIdle[i]=ulTaskGetIdleRunTimeCounterForCore(i);sampleRam();lastRamMs=millis();}
 void sampleRam(){total=ESP.getHeapSize();freeRam.add(ESP.getFreeHeap());largest=ESP.getMaxAllocHeap();minEver=ESP.getMinFreeHeap();ramUsedTenths.add(total?uint32_t(uint64_t(total-std::min(total,freeRam.now))*1000/total):0);++samples;}
 void update(){uint32_t ms=millis();if(ms-lastRamMs>=1000){lastRamMs=ms;sampleRam();}uint64_t us=esp_timer_get_time();if(us-lastCpuUs<5000000)return;for(int i=0;i<2;++i){auto idle=ulTaskGetIdleRunTimeCounterForCore(i);auto delta=static_cast<configRUN_TIME_COUNTER_TYPE>(idle-previousIdle[i]);cpu[i].add(monitorCpuTenths(us-lastCpuUs,delta));previousIdle[i]=idle;}lastCpuUs=us;}
 void print()const{Serial.printf("[MONITOR] uptime_s=%u samples=%u heap_total=%u free_now=%u free_min=%u free_max=%u free_delta=%u boot_min=%u largest=%u ram_used_pct=%.1f cpu0_valid=%d cpu0_now=%.1f cpu0_min=%.1f cpu0_max=%.1f cpu1_valid=%d cpu1_now=%.1f cpu1_min=%.1f cpu1_max=%.1f state_bytes=%u\n",millis()/1000,samples,total,freeRam.now,freeRam.min,freeRam.max,freeRam.span(),minEver,largest,ramUsedTenths.now/10.f,cpu[0].valid,cpu[0].now/10.f,cpu[0].min/10.f,cpu[0].max/10.f,cpu[1].valid,cpu[1].now/10.f,cpu[1].min/10.f,cpu[1].max/10.f,unsigned(sizeof(*this)));}
};
#endif

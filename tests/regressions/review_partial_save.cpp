#include "review_storage_fault_seam.cpp"
#include "WebSettings.h"
#include <cassert>
#include <iostream>
int main(){StorageManager storage;storage.begin();MaintenanceEngine maint;OdometerEngine odo;odo.calibrate(98540);ObdTask obd;bool dirty=false;assert(storage.saveItems(maint.getItems()));s_reviewFailKey="bt_v2";
int result=WebSettings::handleSavePayload("km_oil=15000&bt_name=Changed",odo,maint,storage,obd,dirty);assert(result==503);std::vector<MaintenanceItem> persisted;assert(storage.loadItems(persisted));uint32_t ram=0,disk=0;for(auto&i:maint.getItems())if(i.id=="oil")ram=i.intervalKm;for(auto&i:persisted)if(i.id=="oil")disk=i.intervalKm;assert(ram==10000&&disk==15000);std::cout<<"REPRODUCED: rejected503 Save All: RAM oil="<<ram<<" persisted oil="<<disk<<"\n";}

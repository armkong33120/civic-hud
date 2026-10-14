#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "MaintenanceItem.h"

// Exact 132-byte packed POD struct for NVS items blob
#pragma pack(push, 1)
struct PackedItemRecord {
    char     id[12];
    char     name[80];        // UTF-8 Thai name with emoji
    uint32_t intervalKm;
    uint16_t intervalDays;
    uint8_t  warnPct;
    uint32_t lastKm;
    uint32_t lastEpoch;
    char     lastDate[24];
    uint8_t  flags;           // bit 0 = visible, bit 1 = isCustom (exact 132 bytes)
};
#pragma pack(pop)
static_assert(sizeof(PackedItemRecord) == 132, "PackedItemRecord must be exactly 132 bytes");


class StorageManager {
public:
    StorageManager();

    bool begin();

    static bool normalizeUtf8(const std::string& input, size_t maxBytes, std::string& output);
    bool hasItemsData() const;
    bool saveOdometerCheckpoint(uint32_t odo, uint32_t pid, bool valid, float fractional = 0);
    bool loadOdometerCheckpoint(uint32_t& odo, uint32_t& pid, bool& valid, float* fractional = nullptr);

    // Odometer persistence
    bool loadOdometer(uint32_t& outOdo);
    bool saveOdometer(uint32_t odo);

    // PID 31 persistence
    bool loadLastPid31(uint32_t& outPid);
    bool saveLastPid31(uint32_t pid);

    // Maintenance items persistence
    bool loadItems(std::vector<MaintenanceItem>& items);
    bool saveItems(const std::vector<MaintenanceItem>& items);

    // Bluetooth OBD configuration persistence
    bool loadBtConfig(std::string& outName, std::string& outPin);
    bool saveBtConfig(const std::string& name, const std::string& pin);

    // LittleFS CSV history logging (/service_history.csv)
    bool appendHistoryLog(const ServiceHistoryRecord& record);
    std::string exportHistoryCsv() const;
    std::string readHistoryLogsCsv() const { return exportHistoryCsv(); }
    std::vector<ServiceHistoryRecord> loadHistoryLogs() const;

private:
    std::string m_csvHistoryPath;
};

inline bool StorageManager::normalizeUtf8(const std::string& in,size_t limit,std::string& out){
 out.clear();bool capped=false;
 for(size_t i=0;i<in.size();){unsigned char c=in[i];size_t n;uint32_t cp;
 if(c<0x80){n=1;cp=c;}else if(c>=0xC2&&c<=0xDF){n=2;cp=c&31;}else if(c>=0xE0&&c<=0xEF){n=3;cp=c&15;}else if(c>=0xF0&&c<=0xF4){n=4;cp=c&7;}else return false;
 if(i+n>in.size())return false;for(size_t j=1;j<n;j++){unsigned char t=in[i+j];if((t&0xC0)!=0x80)return false;cp=(cp<<6)|(t&63);}
 if((n==2&&cp<0x80)||(n==3&&cp<0x800)||(n==4&&cp<0x10000)||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF)||cp<32||(cp>=127&&cp<=159))return false;
 if(!capped && out.size()+n<=limit)out.append(in,i,n);else capped=true;i+=n;
 }return true;
}

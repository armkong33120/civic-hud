#include "StorageManager.h"
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <set>
#ifdef ARDUINO
#include <Preferences.h>
#include <FS.h>
#include <LittleFS.h>
static Preferences prefs;
static bool s_nvsReady = false, s_fsReady = false;
#else
#include <map>
static std::map<std::string, uint32_t> s_hostUints;
static std::map<std::string, std::string> s_hostStrings;
static std::map<std::string, std::vector<uint8_t>> s_hostBlobs;
static std::string s_hostCsvFile;
static bool s_hostFailWrites = false;
static std::string s_reviewFailKey;
#endif
namespace {
uint32_t checksum(const uint8_t* p, size_t n) { uint32_t h=2166136261u; while(n--) h=(h ^ *p++) * 16777619u; return h; }
bool hasKey(const char* key) {
#ifdef ARDUINO
 return s_nvsReady && prefs.isKey(key);
#else
 return s_hostUints.count(key)||s_hostStrings.count(key)||s_hostBlobs.count(key);
#endif
}
uint32_t getUint(const char* key, uint32_t fallback=0) {
#ifdef ARDUINO
 return prefs.getUInt(key,fallback);
#else
 auto i=s_hostUints.find(key); return i==s_hostUints.end()?fallback:i->second;
#endif
}
bool putUint(const char* key,uint32_t val) {
#ifdef ARDUINO
 return s_nvsReady && prefs.putUInt(key,val)==sizeof(uint32_t);
#else
 if(s_hostFailWrites) return false; s_hostUints[key]=val; return true;
#endif
}
std::vector<uint8_t> readBlob(const char* key) {
#ifdef ARDUINO
 if(!s_nvsReady) return {}; size_t n=prefs.getBytesLength(key); if(n>4096) return {};
 std::vector<uint8_t> b(n); if(n && prefs.getBytes(key,b.data(),n)!=n) return {}; return b;
#else
 auto i=s_hostBlobs.find(key); return i==s_hostBlobs.end()?std::vector<uint8_t>{}:i->second;
#endif
}
bool writeBlob(const char* key,const std::vector<uint8_t>& b) {
#ifdef ARDUINO
 if(!s_nvsReady || prefs.putBytes(key,b.data(),b.size())!=b.size()) return false;
#else
 if(s_hostFailWrites || s_reviewFailKey==key) return false; s_hostBlobs[key]=b;
#endif
 return readBlob(key)==b;
}
#pragma pack(push,1)
struct BlobHeader { uint32_t magic; uint16_t version,count; uint32_t crc; };
struct LegacyItem { char id[12],name[48]; uint32_t intervalKm; uint16_t intervalDays; uint8_t warnPct; uint32_t lastKm,lastEpoch; char lastDate[24]; uint8_t flags; };
struct OdoRecord { uint32_t odo,pid; float fractional; uint8_t valid; };
#pragma pack(pop)
static_assert(sizeof(LegacyItem)==100,"legacy layout");
std::vector<uint8_t> envelope(uint32_t magic,uint16_t count,const void* data,size_t n) {
 BlobHeader h{magic,2,count,checksum(static_cast<const uint8_t*>(data),n)};
 std::vector<uint8_t>b(sizeof(h)+n); std::memcpy(b.data(),&h,sizeof(h)); if(n)std::memcpy(b.data()+sizeof(h),data,n); return b;
}
bool checkEnvelope(const std::vector<uint8_t>& b,uint32_t magic,size_t recordSize,uint16_t& count) {
 if(b.size()<sizeof(BlobHeader))return false; BlobHeader h;std::memcpy(&h,b.data(),sizeof(h));
 if(h.magic!=magic||h.version!=2||h.count>24||b.size()!=sizeof(h)+size_t(h.count)*recordSize)return false;
 if(h.crc!=checksum(b.data()+sizeof(h),b.size()-sizeof(h)))return false;count=h.count;return true;
}
bool parseNumber(const std::string&s,uint32_t& out) { if(s.empty())return false; uint64_t n=0;for(char c:s){if(c<'0'||c>'9')return false;n=n*10+(c-'0');if(n>UINT32_MAX)return false;}out=uint32_t(n);return true; }
std::string csvEscape(const std::string&s){std::string o="\"";for(char c:s){if(c=='"')o+='"';o+=c;}return o+'"';}
}

StorageManager::StorageManager():m_csvHistoryPath("/service_history.csv"){}
bool StorageManager::begin(){
#ifdef ARDUINO
 s_nvsReady=prefs.begin("civichud",false); s_fsReady=LittleFS.begin(false);
 if(!s_nvsReady)Serial.println("[STORAGE] NVS unavailable; saves disabled");
 if(!s_fsReady)Serial.println("[STORAGE] LittleFS unavailable; history preserved, not formatted");
 return s_nvsReady && s_fsReady;
#else
 return true;
#endif
}
// Validate complete Unicode scalar sequences and controls before truncating at a byte boundary.
bool StorageManager::hasItemsData()const{return hasKey("items_v2")||hasKey("items_blob")||hasKey("item_count");}
bool StorageManager::saveOdometerCheckpoint(uint32_t odo,uint32_t pid,bool valid,float fractional){
 if(pid>65535 || !std::isfinite(fractional)||fractional<0||fractional>=1)return false;
 OdoRecord r{odo,pid,fractional,uint8_t(valid)};return writeBlob("odo_v2",envelope(0x324F444Fu,1,&r,sizeof(r)));
}
bool StorageManager::loadOdometerCheckpoint(uint32_t& odo,uint32_t& pid,bool& valid,float* fractional){
 if(hasKey("odo_v2")){auto b=readBlob("odo_v2");uint16_t n;if(!checkEnvelope(b,0x324F444Fu,sizeof(OdoRecord),n)||n!=1)return false;OdoRecord r;std::memcpy(&r,b.data()+sizeof(BlobHeader),sizeof(r));if(r.valid>1||r.pid>65535||!std::isfinite(r.fractional)||r.fractional<0||r.fractional>=1)return false;odo=r.odo;pid=r.pid;valid=r.valid;if(fractional)*fractional=r.fractional;return true;}
 if(!hasKey("odo"))return false;odo=getUint("odo",98540);pid=0;valid=false;if(fractional)*fractional=0;return true; // Legacy separate keys are not a matched checkpoint.
}
bool StorageManager::loadOdometer(uint32_t& odo){uint32_t pid;bool v;odo=98540;return loadOdometerCheckpoint(odo,pid,v);}
bool StorageManager::saveOdometer(uint32_t odo){return saveOdometerCheckpoint(odo,0,false);}
bool StorageManager::loadLastPid31(uint32_t& pid){if(!hasKey("last_pid31")){pid=0;return false;}pid=getUint("last_pid31");return pid<=65535;}
bool StorageManager::saveLastPid31(uint32_t pid){return pid<=65535 && putUint("last_pid31",pid);}
bool StorageManager::loadBtConfig(std::string& name,std::string& pin){
 if(hasKey("bt_v2")){auto b=readBlob("bt_v2");uint16_t n;if(!checkEnvelope(b,0x32544243u,64,n)||n!=1)return false;const char*d=reinterpret_cast<const char*>(b.data()+sizeof(BlobHeader));name=std::string(d,strnlen(d,32));pin=std::string(d+32,strnlen(d+32,32));return true;}
#ifdef ARDUINO
 name=prefs.getString("bt_name","OBDII").c_str();pin=prefs.getString("bt_pin","1234").c_str();
#else
 name=s_hostStrings.count("bt_name")?s_hostStrings["bt_name"]:"OBDII";pin=s_hostStrings.count("bt_pin")?s_hostStrings["bt_pin"]:"1234";
#endif
 return true;
}
bool StorageManager::saveBtConfig(const std::string& name,const std::string& pin){std::string a,b;if(name.empty()||pin.empty()||name.size()>31||pin.size()>16||!normalizeUtf8(name,31,a)||!normalizeUtf8(pin,16,b))return false;char data[64]={};std::memcpy(data,a.data(),a.size());std::memcpy(data+32,b.data(),b.size());return writeBlob("bt_v2",envelope(0x32544243u,1,data,64));}
bool StorageManager::saveItems(const std::vector<MaintenanceItem>& items){
 if(items.empty()||items.size()>24)return false;std::set<std::string>ids;std::vector<PackedItemRecord>records(items.size());
 for(size_t i=0;i<items.size();i++){const auto&it=items[i];std::string name,date;if(it.id.empty()||it.id.size()>11||!ids.insert(it.id).second||it.warnPct>100||!normalizeUtf8(it.name,79,name)||!normalizeUtf8(it.lastDate,23,date))return false;
 for(char c:it.id)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;
 auto&r=records[i];std::memset(&r,0,sizeof(r));std::memcpy(r.id,it.id.data(),it.id.size());std::memcpy(r.name,name.data(),name.size());std::memcpy(r.lastDate,date.data(),date.size());r.intervalKm=it.intervalKm;r.intervalDays=it.intervalDays;r.warnPct=it.warnPct;r.lastKm=it.lastKm;r.lastEpoch=it.lastEpoch;r.flags=(it.visible?1:0)|(it.id.rfind("custom_",0)==0?2:0);
 }return writeBlob("items_v2",envelope(0x32495443u,uint16_t(records.size()),records.data(),records.size()*sizeof(PackedItemRecord)));
}
bool StorageManager::loadItems(std::vector<MaintenanceItem>& items){
 std::vector<PackedItemRecord>records;bool migration=false;
 if(hasKey("items_v2")){auto b=readBlob("items_v2");uint16_t n;if(!checkEnvelope(b,0x32495443u,sizeof(PackedItemRecord),n)||!n)return false;records.resize(n);std::memcpy(records.data(),b.data()+sizeof(BlobHeader),n*sizeof(PackedItemRecord));}
 else {uint32_t n=getUint("item_count");auto b=readBlob("items_blob");if(!n||n>24)return false;records.resize(n);
 if(b.size()==n*sizeof(PackedItemRecord))std::memcpy(records.data(),b.data(),b.size());
 else if(b.size()==n*sizeof(LegacyItem)){for(size_t i=0;i<n;i++){LegacyItem old;std::memcpy(&old,b.data()+i*sizeof(old),sizeof(old));auto&r=records[i];std::memset(&r,0,sizeof(r));std::memcpy(r.id,old.id,12);std::memcpy(r.name,old.name,48);r.intervalKm=old.intervalKm;r.intervalDays=old.intervalDays;r.warnPct=old.warnPct;r.lastKm=old.lastKm;r.lastEpoch=old.lastEpoch;std::memcpy(r.lastDate,old.lastDate,24);r.flags=old.flags;}}
 else return false;migration=true;
 }
 std::vector<MaintenanceItem>loaded;std::set<std::string>ids;
 for(auto&r:records){if(!std::memchr(r.id,0,12)||!std::memchr(r.name,0,80)||!std::memchr(r.lastDate,0,24)||r.warnPct>100||(r.flags&~3))return false;MaintenanceItem it{};it.id=r.id;it.name=r.name;it.lastDate=r.lastDate;std::string clean;
 if(it.id.empty()||!ids.insert(it.id).second)return false;for(char c:it.id)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;
 if(!normalizeUtf8(it.name,79,clean)){// Old firmware could cut a final UTF-8 codepoint. Only recover a trailing incomplete sequence.
 bool recovered=false;if(migration){for(size_t cut=1;cut<=3&&cut<=it.name.size();cut++)if(normalizeUtf8(it.name.substr(0,it.name.size()-cut),79,clean)){it.name=clean;recovered=true;break;}}if(!recovered)return false;
 }else it.name=clean;
 if(!normalizeUtf8(it.lastDate,23,clean))return false;it.lastDate=clean;it.intervalKm=r.intervalKm;it.intervalDays=r.intervalDays;it.warnPct=r.warnPct;it.lastKm=r.lastKm;it.lastEpoch=r.lastEpoch;it.visible=r.flags&1;it.remainingRatio=1;loaded.push_back(it);
 }
 if(migration&&!saveItems(loaded))return false;items=std::move(loaded);return true; // Old keys kept for recovery.
}
bool StorageManager::appendHistoryLog(const ServiceHistoryRecord& r){std::string a,b;if(!normalizeUtf8(r.itemName,79,a)||!normalizeUtf8(r.dateStr,23,b))return false;std::ostringstream ss;ss<<csvEscape(b)<<','<<csvEscape(a)<<','<<r.odoKm<<','<<r.prevOdoKm<<','<<r.deltaKm<<','<<r.nextDueKm<<'\n';auto row=ss.str();
#ifdef ARDUINO
 if(!s_fsReady)return false;File f=LittleFS.open(m_csvHistoryPath.c_str(),"a");if(!f)return false;size_t n=f.write(reinterpret_cast<const uint8_t*>(row.data()),row.size());f.flush();f.close();return n==row.size();
#else
 if(s_hostFailWrites)return false;s_hostCsvFile+=row;return true;
#endif
}
std::string StorageManager::exportHistoryCsv()const{
#ifdef ARDUINO
 if(!s_fsReady)return "";File f=LittleFS.open(m_csvHistoryPath.c_str(),"r");if(!f)return "";String s=f.readString();f.close();return s.c_str();
#else
 return s_hostCsvFile;
#endif
}
std::vector<ServiceHistoryRecord> StorageManager::loadHistoryLogs()const{std::vector<ServiceHistoryRecord>result;std::istringstream ss(exportHistoryCsv());std::string line;
 while(std::getline(ss,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();std::vector<std::string>cols;std::string cell;bool quote=false;
 for(size_t i=0;i<line.size();i++){char c=line[i];if(c=='"'){if(quote&&i+1<line.size()&&line[i+1]=='"'){cell+='"';i++;}else quote=!quote;}else if(c==','&&!quote){cols.push_back(cell);cell.clear();}else cell+=c;}cols.push_back(cell);
 if(quote||cols.size()!=6)continue;ServiceHistoryRecord r{};std::string cleaned;if(!normalizeUtf8(cols[0],23,cleaned)||!normalizeUtf8(cols[1],79,cleaned)||!parseNumber(cols[2],r.odoKm)||!parseNumber(cols[3],r.prevOdoKm)||!parseNumber(cols[4],r.deltaKm)||!parseNumber(cols[5],r.nextDueKm))continue;r.dateStr=cols[0];r.itemName=cols[1];result.push_back(r);
 }return result;
}

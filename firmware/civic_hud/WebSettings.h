#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <streambuf>
#include <cstring>

#include "MaintenanceItem.h"
#include "MaintenanceEngine.h"
#include "OdometerEngine.h"
#include "StorageManager.h"
#include "ObdTask.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <WebServer.h>
#endif

class WebSettings {
public:
    static std::string escapeHtml(const std::string& value) {
        std::string out;
        for (char c : value) { switch (c) {
        case '&': out += "&amp;"; break; case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break; case '\"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break; default: out += c;
        }} return out;
    }
    static int hexValue(char c) { if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1; }
    static std::string urlDecode(const std::string& in) {
        std::string out;
        for (size_t i=0;i<in.size();++i) {
            if(in[i]=='+')out+=' ';
            else if(in[i]=='%'&&i+2<in.size()&&hexValue(in[i+1])>=0&&hexValue(in[i+2])>=0) {
                out += char(hexValue(in[i+1])*16+hexValue(in[i+2]));i+=2;
            } else out+=in[i];
        } return out;
    }
    static bool parseUint(const std::string& text, uint32_t& result, uint32_t maximum=1000000) {
        if(text.empty())return false;uint64_t value=0;
        for(char c:text){if(c<'0'||c>'9')return false;value=value*10+unsigned(c-'0');if(value>maximum)return false;}
        result=uint32_t(value);return true;
    }

    // A reusable fixed-size sink: HTTP and serial share the exact portal renderer.
    class ChunkedHtmlBuffer : public std::streambuf {
    public:
        explicit ChunkedHtmlBuffer(std::function<void(const char*,size_t)> send):m_send(send) {}
        int sync() override { if(m_used){m_send(m_buffer,m_used);m_used=0;}return 0; }
    protected:
        int_type overflow(int_type c) override {
            if(traits_type::eq_int_type(c,traits_type::eof()))return traits_type::not_eof(c);
            m_buffer[m_used++]=traits_type::to_char_type(c);if(m_used==sizeof(m_buffer))sync();return c;
        }
        std::streamsize xsputn(const char* data,std::streamsize count) override {
            std::streamsize remaining=count;
            while(remaining>0){size_t n=std::min(size_t(remaining),sizeof(m_buffer)-m_used);std::memcpy(m_buffer+m_used,data,n);m_used+=n;data+=n;remaining-=n;if(m_used==sizeof(m_buffer))sync();}
            return count;
        }
    private:
        char m_buffer[256];size_t m_used=0;
        std::function<void(const char*,size_t)> m_send;
    };
    static std::string renderHtmlPage(uint32_t currentOdo,const std::string& btName,const std::string& btPin,
                                      const std::vector<MaintenanceItem>& items,const std::vector<ServiceHistoryRecord>& history) {
        std::ostringstream h;renderHtmlTo(h,currentOdo,btName,btPin,items,history);return h.str();
    }
    // Same existing layout, without requiring the board to allocate a full HTML string.
    static void renderHtmlTo(std::ostream& h,uint32_t currentOdo,const std::string& btName,const std::string& btPin,
                             const std::vector<MaintenanceItem>& items,const std::vector<ServiceHistoryRecord>& history) {
        h << "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
        h << "<title>Civic FC 1.8 HUD Settings</title>";
        h << "<style>";
        h << "body{background:#0b0f14;color:#e6edf3;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;margin:0;padding:16px;line-height:1.5;}";
        h << ".container{max-width:580px;margin:0 auto;}";
        h << "h2{color:#38bdf8;margin-top:0;font-size:18px;display:flex;align-items:center;gap:8px;}";
        h << ".badge{padding:2px 8px;font-size:12px;border-radius:4px;background:rgba(56,189,248,0.2);color:#38bdf8;font-family:monospace;}";
        h << ".card{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:14px;margin-bottom:14px;box-shadow:0 2px 4px rgba(0,0,0,0.3);}";
        h << ".card-title{font-size:14px;font-weight:600;margin-bottom:4px;color:#fff;}";
        h << ".card-desc{font-size:12px;color:#8b949e;margin-bottom:10px;}";
        h << "label{display:block;font-size:11px;color:#8b949e;margin-bottom:2px;}";
        h << "input[type='text'],input[type='number']{background:#0b0f14;border:1px solid #30363d;color:#fff;padding:8px 10px;border-radius:6px;font-size:13px;box-sizing:border-box;}";
        h << "input:focus{border-color:#38bdf8;outline:none;}";
        h << ".grid-2{display:grid;grid-template-columns:1fr 1fr;gap:10px;}";
        h << ".btn{background:#10b981;color:#fff;border:none;padding:12px 20px;border-radius:8px;font-size:14px;font-weight:bold;cursor:pointer;width:100%;transition:background 0.2s;}";
        h << ".btn:hover{background:#059669;}";
        h << "table{width:100%;border-collapse:collapse;}";
        h << "th{text-align:left;padding:6px 8px;font-size:11px;color:#8b949e;border-bottom:1px solid #30363d;}";
        h << "td{padding:8px;font-size:12px;border-bottom:1px solid #21262d;}";
        h << ".mono{font-family:monospace;}";
        h << ".text-right{text-align:right;}";
        h << "</style></head><body><div class='container'>";

        h << "<h2><span class='badge'>http://192.168.1.1</span> หน้าเว็บตั้งค่าผ่าน Wi-Fi (Web Settings)</h2>";
        h << "<p style='font-size:12px;color:#8b949e;margin-top:-6px;margin-bottom:16px;'>เชื่อมต่อ Wi-Fi \"CivicHUD\" จากมือถือ จัดการการตั้งค่าและประวัติบำรุงรักษา</p>";

        h << "<form method='POST' action='/save'><input type='hidden' name='full_form' value='1'>";

        // CARD 1: Odometer Sync
        h << "<div class='card'>";
        h << "<div class='card-title'><b>เลขไมล์รถจริง (Odometer):</b></div>";
        h << "<div class='card-desc'>ซิงค์เลขไมล์หน้าปัดรถจริง (Odometer Sync)</div>";
        h << "<input type='number' name='odo' value='" << currentOdo << "' style='width:100%;' class='mono'>";
        h << "</div>";

        // CARD 2: Bluetooth OBD-II Settings
        h << "<div class='card'>";
        h << "<div class='card-title'><b>Bluetooth OBD-II Settings:</b></div>";
        h << "<div class='card-desc'>การเชื่อมต่อ Bluetooth กับหัวอ่าน OBD-II (ELM327) - Auto-Connect (PIN: 1234)</div>";
        h << "<div class='grid-2'>";
        h << "<div><label>Device Name:</label><input type='text' name='bt_name' value='" << escapeHtml(btName) << "' style='width:100%;' class='mono'></div>";
        h << "<div><label>Pairing PIN:</label><input type='text' name='bt_pin' value='" << escapeHtml(btPin) << "' style='width:100%;' class='mono'></div>";
        h << "</div></div>";

        // CARD 3: Add Custom Item
        h << "<div class='card'>";
        h << "<div class='card-title'><b>เพิ่มรายการใหม่:</b></div>";
        h << "<div class='card-desc'>เพิ่มรายการบำรุงรักษาใหม่ (Add Custom Item)</div>";
        h << "<div class='grid-2'>";
        h << "<div><label>ชื่อรายการ:</label><input type='text' name='new_name' placeholder='ชื่อรายการ' style='width:100%;'></div>";
        h << "<div><label>รอบระยะทาง (กม.):</label><input type='number' name='new_km' placeholder='รอบระยะ (กม.)' style='width:100%;' class='mono'></div>";
        h << "</div></div>";

        // CARD 4: Items Table
        h << "<div class='card'>";
        h << "<div class='card-title'><b>รายการบำรุงรักษา:</b></div>";
        h << "<div class='card-desc'>ปรับแต่งรอบระยะทาง (กม.) / ซ่อนรายการ</div>";
        h << "<table>";
        for (const auto& it : items) {
            h << "<tr>";
            h << "<td style='width:28px;'><input type='checkbox' name='vis_" << escapeHtml(it.id) << "'"
              << (it.visible ? " checked" : "") << " style='accent-color:#10b981;'></td>";
            h << "<td style='font-size:13px;font-weight:500;'>" << escapeHtml(it.name) << "</td>";
            h << "<td class='text-right' style='width:120px;'>";
            h << "<input type='number' name='km_" << escapeHtml(it.id) << "' value='" << it.intervalKm
              << "' style='width:90px;text-align:right;' class='mono'> <span style='font-size:11px;color:#8b949e;'>กม.</span>";
            h << "</td></tr>";
        }
        h << "</table></div>";

        // Save All Button
        h << "<button type='submit' class='btn'>💾 บันทึกการตั้งค่าทั้งหมด (Save All)</button>";
        h << "</form>";

        // CARD 5: Service History Log Table
        h << "<div class='card' style='margin-top:20px;'>";
        h << "<div class='card-title'><b>สมุดประวัติการเปลี่ยนถ่าย:</b></div>";
        h << "<div class='card-desc'>สมุดประวัติการเปลี่ยนถ่ายที่บันทึกไว้ (Service History Log)</div>";
        h << "<div style='overflow-x:auto;'><table>";
        h << "<thead><tr>";
        h << "<th>วันที่</th><th>รายการ</th><th class='text-right'>เลขไมล์</th><th class='text-right'>ระยะรอบนั้น</th><th class='text-right'>กำหนดรอบถัดไป</th>";
        h << "</tr></thead><tbody>";
        for (const auto& r : history) {
            h << "<tr>";
            h << "<td class='mono' style='color:#9ca3af;'>" << escapeHtml(r.dateStr.empty() ? "ไม่ทราบวันที่" : r.dateStr) << "</td>";
            h << "<td>" << escapeHtml(r.itemName) << "</td>";
            h << "<td class='text-right mono' style='color:#38bdf8;'>" << r.odoKm << " km</td>";
            h << "<td class='text-right mono' style='color:#e3b341;'>" << r.deltaKm << " km</td>";
            h << "<td class='text-right mono' style='color:#10b981;'>" << r.nextDueKm << " km</td>";
            h << "</tr>";
        }
        h << "</tbody></table></div>";
        h << "<div style='text-align:right;margin-top:8px;'><a href='/history.csv' style='font-size:11px;color:#38bdf8;text-decoration:none;'>📥 ดาวน์โหลดไฟล์ประวัติ (history.csv)</a></div>";
        h << "</div>";

        h << "<p style='font-size:11px;color:#6b7280;text-align:center;margin-top:16px;'>เมื่อกดบันทึก หน้าจอรถจะอัปเดตและจัดเรียงความเร่งด่วนทันที</p>";
        h << "</div></body></html>";
    }

    // Handles form parameter decoding and atomic persistence
    static int handleSavePayload(const std::string& postBody,
                                 OdometerEngine& odoEngine,
                                 MaintenanceEngine& maintEngine,
                                 StorageManager& storage,
                                 ObdTask& obdTask,
                                 bool& screenDirty) {
        std::map<std::string, std::string> params;
        std::istringstream stream(postBody);
        std::string pair;

        while (std::getline(stream, pair, '&')) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string k = urlDecode(pair.substr(0, eqPos));
                std::string v = urlDecode(pair.substr(eqPos + 1));
                params[k] = v;
            }
        }

        return handleDecodedParams(params, odoEngine, maintEngine, storage, obdTask, screenDirty);
    }

    // Browser WebServer arguments have already been decoded. Never serialize/decode them again.
    static int handleDecodedParams(const std::map<std::string,std::string>& params,
                                    OdometerEngine& odoEngine, MaintenanceEngine& maintEngine,
                                    StorageManager& storage, ObdTask& obdTask, bool& screenDirty) {
        const auto originalItems = maintEngine.getItems();
        auto candidate = originalItems;
        uint32_t newOdo=odoEngine.getCurrentOdo();
        auto at=params.find("odo");
        if(at!=params.end() && (!parseUint(at->second,newOdo) || newOdo==0))return 400;
        std::string btName,btPin;
        if(!storage.loadBtConfig(btName,btPin))return 503;
        const std::string oldName=btName,oldPin=btPin;
        at=params.find("bt_name"); if(at!=params.end()&&!at->second.empty())btName=at->second;
        at=params.find("bt_pin"); if(at!=params.end()&&!at->second.empty())btPin=at->second;
        std::string checked;
        if(btName.empty() || btName.size()>31 || btPin.empty() || btPin.size()>16 ||
           !StorageManager::normalizeUtf8(btName,31,checked) || !StorageManager::normalizeUtf8(btPin,16,checked))return 400;
        bool full=params.count("full_form")&&params.at("full_form")=="1";
        for(auto& it:candidate) {
            at=params.find("km_"+it.id);
            if(at!=params.end() && !parseUint(at->second,it.intervalKm))return 400;
            at=params.find("vis_"+it.id);
            if(full || at!=params.end())it.visible=at!=params.end()&&at->second!="0";
        }
        at=params.find("new_name");
        if(at!=params.end()&&!at->second.empty()) {
            auto km=params.find("new_km");uint32_t interval=0;
            if(km==params.end()||!parseUint(km->second,interval)||interval==0||candidate.size()>=24)return 400;
            std::string name;
            if(!StorageManager::normalizeUtf8("🔧 "+at->second,79,name))return 400;
            MaintenanceEngine staging;staging.getItems()=candidate;
            if(!staging.addItem(at->second,interval,0,newOdo,""))return 400;
            candidate=staging.getItems();
        }
        // Persist checked candidate before publishing it to the live UI. Each NVS blob is a checked commit.
        if(!storage.saveItems(candidate))return 503;
        bool btSaved = false;
        auto rollback = [&]() {
            bool btRestored = !btSaved || storage.saveBtConfig(oldName,oldPin);
            bool itemsRestored = storage.saveItems(originalItems);
            if(!btRestored || !itemsRestored) {
                // If flash also refuses rollback, expose the durable state immediately rather than hiding it until reboot.
                std::vector<MaintenanceItem> durable;
                if(storage.loadItems(durable))maintEngine.getItems()=durable;
                std::string durableName,durablePin;
                if(storage.loadBtConfig(durableName,durablePin) && (durableName!=oldName || durablePin!=oldPin))
                    obdTask.begin(durableName,durablePin,&odoEngine);
                maintEngine.evaluate(odoEngine.getCurrentOdo(),0);
                screenDirty=true;
            }
            return 503;
        };
        if(btName!=oldName||btPin!=oldPin) {
            if(!storage.saveBtConfig(btName,btPin))return rollback();
            btSaved=true;
        }
        if(newOdo!=odoEngine.getCurrentOdo()) {
            auto snap=odoEngine.snapshot();snap.odometerKm=newOdo;snap.fractionalKm=0;
            if(!storage.saveOdometerCheckpoint(newOdo,snap.lastPid31,snap.hasPid31Baseline,snap.fractionalKm))return rollback();
            odoEngine.calibrate(newOdo);
        }
        maintEngine.getItems()=candidate;
        if(btName!=oldName||btPin!=oldPin)obdTask.begin(btName,btPin,&odoEngine);
        maintEngine.evaluate(odoEngine.getCurrentOdo(),0);
        screenDirty=true;
        return 303;
    }

    // Serial Bridge handler for zero-risk verification (CMD:WEB_SAVE <payload>)
    static bool handleSerialCommand(const std::string& line,
                                    OdometerEngine& odoEngine,
                                    MaintenanceEngine& maintEngine,
                                    StorageManager& storage,
                                    ObdTask& obdTask,
                                    bool& screenDirty) {
        if (line.rfind("CMD:WEB_SAVE ", 0) == 0) {
            std::string payload = line.substr(13);
            int res = handleSavePayload(payload, odoEngine, maintEngine, storage, obdTask, screenDirty);
#ifdef ARDUINO
            Serial.printf("[WEB_SAVE] result (%d): ODO=%u, Items=%u, Screen refreshed\n",
                          res, odoEngine.getCurrentOdo(), (unsigned)maintEngine.getItems().size());
#endif
            return true;
        }

        if (line.rfind("CMD:DUMP_STORAGE", 0) == 0) {
#ifdef ARDUINO
            std::string btName, btPin;
            storage.loadBtConfig(btName, btPin);
            Serial.printf("[DUMP] ODO=%u, BT_NAME=%s, BT_PIN=%s, ITEMS_COUNT=%u\n",
                          odoEngine.getCurrentOdo(), btName.c_str(), btPin.c_str(),
                          (unsigned)maintEngine.getItems().size());
            for (const auto& it : maintEngine.getItems()) {
                Serial.printf("  Item: id=%s, name=%s, km=%u, vis=%d\n",
                              it.id.c_str(), it.name.c_str(), it.intervalKm, it.visible ? 1 : 0);
            }
            Serial.println(F("[DUMP] History CSV:"));
            Serial.println(storage.exportHistoryCsv().c_str());
            Serial.println(F("[DUMP] End."));
#endif
            return true;
        }

        return false;
    }

#ifdef ARDUINO
    // Registers web endpoints onto ESP32 WebServer
    static void setupWebServer(WebServer& server,
                               OdometerEngine& odoEngine,
                               MaintenanceEngine& maintEngine,
                               StorageManager& storage,
                               ObdTask& obdTask,
                               bool& screenDirty,
                               std::function<void(uint16_t, uint16_t)> playChime) {
        // GET / : Settings Portal
        server.on("/", HTTP_GET, [&server, &odoEngine, &maintEngine, &storage]() {
            std::string btName, btPin;
            storage.loadBtConfig(btName, btPin);
            auto history = storage.loadHistoryLogs();
            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200,"text/html; charset=utf-8","");
            ChunkedHtmlBuffer buffer([&server](const char* bytes,size_t length){server.sendContent(bytes,length);});
            std::ostream output(&buffer);
            renderHtmlTo(output,odoEngine.getCurrentOdo(),btName,btPin,maintEngine.getItems(),history);
            output.flush();
            server.sendContent("",0);
        });

        // POST /save : Save All form submission
        server.on("/save", HTTP_POST, [&server, &odoEngine, &maintEngine, &storage, &obdTask, &screenDirty, playChime]() {
            std::map<std::string,std::string> params;
            for(int i=0;i<server.args();++i)params[server.argName(i).c_str()]=server.arg(i).c_str();
            int result=handleDecodedParams(params,odoEngine,maintEngine,storage,obdTask,screenDirty);
            if(result!=303){server.send(result,"text/plain; charset=utf-8",result==400?"ค่าตั้งค่าไม่ถูกต้อง":"บันทึกไม่สำเร็จ กรุณาลองอีกครั้ง");return;}

            if (playChime) {
                playChime(1200, 150);
            }

            server.sendHeader("Location", "/");
            server.send(303);
        });

        // GET /history.csv : CSV Download endpoint
        server.on("/history.csv", HTTP_GET, [&server, &storage]() {
            std::string csv = storage.exportHistoryCsv();
            server.sendHeader("Content-Disposition", "attachment; filename=\"service_history.csv\"");
            server.send(200, "text/csv", csv.c_str());
        });
    }
#endif
};

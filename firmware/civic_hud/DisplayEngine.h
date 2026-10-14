#pragma once
#include "UiController.h"
#include "ThaiFont.h"
#include "ResourceMonitor.h"
#include "HudPalette.h"
#include "BarAppearance.h"
#ifdef ARDUINO
#include <TFT_eSPI.h>
class HUDDisplay {
 TFT_eSPI& tft; TFT_eSprite tile; bool allocated=false; bool lastModal=false; bool monitorVisible=false;
 bool painted=false;uint64_t headerKey=0,footerKey=0,bodyKey=0,cardKeys[5]={};int lastSlider=-1;uint32_t sliderFrame=0;
 static uint64_t mix(uint64_t h,uint32_t v){for(int i=0;i<4;++i){h^=(v>>(i*8))&255;h*=1099511628211ULL;}return h;}
 static uint64_t hashText(const std::string& v,uint64_t h=14695981039346656037ULL){for(unsigned char c:v){h^=c;h*=1099511628211ULL;}return h;}
 uint8_t warningMask=0;uint32_t lastBlinkTick=0;
 bool nightMode=true;
 // Sprite colours are palette indices, never raw RGB565 values.
 uint16_t bg=0,normal=0,green=8,amber=9,red=10,white=6,muted=5,cyan=6;
 uint16_t track=2,cardWarn=0,cardOver=0,line=2;
 void text(const std::string& s,int x,int y,uint16_t color,int width=220){HudThaiFont::draw(tile,s,x,y,color,width);}
 void paintBar(int row,const MaintenanceItem& it,uint32_t tick){
  // Draw 24 possible status shades directly in RGB565. Keep the small 4-bit UI sprite.
  auto appearance=hudBarAppearance(it);int height=nightMode?5:7;
  int fill=std::max(0,std::min(82,int(it.usedPct*82/100)));
  tft.fillRect(141,32+row*52+31,82,height,tile.getPaletteColor(track));
  if(fill&&(!appearance.blink||(tick%2)==0))tft.fillRect(141,32+row*52+31,fill,height,appearance.rgb);
 }
 void renderModal(const MaintenanceItem& it,uint32_t odo,const std::string& date){
  tile.fillSprite(normal);text(HudThaiFont::plainName(it.name),14,0,white,160);text("กลับ",180,12,white,54);tile.pushSprite(0,32);
  tile.fillSprite(bg);text("ครั้งก่อน: "+(it.lastDate.empty()?std::string("ไม่ทราบวันที่"):it.lastDate),14,0,amber,215);text("ไมล์: "+std::to_string(it.lastKm)+" km",14,22,white);tile.pushSprite(0,80);
  uint32_t delta=odo>=it.lastKm?odo-it.lastKm:0;
  tile.fillSprite(bg);text("วิ่งแล้ว: "+std::to_string(delta)+" km",14,0,amber);text("วันนี้: "+(date.empty()?std::string("ยังไม่ตั้งวันที่"):date),14,22,green);tile.pushSprite(0,128);
  tile.fillSprite(bg);text("ไมล์ใหม่: "+std::to_string(odo)+" km",14,0,white);
  std::string next=it.intervalKm?"รอบหน้า: "+std::to_string((uint64_t)odo+it.intervalKm)+" km":"รอบหน้า: "+std::to_string(it.intervalDays)+" วัน";
  text(next,14,22,green);tile.pushSprite(0,176);
  renderSlider();
  tile.fillSprite(bg);tile.pushSprite(0,280,0,0,240,12);
 }
 void renderSlider(){
  // Redraw just this strip while dragging; no frame buffer or animation task.
  tile.fillSprite(normal);tile.fillRoundRect(18,4,204,36,16,track);tile.drawRoundRect(18,4,204,36,16,green);

  bool ready=controller.progress>=85;sliderFrame=millis()/80;
  text(ready?"ปล่อยเพื่อยืนยัน":"รูดเพื่อยืนยัน",60,8,white,158);
  int knob=36+168*controller.progress/100;uint16_t knobColor=ready?amber:green;
  tile.fillCircle(knob,22,15,knobColor);
  text("รูด",knob-11,8,nightMode||ready?0:7,26);
  tile.pushSprite(0,232);lastSlider=controller.progress;
 }
public:
 HudUiController controller;
 explicit HUDDisplay(TFT_eSPI& screen):tft(screen),tile(&screen){}
 bool begin(){tile.setColorDepth(4);allocated=tile.createSprite(240,48)!=nullptr;if(allocated)setNight(nightMode);controller.changed=true;return allocated;}
 bool dirty()const{return controller.changed||(!monitorVisible&&!lastModal&&warningMask&&millis()/500!=lastBlinkTick);}
 size_t page()const{return controller.currentPage;}
 int sliderProgress()const{return controller.progress;}
 const std::string& activeId()const{return controller.modalId;}
 void openItem(const std::string& id){controller.open(id);}
 void closeModal(){controller.close();}
 void invalidate(){painted=false;controller.changed=true;monitorVisible=false;}
 bool isNight()const{return nightMode;}
 void setNight(bool night){nightMode=night;
  if(night){bg=0;normal=0;white=6;muted=5;cyan=6;track=2;line=2;}
  else{bg=6;normal=7;white=0;muted=3;cyan=0;track=6;line=5;}
  cardWarn=cardOver=normal;
  if(allocated){uint16_t palette[16];HudPalette::make(night,palette);tile.createPalette(palette,16);}
  invalidate();
 }
 void setPage(size_t p){controller.currentPage=p;controller.changed=true;}
 void setSliderForPreview(int p){controller.progress=std::max(0,std::min(100,p));controller.changed=true;}
 std::string touch(bool down,int x,int y,const std::vector<MaintenanceItem>& items,float speedKph){return controller.touch(down,x,y,items,speedKph);}
 static void mapPortraitTouch(int rx,int ry,int& x,int& y){HudUiController::mapPortraitTouch(rx,ry,x,y);}
 void renderMonitor(const ResourceMonitor& m,uint8_t hotspotState=0,bool moving=false){
  if(!allocated)return;
  if(!monitorVisible){tft.fillScreen(tile.getPaletteColor(bg));monitorVisible=true;}lastModal=true;
  tile.fillSprite(normal);text("CPU / RAM",14,0,white);tile.pushSprite(0,0);
  char a[90],b[90];
  auto row=[&](int y){tile.fillSprite(bg);text(a,10,0,white,225);text(b,10,22,cyan,225);tile.pushSprite(0,y);};
  snprintf(a,sizeof(a),"RAM ใช้ %.1f%%",m.ramUsedTenths.now/10.f);
  snprintf(b,sizeof(b),"Min %.1f Max %.1f %%",m.ramUsedTenths.min/10.f,m.ramUsedTenths.max/10.f);row(32);
  snprintf(a,sizeof(a),"ต่าง %.1f จุดเปอร์เซ็นต์",m.ramUsedTenths.span()/10.f);
  snprintf(b,sizeof(b),"บล็อกใหญ่สุด %.1f KB",m.largest/1024.f);row(80);
  for(int i=0;i<2;++i){if(m.cpu[i].valid){snprintf(a,sizeof(a),"CPU %d: %.1f%% ต่าง %.1f",i,m.cpu[i].now/10.f,m.cpu[i].span()/10.f);snprintf(b,sizeof(b),"Min %.1f Max %.1f %%",m.cpu[i].min/10.f,m.cpu[i].max/10.f);}else{snprintf(a,sizeof(a),"CPU %d: รอข้อมูล",i);snprintf(b,sizeof(b),"เฉลี่ยช่วงละ 5 วินาที");}row(128+i*48);}
  tile.fillSprite(normal);tile.drawRoundRect(10,0,220,46,3,hotspotState==2?green:line);
  const char* title=hotspotState==2?"ปิด Wi-Fi ตั้งค่า":hotspotState==1?"กำลังเปิด Wi-Fi":hotspotState==3?"กำลังเชื่อม OBD":hotspotState==4?"ลองเปิด Wi-Fi ใหม่":moving?"จอดรถก่อนตั้งค่า":"เปิด Wi-Fi ตั้งค่า";
  const char* subtitle=hotspotState==2?"CivicHUD 192.168.1.1":hotspotState==1?"พัก OBD ชั่วคราว":hotspotState==3?"รอสักครู่":"OBD พักขณะเปิด Wi-Fi";
  text(title,18,0,white,204);text(subtitle,18,22,muted,204);tile.pushSprite(0,224);

  tile.fillSprite(normal);text("กลับ",14,0,white);tile.pushSprite(0,292);controller.changed=false;
 }
 void render(const std::vector<MaintenanceItem>& items,uint32_t odo,bool btConnected,float voltage,const std::string& dateLabel){
  if(!allocated)return;
  controller.reconcile(items);
  bool modal=!controller.modalId.empty();bool full=!painted||monitorVisible||modal!=lastModal;
  if(full){tft.fillScreen(tile.getPaletteColor(bg));monitorVisible=false;lastModal=modal;headerKey=footerKey=bodyKey=0;for(auto& k:cardKeys)k=0;lastSlider=-1;painted=true;}
  uint64_t hk=mix(mix(14695981039346656037ULL,odo),btConnected);hk=mix(hk,voltage>0?uint32_t(voltage*10+.5f):0);
  if(full||hk!=headerKey){
  tile.fillSprite(normal);text(btConnected?"OBD OK":"OBD...",6,-3,btConnected?green:amber,74);
  char v[20];if(voltage>0)snprintf(v,sizeof(v),"%.1fV",voltage);else snprintf(v,sizeof(v),"--V");text(v,85,-3,muted,50);text(std::to_string(odo)+"km",141,-3,cyan,96);
  // Clip at the header boundary so refreshing voltage cannot overwrite a card.
  tile.pushSprite(0,0,0,0,240,32);headerKey=hk;
  }
  if(modal){for(const auto& it:items)if(it.id==controller.modalId){
   uint64_t k=mix(mix(mix(hashText(it.id),odo),it.lastKm),it.intervalKm);k=mix(k,it.intervalDays);k=hashText(it.name,k);k=hashText(it.lastDate,k);k=hashText(dateLabel,k);
   if(full||k!=bodyKey){renderModal(it,odo,dateLabel);bodyKey=k;}
   else if(lastSlider!=controller.progress)renderSlider();
   break;}}
  else {
   uint32_t tick=millis()/500;bool blinkChanged=tick!=lastBlinkTick;uint8_t mask=0;
   for(int row=0;row<5;++row){size_t idx=controller.currentPage*5+row;uint64_t k=1;
    if(idx<items.size()){const auto& it=items[idx];k=hashText(it.name,hashText(it.id));k=mix(k,uint32_t(it.status));k=mix(k,it.isLimitedByTime);k=mix(k,uint32_t(it.isLimitedByTime?it.daysLeft:it.kmLeft));k=mix(k,uint32_t(std::max(0.f,std::min(100.f,it.usedPct))*82/100));}
    bool blink=idx<items.size()&&hudBarAppearance(items[idx]).blink;if(blink)mask|=uint8_t(1<<row);
    if(!full&&k==cardKeys[row]){if(blink&&blinkChanged)paintBar(row,items[idx],tick);continue;}
    tile.fillSprite(bg);
    if(idx<items.size()){const auto& it=items[idx];uint8_t family=hudBarAppearance(it).family;uint16_t col=family==2?red:family==1?amber:green;uint16_t card=it.status==AlertStatus::OVERDUE?cardOver:it.status==AlertStatus::DUE_SOON?cardWarn:normal;
     if(nightMode){tile.fillRect(6,0,228,48,bg);tile.fillRect(6,47,228,1,line);}
     else{tile.fillRoundRect(6,0,228,48,3,card);tile.drawRoundRect(6,0,228,48,3,line);}
     tile.fillRect(6,0,3,48,col);text(HudThaiFont::plainName(it.name),14,-1,white,210);
     int32_t left=it.isLimitedByTime?it.daysLeft:it.kmLeft;std::string label=left<0?"เกิน ":"เหลือ ";label+=std::to_string(left<0?-(int64_t)left:left)+(it.isLimitedByTime?" วัน":" km");if(left==0)label="ถึงกำหนดเปลี่ยน";text(label,14,19,col,125);
     tile.fillRect(141,31,82,nightMode?5:7,track);
    }tile.pushSprite(0,32+row*52);if(idx<items.size())paintBar(row,items[idx],tick);cardKeys[row]=k;
   }
   lastBlinkTick=tick;warningMask=mask;
  }
  uint64_t fk=mix(mix(14695981039346656037ULL,modal),controller.currentPage);fk=mix(fk,controller.pages(items.size()));
  if(full||fk!=footerKey){tile.fillSprite(normal);if(!modal){text("ก่อนหน้า",6,-1,white,78);text(std::to_string(controller.currentPage+1)+" / "+std::to_string(controller.pages(items.size())),94,-1,muted,70);text("ถัดไป",185,-1,white,54);}else text("กลับ",14,-1,white,210);tile.pushSprite(0,292,0,0,240,28);footerKey=fk;}
  controller.changed=false;
 }
};
#endif

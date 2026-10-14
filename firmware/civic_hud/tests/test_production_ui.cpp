#include "../UiController.h"
#include "../ThaiFont.h"
#include <cassert>
#include <iostream>
#include <vector>
struct Canvas {int count=0;bool outside=false;void drawPixel(int x,int y,uint16_t){++count;if(x<0||x>=240||y<0||y>=28)outside=true;}};
int main(){
 std::vector<MaintenanceItem> items(24);for(size_t i=0;i<items.size();++i){items[i].id=std::to_string(i);items[i].visible=true;}
 HudUiController ui;assert(ui.pages(0)==1&&ui.pages(14)==3&&ui.pages(24)==5);
 ui.currentPage=4;ui.touch(true,20,32,items,0);assert(ui.modalId=="20");ui.touch(false,0,0,items,0);
 assert(ui.touch(true,205,250,items,0).empty());assert(ui.touch(false,205,250,items,0).empty());assert(ui.modalId=="20");
 ui.touch(true,36,250,items,0);ui.touch(true,120,250,items,0);assert(ui.progress==50);assert(ui.touch(false,120,250,items,0).empty());assert(ui.progress==0);
 ui.touch(true,36,250,items,0);ui.touch(true,205,250,items,0);assert(ui.touch(false,205,250,items,0)=="20");assert(ui.modalId.empty());assert(ui.touch(false,205,250,items,0).empty());
 ui.open("4");std::reverse(items.begin(),items.end());ui.touch(true,36,250,items,0);ui.touch(true,205,250,items,0);assert(ui.touch(false,205,250,items,0)=="4");
 ui.open("4");ui.touch(true,36,250,items,0);ui.touch(true,205,190,items,0);assert(ui.touch(false,205,250,items,0).empty());
 ui.touch(true,36,250,items,0);ui.touch(true,205,250,items,0);assert(ui.touch(false,205,250,items,6).empty());assert(ui.modalId.empty());
 ui.open("4");items.erase(std::remove_if(items.begin(),items.end(),[](const MaintenanceItem&i){return i.id=="4";}),items.end());ui.reconcile(items);assert(ui.modalId.empty());
 // Wider thumb target and vertical finger drift still require a real drag/release.
 ui.open("20");assert(ui.touch(true,70,280,items,0).empty());ui.touch(true,150,290,items,0);assert(ui.progress>0);
 assert(ui.touch(false,150,290,items,0).empty());assert(ui.modalId=="20");
 ui.touch(true,70,280,items,0);ui.touch(true,204,290,items,0);assert(ui.progress==100);
 assert(ui.touch(false,204,290,items,0)=="20");
 ui.open("20");assert(ui.touch(true,204,250,items,0).empty());assert(ui.touch(false,204,250,items,0).empty());assert(ui.modalId=="20");
 // Close controls must match the displayed header and footer, without recording service.
 ui.open("20");assert(ui.touch(true,202,36,items,0).empty());assert(ui.modalId.empty());ui.touch(false,202,36,items,0);
 ui.open("20");assert(ui.touch(true,14,304,items,0).empty());assert(ui.modalId.empty());ui.touch(false,14,304,items,0);
 ui.open("20");ui.touch(true,36,250,items,0);ui.touch(true,205,250,items,0);
 assert(ui.touch(false,205,250,items,0)=="20");ui.open("20");
 assert(ui.touch(true,220,304,items,0).empty());assert(ui.modalId.empty()&&ui.progress==0);ui.touch(false,220,304,items,0);
 items.resize(1);ui.reconcile(items);assert(ui.currentPage==0);
 auto p=mapPortraitTouch(240,3800);assert(p.x==0&&p.y==0);p=mapPortraitTouch(3800,260);assert(p.x==239&&p.y==319);
 Canvas c;int a=HudThaiFont::draw(c,"ก",0,0,1,240);Canvas d;int b=HudThaiFont::draw(d,"กิ้",0,0,1,240);assert(a==b&&d.count>c.count&&!d.outside);Canvas clipped;HudThaiFont::draw(clipped,"น้ำมันเครื่องและกรองเกินพื้นที่",0,0,1,30);assert(!clipped.outside&&clipped.count>0);
 auto base=HudThaiFont::glyph(0xe01);auto upper=HudThaiFont::glyph(0xe34);auto low=HudThaiFont::glyph(0xe38);assert(base.advance>0&&upper.advance==0&&low.advance==0);for(int y=12;y<28;++y)assert(upper.rows[y]==0);for(int y=0;y<22;++y)assert(low.rows[y]==0);
 assert(HudThaiFont::plainName("⚙️ น้ำมันเกียร์ CVT")=="น้ำมันเกียร์ CVT");
 assert(HudThaiFont::plainName("🛢️ น้ำมันเครื่อง + กรอง")=="น้ำมันเครื่อง + กรอง");
 assert(HudThaiFont::plainName("❄️ กรองแอร์ห้องโดยสาร")=="กรองแอร์ห้องโดยสาร");
 assert(HudThaiFont::plainName("รายการ ? ที่ผู้ใช้ตั้ง")=="รายการ ? ที่ผู้ใช้ตั้ง");
 assert(hudHotspotHit(10,224)&&hudHotspotHit(229,279));assert(!hudHotspotHit(9,224)&&!hudHotspotHit(230,250)&&!hudHotspotHit(50,280));
 std::string invalid="\xf0\x80\x80\x80";size_t idx=0;assert(HudThaiFont::next(invalid,idx)=='?');
 std::cout<<"Production UI controller/font: all gesture, stable-ID, pagination, clipping, glyph and UTF-8 checks passed\n";
}

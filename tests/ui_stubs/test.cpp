#include "DisplayEngine.h"
#include <cassert>
#include <iostream>
uint32_t clockMs=0;
int main(){TFT_eSPI t;HUDDisplay d(t);assert(d.begin());std::vector<MaintenanceItem> items(6);for(int i=0;i<6;++i){items[i].id=std::to_string(i);items[i].name="น้ำมันเครื่อง";items[i].intervalKm=10000;items[i].visible=true;items[i].status=AlertStatus::OK;items[i].kmLeft=9000;items[i].usedPct=10;}
 d.render(items,298000,true,14.4,"");assert(t.clears==1&&t.pushes.size()==7);t.pushes.clear();d.render(items,298000,true,14.4,"");assert(t.pushes.empty());
 d.render(items,298000,true,14.5,"");assert(t.pushes.size()==1&&t.pushes[0].y==0&&t.pushes[0].h==32);t.pushes.clear();items[2].kmLeft=8999;d.render(items,298000,true,14.5,"");assert(t.pushes.size()==1&&t.pushes[0].y==136);t.pushes.clear();
 d.openItem("0");d.render(items,298000,true,14.5,"");assert(t.clears==2);t.pushes.clear();d.setSliderForPreview(90);d.render(items,298000,true,14.5,"");assert(t.pushes.size()==1&&t.pushes[0].y==232);t.pushes.clear();clockMs=80;assert(!d.dirty());d.render(items,298000,true,14.5,"");assert(t.pushes.empty());t.pushes.clear();
 d.closeModal();d.render(items,298000,true,14.5,"");assert(t.clears==3&&t.pushes.size()==7);t.pushes.clear();d.setPage(1);d.render(items,298000,true,14.5,"");assert(t.pushes.size()==6);ResourceMonitor m;d.renderMonitor(m);t.pushes.clear();d.render(items,298000,true,14.5,"");assert(t.pushes.size()==7);
 d.setNight(false);d.render(items,298000,true,14.5,"");assert(t.clearColor==HudPalette::rgb565(219,219,219));d.setNight(true);d.render(items,298000,true,14.5,"");assert(t.clearColor==0);
 d.setPage(0);items[0].kmLeft=500;items[0].usedPct=95;clockMs=1000;d.render(items,298000,true,14.5,"");t.pushes.clear();int before=t.rects;clockMs=1500;assert(d.dirty());d.render(items,298000,true,14.5,"");assert(t.pushes.empty()&&t.rects==before+1);before=t.rects;clockMs=2000;d.render(items,298000,true,14.5,"");assert(t.pushes.empty()&&t.rects==before+2);items[0].kmLeft=10000;items[0].usedPct=0;d.render(items,298000,true,14.5,"");clockMs=2500;assert(!d.dirty());
 std::cout<<"Actual display: unchanged sends zero; header is clipped; changed card/slider only; page and modal transitions repaint correctly\n";}

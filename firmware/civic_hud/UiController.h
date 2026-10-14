#pragma once
#include "MaintenanceItem.h"
#include <vector>
#include <string>
#include <algorithm>
class HudUiController {
public:
 static constexpr int perPage=5;
 size_t currentPage=0; std::string modalId; int progress=0;bool changed=true;
 size_t pages(size_t count)const{return std::max<size_t>(1,(count+perPage-1)/perPage);}
 void reconcile(const std::vector<MaintenanceItem>& items){if(currentPage>=pages(items.size())){currentPage=pages(items.size())-1;changed=true;}if(!modalId.empty()&&std::none_of(items.begin(),items.end(),[&](const MaintenanceItem& i){return i.id==modalId;}))close();}
 void open(const std::string& id){modalId=id;progress=0;dragging=false;changed=true;}
 void close(){modalId.clear();progress=0;dragging=false;changed=true;}
 static void mapPortraitTouch(int rawX,int rawY,int& x,int& y){x=std::max(0,std::min(239,(3800-rawY)*240/3540));y=std::max(0,std::min(319,(rawX-240)*320/3560));}
 std::string touch(bool down,int x,int y,const std::vector<MaintenanceItem>& items,float speed){
  reconcile(items);std::string commit;
  if(speed>5){if(!modalId.empty())close();dragging=false;wasDown=down;return commit;}
  bool press=down&&!wasDown,release=!down&&wasDown;
  if(modalId.empty()){
   if(press&&y>=292){if(x<80&&currentPage>0){--currentPage;changed=true;}else if(x>160&&currentPage+1<pages(items.size())){++currentPage;changed=true;}}
   else if(press&&x>=6&&x<234&&y>=32&&y<292){int row=(y-32)/52;size_t idx=currentPage*perPage+row;if(row<5&&(y-32)%52<48&&idx<items.size())open(items[idx].id);}
  }else{
   if(press&&((x>=176&&x<240&&y>=32&&y<80)||(x>=0&&x<240&&y>=292&&y<320)))close();
   else if(press&&x>=8&&x<=82&&y>=224&&y<=286){dragging=true;startX=x;progress=0;changed=true;}
   if(down&&dragging){if(y<212||y>=292){dragging=false;progress=0;}else progress=std::max(0,std::min(100,(x-startX)*100/(204-startX)));changed=true;}
   if(release){if(dragging&&progress>=85)commit=modalId;dragging=false;progress=0;changed=true;if(!commit.empty())close();}
  }
  wasDown=down;return commit;
 }
private: bool dragging=false,wasDown=false;int startX=0;
};

struct HudTouchPoint { int x; int y; };
// Initial portrait calibration from the existing board plan; confirm corners on device.
inline HudTouchPoint mapPortraitTouch(int rawX,int rawY){HudTouchPoint p;HudUiController::mapPortraitTouch(rawX,rawY,p.x,p.y);return p;}

inline bool hudHotspotHit(int x,int y){return x>=10&&x<230&&y>=224&&y<280;}

from pathlib import Path
import subprocess, tempfile, zipfile
root = Path(__file__).resolve().parent.parent
archive = root / 'Ender-3 V2 v33 - ZRODLA ROBOCZE NIE INSTALOWAC.zip'
with zipfile.ZipFile(archive) as z:
    s = z.read('Marlin/src/sd/cardreader.cpp').decode()

a=s.index('#if ALL(AUTO_BED_LEVELING_UBL, HAS_HEATED_BED)\n  // Only the LCD'); b=s.index('\n/**\n * Open a G-code file',a)
c=s.index('void CardReader::openAndPrintFile(');d=s.index('\n}\n',c)+2
prefix=r'''#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <string>
#include <iostream>
#include <limits>
#define ALL(...) 1
#define ENABLED(x) 1
#define PROUI_EX 1
#define TERN0(a,b) 0
#define PSTR(x) x
#define F(x) x
#define O_READ 1
#define GRID_MAX_POINTS_X 5
#define GRID_MAX_POINTS_Y 5
#define GRID_MAX_POINTS 25
#define GRID_LOOP(x,y) for(int x=0;x<5;x++) for(int y=0;y<5;y++)
#define MESH_MIN_X 25
#define MESH_MAX_X 205
#define MESH_MIN_Y 25
#define MESH_MAX_Y 205
#define MESH_X_DIST 45
#define MESH_Y_DIST 45
#define X_MIN_POS 0
#define Y_MIN_POS 0
#define X_BED_SIZE 230
#define Y_BED_SIZE 230
#define BED_MAXTEMP 120
#define SERIAL_ECHOLNPGM(...) ((void)0)
#define SERIAL_ERROR_MSG(...) ((void)0)
#define sprintf_P sprintf
#define strcat_P strcat
using std::isnan;using std::isfinite;
static const char *M23_STR="M23 %s";
struct UI {void set_status(const char*,bool=false){}} ui;
struct Planner {bool leveling_active=false;} planner;
struct Bedlevel {
  int8_t storage_slot=0; float z_values[5][5];
  float get_mesh_x(int x){return 25+45*x;}
  float get_mesh_y(int y){return 25+45*y;}
  bool mesh_is_valid(){GRID_LOOP(x,y) if(!isfinite(z_values[x][y]))return false; return true;}
} bedlevel;
void set_bed_leveling_enabled(bool b){planner.leveling_active=b && bedlevel.mesh_is_valid();}
int axes_should_home(){return 0;}
struct Temperature {int target=0; float actual=20; bool success=true; void setTargetBed(int t){target=t;} int degTargetBed(){return target;} float degBed(){return actual;} bool wait_for_bed(bool){if(success)actual=target;return success;}} thermalManager;
struct Probe {bool can_reach(float x,float){return x<=188.5;}} probe;
struct Settings {int calc_num_meshes(){return 1;} void load_mesh(int){GRID_LOOP(x,y) bedlevel.z_values[x][y]=0;}}settings;
struct Queue {char injected_commands[128]={0};const char *injected_commands_P=nullptr; void clear(){} void inject(const char *s){strncpy(injected_commands,s,127);injected_commands[127]=0;}void inject_P(const char *s){injected_commands_P=s;}} queue;
struct FileProp {bool isConfig=false,isLaser=false;float minx=0,miny=0,maxx=0,maxy=0;} fileprop;
struct MediaFile {std::string data;size_t pos=0;bool open(MediaFile*, const char*,int){pos=0;return true;} void seekSet(size_t p){pos=p;} int read(){return pos<data.size()?(unsigned char)data[pos++]:-1;}int read(void* dst,int n){int take=std::min(n,int(data.size()-pos));memcpy(dst,data.data()+pos,take);pos+=take;return take;}void close(){}};
struct CardReader {MediaFile myfile;const char* diveToFile(bool,MediaFile*&,const char *n){return n;}void openAndPrintFile(const char*);static void auto_preheat();static void auto_prepare_probe();static void auto_start_cancel();static void auto_probe_result();static void auto_start_check();};
'''
tests=r'''
void reset(CardReader &c,const std::string &text) {
 c.myfile.data=text; fileprop={}; queue.injected_commands[0]=0; queue.injected_commands_P=nullptr;
 GRID_LOOP(x,y) bedlevel.z_values[x][y]=0.1f;
 thermalManager.target=0;thermalManager.actual=20;thermalManager.success=true;planner.leveling_active=false;
 auto_start_armed=false; auto_probe_ok=false;
}
std::string header(const char *t="60",const char *min="100,100", const char *max="120,120"){
 return std::string("; arena_bed_temp = ")+t+"\n; first_layer_print_min = "+min+"\n; first_layer_print_max = "+max+"\nG90\n";
}
int main(){CardReader c;
 reset(c,header());c.openAndPrintFile("TEST.GCO");assert(auto_start_armed);assert(auto_bed_temp==60);assert(strstr(queue.injected_commands,"M1005\nG28\nM1007\nG29 P1 C\nM1006\nM23"));CardReader::auto_preheat();assert(thermalManager.actual==60);CardReader::auto_prepare_probe();GRID_LOOP(x,y) if(auto_required(x,y)) bedlevel.z_values[x][y]=0.2f;CardReader::auto_probe_result();CardReader::auto_start_check();assert(planner.leveling_active&&!auto_start_armed);
 reset(c,header("70"));c.openAndPrintFile("test.gco");assert(auto_bed_temp==70);thermalManager.success=false;CardReader::auto_preheat();assert(!auto_start_armed && queue.injected_commands[0]==0 && thermalManager.target==0);
 for(const char *t:{"", "0", "29", "106", "abc", "60x", "60.5"}) {reset(c,header(t));c.openAndPrintFile("t.gco");assert(!auto_start_armed && queue.injected_commands[0]==0);}
 reset(c,"G90\n");c.openAndPrintFile("t.gco");assert(!auto_start_armed);
 reset(c,header("60","2,10","10,20"));c.openAndPrintFile("t.gco");assert(!auto_start_armed);
 reset(c,header("60","170,100","190,120"));c.openAndPrintFile("t.gco");assert(!auto_start_armed);
 reset(c,header());bedlevel.z_values[0][0]=NAN;c.openAndPrintFile("t.gco");assert(!auto_start_armed);
 reset(c,header());c.openAndPrintFile("t.gco");assert(auto_start_armed);CardReader::auto_preheat();CardReader::auto_prepare_probe();GRID_LOOP(x,y) if(auto_required(x,y))bedlevel.z_values[x][y]=0.2f;bedlevel.z_values[2][2]=NAN;CardReader::auto_probe_result();bedlevel.z_values[2][2]=0.2f;CardReader::auto_start_check();assert(!auto_start_armed && queue.injected_commands[0]==0 && thermalManager.target==0);
 std::cout << "PASS: heat PLA/PETG, cancellation, missing/invalid temp, edges, unreachable point, bad base, probe failure and success\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'test.cpp'
    exe = Path(tmp) / 'test'
    cpp.write_text(prefix+s[a:b]+s[c:d]+tests)
    subprocess.run(['g++', '-std=gnu++17', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


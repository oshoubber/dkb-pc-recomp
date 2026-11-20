#include "ui/option_rows.cpp"
extern "C" {
#include "ui/menu.h"
extern const int rt_game_version = 0;
void rt_log(const char *, ...) {}
void rt_fatal(const char *, ...) {abort();}
const char* ov_language(void){return "en-US";}
int vk_hw_probe(unsigned *memory,int *integrated,int *nvidia){
    *memory=16384;*integrated=0;*nvidia=1;return 1;
}
}
static std::string keys, help;
static int current; static bool show_modes;
static int rt_patch_set(const char*) {return 1;}
static void gfx_fit_rect(int,int,int,int,int out[4]) {out[2]=1280;out[3]=720;}
static void traced_list(const char* key,UiTextId label,const char* const* raw,const char* const* names,int n,UiTextId desc,int cur,const char* note){
    keys.clear(); for(int i=0;i<n;i++){if(i)keys+=",";keys+=raw[i];}
    current=cur;help=note?note:"";opt_choice_list(key,label,raw,names,n,desc,cur,note);
}
static void traced_modes(const char* key,UiTextId label,const UiTextId* names,int n,UiTextId desc){show_modes=true;opt_choice(key,label,names,n,desc);}
static void production(const MenuTarget& target){
#define opt_choice_list traced_list
#define opt_choice traced_modes
#include "choices.inc"
#undef opt_choice_list
#undef opt_choice
}
static void frame(){
    ImGui::NewFrame();ImGui::SetNextWindowSize({870,550});ImGui::Begin("Test");
    opt_begin("##graphics",1);MenuTarget target={};target.width=target.image_w=1280;target.height=target.image_h=720;
    show_modes=false; production(target); opt_end();ImGui::End();ImGui::Render();
}
static void key(ImGuiKey k){ImGui::GetIO().AddKeyEvent(k,true);frame();ImGui::GetIO().AddKeyEvent(k,false);frame();frame();}
#define ck(cond) do{if(!(cond)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#cond);return 1;}}while(0)
int main(int argc,char** argv){
    cfg_init();
    ck(argc>=4);bool native=!strcmp(argv[1],"native");const char *selected=argv[2];
    ck(CFG_HAS_DLSS==!native);
    ck(!strcmp(cfg_value("TOTK_UPSCALER"),selected));
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,600};
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;unsigned char *pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    frame();opt_focus_key("TOTK_UPSCALER");frame();frame();
    bool unavailable=native&&!strcmp(selected,"dlss");
    ck(keys==(native?"fsr,fsr3,fsr4":"fsr,dlss,fsr3,fsr4"));
    ck((current==-1)==unavailable);
    ck(help==(unavailable?menu_text(UI_MENU_DLSS_WINDOWS_ONLY):""));
    ck(!unavailable || (ui_text_defined("en",UI_MENU_DLSS_WINDOWS_ONLY)&&ui_text_defined("ru",UI_MENU_DLSS_WINDOWS_ONLY)));
    ck(show_modes==(!unavailable&&strcmp(selected,"bilinear")&&strcmp(selected,"fsr")));
    ck(!strcmp(cfg_value("TOTK_UPSCALER"),selected));
    if(!strcmp(argv[3],"edit") || !strcmp(argv[3],"env")){
        key(ImGuiKey_RightArrow);
        const char* then = !strcmp(argv[3],"env") ? selected : unavailable ? "fsr" : (!strcmp(selected,"dlss") || native) ? "fsr3" : "dlss";
        ck(!strcmp(cfg_value("TOTK_UPSCALER"),then));
        ck(cfg_save());
    }
    ImGui::DestroyContext();
    return 0;
}

/* RESTMORE Native Atlas: 100% C application UI, raylib rendering.
 * Desktop, Android and iOS use the same source. No embedded browser. */
#include "restmore/restmore.h"
#include "raylib.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define NOGDI
#define NOUSER
#include <windows.h>
#else
#include <pthread.h>
#endif
typedef struct {
#ifdef _WIN32
 CRITICAL_SECTION mutex;
 HANDLE thread;
#else
 pthread_mutex_t mutex;
 pthread_t thread;
#endif
 int active,finished,success;
 char input[RM_MAX_URL],out[512],error[256],stage[48],detail[RM_MAX_URL];
 rm_catalog *catalog;
} work;
typedef struct {
 rm_catalog *catalog;
 work task;
 char domain[RM_MAX_URL],filename[512],message[256];
 int consent,focus,tab,selected;
 float scroll,scope_scroll,atlas_scroll;
 int view;
} app;

/* RESTMORE / visual system v2: calm dark surfaces, mint evidence, deliberate density.
   All drawing is vector/native raylib; no browser or embedded web assets. */
static const Color BG={7,16,27,255},PANEL={14,27,43,255},PANEL2={24,44,62,255};
static const Color RM_WHITE={238,249,247,255},MUTED={150,173,190,255};
static const Color MINT={121,249,220,255},AQUA={94,190,226,255};
static const Color BORDER={39,62,82,255},RM_ORANGE={232,190,116,255};
static const Color CORAL={255,146,135,255},PURPLE={177,163,255,255};

static void mutex_init(work *w){
#ifdef _WIN32
 InitializeCriticalSection(&w->mutex);
#else
 pthread_mutex_init(&w->mutex,NULL);
#endif
}
static void mutex_lock(work *w){
#ifdef _WIN32
 EnterCriticalSection(&w->mutex);
#else
 pthread_mutex_lock(&w->mutex);
#endif
}
static void mutex_unlock(work *w){
#ifdef _WIN32
 LeaveCriticalSection(&w->mutex);
#else
 pthread_mutex_unlock(&w->mutex);
#endif
}
static Font FACE={0};
static int FACE_OWNED=0;
static float UI_SCALE=1.0f;
static Vector2 pointer(void){Vector2 p=GetMousePosition();return (Vector2){p.x/UI_SCALE,p.y/UI_SCALE};}
static int textwidth(const char *str,int px){
 return (int)MeasureTextEx(FACE,str?str:"",(float)px,1.0f).x;
}
static void textx(const char *str,int x,int y,int px,Color color){
 DrawTextEx(FACE,str?str:"",(Vector2){(float)x,(float)y},(float)px,1.0f,color);
}
static void cut_text(const char *s,char *out,size_t cap,int pixels,int font){
 if(!cap)return;
 snprintf(out,cap,"%s",s?s:"");
 while(out[0]&&textwidth(out,font)>pixels){
  size_t n=strlen(out);
  if(n<=3){out[0]=0;break;}
  out[n-1]=0;
 }
 if(s&&strcmp(out,s)&&strlen(out)+3<cap)strcat(out,"...");
}
static void init_type(void){
 const char *fonts[]={
#ifdef _WIN32
 "C:/Windows/Fonts/segoeui.ttf","C:/Windows/Fonts/arial.ttf",
#elif defined(__APPLE__)
 "/System/Library/Fonts/Supplemental/Arial.ttf",
 "/System/Library/Fonts/Supplemental/Helvetica.ttf",
#else
 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
 "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
 NULL};
 FACE=GetFontDefault();
 for(int i=0;fonts[i];i++){
  if(!FileExists(fonts[i]))continue;
  Font candidate=LoadFontEx(fonts[i],48,NULL,0);
  if(candidate.texture.id!=0){
   FACE=candidate;FACE_OWNED=1;
   SetTextureFilter(FACE.texture,TEXTURE_FILTER_BILINEAR);
   break;
  }
 }
}
static void panel(Rectangle r,Color fill){
 DrawRectangleRounded((Rectangle){r.x+4,r.y+6,r.width,r.height},.045f,8,Fade(BLACK,.17f));
 DrawRectangleRounded(r,.045f,8,fill);
 DrawRectangleRoundedLinesEx(r,.045f,8,1,BORDER);
}
static void card(Rectangle r,Color fill){panel(r,fill);}
static void glyph_line(float x1,float y1,float x2,float y2,float weight,Color color){
 DrawLineEx((Vector2){x1,y1},(Vector2){x2,y2},weight,color);
}
static void rule(float x,float y,float width){glyph_line(x,y,x+width,y,1,BORDER);}
static void line(int x,int y,int font,const char *caption){textx(caption,x,y,font,MUTED);}
static int hit(Rectangle r){return CheckCollisionPointRec(pointer(),r);}
static int clicked(Rectangle r){return hit(r)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT);}
static void label(int x,int y,const char *s){textx(s,x,y,11,MUTED);}
static void method_pill(const char *verb,float x,float y,int width);
static int button(Rectangle r,const char *caption,Color color,int enabled){
 int over=enabled&&hit(r),press=over&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
 Color fill=enabled?(over?Fade(color,.25f):Fade(color,.12f)):Fade(PANEL2,.5f);
 DrawRectangleRounded(r,.12f,8,fill);
 DrawRectangleRoundedLinesEx(r,.12f,8,1,enabled?Fade(color,.8f):BORDER);
 int px=r.width<85?11:r.width<132?13:15;
 int tw=textwidth(caption,px);
 textx(caption,(int)(r.x+(r.width-tw)*.5f),(int)(r.y+(r.height-px)*.5f)-2,px,enabled?RM_WHITE:MUTED);
 return press;
}
static void scissor(Rectangle r){
 BeginScissorMode((int)(r.x*UI_SCALE),(int)(r.y*UI_SCALE),
  (int)(r.width*UI_SCALE),(int)(r.height*UI_SCALE));
}
static void inputbox(Rectangle r,char *dest,size_t cap,int *focus,int id,int enabled){
 int active=*focus==id;
 DrawRectangleRounded(r,.1f,8,hit(r)?PANEL2:PANEL);
 DrawRectangleRoundedLinesEx(r,.1f,8,active?2:1,active?MINT:BORDER);
 scissor((Rectangle){r.x+8,r.y+3,r.width-16,r.height-6});
 const char *visible=dest;while(*visible&&textwidth(visible,15)>r.width-28)visible++;
 textx(visible,(int)r.x+13,(int)r.y+13,15,RM_WHITE);
 EndScissorMode();
 if(enabled&&clicked(r))*focus=id;
 if(!enabled||*focus!=id)return;
 for(int c=GetCharPressed();c>0;c=GetCharPressed()){
  size_t n=strlen(dest);
  if(c>=32&&c<127&&n+1<cap){dest[n]=(char)c;dest[n+1]=0;}
 }
 if(IsKeyPressed(KEY_BACKSPACE)){size_t n=strlen(dest);if(n)dest[n-1]=0;}
 if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_ESCAPE))*focus=0;
 if((IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)||IsKeyDown(KEY_LEFT_SUPER)||IsKeyDown(KEY_RIGHT_SUPER))&&IsKeyPressed(KEY_V)){
  const char *clip=GetClipboardText();
  if(clip&&strlen(clip)<cap)snprintf(dest,cap,"%s",clip);
 }
}
static Color method_color(const char *verb){
 if(!strcmp(verb,"GET"))return MINT;
 if(!strcmp(verb,"POST"))return RM_ORANGE;
 if(!strcmp(verb,"DELETE"))return CORAL;
 if(!strcmp(verb,"PUT")||!strcmp(verb,"PATCH"))return PURPLE;
 return AQUA;
}
static void method_pill(const char *verb,float x,float y,int width){
 Color c=method_color(verb);
 DrawRectangleRounded((Rectangle){x,y,(float)width,27},.19f,6,Fade(c,.12f));
 DrawRectangleRoundedLinesEx((Rectangle){x,y,(float)width,27},.19f,6,Fade(c,.6f));
 textx(verb,(int)x+9,(int)y+7,12,c);
}
static void orbital(float cx,float cy,float radius,float time){
 DrawCircleLines((int)cx,(int)cy,(int)radius,Fade(MINT,.2f));
 DrawCircleLines((int)cx,(int)cy,(int)(radius*.70f),Fade(AQUA,.14f));
 DrawCircleLines((int)cx,(int)cy,(int)(radius*.40f),Fade(MINT,.17f));
 glyph_line(cx-radius-16,cy,cx+radius+16,cy,1,Fade(AQUA,.13f));
 glyph_line(cx,cy-radius-16,cx,cy+radius+16,1,Fade(AQUA,.13f));
 float a=time*.20f;
 Vector2 s={cx+cosf(a)*radius,cy+sinf(a)*radius};
 DrawCircleV(s,4,MINT);
 DrawCircleV(s,10,Fade(MINT,.13f));
}
static void progress(const char *event,const char *detail,void *context){
 work *w=(work *)context;mutex_lock(w);
 snprintf(w->stage,sizeof(w->stage),"%s",event);
 snprintf(w->detail,sizeof(w->detail),"%.2047s",detail);
 mutex_unlock(w);
}
#ifdef _WIN32
static DWORD WINAPI worker(LPVOID data)
#else
static void *worker(void *data)
#endif
{
 work *w=(work *)data;
 rm_catalog *cat=(rm_catalog *)calloc(1,sizeof(*cat));
 char err[256]={0};
 rm_options options={0};
 options.input=w->input;options.output_dir=w->out;
 options.max_pages=12;options.max_depth=2;options.delay_ms=2000;
 options.execute=1;options.authorized=1;
 int outcome=cat?rm_crawl(&options,cat,progress,w,err,sizeof(err)):-1;
 mutex_lock(w);
 w->catalog=cat;w->success=outcome==0;
 snprintf(w->error,sizeof(w->error),"%s",cat?err:"out of memory");
 w->finished=1;mutex_unlock(w);
#ifdef _WIN32
 return 0;
#else
 return NULL;
#endif
}
static int start(work *w,const char *input){
 if(w->active)return -1;
 snprintf(w->input,sizeof(w->input),"%s",input);
 snprintf(w->out,sizeof(w->out),".restmore");
 w->finished=0;w->active=1;w->success=0;
 snprintf(w->stage,sizeof(w->stage),"starting");
 w->detail[0]=0;w->error[0]=0;w->catalog=NULL;
#ifdef _WIN32
 w->thread=CreateThread(NULL,0,worker,w,0,NULL);
 if(!w->thread){w->active=0;return -1;}
#else
 if(pthread_create(&w->thread,NULL,worker,w)){w->active=0;return -1;}
#endif
 return 0;
}
static void poll_work(app *ui){
 work *w=&ui->task;
 mutex_lock(w);int done=w->active&&w->finished;
 mutex_unlock(w);if(!done)return;
#ifdef _WIN32
 WaitForSingleObject(w->thread,INFINITE);CloseHandle(w->thread);
#else
 pthread_join(w->thread,NULL);
#endif
 mutex_lock(w);
 if(w->success){
  free(ui->catalog);ui->catalog=w->catalog;w->catalog=NULL;
  ui->selected=0;ui->scroll=0;
  snprintf(ui->message,sizeof(ui->message),"Archive complete. %zu sources, %zu operations.",
     ui->catalog->document_count,ui->catalog->operation_count);
 }else{
  snprintf(ui->message,sizeof(ui->message),"Crawl stopped: %.216s",w->error);
  free(w->catalog);w->catalog=NULL;
 }
 w->active=0;w->finished=0;mutex_unlock(w);
}
static int load_catalog(app *ui){
 FILE *f=fopen(ui->filename,"rb");
 if(!f){snprintf(ui->message,sizeof(ui->message),"Cannot open local JSON file");return -1;}
 if(fseek(f,0,SEEK_END)){fclose(f);return -1;}
 long size=ftell(f);
 if(size<0||(unsigned long)size>RM_MAX_DOCUMENT_BYTES){fclose(f);snprintf(ui->message,sizeof(ui->message),"JSON file exceeds 2 MiB");return -1;}
 rewind(f);
 char *src=(char *)malloc((size_t)size+1);rm_catalog *cat=(rm_catalog *)calloc(1,sizeof(*cat));
 if(!src||!cat){free(src);free(cat);fclose(f);return -1;}
 int success=fread(src,1,(size_t)size,f)==(size_t)size;fclose(f);
 src[size]=0;
 char error[256]={0};int parsed=-1;
 if(success){
  if(strstr(src,"\"openapi\"")){
   char sha[65];rm_sha256(src,(size_t)size,sha);
   parsed=rm_openapi_import(cat,ui->filename,sha,src,(size_t)size,error,sizeof(error));
  }else parsed=rm_catalog_import_json(cat,src,(size_t)size,error,sizeof(error));
 }
 free(src);
 if(!success||parsed<0){free(cat);snprintf(ui->message,sizeof(ui->message),"Import refused: %.214s",success?error:"read error");return -1;}
 free(ui->catalog);ui->catalog=cat;ui->selected=0;ui->scroll=0;
 snprintf(ui->message,sizeof(ui->message),"Loaded %d documented operations from local JSON",parsed);
 return 0;
}
static void draw_brand(int width){
 DrawRectangle(0,0,width,86,(Color){14,22,29,255});
 DrawLine(0,85,width,85,BORDER);
 Vector2 c={48,42};
 DrawCircleV(c,21,Fade(MINT,0.17f));
 DrawCircleLines((int)c.x,(int)c.y,21,MINT);
 DrawLineEx((Vector2){35,49},(Vector2){51,30},4,MINT);
 DrawLineEx((Vector2){51,30},(Vector2){61,45},4,AQUA);
 DrawCircle(49,34,3,RM_WHITE);
 textx("RESTMORE",85,17,32,RM_WHITE);
 textx("NATIVE API ATLAS  /  C11",87,54,13,MUTED);
 const char *badge="EVIDENCE BEFORE ENDPOINTS";
 int b=MeasureText(badge,14);
 if(width>b+520)textx(badge,width-b-28,37,14,AQUA);
}
static void draw_scope(app *ui,Rectangle area,int busy){
 card(area,PANEL);
 int x=(int)area.x+22,y=(int)area.y+24;
 textx("01 / SCOPE",x,y,20,RM_WHITE);y+=45;
 line(x,y,16,"ONE HTTPS HOST");y+=24;
 inputbox((Rectangle){(float)x,(float)y,area.width-44,45},ui->domain,sizeof(ui->domain),&ui->focus,1,!busy);y+=67;
 textx("Bounded. Robots-aware. No guessed paths.",x,y,15,MUTED);y+=36;
 if(button((Rectangle){(float)x,(float)y,(area.width-55)*0.43f,42},"PREVIEW",AQUA,!busy)){
  rm_target target;char error[256]={0};
  if(!rm_normalize(ui->domain,&target,error,sizeof(error)))
   snprintf(ui->message,sizeof(ui->message),"Dry run: %.190s (0 requests)",target.url);
  else snprintf(ui->message,sizeof(ui->message),"Scope refused: %.206s",error);
  ui->focus=0;
 }
 y+=59;
 Rectangle check={(float)x,(float)y,19,19};
 DrawRectangleRounded(check,0.25f,4,ui->consent?MINT:PANEL2);
 DrawRectangleLinesEx(check,1,MINT);
 if(ui->consent)DrawText("X",x+4,y+2,15,BG);
 if(!busy&&hit((Rectangle){(float)x,(float)y,area.width-44,38})&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))ui->consent=!ui->consent;
 textx("I AM AUTHORIZED",x+30,y+2,15,RM_WHITE);y+=45;
 if(button((Rectangle){(float)x,(float)y,area.width-44,47},"BUILD SOURCE LIBRARY",MINT,ui->consent&&!busy)){
  if(start(&ui->task,ui->domain))snprintf(ui->message,sizeof(ui->message),"Cannot start crawler thread");
  ui->focus=0;
 }
 y+=69;
 DrawLine(x,y,(int)area.x+(int)area.width-22,y,BORDER);y+=21;
 textx("02 / IMPORT",x,y,20,RM_WHITE);y+=43;
 line(x,y,15,"LOCAL OPENAPI 3 / LIBRARY.JSON");y+=21;
 inputbox((Rectangle){(float)x,(float)y,area.width-44,43},ui->filename,sizeof(ui->filename),&ui->focus,2,!busy);y+=56;
 if(button((Rectangle){(float)x,(float)y,area.width-44,42},"OPEN LOCAL FILE",AQUA,!busy)){
  load_catalog(ui);ui->focus=0;
 }
 y+=65;
 textx("METHODS ARE DOCUMENTATION,",x,y,13,MUTED);y+=20;
 textx("NEVER EXECUTED REQUESTS.",x,y,13,MUTED);
}
static void graph(app *ui,Rectangle area){
 card(area,PANEL);
 int x=(int)area.x+24,y=(int)area.y+24;
 textx("03 / RESOURCE TOPOLOGY",x,y,20,RM_WHITE);
 char count[96];snprintf(count,sizeof(count),"%zu declared methods",ui->catalog?ui->catalog->operation_count:0u);
 textx(count,x,y+30,15,MUTED);
 DrawLine(x,y+62,(int)(area.x+area.width-24),y+62,BORDER);
 if(!ui->catalog||!ui->catalog->operation_count){
  textx("No documented API loaded",x+24,y+110,21,RM_WHITE);
  textx("Import an OpenAPI file or archive authorized docs.",x+24,y+150,16,MUTED);
  return;
 }
 Rectangle viewport={area.x+12,area.y+94,area.width-24,area.height-110};
 float wheel=GetMouseWheelMove();
 if(hit(viewport)&&wheel!=0)ui->scroll-=wheel*62;
 if(ui->scroll<0)ui->scroll=0;
 float maxscroll=(float)(ui->catalog->operation_count*68+100)-(float)viewport.height;
 if(ui->scroll>maxscroll)ui->scroll=maxscroll>0?maxscroll:0;
 BeginScissorMode((int)viewport.x,(int)viewport.y,(int)viewport.width,(int)viewport.height);
 Vector2 root={area.x+52,area.y+126-ui->scroll};
 DrawCircleV(root,13,AQUA);DrawCircleLines((int)root.x,(int)root.y,20,Fade(AQUA,0.4f));
 for(size_t i=0;i<ui->catalog->operation_count;i++){
  const rm_operation *op=&ui->catalog->operations[i];
  float yy=area.y+160+(float)i*68-ui->scroll;
  if(yy<viewport.y-65||yy>viewport.y+viewport.height+64)continue;
  float node_x=area.x+88;
  Rectangle r={node_x,yy,area.width-117,55};
  if(i==0)DrawLineBezier(root,(Vector2){node_x,yy+27},1.5f,Fade(AQUA,0.6f));
  else DrawLineEx((Vector2){area.x+54,yy-42},(Vector2){node_x,yy+27},1,Fade(AQUA,0.23f));
  DrawRectangleRounded(r,0.19f,8,i==(size_t)ui->selected?PANEL2:Fade(PANEL2,0.68f));
  DrawRectangleRoundedLinesEx(r,0.19f,8,1,i==(size_t)ui->selected?MINT:BORDER);
  Color mc=method_color(op->method);
  DrawRectangleRounded((Rectangle){node_x+9,yy+11,68,31},0.24f,6,Fade(mc,0.15f));
  textx(op->method,(int)node_x+16,(int)yy+19,16,mc);
  const char *path=op->path;
  while(MeasureText(path,16)>r.width-115&&strlen(path)>5)path++;
  textx(path,(int)node_x+87,(int)yy+19,16,RM_WHITE);
  if(hit(r)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))ui->selected=(int)i;
 }
 EndScissorMode();
}
static void inspector(app *ui,Rectangle area){
 card(area,PANEL);
 int x=(int)area.x+22,y=(int)area.y+24;
 textx("04 / INSPECTOR",x,y,20,RM_WHITE);y+=50;
 if(!ui->catalog||!ui->catalog->operation_count){textx("Select an operation",x,y,16,MUTED);return;}
 if(ui->selected<0||ui->selected>=(int)ui->catalog->operation_count)ui->selected=0;
 const rm_operation *op=&ui->catalog->operations[ui->selected];
 Color mc=method_color(op->method);
 DrawRectangleRounded((Rectangle){(float)x,(float)y,82,37},0.26f,6,Fade(mc,0.17f));
 textx(op->method,x+12,y+9,19,mc);y+=61;
 textx("DECLARED PATH",x,y,14,MUTED);y+=25;
 char shown[RM_MAX_PATH];snprintf(shown,sizeof(shown),"%s",op->path);
 if(MeasureText(shown,16)>area.width-47){size_t cut=(size_t)(area.width/10);if(cut<sizeof(shown))shown[cut]=0;}
 textx(shown,x,y,16,RM_WHITE);y+=56;
 textx("DOCUMENTATION SUMMARY",x,y,14,MUTED);y+=23;
 char shortsum[256];snprintf(shortsum,sizeof(shortsum),"%.45s",op->summary[0]?op->summary:"Not documented");
 textx(shortsum,x,y,16,RM_WHITE);y+=52;
 textx("OPERATION ID",x,y,14,MUTED);y+=23;
 textx(op->operation_id[0]?op->operation_id:"Not provided",x,y,15,RM_WHITE);y+=51;
 textx("SOURCE PROVENANCE / SHA-256",x,y,14,MUTED);y+=27;
 char sha[27];snprintf(sha,sizeof(sha),"%.24s",op->source_sha256);
 textx(op->source_sha256[0]?sha:"Local JSON - unverified",x,y,15,AQUA);y+=46;
 textx("SOURCE DOCUMENT",x,y,14,MUTED);y+=22;
 char url[70];snprintf(url,sizeof(url),"%.34s",op->source_url);
 textx(url,x,y,14,RM_WHITE);y+=58;
 if(button((Rectangle){(float)x,(float)y,area.width-44,43},"COPY REQUEST PREVIEW",AQUA,1)){
  char copy[RM_MAX_PATH+128];
  snprintf(copy,sizeof(copy),"curl -X %s '{{baseUrl}}%s'",op->method,op->path);
  SetClipboardText(copy);
  snprintf(ui->message,sizeof(ui->message),"Copied example. Placeholder baseUrl; no request sent.");
 }
 y+=62;
 if(button((Rectangle){(float)x,(float)y,area.width-44,43},"EXPORT LIBRARY.JSON",MINT,1)){
  char error[256]={0};
  if(rm_make_dir("restmore-export")||rm_catalog_save(ui->catalog,"restmore-export",error,sizeof(error)))
   snprintf(ui->message,sizeof(ui->message),"Export failed: %.200s",error);
  else snprintf(ui->message,sizeof(ui->message),"Saved restmore-export/library.json");
 }
}
int main(int argc,char **argv){
 int smoke=argc>1&&!strcmp(argv[1],"--smoke");
 SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);
 InitWindow(1350,820,"RESTMORE / Native C11 API Atlas");
 SetWindowMinSize(360,580);SetTargetFPS(60);init_type();
 app *ui=(app *)calloc(1,sizeof(*ui));if(!ui){CloseWindow();return 1;}
 snprintf(ui->domain,sizeof(ui->domain),"docs.example.org");
 snprintf(ui->filename,sizeof(ui->filename),"examples/openapi-petstore.json");
 snprintf(ui->message,sizeof(ui->message),"Ready. Import local OpenAPI JSON or preview an authorized scope.");
 mutex_init(&ui->task);
 load_catalog(ui);
 int frames=0;
 while(!WindowShouldClose()&&(!smoke||frames++<60)){
  poll_work(ui);
  int W=GetScreenWidth(),H=GetScreenHeight();
  BeginDrawing();ClearBackground(BG);
  draw_brand(W);
  if(W>=1130){
   Rectangle left={20,106,330,(float)H-178};
   Rectangle center={370,106,(float)W-786,(float)H-178};
   Rectangle right={(float)W-396,106,376,(float)H-178};
   draw_scope(ui,left,ui->task.active);graph(ui,center);inspector(ui,right);
  }else{
   const char *tabs[]={"SCOPE","ATLAS","INSPECT"};
   for(int i=0;i<3;i++)if(button((Rectangle){22+(float)i*((W-44)/3),98,(float)(W-44)/3-8,40},tabs[i],i==ui->tab?MINT:AQUA,1))ui->tab=i;
   Rectangle whole={20,151,(float)W-40,(float)H-221};
   if(ui->tab==0)draw_scope(ui,whole,ui->task.active);
   if(ui->tab==1)graph(ui,whole);
   if(ui->tab==2)inspector(ui,whole);
  }
  DrawRectangle(0,H-55,W,55,(Color){12,20,26,255});
  DrawLine(0,H-55,W,H-55,BORDER);
  char notice[256];snprintf(notice,sizeof(notice),"%.210s",ui->message);
  textx(notice,22,H-35,15,MUTED);
  if(ui->task.active){
   char stage[48],detail[80];mutex_lock(&ui->task);
   snprintf(stage,sizeof(stage),"%.47s",ui->task.stage);
   snprintf(detail,sizeof(detail),"%.79s",ui->task.detail);
   mutex_unlock(&ui->task);
   textx(stage,W-150,H-35,15,MINT);
  }
  EndDrawing();
 }
 if(!ui->task.active){
#ifdef _WIN32
 DeleteCriticalSection(&ui->task.mutex);
#else
 pthread_mutex_destroy(&ui->task.mutex);
#endif
 }
 free(ui->catalog);free(ui);
 if(FACE_OWNED)UnloadFont(FACE);
 CloseWindow();return 0;
}

/* RESTMORE Native Atlas: 100% C application UI, raylib rendering.
 * Desktop, Android and iOS use the same source. No embedded browser. */
#include "restmore/restmore.h"
#include "raylib.h"
#ifndef RESTMORE_NETWORK_ENABLED
#define RESTMORE_NETWORK_ENABLED 1
#endif
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
static const Color CORAL={255,146,135,255},RM_VIOLET={177,163,255,255};

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
 if(!strcmp(verb,"PUT")||!strcmp(verb,"PATCH"))return RM_VIOLET;
 return AQUA;
}
static void method_pill(const char *verb,float x,float y,int width){
 Color c=method_color(verb);
 DrawRectangleRounded((Rectangle){x,y,(float)width,27},.19f,6,Fade(c,.12f));
 DrawRectangleRoundedLinesEx((Rectangle){x,y,(float)width,27},.19f,6,1,Fade(c,.6f));
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
 size_t length=0;
 char *src=NULL;
 FILE *f=fopen(ui->filename,"rb");
 if(f){
  if(fseek(f,0,SEEK_END)){fclose(f);return -1;}
  long n=ftell(f);
  if(n<0||(unsigned long)n>RM_MAX_DOCUMENT_BYTES){fclose(f);snprintf(ui->message,sizeof(ui->message),"JSON source exceeds 2 MiB");return -1;}
  rewind(f);
  length=(size_t)n;
  src=(char *)malloc(length+1);
  if(!src){fclose(f);return -1;}
  if(fread(src,1,length,f)!=length){free(src);fclose(f);return -1;}
  fclose(f);
 }else if(!strcmp(ui->filename,"examples/openapi-petstore.json")){
  /* The same C GUI can read this bundled, known-small example through
     raylib's Android AssetManager, without any Java/Kotlin application code. */
  int asset_bytes=0;
  unsigned char *asset=LoadFileData(ui->filename,&asset_bytes);
  if(!asset||asset_bytes<0||(unsigned)asset_bytes>RM_MAX_DOCUMENT_BYTES){
   if(asset)UnloadFileData(asset);
   snprintf(ui->message,sizeof(ui->message),"Bundled sample could not be read");
   return -1;
  }
  length=(size_t)asset_bytes;
  src=(char *)malloc(length+1);
  if(src)memcpy(src,asset,length);
  UnloadFileData(asset);
  if(!src)return -1;
 }else{
  snprintf(ui->message,sizeof(ui->message),"Cannot open source JSON; choose a readable local file");
  return -1;
 }
 src[length]=0;
 rm_catalog *cat=(rm_catalog *)calloc(1,sizeof(*cat));
 if(!cat){free(src);return -1;}
 char error[256]={0};
 char sha[65];
 rm_sha256(src,length,sha);
 /* Preserve the SHA-256 of the actual local source when importing OpenAPI.
    Library JSON retains its document-level provenance through catalog parsing. */
 int parsed=rm_openapi_import(cat,ui->filename,sha,src,length,error,sizeof(error));
 if(parsed<0)parsed=rm_catalog_import_json(cat,src,length,error,sizeof(error));
 free(src);
 if(parsed<0){
  free(cat);
  snprintf(ui->message,sizeof(ui->message),"Import refused: %.214s",error);
  return -1;
 }
 free(ui->catalog);
 ui->catalog=cat;
 ui->selected=0;ui->scroll=0;ui->atlas_scroll=0;ui->view=0;
 snprintf(ui->message,sizeof(ui->message),"Loaded %d documented operations from local JSON",parsed);
 return 0;
}
static void draw_brand(int width){
 DrawRectangle(0,0,width,94,(Color){9,20,33,255});
 DrawRectangleGradientH(0,0,width,4,MINT,AQUA);
 rule(0,93,(float)width);
 Rectangle sig={22,22,55,50};
 DrawRectangleRounded(sig,.18f,8,Fade(MINT,.08f));
 DrawRectangleRoundedLinesEx(sig,.18f,8,1,Fade(MINT,.50f));
 glyph_line(32,57,42,36,3,MINT);
 glyph_line(42,36,51,56,3,MINT);
 glyph_line(51,56,64,35,3,AQUA);
 DrawCircle(64,35,3,MINT);
 textx("RESTMORE",91,21,width<650?27:31,RM_WHITE);
 textx(width<650?"NATIVE API ATLAS":"API CARTOGRAPHY / EVIDENCE-FIRST ENGINE",92,59,11,MUTED);
 if(width>980){
  textx("C11 / RESTLESS EDITION",width-274,29,14,MINT);
  textx("DISCOVER THE DOCUMENTED",width-270,56,11,MUTED);
 }
}

static void draw_scope(app *ui,Rectangle area,int busy){
 panel(area,PANEL);
 float maxscroll=770-area.height+28;
 if(maxscroll<0)maxscroll=0;
 if(hit(area)&&GetMouseWheelMove()!=0)ui->scope_scroll-=GetMouseWheelMove()*45;
 if(ui->scope_scroll<0)ui->scope_scroll=0;
 if(ui->scope_scroll>maxscroll)ui->scope_scroll=maxscroll;
 int x=(int)area.x+22,y=(int)(area.y+21-ui->scope_scroll);
 int inner=(int)area.width-44;
 scissor((Rectangle){area.x+1,area.y+1,area.width-2,area.height-2});
 label(x,y,"01  /  ACQUISITION DESK");y+=26;
 textx("Map a service.",x,y,28,RM_WHITE);y+=43;
 textx("Start with an authorized source.",x,y,13,MUTED);y+=35;
 label(x,y,"HTTPS ORIGIN  /  DOCUMENTATION URL");y+=22;
 inputbox((Rectangle){(float)x,(float)y,(float)inner,48},ui->domain,sizeof(ui->domain),&ui->focus,1,!busy);y+=62;
 if(button((Rectangle){(float)x,(float)y,inner,42},"PREVIEW SCOPE",AQUA,!busy)){
  rm_target target;char error[256]={0};
  if(!rm_normalize(ui->domain,&target,error,sizeof(error)))
   snprintf(ui->message,sizeof(ui->message),"Scope preview: %.185s  /  zero requests",target.url);
  else snprintf(ui->message,sizeof(ui->message),"Scope refused: %.200s",error);
  ui->focus=0;
 }
 y+=59;
 DrawRectangleRounded((Rectangle){(float)x,(float)y,(float)inner,85},.08f,8,Fade(AQUA,.06f));
 DrawRectangleRoundedLinesEx((Rectangle){(float)x,(float)y,(float)inner,85},.08f,8,1,Fade(AQUA,.20f));
 Rectangle check={(float)x+12,(float)y+14,20,20};
 DrawRectangleRounded(check,.22f,6,ui->consent?MINT:PANEL2);
 DrawRectangleRoundedLinesEx(check,.22f,6,1,MINT);
 if(ui->consent)textx("X",(int)check.x+4,(int)check.y+3,14,BG);
 if(!busy&&clicked((Rectangle){(float)x,(float)y,(float)inner,61}))ui->consent=!ui->consent;
 textx("AUTHORIZED TO INSPECT",x+44,y+17,13,RM_WHITE);
 textx("HTTPS docs only. No methods executed.",x+12,y+53,12,MUTED);
 y+=103;
 if(button((Rectangle){(float)x,(float)y,(float)inner,48},"BUILD SOURCE LIBRARY",MINT,!busy&&ui->consent&&RESTMORE_NETWORK_ENABLED)){
  rm_target target;char error[256]={0};
  if(rm_normalize(ui->domain,&target,error,sizeof(error)))
   snprintf(ui->message,sizeof(ui->message),"Scope refused: %.200s",error);
  else if(start(&ui->task,ui->domain))
   snprintf(ui->message,sizeof(ui->message),"Could not start HTTPS worker");
  ui->focus=0;
 }
 y+=66;
 rule((float)x,(float)y,(float)inner);y+=26;
 label(x,y,"02  /  OFFLINE INGEST");y+=28;
 textx("Import an atlas.",x,y,23,RM_WHITE);y+=35;
 label(x,y,"OPENAPI 3 / RESTMORE LIBRARY.JSON");y+=22;
 inputbox((Rectangle){(float)x,(float)y,(float)inner,46},ui->filename,sizeof(ui->filename),&ui->focus,2,!busy);y+=61;
 if(button((Rectangle){(float)x,(float)y,(float)inner,44},"OPEN LOCAL DOCUMENT",AQUA,!busy)){
  load_catalog(ui);ui->focus=0;
 }
 y+=63;
 rule((float)x,(float)y,(float)inner);y+=20;
 textx("SAFE BY DEFAULT",x,y,12,MINT);y+=23;
 textx(RESTMORE_NETWORK_ENABLED?"Capped pages / robots-aware / same host":"Offline preview build / network disabled",x,y,12,MUTED);y+=21;
 textx("Document metadata is never executed.",x,y,12,MUTED);
 EndScissorMode();
 if(maxscroll>0){
  float track=area.height-35,thumb=track*area.height/(770+28);
  DrawRectangleRounded((Rectangle){area.x+area.width-6,area.y+17,3,track},.4f,6,Fade(MUTED,.13f));
  DrawRectangleRounded((Rectangle){area.x+area.width-6,area.y+17+ui->scope_scroll/maxscroll*(track-thumb),3,thumb},.4f,6,Fade(MINT,.65f));
 }
}

static void graph(app *ui,Rectangle area){
 panel(area,PANEL);
 if(area.width<220||area.height<240)return;
 int x=(int)area.x+20,y=(int)area.y+21;
 label(x,y,"03  /  EVIDENCE TOPOLOGY");
 textx("Operation atlas",x,y+23,25,RM_WHITE);
 size_t count=ui->catalog?ui->catalog->operation_count:0;
 char summary[90];snprintf(summary,sizeof(summary),"%zu documented operations",count);
 textx(summary,x,y+60,12,MUTED);
 int narrow=area.width<460;
 float bh=33,bw=narrow?66:84, bx=area.x+area.width-(narrow?145:188);
 if(button((Rectangle){bx,area.y+21,bw,bh},"MAP",ui->view==0?MINT:AQUA,1))ui->view=0;
 if(button((Rectangle){bx+bw+7,area.y+21,bw,bh},"ROUTES",ui->view==1?MINT:AQUA,1))ui->view=1;
 rule(area.x+20,area.y+99,area.width-40);
 Rectangle viewport={area.x+13,area.y+109,area.width-26,area.height-132};
 scissor(viewport);
 if(!count){
  orbital(area.x+area.width*.5f,viewport.y+viewport.height*.43f,
          fminf(viewport.width,viewport.height)*.23f,(float)GetTime());
  textx("NO SOURCE LIBRARY",x+10,(int)(viewport.y+viewport.height*.72f),19,RM_WHITE);
  char hint[110];cut_text("Import OpenAPI JSON or scan permitted documentation.",hint,sizeof(hint),(int)area.width-60,13);
  textx(hint,x+10,(int)(viewport.y+viewport.height*.72f)+29,13,MUTED);
 }else if(ui->view==0){
  float cx=area.x+area.width*.50f;
  float cy=viewport.y+viewport.height*.51f;
  float rx=fmaxf(55.0f,(viewport.width-130.0f)*.45f);
  float ry=fmaxf(52.0f,(viewport.height-118.0f)*.43f);
  if(rx>viewport.width*.37f)rx=viewport.width*.37f;
  if(ry>viewport.height*.35f)ry=viewport.height*.35f;
  orbital(cx,cy,fminf(rx,ry)*.76f,(float)GetTime());
  const size_t shown=count<12?count:12;
  const float pi=3.14159265358979323846f;
  Vector2 nodes[12];
  for(size_t i=0;i<shown;i++){
   float a=2.0f*pi*(float)i/(float)shown-pi*.5f;
   nodes[i]=(Vector2){cx+cosf(a)*rx,cy+sinf(a)*ry};
   glyph_line(cx,cy,nodes[i].x,nodes[i].y,1.3f,Fade(AQUA,.27f));
  }
  DrawCircleV((Vector2){cx,cy},35,(Color){24,57,66,255});
  DrawCircleLines((int)cx,(int)cy,35,MINT);
  textx("API",(int)cx-17,(int)cy-10,19,RM_WHITE);
  for(size_t i=0;i<shown;i++){
   const rm_operation *op=&ui->catalog->operations[i];
   Vector2 p=nodes[i];Color c=method_color(op->method);
   int selected=(int)i==ui->selected;
   DrawCircleV(p,selected?15:12,selected?MINT:PANEL2);
   DrawCircleLines((int)p.x,(int)p.y,selected?17:13,c);
   DrawCircleV(p,4,c);
   Rectangle target={p.x-19,p.y-19,38,38};
   if(clicked(target))ui->selected=(int)i;
   char path[80];cut_text(op->path,path,sizeof(path),narrow?85:134,11);
   int txtw=textwidth(path,11);
   float tx=p.x+(p.x<cx?-txtw-18:18);
   if(tx<viewport.x+7)tx=viewport.x+7;
   if(tx+txtw>viewport.x+viewport.width-7)tx=viewport.x+viewport.width-txtw-7;
   float ty=p.y+((i%2)?14:-24);
   textx(path,(int)tx,(int)ty,11,selected?RM_WHITE:MUTED);
  }
  if(count>shown){
   char rest[80];snprintf(rest,sizeof(rest),"+%zu ROUTES  /  SWITCH TO ROUTES VIEW",count-shown);
   textx(rest,(int)viewport.x+16,(int)(viewport.y+viewport.height-22),12,MINT);
  }
 }else{
  const int card_height=62,step=70;
  float content=(float)count*step+15;
  float maxscroll=fmaxf(0,content-viewport.height);
  if(hit(viewport)&&GetMouseWheelMove()!=0)ui->scroll-=GetMouseWheelMove()*58;
  if(ui->scroll<0)ui->scroll=0;
  if(ui->scroll>maxscroll)ui->scroll=maxscroll;
  for(size_t i=0;i<count;i++){
   const rm_operation *op=&ui->catalog->operations[i];
   float yy=viewport.y+8+(float)i*step-ui->scroll;
   if(yy+card_height<viewport.y||yy>viewport.y+viewport.height)continue;
   Rectangle row={viewport.x+6,yy,viewport.width-15,card_height};
   DrawRectangleRounded(row,.08f,8,(int)i==ui->selected?PANEL2:Fade(PANEL2,.50f));
   DrawRectangleRoundedLinesEx(row,.08f,8,1,(int)i==ui->selected?MINT:BORDER);
   method_pill(op->method,row.x+10,row.y+16,65);
   char path[RM_MAX_PATH];cut_text(op->path,path,sizeof(path),(int)row.width-107,14);
   textx(path,(int)row.x+88,(int)row.y+12,14,RM_WHITE);
   char summary_label[125];cut_text(op->summary,summary_label,sizeof(summary_label),(int)row.width-107,11);
   textx(summary_label,(int)row.x+88,(int)row.y+37,11,MUTED);
   if(clicked(row))ui->selected=(int)i;
  }
  if(maxscroll>0){
   float track=viewport.height-12,thumb=track*viewport.height/content;
   DrawRectangleRounded((Rectangle){viewport.x+viewport.width-4,viewport.y+5,3,track},1,6,Fade(MUTED,.16f));
   DrawRectangleRounded((Rectangle){viewport.x+viewport.width-4,viewport.y+5+ui->scroll/maxscroll*(track-thumb),3,thumb},1,6,MINT);
  }
 }
 EndScissorMode();
 label((int)area.x+20,(int)(area.y+area.height-18),"OBSERVED / DOCUMENTED / NO REQUEST EXECUTION");
}

static void inspector(app *ui,Rectangle area){
 panel(area,PANEL);
 if(area.width<220||area.height<210)return;
 float maxscroll=fmaxf(0,640-area.height+25);
 if(hit(area)&&GetMouseWheelMove()!=0)ui->atlas_scroll-=GetMouseWheelMove()*45;
 if(ui->atlas_scroll<0)ui->atlas_scroll=0;
 if(ui->atlas_scroll>maxscroll)ui->atlas_scroll=maxscroll;
 int x=(int)area.x+22,y=(int)(area.y+23-ui->atlas_scroll),inner=(int)area.width-44;
 scissor((Rectangle){area.x+1,area.y+1,area.width-2,area.height-2});
 label(x,y,"04  /  OPERATION INSPECTOR");y+=28;
 textx("Source intelligence",x,y,24,RM_WHITE);y+=45;
 rule((float)x,(float)y,(float)inner);y+=25;
 if(!ui->catalog||!ui->catalog->operation_count){
  textx("NO OPERATION SELECTED",x,y,16,RM_WHITE);y+=31;
  textx("Click a mapped route for its evidence.",x,y,12,MUTED);
  EndScissorMode();return;
 }
 if(ui->selected<0||ui->selected>=(int)ui->catalog->operation_count)ui->selected=0;
 const rm_operation *op=&ui->catalog->operations[ui->selected];
 method_pill(op->method,(float)x,(float)y,85);y+=51;
 label(x,y,"DOCUMENTED PATH");y+=23;
 char path[RM_MAX_PATH];cut_text(op->path,path,sizeof(path),inner,17);
 textx(path,x,y,17,RM_WHITE);y+=47;
 rule((float)x,(float)y,(float)inner);y+=23;
 label(x,y,"SUMMARY");y+=24;
 char summary[256];cut_text(op->summary[0]?op->summary:"Not declared by source",summary,sizeof(summary),inner,15);
 textx(summary,x,y,15,RM_WHITE);y+=43;
 label(x,y,"OPERATION IDENTIFIER");y+=24;
 char operation[130];cut_text(op->operation_id[0]?op->operation_id:"Not declared",operation,sizeof(operation),inner,14);
 textx(operation,x,y,14,RM_WHITE);y+=49;
 rule((float)x,(float)y,(float)inner);y+=23;
 label(x,y,"SOURCE / PROVENANCE");y+=28;
 char source[RM_MAX_URL];cut_text(op->source_url[0]?op->source_url:"Local catalog, source not provided",source,sizeof(source),inner,13);
 textx(source,x,y,13,MUTED);y+=29;
 label(x,y,"SHA-256 DOCUMENT FINGERPRINT");y+=24;
 char fingerprint[70];cut_text(op->source_sha256[0]?op->source_sha256:"No provenance hash available",fingerprint,sizeof(fingerprint),inner,13);
 textx(fingerprint,x,y,13,op->source_sha256[0]?MINT:RM_ORANGE);y+=47;
 rule((float)x,(float)y,(float)inner);y+=24;
 if(button((Rectangle){(float)x,(float)y,(float)inner,43},"COPY REQUEST TEMPLATE",AQUA,1)){
  char copy[RM_MAX_PATH+140];
  snprintf(copy,sizeof(copy),"curl -X %s '{{baseUrl}}%s'",op->method,op->path);
  SetClipboardText(copy);
  snprintf(ui->message,sizeof(ui->message),"Copied illustrative curl template. No request was made.");
 }
 y+=59;
 if(button((Rectangle){(float)x,(float)y,(float)inner,43},"EXPORT EVIDENCE ATLAS",MINT,1)){
  char error[256]={0};
  if(rm_make_dir("restmore-export")||rm_catalog_save(ui->catalog,"restmore-export",error,sizeof(error)))
   snprintf(ui->message,sizeof(ui->message),"Export failed: %.200s",error[0]?error:"Cannot create directory");
  else snprintf(ui->message,sizeof(ui->message),"Saved restmore-export/library.json and related formats");
 }
 y+=62;
 textx("GET / POST / PUT / PATCH / DELETE",x,y,12,MUTED);y+=19;
 textx("Routes are declared, never executed.",x,y,12,MUTED);
 EndScissorMode();
 if(maxscroll>0){
  float track=area.height-32,thumb=track*area.height/(640+25);
  DrawRectangleRounded((Rectangle){area.x+area.width-6,area.y+16,3,track},1,6,Fade(MUTED,.14f));
  DrawRectangleRounded((Rectangle){area.x+area.width-6,area.y+16+ui->atlas_scroll/maxscroll*(track-thumb),3,thumb},1,6,MINT);
 }
}

int main(int argc,char **argv){
 int smoke=argc>1&&!strcmp(argv[1],"--smoke");
 SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);
 InitWindow(1350,820,"RESTMORE  /  NATIVE API ATLAS");
 SetWindowMinSize(360,580);
 SetTargetFPS(60);
 init_type();
 app *ui=(app *)calloc(1,sizeof(*ui));
 if(!ui){
  if(FACE_OWNED)UnloadFont(FACE);
  CloseWindow();
  return 1;
 }
 snprintf(ui->domain,sizeof(ui->domain),"docs.example.org");
 snprintf(ui->filename,sizeof(ui->filename),"examples/openapi-petstore.json");
 snprintf(ui->message,sizeof(ui->message),"Atlas ready. Import OpenAPI documentation or authorize a bounded scan.");
 mutex_init(&ui->task);
 load_catalog(ui);
 int frames=0;
 while(!WindowShouldClose()&&(!smoke||frames++<60)){
  poll_work(ui);
  int actualW=GetScreenWidth(),actualH=GetScreenHeight();
  UI_SCALE=fminf(1.0f,fmaxf(.84f,(float)actualW/690.0f));
  int W=(int)((float)actualW/UI_SCALE),H=(int)((float)actualH/UI_SCALE);
  if(IsKeyPressed(KEY_TAB)&&ui->focus==0)ui->tab=(ui->tab+1)%3;
  BeginDrawing();
  ClearBackground(BG);
  BeginMode2D((Camera2D){.offset={0,0},.target={0,0},.rotation=0,.zoom=UI_SCALE});
  /* Subtle, fixed architectural point grid, not a claim of live telemetry. */
  for(int x=0;x<W;x+=38)for(int y=101;y<H-50;y+=38)
   DrawCircle(x,y,1,Fade(AQUA,.055f));
  draw_brand(W);
  if(W>=1180){
   float h=(float)H-178;
   Rectangle left={20,108,335,h};
   Rectangle center={375,108,(float)W-795,h};
   Rectangle right={(float)W-400,108,380,h};
   draw_scope(ui,left,ui->task.active);
   graph(ui,center);
   inspector(ui,right);
  }else{
   const char *tabs[]={"01  SCOPE","02  ATLAS","03  INSPECT"};
   float tabW=((float)W-44.0f)/3.0f;
   for(int i=0;i<3;i++){
    Rectangle tab={22+tabW*(float)i,104,tabW-8,41};
    if(button(tab,tabs[i],i==ui->tab?MINT:AQUA,1))ui->tab=i;
   }
   Rectangle whole={19,155,(float)W-38,(float)H-226};
   if(ui->tab==0)draw_scope(ui,whole,ui->task.active);
   else if(ui->tab==1)graph(ui,whole);
   else inspector(ui,whole);
  }
  DrawRectangle(0,H-56,W,56,(Color){8,20,31,255});
  rule(0,H-56,(float)W);
  char notice[256];
  int width=ui->task.active?W-220:W-46;
  cut_text(ui->message,notice,sizeof(notice),width,13);
  textx(notice,21,H-36,13,MUTED);
  if(ui->task.active){
   char stage[48];
   mutex_lock(&ui->task);
   snprintf(stage,sizeof(stage),"%.47s",ui->task.stage);
   mutex_unlock(&ui->task);
   textx(stage,W-165,H-38,14,MINT);
   DrawCircle(W-187,H-30,5,MINT);
  }else if(W>850)textx("R/M  42  /  C11",W-182,H-38,13,MINT);
  EndMode2D();
  if(smoke && frames==30)TakeScreenshot("restmore-native-preview.png");
  EndDrawing();
 }
 /* Never release the app while its crawler thread still references it. */
 if(ui->task.active){
#ifdef _WIN32
  WaitForSingleObject(ui->task.thread,INFINITE);
  CloseHandle(ui->task.thread);
#else
  pthread_join(ui->task.thread,NULL);
#endif
  free(ui->task.catalog);
  ui->task.catalog=NULL;
  ui->task.active=0;
 }
#ifdef _WIN32
 DeleteCriticalSection(&ui->task.mutex);
#else
 pthread_mutex_destroy(&ui->task.mutex);
#endif
 free(ui->catalog);
 free(ui);
 if(FACE_OWNED)UnloadFont(FACE);
 CloseWindow();
 return 0;
}

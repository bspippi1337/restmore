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
 int active,finished,success,mode; /* 1 documentation, 2 verification */
 unsigned pages,depth;
 rm_verification *verification;
 char input[RM_MAX_URL],out[512],error[256],stage[48],detail[RM_MAX_URL];
 rm_catalog *catalog;
} work;
typedef struct {
 rm_catalog *catalog;
 rm_verification *verification;
 int verify_stage; /* 0 idle, 1 github review done/await consent, 2 results */
 char review_domain[RM_MAX_URL];
 work task;
 char domain[RM_MAX_URL],filename[512],archive_dir[512],message[256];
 int consent,focus,tab,selected;
 size_t check_budget; /* 0 = all eligible documented GETs */
 unsigned crawl_pages,crawl_depth;
 float scroll,scope_scroll,atlas_scroll;
 int view;
} app;

/* RESTMORE / visual system v2: calm dark surfaces, mint evidence, deliberate density.
   All drawing is vector/native raylib; no browser or embedded web assets. */
static const Color BG={7,17,17,255},PANEL={15,29,29,255},PANEL2={25,48,46,255};
static const Color RM_WHITE={238,249,247,255},MUTED={150,173,190,255};
static const Color MINT={166,242,174,255},AQUA={101,183,155,255};
static const Color BORDER={48,75,66,255},RM_ORANGE={244,190,103,255};
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
static float UI_SCALE=1.0f,TOUCH_DY=0;
static Vector2 LAST_TOUCH={0},TOUCH_START={0};
static int TOUCH_ACTIVE=0,TOUCH_PRESSED=0;
static void sample_touch(void){
 int now=GetTouchPointCount()>0;
 TOUCH_DY=0;TOUCH_PRESSED=0;
 if(now){
  Vector2 p=GetTouchPosition(0);
  if(TOUCH_ACTIVE)TOUCH_DY=(p.y-LAST_TOUCH.y)/UI_SCALE;
  else TOUCH_START=p;
  LAST_TOUCH=p;
 }else if(TOUCH_ACTIVE){
  /* Commit a tap only after release, not at touch-down. Previously every
     vertical swipe could also activate the row/button it started on. */
  float dx=(LAST_TOUCH.x-TOUCH_START.x)/UI_SCALE;
  float dy=(LAST_TOUCH.y-TOUCH_START.y)/UI_SCALE;
  TOUCH_PRESSED=dx*dx+dy*dy<196.0f;
 }
 TOUCH_ACTIVE=now;
}
static Vector2 pointer(void){
 Vector2 p=(TOUCH_ACTIVE||TOUCH_PRESSED)?LAST_TOUCH:GetMousePosition();
 return (Vector2){p.x/UI_SCALE,p.y/UI_SCALE};
}
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
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
 /* Prefer the platform's high-resolution sans-serif if available; fall back
    to raylib's embedded font without shipping an unlicensed typeface. */
 "/system/fonts/Roboto-Regular.ttf","/system/fonts/NotoSans-Regular.ttf",
#elif defined(_WIN32)
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
/* Blueprint registration lines are intentionally decorative.
   They do not describe a fetched operation, host or verified network state. */
static void glyph_line(float x1,float y1,float x2,float y2,float weight,Color color);
static void registration_mark(float x,float y,Color ink){
 glyph_line(x-5,y,x+5,y,.8f,ink);
 glyph_line(x,y-5,x,y+5,.8f,ink);
 DrawCircleLines((int)x,(int)y,3,ink);
}
static void panel(Rectangle r,Color fill){
 DrawRectangleRounded((Rectangle){r.x+4,r.y+6,r.width,r.height},.045f,8,Fade(BLACK,.17f));
 DrawRectangleRounded(r,.045f,8,fill);
 DrawRectangleRoundedLinesEx(r,.045f,8,1,BORDER);
 Color guide=Fade(AQUA,.26f);
 registration_mark(r.x+9,r.y+9,guide);
 registration_mark(r.x+r.width-9,r.y+9,guide);
 registration_mark(r.x+9,r.y+r.height-9,guide);
 registration_mark(r.x+r.width-9,r.y+r.height-9,guide);
}
/* Native C11 architectural pencil plate, matching docs/assets/engraved-arch.svg.
   Drawn in raylib primitives; SVG parsing and external image assets are unnecessary. */
static void draw_arch_study(float x,float bottom,float scale){
 const float pi=3.14159265358979323846f;
 const float cx=x+65*scale,cy=bottom-79*scale;
 const float outer=62*scale,inner=43*scale;
 Color stone=Fade(MUTED,.27f),chalk=Fade(RM_WHITE,.18f);
 for(int i=0;i<32;i++){
  float a=pi+pi*(float)i/32.0f,b=pi+pi*(float)(i+1)/32.0f;
  glyph_line(cx+cosf(a)*outer,cy+sinf(a)*outer,
             cx+cosf(b)*outer,cy+sinf(b)*outer,.85f,stone);
  glyph_line(cx+cosf(a)*inner,cy+sinf(a)*inner,
             cx+cosf(b)*inner,cy+sinf(b)*inner,.9f,chalk);
 }
 glyph_line(cx-outer,cy,cx-outer,bottom,1,stone);
 glyph_line(cx+outer,cy,cx+outer,bottom,1,stone);
 glyph_line(cx-inner,cy,cx-inner,bottom,1,chalk);
 glyph_line(cx+inner,cy,cx+inner,bottom,1,chalk);
 glyph_line(cx-outer-7*scale,bottom,cx+outer+7*scale,bottom,1.2f,stone);
 for(int i=1;i<=3;i++){
  float yy=cy+((float)i*22)*scale;
  glyph_line(cx-outer,yy,cx-inner,yy,.8f,stone);
  glyph_line(cx+inner,yy,cx+outer,yy,.8f,stone);
 }
 glyph_line(cx-outer-8*scale,cy,cx+outer+8*scale,cy,.7f,Fade(AQUA,.17f));
}
static void card(Rectangle r,Color fill){panel(r,fill);}
static void glyph_line(float x1,float y1,float x2,float y2,float weight,Color color){
 DrawLineEx((Vector2){x1,y1},(Vector2){x2,y2},weight,color);
}
static void rule(float x,float y,float width){glyph_line(x,y,x+width,y,1,BORDER);}
static void line(int x,int y,int font,const char *caption){textx(caption,x,y,font,MUTED);}
static int hit(Rectangle r){return CheckCollisionPointRec(pointer(),r);}
static int clicked(Rectangle r){
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
 /* Android may mirror touch-down as a mouse press. Use release-only taps. */
 return hit(r)&&TOUCH_PRESSED;
#else
 return hit(r)&&(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)||TOUCH_PRESSED);
#endif
}
static void label(int x,int y,const char *s){textx(s,x,y,11,MUTED);}
static void method_pill(const char *verb,float x,float y,int width);
static int button(Rectangle r,const char *caption,Color color,int enabled){
 int over=enabled&&hit(r),press=enabled&&clicked(r);
 Color fill=enabled?(over?Fade(color,.25f):Fade(color,.12f)):Fade(PANEL2,.5f);
 DrawRectangleRounded(r,.12f,8,fill);
 DrawRectangleRoundedLinesEx(r,.12f,8,1,enabled?Fade(color,.8f):BORDER);
 int px=r.width<85?11:r.width<132?13:15;
 char fitted[160];
 cut_text(caption,fitted,sizeof(fitted),(int)r.width-16,px);
 int tw=textwidth(fitted,px);
 textx(fitted,(int)(r.x+(r.width-tw)*.5f),(int)(r.y+(r.height-px)*.5f)-2,px,enabled?RM_WHITE:MUTED);
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
 rm_catalog *cat=w->mode==2?NULL:(rm_catalog *)calloc(1,sizeof(*cat));
 char err[256]={0};
 int outcome=-1;
 if(w->mode==2){
  outcome=rm_verify_run(w->verification,progress,w,err,sizeof(err));
 }else{
  rm_options options={0};
  options.input=w->input;options.output_dir=w->out;
  options.max_pages=w->pages?w->pages:12;
  options.max_depth=w->depth;options.delay_ms=2000;
  options.execute=1;options.authorized=1;
  outcome=cat?rm_crawl(&options,cat,progress,w,err,sizeof(err)):-1;
 }
 mutex_lock(w);
 w->catalog=cat;w->success=outcome==0;
 snprintf(w->error,sizeof(w->error),"%s",err[0]?err:(w->mode==2?"Verification failed":cat?"Documentation crawl failed":"out of memory"));
 w->finished=1;mutex_unlock(w);
#ifdef _WIN32
 return 0;
#else
 return NULL;
#endif
}
static int start(work *w,const char *input,unsigned pages,unsigned depth){
 if(w->active)return -1;
 w->mode=1;w->verification=NULL;
 w->pages=pages;w->depth=depth;
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
/* The verification plan is detached from the UI before the worker can write to it. */
static int start_verification(work *w,rm_verification *plan){
 if(w->active||!plan||!plan->authorized)return -1;
 w->mode=2;w->verification=plan;
 snprintf(w->input,sizeof(w->input),"%s",plan->target.host);
 w->finished=0;w->active=1;w->success=0;
 w->catalog=NULL;w->error[0]=0;w->detail[0]=0;
 snprintf(w->stage,sizeof(w->stage),"verifying");
#ifdef _WIN32
 w->thread=CreateThread(NULL,0,worker,w,0,NULL);
 if(!w->thread){w->active=0;w->verification=NULL;return -1;}
#else
 if(pthread_create(&w->thread,NULL,worker,w)){w->active=0;w->verification=NULL;return -1;}
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
 if(w->mode==2){
  free(ui->verification);
  ui->verification=w->verification;w->verification=NULL;
  ui->verify_stage=2;
  if(w->success){
   size_t passed=0,mismatch=0,partial=0;
   for(size_t i=0;i<ui->verification->count;i++){
    const rm_check *check=&ui->verification->checks[i];
    if(check->outcome==1)passed++;
    else if(check->outcome==2)partial++;
    else if(check->outcome==3||check->outcome==4)mismatch++;
   }
   snprintf(ui->message,sizeof(ui->message),
     "Verification: %zu matched, %zu status-only, %zu mismatched. See inspector.",
     passed,partial,mismatch);
  }else snprintf(ui->message,sizeof(ui->message),"Verification stopped: %.206s",w->error);
 }else if(w->success){
  free(ui->catalog);ui->catalog=w->catalog;w->catalog=NULL;
  free(ui->verification);ui->verification=NULL;ui->verify_stage=0;
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
 int is_openapi=parsed>=0;
 if(parsed<0)parsed=rm_catalog_import_json(cat,src,length,error,sizeof(error));
 /* A local OpenAPI input is evidence too. Keep its actual byte digest and
    filename in the document ledger; never invent an HTTP response status. */
 if(parsed>=0&&is_openapi&&cat->document_count==0){
  rm_document *doc=&cat->documents[cat->document_count++];
  snprintf(doc->url,sizeof(doc->url),"%s",ui->filename);
  snprintf(doc->sha256,sizeof(doc->sha256),"%s",sha);
  snprintf(doc->content_type,sizeof(doc->content_type),"application/json");
  doc->bytes=length;
  doc->status=0;doc->response_status=0;
 }
 free(src);
 if(parsed<0){
  free(cat);
  snprintf(ui->message,sizeof(ui->message),"Import refused: %.214s",error);
  return -1;
 }
 free(ui->catalog);
 free(ui->verification);ui->verification=NULL;ui->verify_stage=0;
 ui->catalog=cat;
 ui->selected=0;ui->scroll=0;ui->atlas_scroll=0;ui->view=0;
 snprintf(ui->message,sizeof(ui->message),"Loaded %d documented operations from local JSON",parsed);
 return 0;
}

/* A genuinely offline, evidence-checked interchange between RESTLESS 42 and
 * RESTMORE. No domain or network access in this action. */
static int load_restless_archive(app *ui){
 rm_catalog *next=(rm_catalog *)calloc(1,sizeof(*next));
 if(!next){snprintf(ui->message,sizeof(ui->message),"Cannot allocate fusion catalog");return -1;}
 rm_fusion_stats stats={0};char error[256]={0};
 if(rm_restless_import(ui->archive_dir,next,&stats,error,sizeof(error))){
  free(next);
  snprintf(ui->message,sizeof(ui->message),"RESTLESS evidence refused: %.202s",error);
  return -1;
 }
 free(ui->catalog);ui->catalog=next;
 free(ui->verification);ui->verification=NULL;ui->verify_stage=0;
 ui->selected=0;ui->scroll=0;ui->atlas_scroll=0;ui->view=0;
 snprintf(ui->message,sizeof(ui->message),
   "RESTLESS fusion: %zu SHA-256 verified documents / %zu OpenAPI operations / no network",
   stats.documents,stats.operations);
 return 0;
}
static void draw_brand(int width){
 DrawRectangle(0,0,width,94,(Color){9,24,24,255});
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
 textx(width<650?"OPS 42 / EVIDENCE ATLAS":"OPS 42 / SOURCE INTELLIGENCE / C11",92,59,11,MUTED);
 if(width>980){
  textx("MISSION CONTROL / OPS 42",width-295,29,14,MINT);
  textx("LOCAL SOURCE BRIEF / AUTHORIZATION FIRST",width-295,56,10,MUTED);
 }
}

static void draw_scope(app *ui,Rectangle area,int busy){
 panel(area,PANEL);
 float maxscroll=fmaxf(0.0f,1415.0f-area.height);
 if(hit(area))ui->scope_scroll-=GetMouseWheelMove()*45+TOUCH_DY;
 if(ui->scope_scroll<0)ui->scope_scroll=0;
 if(ui->scope_scroll>maxscroll)ui->scope_scroll=maxscroll;
 int x=(int)area.x+22,y=(int)(area.y+21-ui->scope_scroll);
 int inner=(int)area.width-44;
 scissor((Rectangle){area.x+1,area.y+1,area.width-2,area.height-2});
 label(x,y,"01  /  OPERATIONS DESK");y+=27;
 textx("Plan the mission.",x,y,25,RM_WHITE);y+=41;
 textx("Document. Attribute. Review. Authorize.",x,y,11,MUTED);y+=34;
 label(x,y,"PUBLIC HTTPS DOMAIN");y+=23;
 inputbox((Rectangle){(float)x,(float)y,(float)inner,48},ui->domain,
          sizeof(ui->domain),&ui->focus,1,!busy);y+=61;

 label(x,y,"CONTRACT COVERAGE / CHOOSE BEFORE MAP");y+=22;
 int gap=7;float bw=((float)inner-2*gap)/3.0f;
 size_t wanted=ui->check_budget;
 if(button((Rectangle){(float)x,(float)y,bw,38},"5 CHECKS",
           wanted==5?MINT:AQUA,!busy))ui->check_budget=5;
 if(button((Rectangle){(float)x+bw+gap,(float)y,bw,38},"25 CHECKS",
           wanted==25?MINT:AQUA,!busy))ui->check_budget=25;
 if(button((Rectangle){(float)x+2*(bw+gap),(float)y,bw,38},"FULL ATLAS",
           wanted==0?MINT:AQUA,!busy))ui->check_budget=0;
 if(wanted!=ui->check_budget&&ui->verify_stage){
  free(ui->verification);ui->verification=NULL;ui->verify_stage=0;
  snprintf(ui->message,sizeof(ui->message),
           "Coverage changed. Review GitHub and authorize the new plan.");
 }
 y+=54;
 /* Any edit after GitHub review invalidates authorization. */
 if(!busy&&ui->verify_stage&&strcmp(ui->domain,ui->review_domain)){
  free(ui->verification);ui->verification=NULL;ui->verify_stage=0;
  snprintf(ui->message,sizeof(ui->message),"Domain changed. Review GitHub and authorize again.");
 }
 if(button((Rectangle){(float)x,(float)y,(float)inner,45},
           "MAP / REVIEW GITHUB",MINT,!busy)){
  char lookup[RM_MAX_URL],error[256]={0};
  if(rm_github_lookup(ui->domain,lookup,sizeof(lookup),error,sizeof(error)))
   snprintf(ui->message,sizeof(ui->message),"Domain refused: %.204s",error);
  else{
   /* The FIRST action opens a human-reviewed GitHub repository search.
      A matching search result is NOT proof of domain ownership. */
   OpenURL(lookup);
   rm_verification *candidate=(rm_verification *)calloc(1,sizeof(*candidate));
   if(!candidate){
    snprintf(ui->message,sizeof(ui->message),"Verification plan out of memory");
   }else if(rm_verify_plan_limit(ui->catalog,ui->domain,candidate,ui->check_budget,error,sizeof(error))){
    free(candidate);
    snprintf(ui->message,sizeof(ui->message),"GitHub opened. Atlas not testable: %.177s",error);
   }else{
    free(ui->verification);
    ui->verification=candidate;ui->verify_stage=1;
    snprintf(ui->review_domain,sizeof(ui->review_domain),"%s",ui->domain);
    snprintf(ui->message,sizeof(ui->message),
     "GitHub repository search opened. Review ownership and permission, then click AUTHORIZE.");
   }
  }
  ui->focus=0;
 }
 y+=60;
 textx("GitHub search: lead only, not ownership proof.",x,y,11,MUTED);y+=25;
 Rectangle review={(float)x,(float)y,(float)inner,215};
 DrawRectangleRounded(review,.08f,8,Fade(AQUA,.075f));
 DrawRectangleRoundedLinesEx(review,.08f,8,1,Fade(AQUA,.28f));
 label(x+12,y+14,"02  /  SOURCE & AUTHORIZATION GATE");
 if(ui->verify_stage==1&&ui->verification){
  char scope[RM_MAX_URL];
  cut_text(ui->verification->target.host,scope,sizeof(scope),inner-24,15);
  textx(scope,x+12,y+35,15,RM_WHITE);
  textx(ui->verification->source_bound?"OPENAPI SERVER MATCHED TO DOMAIN":
        "BLOCKED: IMPORT DOMAIN-BOUND OPENAPI",
        x+12,y+61,11,ui->verification->source_bound?MINT:RM_ORANGE);
  Rectangle consent_area={(float)x+12,(float)y+88,(float)inner-24,44};
  if(button(consent_area,
      ui->verification->github_reviewed?"GITHUB REVIEW CONFIRMED":"I REVIEWED GITHUB & HAVE PERMISSION",
      AQUA,!busy&&!ui->verification->github_reviewed)){
    /* GitHub search is merely a lead. The human explicitly confirms their
       permission; the application does not claim to verify domain ownership. */
    ui->verification->github_reviewed=1;
    snprintf(ui->message,sizeof(ui->message),
      "GitHub review acknowledged. Authorize only the displayed documented GET plan.");
  }
  textx("No repo ownership claim; check the source yourself.",x+12,y+140,10,MUTED);
  char authorization_label[90];
  snprintf(authorization_label,sizeof(authorization_label),
    "AUTHORIZE / %zu DOCUMENTED GET CHECKS",ui->verification->count);
  if(button((Rectangle){(float)x+12,(float)y+155,(float)inner-24,46},
            RESTMORE_NETWORK_ENABLED?authorization_label:"NETWORK DISABLED",
            MINT,RESTMORE_NETWORK_ENABLED&&!busy&&ui->verification->source_bound&&ui->verification->github_reviewed)){
   /* Separate authorization gesture, bound to this exact host and OpenAPI
      source. Unknown routes, mutations and credentials are never executed. */
   rm_target check={0};char err[256]={0};
   if(rm_normalize(ui->domain,&check,err,sizeof(err)) ||
      strcmp(check.host,ui->verification->target.host)){
    free(ui->verification);ui->verification=NULL;ui->verify_stage=0;
    snprintf(ui->message,sizeof(ui->message),"Domain changed; authorization invalidated");
   }else{
    ui->verification->authorized=1;
    if(start_verification(&ui->task,ui->verification)){
     ui->verification->authorized=0;
     snprintf(ui->message,sizeof(ui->message),"Cannot start verification worker");
    }else{
     ui->verification=NULL;ui->verify_stage=0;
     snprintf(ui->message,sizeof(ui->message),"Authorized bounded GET verification started");
    }
   }
  }
 }else if(busy&&ui->task.mode==2){
  textx("VERIFYING DOCUMENTED GET ROUTES",x+12,y+54,13,MINT);
 }else if(ui->verify_stage==2){
  textx("CHECK COMPLETE / OPEN INSPECTOR",x+12,y+52,13,MINT);
  textx("New MAP review required to repeat.",x+12,y+82,12,MUTED);
 }else{
  textx("1. GitHub search opened by MAP.",x+12,y+51,12,MUTED);
  textx("2. Verify correct repo and permission.",x+12,y+78,12,MUTED);
  textx("3. Click review, then AUTHORIZE.",x+12,y+108,12,MUTED);
 }
 y+=231;
 rule((float)x,(float)y,(float)inner);y+=26;
 label(x,y,"03  /  OPTIONAL DOCUMENTATION DISCOVERY");y+=27;
 Rectangle permission={(float)x,(float)y,(float)inner,76};
 DrawRectangleRounded(permission,.08f,8,Fade(AQUA,.055f));
 DrawRectangleRoundedLinesEx(permission,.08f,8,1,Fade(AQUA,.20f));
 Rectangle checkbox={(float)x+12,(float)y+14,20,20};
 DrawRectangleRounded(checkbox,.22f,6,ui->consent?MINT:PANEL2);
 DrawRectangleRoundedLinesEx(checkbox,.22f,6,1,MINT);
 if(ui->consent)textx("X",(int)checkbox.x+4,(int)checkbox.y+3,14,BG);
 if(!busy&&clicked((Rectangle){(float)x,(float)y,(float)inner,58}))ui->consent=!ui->consent;
 textx("AUTHORIZED TO FETCH DOCS",x+41,y+17,12,RM_WHITE);
 textx("Docs GET only / same host / robots-aware",x+12,y+51,11,MUTED);
 y+=89;
 char crawl_label[112];
 snprintf(crawl_label,sizeof(crawl_label),
          "CRAWL BUDGET: %u DOCS / DEPTH %u",ui->crawl_pages,ui->crawl_depth);
 if(button((Rectangle){(float)x,(float)y,(float)inner,42},
           crawl_label,AQUA,!busy)){
  /* Explicit operator scope selection, never a background scan. */
  if(ui->crawl_pages==12){ui->crawl_pages=64;ui->crawl_depth=4;}
  else if(ui->crawl_pages==64){ui->crawl_pages=256;ui->crawl_depth=8;}
  else{ui->crawl_pages=12;ui->crawl_depth=2;}
 }
 y+=57;
 if(button((Rectangle){(float)x,(float)y,(float)inner,44},"BUILD SOURCE LIBRARY",
           AQUA,!busy&&ui->consent&&RESTMORE_NETWORK_ENABLED)){
  rm_target target={0};char error[256]={0};
  if(rm_normalize(ui->domain,&target,error,sizeof(error)))
   snprintf(ui->message,sizeof(ui->message),"Scope refused: %.200s",error);
  else if(start(&ui->task,ui->domain,ui->crawl_pages,ui->crawl_depth))
   snprintf(ui->message,sizeof(ui->message),"Cannot start documentation worker");
  ui->focus=0;
 }
 y+=61;
 rule((float)x,(float)y,(float)inner);y+=25;
 label(x,y,"04  /  OFFLINE INGEST");y+=25;
 textx("Import an atlas.",x,y,22,RM_WHITE);y+=37;
 label(x,y,"OPENAPI 3 / RESTMORE LIBRARY.JSON");y+=22;
 inputbox((Rectangle){(float)x,(float)y,(float)inner,46},ui->filename,
          sizeof(ui->filename),&ui->focus,2,!busy);y+=58;
 if(button((Rectangle){(float)x,(float)y,(float)inner,44},"OPEN LOCAL DOCUMENT",
           AQUA,!busy)){
  load_catalog(ui);ui->focus=0;
 }
 y+=60;
 rule((float)x,(float)y,(float)inner);y+=22;
 label(x,y,"05  /  RESTLESS BOTANICAL FUSION");y+=26;
 textx("Import verified history.",x,y,20,RM_WHITE);y+=37;
 label(x,y,"RESTLESS INDEX.JSONL + SHA256 DIRECTORY");y+=21;
 inputbox((Rectangle){(float)x,(float)y,(float)inner,44},ui->archive_dir,
          sizeof(ui->archive_dir),&ui->focus,3,!busy);y+=58;
 if(button((Rectangle){(float)x,(float)y,(float)inner,45},
            "IMPORT VERIFIED ARCHIVE",MINT,!busy)){
   load_restless_archive(ui);ui->focus=0;
 }
 y+=54;
 textx("Hashes checked before any atlas operation.",x,y,11,MUTED);y+=20;
 textx("No speculative paths. No unsafe methods.",x,y,11,MUTED);
 EndScissorMode();
 if(maxscroll>0){
  float track=area.height-35,thumb=track*area.height/1415.0f;
  DrawRectangleRounded((Rectangle){area.x+area.width-6,area.y+17,3,track},
                       .4f,6,Fade(MUTED,.13f));
  DrawRectangleRounded((Rectangle){area.x+area.width-6,
     area.y+17+ui->scope_scroll/maxscroll*(track-thumb),3,thumb},.4f,6,Fade(MINT,.65f));
 }
}
static void select_operation(app *ui,int index){
 ui->selected=index;
 ui->atlas_scroll=0;
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
 /* On a handset, opening a route immediately reveals its provenance. */
 ui->tab=2;
#endif
}
static void graph(app *ui,Rectangle area){
 panel(area,PANEL);
 if(area.width<220||area.height<240)return;
 int x=(int)area.x+20,y=(int)area.y+21;
 label(x,y,"03  /  EVIDENCE TOPOLOGY");
 textx("Operational atlas",x,y+23,25,RM_WHITE);
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
   if(clicked(target))select_operation(ui,(int)i);
   char path[80];cut_text(op->path,path,sizeof(path),narrow?85:134,11);
   int txtw=textwidth(path,11);
   float tx=p.x+(p.x<cx?-txtw-18:18);
   if(tx<viewport.x+7)tx=viewport.x+7;
   if(tx+txtw>viewport.x+viewport.width-7)tx=viewport.x+viewport.width-txtw-7;
   float ty=p.y+((i%2)?14:-24);
   textx(path,(int)tx,(int)ty,11,selected?RM_WHITE:MUTED);
  }
  /* A subtle forensic-draughtsman's arch, not part of the API topology. */
  if(viewport.width>=360 && viewport.height>=300)
   draw_arch_study(viewport.x+14,viewport.y+viewport.height-31,.55f);
  if(count>shown){
   char rest[80];snprintf(rest,sizeof(rest),"+%zu ROUTES  /  SWITCH TO ROUTES VIEW",count-shown);
   textx(rest,(int)viewport.x+16,(int)(viewport.y+viewport.height-22),12,MINT);
  }
 }else{
  const int card_height=62,step=70;
  float content=(float)count*step+15;
  float maxscroll=fmaxf(0,content-viewport.height);
  if(hit(viewport))ui->scroll-=GetMouseWheelMove()*58+TOUCH_DY;
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
   if(clicked(row))select_operation(ui,(int)i);
  }
  if(maxscroll>0){
   float track=viewport.height-12,thumb=track*viewport.height/content;
   DrawRectangleRounded((Rectangle){viewport.x+viewport.width-4,viewport.y+5,3,track},1,6,Fade(MUTED,.16f));
   DrawRectangleRounded((Rectangle){viewport.x+viewport.width-4,viewport.y+5+ui->scroll/maxscroll*(track-thumb),3,thumb},1,6,MINT);
  }
 }
 EndScissorMode();
 label((int)area.x+20,(int)(area.y+area.height-18),
  ui->verification&&ui->verify_stage==2?
   "AUTHORIZED GET CHECKS / RESULTS IN INSPECTOR":
   "DOCUMENTED OPERATIONS / NO AUTOMATIC EXECUTION");
}

static void inspector(app *ui,Rectangle area){
 panel(area,PANEL);
 if(area.width<220||area.height<210)return;
 size_t check_rows=ui->verification&&ui->verify_stage==2?
                   ui->verification->count:0;
 float content_height=fmaxf(1080.0f,738.0f+47.0f*(float)check_rows);
 float maxscroll=fmaxf(0,content_height-area.height+25);
 if(hit(area)){
  ui->atlas_scroll-=GetMouseWheelMove()*45+TOUCH_DY;
  if(IsKeyPressed(KEY_PAGE_DOWN))ui->atlas_scroll+=area.height*.8f;
  if(IsKeyPressed(KEY_PAGE_UP))ui->atlas_scroll-=area.height*.8f;
  if(IsKeyPressed(KEY_HOME))ui->atlas_scroll=0;
  if(IsKeyPressed(KEY_END))ui->atlas_scroll=maxscroll;
 }
 if(ui->atlas_scroll<0)ui->atlas_scroll=0;
 if(ui->atlas_scroll>maxscroll)ui->atlas_scroll=maxscroll;
 int x=(int)area.x+22,y=(int)(area.y+23-ui->atlas_scroll),inner=(int)area.width-44;
 scissor((Rectangle){area.x+1,area.y+1,area.width-2,area.height-2});
 label(x,y,"04  /  EVIDENCE INSPECTOR");y+=28;
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
 y+=59;
 if(button((Rectangle){(float)x,(float)y,(float)inner,43},
           "EXPORT OPS 42 MISSION BRIEF",RM_ORANGE,1)){
  char error[256]={0};
  if(rm_make_dir("restmore-export")||
     rm_ops_brief_write(ui->catalog,"restmore-export/ops-brief.json",error,sizeof(error)))
   snprintf(ui->message,sizeof(ui->message),"Mission brief failed: %.202s",error[0]?error:"Cannot create output");
  else snprintf(ui->message,sizeof(ui->message),
                "Exported offline evidence brief. State: REVIEW REQUIRED, no authorization granted.");
 }
 y+=62;
 rule((float)x,(float)y,(float)inner);y+=22;
 label(x,y,"05  /  CONTRACT VERIFICATION");y+=24;
 if(ui->verification&&ui->verify_stage==2){
  char heading[120];
  snprintf(heading,sizeof(heading),"%zu planned GET checks / %.105s",
    ui->verification->count,ui->verification->target.host);
  char shortheading[130];cut_text(heading,shortheading,sizeof(shortheading),inner,12);
  textx(shortheading,x,y,12,MINT);y+=25;
  for(size_t i=0;i<ui->verification->count;i++){
   const rm_check *check=&ui->verification->checks[i];
   /* Large authorized plans remain responsive: only paint visible rows. */
   if(y+47<(int)area.y||y>(int)(area.y+area.height)){
    y+=47;continue;
   }
   char route[RM_MAX_PATH],status[96];
   cut_text(check->path,route,sizeof(route),inner-8,13);
   textx(route,x,y,13,RM_WHITE);y+=19;
   const char *verdict=check->outcome==1?"MATCH":
     check->outcome==2?"STATUS ONLY":check->outcome==3?"MISMATCH":
     check->outcome==4?"ERROR":check->outcome==5?"SKIPPED":"NOT RUN";
   snprintf(status,sizeof(status),"%s / expected %d / HTTP %d",
           verdict,check->expected_status,check->actual_status);
   Color c=check->outcome==1?MINT:check->outcome==2?AQUA:RM_ORANGE;
   textx(status,x,y,11,c);y+=28;
  }
  if(button((Rectangle){(float)x,(float)y,(float)inner,42},
            "EXPORT VERIFICATION JSON",MINT,1)){
   char report_error[256]={0};
   if(rm_verify_report(ui->verification,"restmore-verification.json",
                       report_error,sizeof(report_error)))
    snprintf(ui->message,sizeof(ui->message),"Report export refused: %.187s",report_error);
   else snprintf(ui->message,sizeof(ui->message),
                 "Saved restmore-verification.json / no response bodies stored.");
  }
  y+=55;
  textx("Status + type + JSON syntax, not schema proof.",x,y,11,MUTED);y+=22;
 }else if(ui->task.active&&ui->task.mode==2){
  textx("Awaiting bounded HTTPS results...",x,y,12,MINT);y+=29;
 }else{
  textx("Map > GitHub review > Authorize.",x,y,12,MUTED);y+=27;
 }
 textx("Only explicit, safe GETs may be tested.",x,y,11,MUTED);y+=19;
 textx("No fuzzing, mutation, or credential replay.",x,y,11,MUTED);
 EndScissorMode();
 if(maxscroll>0){
  float track=area.height-32;
  float thumb=fmaxf(14.0f,track*area.height/(content_height+25));
  DrawRectangleRounded((Rectangle){area.x+area.width-6,area.y+16,3,track},1,6,Fade(MUTED,.14f));
  DrawRectangleRounded((Rectangle){area.x+area.width-6,area.y+16+ui->atlas_scroll/maxscroll*(track-thumb),3,thumb},1,6,MINT);
 }
}


/* Analyst Workbench / native mobile view.
 * Unlike the desktop scope desk, the Android preview has no HTTPS worker and
 * no document picker. Expose only usable offline actions; do not dress a
 * disabled AUTHORIZE control up as if it were operational. */
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
static void mobile_kpi(Rectangle box,const char *value,const char *caption){
 DrawRectangleRounded(box,.12f,8,Fade(AQUA,.09f));
 DrawRectangleRoundedLinesEx(box,.12f,8,1,BORDER);
 textx(value,(int)box.x+16,(int)box.y+11,28,RM_WHITE);
 textx(caption,(int)box.x+16,(int)box.y+49,12,MUTED);
}
static void mobile_desk(app *ui,Rectangle area){
 panel(area,PANEL);
 int x=(int)area.x+19,y=(int)area.y+24,inner=(int)area.width-38;
 label(x,y,"01  /  ANALYST WORKBENCH"); y+=26;
 textx("Understand first.",x,y,26,RM_WHITE); y+=43;
 textx("A documented API is not permission.",x,y,13,MUTED); y+=37;
 size_t operations=ui->catalog?ui->catalog->operation_count:0;
 size_t documents=ui->catalog?ui->catalog->document_count:0;
 char routes[24],sources[24];
 snprintf(routes,sizeof(routes),"%zu",operations);
 snprintf(sources,sizeof(sources),"%zu",documents);
 float half=((float)inner-10)/2.0f;
 mobile_kpi((Rectangle){(float)x,(float)y,half,83},routes,"ROUTES");
 mobile_kpi((Rectangle){(float)x+half+10,(float)y,half,83},sources,"SOURCES");
 y+=101;
 DrawRectangleRounded((Rectangle){(float)x,(float)y,(float)inner,72},
                       .10f,8,Fade(MINT,.055f));
 DrawRectangleRoundedLinesEx((Rectangle){(float)x,(float)y,(float)inner,72},
                             .10f,8,1,Fade(MINT,.42f));
 textx("OFFLINE / REVIEW REQUIRED",x+15,y+12,14,MINT);
 textx("No target contacted. No authority inferred.",x+15,y+40,12,MUTED);
 y+=94;
 label(x,y,"LOCAL SOURCE INTELLIGENCE");y+=27;
 if(button((Rectangle){(float)x,(float)y,(float)inner,53},
           "OPEN INCLUDED SAMPLE",MINT,!ui->task.active)){
  snprintf(ui->filename,sizeof(ui->filename),"examples/openapi-petstore.json");
  if(load_catalog(ui)==0){ui->view=1;ui->tab=1;}
 } y+=67;
 if(button((Rectangle){(float)x,(float)y,(float)inner,53},
           "EXPLORE DOCUMENTED ROUTES",AQUA,operations>0)){
  ui->view=1;ui->tab=1;
 } y+=67;
 if(button((Rectangle){(float)x,(float)y,(float)inner,53},
           "INSPECT SOURCE EVIDENCE",AQUA,operations>0)){
  ui->tab=2;
 } y+=77;
 rule((float)x,(float)y,(float)inner);y+=24;
 label(x,y,"CAPABILITY BOUNDARY");y+=26;
 textx("Android preview: local sample only.",x,y,14,RM_WHITE);y+=25;
 textx("Desktop CLI: verified import and export.",x,y,12,MUTED);y+=24;
 textx("No live scan, credentials, or bypass.",x,y,12,MUTED);
}
static void mobile_evidence(app *ui,Rectangle area){
 panel(area,PANEL);
 int x=(int)area.x+19,y=(int)area.y+23,inner=(int)area.width-38;
 label(x,y,"03  /  SOURCE INTELLIGENCE");y+=28;
 textx("Trace the evidence.",x,y,25,RM_WHITE);y+=47;
 if(!ui->catalog||ui->catalog->operation_count==0){
  textx("NO DOCUMENTED OPERATIONS",x,y,15,RM_WHITE);
  textx("Open the included offline sample.",x,y+32,13,MUTED);
  return;
 }
 if(ui->selected<0||ui->selected>=(int)ui->catalog->operation_count)
  ui->selected=0;
 const rm_operation *op=&ui->catalog->operations[ui->selected];
 char route[RM_MAX_PATH],summary[256],source[RM_MAX_URL];
 method_pill(op->method,(float)x,(float)y,84);
 char position[48];
 snprintf(position,sizeof(position),"%d / %zu",ui->selected+1,ui->catalog->operation_count);
 textx(position,x+inner-69,y+9,15,MUTED);y+=53;
 label(x,y,"DOCUMENTED ROUTE");y+=25;
 cut_text(op->path,route,sizeof(route),inner,19);
 textx(route,x,y,19,RM_WHITE);y+=45;
 label(x,y,"SOURCE DESCRIPTION");y+=24;
 cut_text(op->summary[0]?op->summary:"No summary declared",
          summary,sizeof(summary),inner,14);
 textx(summary,x,y,14,RM_WHITE);y+=46;
 rule((float)x,(float)y,(float)inner);y+=25;
 label(x,y,"ORIGIN / PROVENANCE");y+=27;
 cut_text(op->source_url[0]?op->source_url:"Not present in source",
          source,sizeof(source),inner,13);
 textx(source,x,y,13,RM_WHITE);y+=38;
 label(x,y,"SOURCE SHA-256 / RECORDED, NOT REVERIFIED");y+=24;
 if(strlen(op->source_sha256)==64){
  char first[33],second[33];
  memcpy(first,op->source_sha256,32);first[32]=0;
  memcpy(second,op->source_sha256+32,32);second[32]=0;
  textx(first,x,y,12,MINT);y+=21;
  textx(second,x,y,12,MINT);y+=39;
 }else{
  textx("No document fingerprint supplied",x,y,13,RM_ORANGE);y+=52;
 }
 rule((float)x,(float)y,(float)inner);y+=25;
 char response[96];
 snprintf(response,sizeof(response),"HTTP %d / %s",
  op->expected_status,
  op->expected_content_type[0]?op->expected_content_type:"MIME not specified");
 label(x,y,"DOCUMENTED RESPONSE CONTRACT");y+=24;
 char cut[96];
 cut_text(response,cut,sizeof(cut),inner,13);
 textx(cut,x,y,13,RM_WHITE);y+=55;
 if(button((Rectangle){(float)x,(float)y,(float)inner,53},
           "COPY ROUTE TEMPLATE",AQUA,1)){
  char copy[RM_MAX_PATH+140];
  snprintf(copy,sizeof(copy),"curl -X %s '{{baseUrl}}%s'",op->method,op->path);
  SetClipboardText(copy);
  snprintf(ui->message,sizeof(ui->message),
           "Copied illustrative template. Zero target requests.");
 } y+=73;
 textx("IMPORTED CONTRACT / NOT LIVE-VERIFIED",x,y,12,RM_ORANGE);
}
#endif

int main(int argc,char **argv){
 int smoke=argc>1&&!strcmp(argv[1],"--smoke");
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
 /* CRITICAL: raylib Android treats InitWindow dimensions as the logical
  * framebuffer AND uses their aspect ratio to decide orientation. Reusing
  * the desktop 1350x820 buffer made real phones draw an oversized landscape
  * canvas, cropped in both portrait and landscape (OPS 42 field report).
  *
  * Target a legible, handset-first 20:9 portrait framebuffer. The Android
  * platform maps it to the physical display. Do not apply a SECOND scale
  * inside our camera or touch coordinates. */
 SetConfigFlags(FLAG_MSAA_4X_HINT);
 InitWindow(432,960,"RESTMORE / OPS 42");
#else
 SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);
 InitWindow(1350,820,"RESTMORE  /  OPS 42 MISSION CONTROL");
 SetWindowMinSize(360,580);
#endif
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
 snprintf(ui->archive_dir,sizeof(ui->archive_dir),".restless/botanical");
 ui->check_budget=5;ui->crawl_pages=12;ui->crawl_depth=2;
 snprintf(ui->message,sizeof(ui->message),"OPS 42 ready. Offline evidence briefing available. No target GET without review and authorization.");
 mutex_init(&ui->task);
 load_catalog(ui);
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
 /* Show actual imported routes first, rather than a desktop-sized
    authorization form that cannot perform remote checks on Android. */
 ui->tab=1;
 ui->view=1;
#endif
 int frames=0;
 while(!WindowShouldClose()&&(!smoke||frames++<60)){
  poll_work(ui);
  int actualW=GetScreenWidth(),actualH=GetScreenHeight();
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
  /* The native framebuffer is already 432x960 logical portrait pixels;
     Android scales this to the physical display. Double scaling would
     enlarge/crop the interface and de-align touch and scissor regions. */
  UI_SCALE=1.0f;
#else
  UI_SCALE=fminf(1.0f,fmaxf(.84f,(float)actualW/690.0f));
#endif
  sample_touch();
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
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
   const char *tabs[]={"01  SCOPE","02  ATLAS","03  EVIDENCE"};
#else
   const char *tabs[]={"01  SCOPE","02  ATLAS","03  INSPECT"};
#endif
   float tabW=((float)W-44.0f)/3.0f;
   for(int i=0;i<3;i++){
    Rectangle tab={22+tabW*(float)i,104,tabW-8,41};
    if(button(tab,tabs[i],i==ui->tab?MINT:AQUA,1))ui->tab=i;
   }
   Rectangle whole={19,155,(float)W-38,(float)H-226};
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
   if(ui->tab==0)mobile_desk(ui,whole);
   else if(ui->tab==1)graph(ui,whole);
   else mobile_evidence(ui,whole);
#else
   if(ui->tab==0)draw_scope(ui,whole,ui->task.active);
   else if(ui->tab==1)graph(ui,whole);
   else inspector(ui,whole);
#endif
  }
  DrawRectangle(0,H-56,W,56,(Color){8,25,24,255});
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
  }else if(W>850)textx("OPS 42 / CONSENT GATED",W-232,H-38,12,MINT);
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
  free(ui->task.verification);
  ui->task.catalog=NULL;ui->task.verification=NULL;
  ui->task.active=0;
 }
#ifdef _WIN32
 DeleteCriticalSection(&ui->task.mutex);
#else
 pthread_mutex_destroy(&ui->task.mutex);
#endif
 free(ui->catalog);
 free(ui->verification);
 free(ui);
 if(FACE_OWNED)UnloadFont(FACE);
 CloseWindow();
 return 0;
}

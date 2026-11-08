"use strict";
/* RESTMORE's Pages gallery displays a local, entirely synthetic sample.
   The actual documentation crawler and JSON parser live in the native C11 app. */
const sample=[
 {method:"GET",path:"/v1/users",id:"listUsers",summary:"List users"},
 {method:"POST",path:"/v1/users",id:"createUser",summary:"Create a user"},
 {method:"GET",path:"/v1/users/{id}",id:"getUser",summary:"Read a user"},
 {method:"PATCH",path:"/v1/users/{id}",id:"updateUser",summary:"Update user details"},
 {method:"GET",path:"/v1/projects",id:"listProjects",summary:"List projects"},
 {method:"POST",path:"/v1/projects",id:"createProject",summary:"Create a project"},
 {method:"GET",path:"/v1/projects/{id}",id:"getProject",summary:"Read a project"},
 {method:"DELETE",path:"/v1/projects/{id}",id:"deleteProject",summary:"Delete a project"},
 {method:"GET",path:"/v1/events",id:"listEvents",summary:"List events"},
 {method:"GET",path:"/v1/health",id:"healthStatus",summary:"Check service health"}
];
const ns="http://www.w3.org/2000/svg";
const colors={GET:"#79f9dc",POST:"#e9c078",PATCH:"#b1a4ff",DELETE:"#ff9387"};
let selected=0,mode="graph";
const byId=id=>document.getElementById(id);
function svgElement(tag,attrs={},text){const node=document.createElementNS(ns,tag);for(const [key,value] of Object.entries(attrs))node.setAttribute(key,String(value));if(text!==undefined)node.textContent=text;return node}
function operationTemplate(op){return "curl -X "+op.method+" '{{baseUrl}}"+op.path+"'";}
function selectRoute(i){
 if(!Number.isInteger(i)||i<0||i>=sample.length)return;
 selected=i;
 const op=sample[i];
 byId("inspector-method").textContent=op.method;
 byId("inspector-method").dataset.method=op.method;
 byId("inspector-path").textContent=op.path;
 byId("inspector-path-detail").textContent=op.path;
 byId("inspector-summary").textContent=op.summary;
 byId("inspector-id").textContent=op.id;
 byId("inspector-template").textContent=operationTemplate(op);
 byId("copy-status").textContent="";
 document.querySelectorAll("#vertices g[data-index]").forEach(node=>node.dataset.selected=String(Number(node.dataset.index)===selected));
 document.querySelectorAll("#list .route-item").forEach((node,i)=>node.setAttribute("aria-pressed",String(i===selected)));
}
function plot(){
 const lines=byId("edges"),vertices=byId("vertices");
 lines.replaceChildren();vertices.replaceChildren();
 const cx=425,cy=170;
 const guides=svgElement("g",{"aria-hidden":"true",stroke:"#34586a",fill:"none"});
 for(const r of [50,95,144])guides.append(svgElement("circle",{cx,cy,r,"stroke-width":1,"stroke-dasharray":r===144?"3 7":"none",opacity:".5"}));
 guides.append(svgElement("path",{d:"M40 170H811 M425 8V319",opacity:".35","stroke-width":1}));
 lines.append(guides);
 sample.forEach((op,i)=>{
  const a=i*Math.PI*2/sample.length-Math.PI/2;
  const x=cx+Math.cos(a)*291,y=cy+Math.sin(a)*128;
  lines.append(svgElement("line",{x1:cx,y1:cy,x2:x,y2:y,stroke:"#31596a","stroke-width":1.4}));
  const group=svgElement("g",{"role":"button","tabindex":0,"data-index":i,"data-selected":String(i===selected),"aria-label":"Inspect "+op.method+" "+op.path});
  group.append(svgElement("circle",{cx:x,cy:y,r:14,fill:"#0b2230",stroke:colors[op.method]||"#79f9dc","stroke-width":2,class:"route-node"}));
  group.append(svgElement("circle",{cx:x,cy:y,r:5,fill:colors[op.method]||"#79f9dc"}));
  group.append(svgElement("text",{x:x+(x<cx?-17:17),y:y-12,"text-anchor":x<cx?"end":"start",fill:"#b5d9da","font-size":10,"font-family":"monospace"},op.path));
  group.addEventListener("click",()=>selectRoute(i));
  group.addEventListener("keydown",e=>{if(e.key==="Enter"||e.key===" "){e.preventDefault();selectRoute(i)}});
  vertices.append(group);
 });
 vertices.append(svgElement("circle",{cx,cy,r:39,fill:"#173c44",stroke:"#79f9dc","stroke-width":2}));
 vertices.append(svgElement("text",{x:cx,y:cy+6,"text-anchor":"middle",fill:"#e0fcf3","font-size":20,"font-weight":"700","font-family":"monospace"},"API"));
}
function list(){
 const panel=byId("list");panel.replaceChildren();
 sample.forEach((op,i)=>{
  const row=document.createElement("button");row.type="button";
  row.className="route-item";row.setAttribute("aria-pressed",String(i===selected));
  const method=document.createElement("span"),path=document.createElement("span");
  method.className="method";method.textContent=op.method;method.style.color=colors[op.method]||colors.GET;
  path.textContent=op.path;
  row.append(method,path);
  row.addEventListener("click",()=>selectRoute(i));panel.append(row);
 });
}
document.querySelectorAll("[data-mode]").forEach(button=>{
 button.addEventListener("click",()=>{
  mode=button.dataset.mode;
  document.querySelectorAll("[data-mode]").forEach(b=>{const active=b.dataset.mode===mode;b.classList.toggle("selected",active);b.setAttribute("aria-pressed",String(active))});
  byId("viz").hidden=mode!=="graph";byId("list").hidden=mode!=="list";
 });
});
byId("render").addEventListener("click",()=>{
 plot();list();selectRoute(0);
 byId("render").textContent="SAMPLE REDRAWN ✓";
 window.setTimeout(()=>{byId("render").textContent="REDRAW SAMPLE ↻"},1200);
});
byId("copy-template").addEventListener("click",async()=>{
 const text=operationTemplate(sample[selected]);
 try{
  if(!navigator.clipboard?.writeText)throw Error("Clipboard unavailable");
  await navigator.clipboard.writeText(text);
  byId("copy-status").textContent="Example copied. Nothing executed.";
 }catch{
  byId("copy-status").textContent="Copy unavailable. Example shown below.";
 }
});
plot();list();selectRoute(0);

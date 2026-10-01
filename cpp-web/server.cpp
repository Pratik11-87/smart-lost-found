#include "matcher.hpp"
#include "httplib.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

using smart_lost_found::Item;
namespace {
std::mutex data_mutex;
std::vector<Item> reports;
std::string esc(const std::string& s) {
 std::string o;
 for(char c:s) switch(c) {
 case '&':o+="&amp;";break; case '<':o+="&lt;";break; case '>':o+="&gt;";break;
 case '"':o+="&quot;";break; case '\'':o+="&#39;";break; default:o+=c;
 }
 return o;
}
std::string trim(const std::string& s) {
 auto a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos)return {};
 return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
std::string start(const std::string& title) {
 return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>"+esc(title)+" · Smart Lost &amp; Found</title><link rel=\"stylesheet\" href=\"/style.css\"></head><body><header><a class=\"brand\" href=\"/\">SMART <span>LOST &amp; FOUND</span></a><nav><a href=\"/\">Browse</a><a href=\"/report\">Report an item</a></nav></header><main>";
}
std::string end() { return "</main><footer>Smart Lost &amp; Found · Campus item recovery</footer></body></html>"; }
void page(httplib::Response& r,const std::string& title,const std::string& body) { r.set_content(start(title)+body+end(),"text/html; charset=utf-8"); }
std::string field(const std::string& label,const std::string& name,const std::string& type="text") {
 return "<label>"+label+"<input name=\""+name+"\" type=\""+type+"\" required></label>";
}
std::string card(const Item& x,std::size_t i) {
 std::ostringstream o;
 o<<"<article class=\"item\"><span class=\"pill\">"<<esc(x.type)<<"</span><h3>"<<esc(x.title)<<"</h3><p>"<<esc(x.description)<<"</p><dl><dt>Category</dt><dd>"<<esc(x.category)<<"</dd><dt>Location</dt><dd>"<<esc(x.location)<<"</dd><dt>Date</dt><dd>"<<esc(x.date)<<"</dd></dl>";
 if(x.type=="found")o<<"<form method=\"post\" action=\"/match\"><input type=\"hidden\" name=\"index\" value=\""<<i<<"\"><button>Find possible matches</button></form>";
 return o.str()+"</article>";
}
std::string css() { return R"CSS(:root{font-family:system-ui,sans-serif;color:#172033;background:#f4f7fb}*{box-sizing:border-box}body{margin:0}header{display:flex;justify-content:space-between;align-items:center;gap:20px;padding:22px max(5vw,20px);background:#101b36;color:white}.brand{font-weight:900;color:white;text-decoration:none}.brand span{color:#71d8cb}nav{display:flex;gap:18px}nav a{color:white;text-decoration:none}main{max-width:1050px;margin:36px auto;padding:0 20px}.hero{padding:34px;border-radius:20px;background:linear-gradient(120deg,#142447,#246a80);color:white}.hero h1{font-size:clamp(2rem,5vw,3.6rem);margin:0}.hero p{max-width:650px;line-height:1.6;color:#dceaf5}.actions{display:flex;gap:12px;margin-top:22px;flex-wrap:wrap}.btn,button{display:inline-block;background:#087e8b;color:white;padding:11px 16px;border:0;border-radius:9px;text-decoration:none;font-weight:700;cursor:pointer}.btn.secondary{background:white;color:#132341}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px;margin:20px 0}.item,.panel{background:white;border:1px solid #e0e6ef;border-radius:15px;padding:20px;box-shadow:0 5px 20px #1a2b4408}.item p{color:#526078;white-space:pre-wrap;overflow-wrap:anywhere}.pill{display:inline-block;text-transform:uppercase;font-size:.72rem;font-weight:800;background:#e1f7f4;color:#087e8b;border-radius:20px;padding:5px 9px}dl{display:grid;grid-template-columns:90px 1fr;gap:7px;font-size:.9rem}dt{color:#69768a}dd{margin:0;overflow-wrap:anywhere}form.report{display:grid;gap:15px;max-width:700px}label{display:grid;gap:7px;font-weight:650}input,textarea,select{font:inherit;width:100%;padding:11px 12px;border:1px solid #cbd5e1;border-radius:9px;background:white}textarea{min-height:100px}footer{text-align:center;color:#7b879a;padding:28px}.muted{color:#68768c}@media(max-width:520px){header{align-items:flex-start;flex-direction:column}.hero{padding:24px}})CSS"; }
}
int main() {
 httplib::Server app;
 app.Get("/style.css",[](const httplib::Request&,httplib::Response& r){r.set_content(css(),"text/css; charset=utf-8");});
 app.Get("/",[](const httplib::Request&,httplib::Response& r){
  std::lock_guard<std::mutex> lock(data_mutex); std::ostringstream b;
  b<<"<section class=\"hero\"><h1>Lost something?<br>Found something?</h1><p>A campus noticeboard for lost and found items, with possible matches ranked by an explainable C++ scoring system.</p><div class=\"actions\"><a class=\"btn\" href=\"/report\">Report an item</a><a class=\"btn secondary\" href=\"#reports\">Browse reports</a></div></section><h2 id=\"reports\">Recent reports</h2><p class=\"muted\">"<<reports.size()<<" report(s) in this server session.</p>";
  if(reports.empty())b<<"<section class=\"panel\"><h3>No reports yet</h3><p>Be the first to add a lost or found item.</p></section>";
  b<<"<div class=\"grid\">";for(std::size_t i=0;i<reports.size();++i)b<<card(reports[i],i);b<<"</div>";page(r,"Campus item board",b.str());
 });
 app.Get("/report",[](const httplib::Request&,httplib::Response& r){
  std::string b="<h1>Report an item</h1><p class=\"muted\">Share enough detail to help its owner identify it.</p><section class=\"panel\"><form class=\"report\" method=\"post\" action=\"/report\"><label>Report type<select name=\"type\"><option value=\"lost\">I lost an item</option><option value=\"found\">I found an item</option></select></label>"+field("Item name","title")+"<label>Description<textarea name=\"description\" required maxlength=\"1500\"></textarea></label>"+field("Category","category")+field("Location","location")+field("Date","date","date")+"<button>Publish report</button></form></section>";
  page(r,"Report an item",b);
 });
 app.Post("/report",[](const httplib::Request& q,httplib::Response& r){
  Item x; x.type=q.get_param_value("type");x.title=trim(q.get_param_value("title"));x.description=trim(q.get_param_value("description"));x.category=trim(q.get_param_value("category"));x.location=trim(q.get_param_value("location"));x.date=q.get_param_value("date");
  if((x.type!="lost"&&x.type!="found")||x.title.empty()||x.description.empty()||x.category.empty()||x.location.empty()||x.date.empty()){r.status=400;page(r,"Invalid report","<h1>Missing or invalid information</h1><a href=\"/report\">Go back</a>");return;}
  {std::lock_guard<std::mutex> lock(data_mutex);reports.push_back(x);}r.set_redirect("/",303);
 });
 app.Post("/match",[](const httplib::Request& q,httplib::Response& r){
  std::size_t n=0;try{n=std::stoul(q.get_param_value("index"));}catch(...){r.status=400;r.set_content("Invalid report index","text/plain");return;}
  std::lock_guard<std::mutex> lock(data_mutex);if(n>=reports.size()||reports[n].type!="found"){r.status=404;page(r,"Not found","<h1>Found report not found</h1><a href=\"/\">Back</a>");return;}
  std::ostringstream b;b<<"<h1>Possible matches</h1><p class=\"muted\">Suggestions only—not proof of ownership.</p><div class=\"grid\">";bool any=false;
  for(const auto& x:reports)if(x.type=="lost"){any=true;auto m=smart_lost_found::score(reports[n],x);b<<"<article class=\"item\"><span class=\"pill\">"<<m.percentage<<"% match</span><h3>"<<esc(x.title)<<"</h3><p>"<<esc(x.description)<<"</p><dl><dt>Category</dt><dd>"<<esc(x.category)<<"</dd><dt>Location</dt><dd>"<<esc(x.location)<<"</dd><dt>Date</dt><dd>"<<esc(x.date)<<"</dd></dl><h4>Why it matched</h4><ul>";for(const auto& why:m.reasons)b<<"<li>"<<esc(why)<<"</li>";b<<"</ul></article>";}
  if(!any)b<<"<section class=\"panel\"><h3>No lost reports yet</h3></section>";b<<"</div><a class=\"btn\" href=\"/\">Back to reports</a>";page(r,"Possible matches",b.str());
 });
 const char* e=std::getenv("PORT");int port=e?std::max(1,std::min(65535,std::atoi(e))):8080;
 std::cout<<"Smart Lost & Found C++ server: http://localhost:"<<port<<"\nReports are temporary and disappear when the server stops.\n";
 if(!app.listen("0.0.0.0",port)){std::cerr<<"Could not start server.\n";return 1;}return 0;
}

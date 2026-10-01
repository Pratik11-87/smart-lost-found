#include "matcher.hpp"
#include "httplib.h"
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace {
sqlite3* db = nullptr;
std::mutex db_mutex;
const std::string upload_dir = "uploads";
const int SESSION_DAYS = 7;

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
std::string randomHex(std::size_t bytes=32) {
 std::vector<unsigned char> v(bytes);
 if(RAND_bytes(v.data(),static_cast<int>(v.size()))!=1) throw std::runtime_error("Secure random generator failed");
 static const char* h="0123456789abcdef"; std::string out; out.reserve(bytes*2);
 for(unsigned char c:v){out+=h[c>>4];out+=h[c&15];} return out;
}
std::string sha256(const std::string& value) {
 std::array<unsigned char,EVP_MAX_MD_SIZE> out{}; unsigned int n=0;
 EVP_Digest(value.data(),value.size(),out.data(),&n,EVP_sha256(),nullptr);
 static const char* h="0123456789abcdef"; std::string s;
 for(unsigned i=0;i<n;++i){s+=h[out[i]>>4];s+=h[out[i]&15];} return s;
}
std::string passwordHash(const std::string& password,const std::string& salt) {
 std::array<unsigned char,32> out{};
 if(PKCS5_PBKDF2_HMAC(password.c_str(),static_cast<int>(password.size()),
   reinterpret_cast<const unsigned char*>(salt.data()),static_cast<int>(salt.size()),
   250000,EVP_sha256(),static_cast<int>(out.size()),out.data())!=1)
   throw std::runtime_error("Password hashing failed");
 static const char* h="0123456789abcdef";std::string s;
 for(unsigned char c:out){s+=h[c>>4];s+=h[c&15];}return s;
}
bool validEmail(const std::string& s) {
 auto at=s.find('@'); return at>0 && at!=std::string::npos && at+1<s.size() &&
 s.find('.',at)!=std::string::npos && s.find(' ') == std::string::npos && s.size()<=254;
}
bool execSql(const char* sql) {
 char* err=nullptr;int rc=sqlite3_exec(db,sql,nullptr,nullptr,&err);
 if(rc!=SQLITE_OK){std::cerr<<"Database error: "<<(err?err:"unknown")<<"\n";sqlite3_free(err);return false;}return true;
}
struct Stmt {
 sqlite3_stmt* p=nullptr;
 Stmt(const char* sql){if(sqlite3_prepare_v2(db,sql,-1,&p,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));}
 ~Stmt(){if(p)sqlite3_finalize(p);}
 Stmt(const Stmt&)=delete;Stmt& operator=(const Stmt&)=delete;
 void text(int i,const std::string& s){sqlite3_bind_text(p,i,s.c_str(),static_cast<int>(s.size()),SQLITE_TRANSIENT);}
 void num(int i,long long n){sqlite3_bind_int64(p,i,n);}
 int step(){return sqlite3_step(p);}
 std::string str(int i)const{const auto* v=sqlite3_column_text(p,i);return v?reinterpret_cast<const char*>(v):"";}
 long long integer(int i)const{return sqlite3_column_int64(p,i);}
};
bool initDb() {
 if(sqlite3_open("smart_lost_found.db",&db)!=SQLITE_OK)return false;
 sqlite3_busy_timeout(db,5000);
 return execSql(
 "PRAGMA foreign_keys=ON;"
 "PRAGMA journal_mode=WAL;"
 "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,email TEXT NOT NULL UNIQUE COLLATE NOCASE,salt TEXT NOT NULL,password_hash TEXT NOT NULL,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);"
 "CREATE TABLE IF NOT EXISTS sessions(token_hash TEXT PRIMARY KEY,user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,expires_at INTEGER NOT NULL);"
 "CREATE TABLE IF NOT EXISTS items(id INTEGER PRIMARY KEY AUTOINCREMENT,type TEXT NOT NULL CHECK(type IN ('lost','found')),title TEXT NOT NULL,description TEXT NOT NULL,category TEXT NOT NULL,location TEXT NOT NULL,item_date TEXT NOT NULL,photo_path TEXT NOT NULL DEFAULT '',user_id INTEGER NOT NULL REFERENCES users(id),created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);"
 "CREATE TABLE IF NOT EXISTS claims(id INTEGER PRIMARY KEY AUTOINCREMENT,item_id INTEGER NOT NULL REFERENCES items(id) ON DELETE CASCADE,claimant_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,message TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('pending','approved','rejected')),created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,UNIQUE(item_id,claimant_id));"
 "CREATE INDEX IF NOT EXISTS idx_items_created ON items(id DESC);"
 "CREATE INDEX IF NOT EXISTS idx_claims_item ON claims(item_id);"
 );
}
long long nowEpoch(){return static_cast<long long>(std::time(nullptr));}
std::string cookieValue(const httplib::Request& q,const std::string& key) {
 auto it=q.headers.find("Cookie");if(it==q.headers.end())return {};
 std::istringstream ss(it->second);std::string part;
 while(std::getline(ss,part,';')){auto p=part.find('=');if(p!=std::string::npos&&trim(part.substr(0,p))==key)return trim(part.substr(p+1));}
 return {};
}
struct User {long long id=0;std::string name,email,csrf;};
User currentUser(const httplib::Request& q) {
 User u;auto token=cookieValue(q,"slf_session");if(token.empty())return u;
 std::lock_guard<std::mutex> lock(db_mutex);
 Stmt s("SELECT u.id,u.name,u.email FROM sessions s JOIN users u ON u.id=s.user_id WHERE s.token_hash=? AND s.expires_at>?");
 s.text(1,sha256(token));s.num(2,nowEpoch());
 if(s.step()==SQLITE_ROW){u.id=s.integer(0);u.name=s.str(1);u.email=s.str(2);}
 return u;
}
bool loginUser(const std::string& email,const std::string& password,User& u,std::string& token) {
 std::lock_guard<std::mutex> lock(db_mutex);
 Stmt s("SELECT id,name,email,salt,password_hash FROM users WHERE email=? COLLATE NOCASE");s.text(1,email);
 if(s.step()!=SQLITE_ROW)return false;
 auto expected=s.str(4),actual=passwordHash(password,s.str(3));
 if(expected.size()!=actual.size()||CRYPTO_memcmp(expected.data(),actual.data(),expected.size())!=0)return false;
 u.id=s.integer(0);u.name=s.str(1);u.email=s.str(2);token=randomHex();
 Stmt ins("INSERT INTO sessions(token_hash,user_id,expires_at) VALUES(?,?,?)");
 ins.text(1,sha256(token));ins.num(2,u.id);ins.num(3,nowEpoch()+SESSION_DAYS*86400LL);
 return ins.step()==SQLITE_DONE;
}
void setSessionCookie(httplib::Response& r,const std::string& token) {
 const char* secure=std::getenv("COOKIE_SECURE");
 std::string v="slf_session="+token+"; Path=/; HttpOnly; SameSite=Lax; Max-Age="+std::to_string(SESSION_DAYS*86400);
 if(secure&&std::string(secure)=="1")v+="; Secure";
 r.set_header("Set-Cookie",v);
}
void clearSessionCookie(httplib::Response& r) {
 r.set_header("Set-Cookie","slf_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
}
std::string start(const std::string& title,const User& u={}) {
 std::string h="<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>"+esc(title)+" · Smart Lost &amp; Found</title><link rel=\"stylesheet\" href=\"/style.css\"></head><body><header><a class=\"brand\" href=\"/\">SMART <span>LOST &amp; FOUND</span></a><nav><a href=\"/\">Browse</a>";
 if(u.id){h+="<a href=\"/report\">Report item</a><a href=\"/claims\">Claims</a><span class=\"welcome\">"+esc(u.name)+"</span><form class=\"navform\" method=\"post\" action=\"/logout\"><button class=\"linkbtn\">Sign out</button></form>";}
 else h+="<a href=\"/login\">Sign in</a><a href=\"/signup\">Create account</a>";
 return h+"</nav></header><main>";
}
std::string end(){return "</main><footer>Smart Lost &amp; Found · Campus item recovery</footer></body></html>";}
void page(httplib::Response& r,const std::string& title,const std::string& body,const User& u={}){r.set_content(start(title,u)+body+end(),"text/html; charset=utf-8");}
void redirect(httplib::Response& r,const std::string& url){r.set_redirect(url,303);}
std::string field(const std::string& label,const std::string& name,const std::string& type="text",bool required=true) {
 return "<label>"+label+"<input name=\""+name+"\" type=\""+type+"\" "+(required?"required":"")+" autocomplete=\""+(type=="password"?"current-password":"off")+"\"></label>";
}
std::string css(){return R"CSS(:root{font-family:system-ui,sans-serif;color:#172033;background:#f4f7fb}*{box-sizing:border-box}body{margin:0}header{display:flex;justify-content:space-between;align-items:center;gap:20px;padding:20px max(5vw,20px);background:#101b36;color:white}.brand{font-weight:900;color:white;text-decoration:none;white-space:nowrap}.brand span{color:#71d8cb}nav{display:flex;gap:16px;align-items:center;flex-wrap:wrap}nav a,.linkbtn{color:white;text-decoration:none}.navform{margin:0}.linkbtn{background:none;padding:0;font:inherit}.welcome{color:#a9dcd6;font-size:.9rem}main{max-width:1100px;margin:34px auto;padding:0 20px}.hero{padding:34px;border-radius:20px;background:linear-gradient(120deg,#142447,#246a80);color:white}.hero h1{font-size:clamp(2rem,5vw,3.6rem);margin:0}.hero p{max-width:650px;line-height:1.6;color:#dceaf5}.actions{display:flex;gap:12px;margin-top:22px;flex-wrap:wrap}.btn,button{display:inline-block;background:#087e8b;color:white;padding:11px 16px;border:0;border-radius:9px;text-decoration:none;font-weight:700;cursor:pointer}.btn.secondary{background:white;color:#132341}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px;margin:20px 0}.item,.panel{background:white;border:1px solid #e0e6ef;border-radius:15px;padding:20px;box-shadow:0 5px 20px #1a2b4408}.item p{color:#526078;white-space:pre-wrap;overflow-wrap:anywhere}.pill{display:inline-block;text-transform:uppercase;font-size:.72rem;font-weight:800;background:#e1f7f4;color:#087e8b;border-radius:20px;padding:5px 9px}.pill.pending{background:#fff0c2;color:#785700}.pill.rejected{background:#ffe2e2;color:#8b2020}.pill.approved{background:#d8f7df;color:#17652b}dl{display:grid;grid-template-columns:90px 1fr;gap:7px;font-size:.9rem}dt{color:#69768a}dd{margin:0;overflow-wrap:anywhere}form.report,.stack{display:grid;gap:15px;max-width:720px}label{display:grid;gap:7px;font-weight:650}input,textarea,select{font:inherit;width:100%;padding:11px 12px;border:1px solid #cbd5e1;border-radius:9px;background:white}textarea{min-height:100px}img.photo{display:block;width:100%;max-height:260px;object-fit:contain;background:#edf1f6;border-radius:10px;margin:12px 0}.muted{color:#68768c}.notice{padding:12px 15px;background:#e1f7f4;border-radius:10px;margin:15px 0}.error{padding:12px 15px;background:#ffe2e2;border-radius:10px;margin:15px 0}footer{text-align:center;color:#7b879a;padding:28px}@media(max-width:680px){header{align-items:flex-start;flex-direction:column}nav{gap:12px}.hero{padding:24px}})CSS";}
bool getItem(long long id,smart_lost_found::Item& x,std::string& photo,long long& owner) {
 Stmt s("SELECT type,title,description,category,location,item_date,photo_path,user_id FROM items WHERE id=?");s.num(1,id);
 if(s.step()!=SQLITE_ROW)return false;
 x.type=s.str(0);x.title=s.str(1);x.description=s.str(2);x.category=s.str(3);x.location=s.str(4);x.date=s.str(5);photo=s.str(6);owner=s.integer(7);return true;
}
std::string photoHtml(const std::string& path) {
 if(path.empty())return {};
 return "<img class=\"photo\" src=\"/"+esc(path)+"\" alt=\"Item photo\" loading=\"lazy\">";
}
std::string itemCard(long long id,const smart_lost_found::Item& x,const std::string& photo,long long owner,const User& u) {
 std::ostringstream o;o<<"<article class=\"item\"><span class=\"pill\">"<<esc(x.type)<<"</span><h3>"<<esc(x.title)<<"</h3>"<<photoHtml(photo)<<"<p>"<<esc(x.description)<<"</p><dl><dt>Category</dt><dd>"<<esc(x.category)<<"</dd><dt>Location</dt><dd>"<<esc(x.location)<<"</dd><dt>Date</dt><dd>"<<esc(x.date)<<"</dd></dl>";
 if(u.id&&x.type=="found"&&u.id!=owner)o<<"<p><a class=\"btn\" href=\"/claim?id="<<id<<"\">Claim this item</a></p>";
 if(x.type=="found")o<<"<form method=\"post\" action=\"/match\"><input type=\"hidden\" name=\"id\" value=\""<<id<<"\"><button>Find possible matches</button></form>";
 o<<"</article>";return o.str();
}
bool imageType(const httplib::MultipartFormData& f,std::string& ext) {
 if(f.content_type=="image/jpeg"){ext=".jpg";return f.content.size()>=3&&(unsigned char)f.content[0]==0xff&&(unsigned char)f.content[1]==0xd8&&(unsigned char)f.content[2]==0xff;}
 if(f.content_type=="image/png"){ext=".png";return f.content.size()>=8&&(unsigned char)f.content[0]==0x89&&f.content.substr(1,3)=="PNG";}
 if(f.content_type=="image/webp"){ext=".webp";return f.content.size()>=12&&f.content.substr(0,4)=="RIFF"&&f.content.substr(8,4)=="WEBP";}
 return false;
}
std::string savePhoto(const httplib::Request& q,std::string& error) {
 if(!q.has_file("photo"))return {};
 const auto f=q.get_file_value("photo");
 if(f.content.empty())return {};
 if(f.content.size()>5*1024*1024){error="Image must be 5 MB or smaller.";return {};}
 std::string ext;if(!imageType(f,ext)){error="Use a valid JPEG, PNG, or WebP image.";return {};}
 fs::create_directories(upload_dir);
 const std::string name=randomHex(20)+ext;
 std::ofstream out(fs::path(upload_dir)/name,std::ios::binary);
 if(!out){error="Could not save the image.";return {};}
 out.write(f.content.data(),static_cast<std::streamsize>(f.content.size()));
 if(!out){error="Could not save the image.";return {};}
 return upload_dir+"/"+name;
}
bool loggedIn(const User& u,httplib::Response& r) {
 if(u.id)return true;page(r,"Sign in required","<section class=\"panel\"><h1>Please sign in</h1><p>You need an account to do that.</p><a class=\"btn\" href=\"/login\">Sign in</a></section>");return false;
}
}
int main(){
 if(!initDb()){std::cerr<<"Could not initialize the database.\n";return 1;}
 httplib::Server app;app.set_payload_max_length(6*1024*1024);
 app.set_default_headers({{"X-Content-Type-Options","nosniff"},{"X-Frame-Options","DENY"},{"Referrer-Policy","strict-origin-when-cross-origin"},{"Content-Security-Policy","default-src 'self'; img-src 'self'; style-src 'self' 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'"}});
 app.Get("/style.css",[](const httplib::Request&,httplib::Response& r){r.set_content(css(),"text/css; charset=utf-8");});
 app.Get(R"(/uploads/([a-f0-9]+\.(jpg|png|webp)))",[](const httplib::Request& q,httplib::Response& r){std::string p=upload_dir+"/"+q.matches[1].str();std::ifstream f(p,std::ios::binary);if(!f){r.status=404;return;}std::ostringstream b;b<<f.rdbuf();auto ext=fs::path(p).extension().string();r.set_content(b.str(),ext==".jpg"?"image/jpeg":ext==".png"?"image/png":"image/webp");r.set_header("Cache-Control","public, max-age=86400");});
 app.Get("/",[](const httplib::Request& q,httplib::Response& r){
  User u=currentUser(q);std::lock_guard<std::mutex> lock(db_mutex);Stmt s("SELECT id,type,title,description,category,location,item_date,photo_path,user_id FROM items ORDER BY id DESC LIMIT 100");std::ostringstream b;b<<"<section class=\"hero\"><h1>Lost something?<br>Found something?</h1><p>A campus noticeboard for lost and found items, with photo uploads and possible matches ranked by a C++ scoring system.</p><div class=\"actions\"><a class=\"btn\" href=\"/report\">Report an item</a><a class=\"btn secondary\" href=\"#reports\">Browse reports</a></div></section><h2 id=\"reports\">Recent reports</h2><div class=\"grid\">";bool any=false;
  while(s.step()==SQLITE_ROW){any=true;smart_lost_found::Item x{s.str(1),s.str(2),s.str(3),s.str(4),s.str(5),s.str(6)};b<<itemCard(s.integer(0),x,s.str(7),s.integer(8),u);}
  if(!any)b<<"<section class=\"panel\"><h3>No reports yet</h3><p>Sign in and add the first lost or found item.</p></section>";b<<"</div>";page(r,"Campus item board",b.str(),u);
 });
 app.Get("/signup",[](const httplib::Request&,httplib::Response& r){std::string b="<section class=\"panel\"><h1>Create account</h1><form class=\"stack\" method=\"post\" action=\"/signup\">"+field("Full name","name")+field("Email","email","email")+field("Password (at least 10 characters)","password","password")+field("Confirm password","confirm","password")+"<button>Create account</button></form><p>Already registered? <a href=\"/login\">Sign in</a></p></section>";page(r,"Create account",b);});
 app.Post("/signup",[](const httplib::Request& q,httplib::Response& r){
  std::string name=trim(q.get_param_value("name")),email=trim(q.get_param_value("email")),pw=q.get_param_value("password"),confirm=q.get_param_value("confirm");
  if(name.empty()||name.size()>100||!validEmail(email)||pw.size()<10||pw.size()>200||pw!=confirm){page(r,"Check your details","<section class=\"panel\"><h1>Check your details</h1><p class=\"error\">Enter a valid email and name, use a password of at least 10 characters, and make sure both password fields match.</p><a href=\"/signup\">Try again</a></section>");return;}
  std::lock_guard<std::mutex> lock(db_mutex);try{std::string salt=randomHex(16),hash=passwordHash(pw,salt);Stmt s("INSERT INTO users(name,email,salt,password_hash) VALUES(?,?,?,?)");s.text(1,name);s.text(2,email);s.text(3,salt);s.text(4,hash);if(s.step()!=SQLITE_DONE)throw std::runtime_error("Email already registered");redirect(r,"/login?created=1");}catch(...){page(r,"Account not created","<section class=\"panel\"><h1>Account not created</h1><p class=\"error\">That email may already be registered. Try signing in or use another email.</p><a href=\"/signup\">Back to signup</a></section>");}
 });
 app.Get("/login",[](const httplib::Request& q,httplib::Response& r){std::string b="<section class=\"panel\"><h1>Sign in</h1>";if(q.has_param("created"))b+="<p class=\"notice\">Account created. You can sign in now.</p>";b+="<form class=\"stack\" method=\"post\" action=\"/login\">"+field("Email","email","email")+field("Password","password","password")+"<button>Sign in</button></form><p>New here? <a href=\"/signup\">Create account</a></p></section>";page(r,"Sign in",b);});
 app.Post("/login",[](const httplib::Request& q,httplib::Response& r){User u;std::string token;if(!loginUser(trim(q.get_param_value("email")),q.get_param_value("password"),u,token)){page(r,"Sign in failed","<section class=\"panel\"><h1>Sign in failed</h1><p class=\"error\">Email or password is incorrect.</p><a href=\"/login\">Try again</a></section>");return;}setSessionCookie(r,token);redirect(r,"/");});
 app.Post("/logout",[](const httplib::Request& q,httplib::Response& r){auto token=cookieValue(q,"slf_session");if(!token.empty()){std::lock_guard<std::mutex> lock(db_mutex);Stmt s("DELETE FROM sessions WHERE token_hash=?");s.text(1,sha256(token));s.step();}clearSessionCookie(r);redirect(r,"/");});
 app.Get("/report",[](const httplib::Request& q,httplib::Response& r){User u=currentUser(q);if(!loggedIn(u,r))return;std::string b="<h1>Report an item</h1><section class=\"panel\"><form class=\"report\" enctype=\"multipart/form-data\" method=\"post\" action=\"/report\"><label>Report type<select name=\"type\"><option value=\"lost\">I lost an item</option><option value=\"found\">I found an item</option></select></label>"+field("Item name","title")+ "<label>Description<textarea name=\"description\" required maxlength=\"1500\"></textarea></label>"+field("Category","category")+field("Location","location")+field("Date","date","date")+"<label>Photo (JPEG, PNG, WebP; max 5 MB)<input type=\"file\" name=\"photo\" accept=\"image/jpeg,image/png,image/webp\"></label><button>Publish report</button></form></section>";page(r,"Report an item",b,u);});
 app.Post("/report",[](const httplib::Request& q,httplib::Response& r){User u=currentUser(q);if(!loggedIn(u,r))return;smart_lost_found::Item x;x.type=q.get_param_value("type");x.title=trim(q.get_param_value("title"));x.description=trim(q.get_param_value("description"));x.category=trim(q.get_param_value("category"));x.location=trim(q.get_param_value("location"));x.date=q.get_param_value("date");if((x.type!="lost"&&x.type!="found")||x.title.empty()||x.title.size()>160||x.description.empty()||x.description.size()>1500||x.category.empty()||x.category.size()>80||x.location.empty()||x.location.size()>160||x.date.size()!=10){page(r,"Invalid report","<section class=\"panel\"><h1>Check your report</h1><p class=\"error\">Complete every required field and stay within the displayed character limits.</p><a href=\"/report\">Back to report form</a></section>",u);return;}std::string err,photo=savePhoto(q,err);if(!err.empty()){page(r,"Image upload failed","<section class=\"panel\"><h1>Image upload failed</h1><p class=\"error\">"+esc(err)+"</p><a href=\"/report\">Try again</a></section>",u);return;}std::lock_guard<std::mutex> lock(db_mutex);Stmt s("INSERT INTO items(type,title,description,category,location,item_date,photo_path,user_id) VALUES(?,?,?,?,?,?,?,?)");s.text(1,x.type);s.text(2,x.title);s.text(3,x.description);s.text(4,x.category);s.text(5,x.location);s.text(6,x.date);s.text(7,photo);s.num(8,u.id);if(s.step()!=SQLITE_DONE){page(r,"Could not save report","<h1>Could not save report</h1>",u);return;}redirect(r,"/");});
 app.Post("/match",[](const httplib::Request& q,httplib::Response& r){User u=currentUser(q);long long id=0;try{id=std::stoll(q.get_param_value("id"));}catch(...){r.status=400;return;}std::lock_guard<std::mutex> lock(db_mutex);smart_lost_found::Item base;std::string photo;long long owner=0;if(!getItem(id,base,photo,owner)||base.type!="found"){r.status=404;page(r,"Report not found","<h1>Report not found</h1>",u);return;}Stmt s("SELECT id,type,title,description,category,location,item_date,photo_path,user_id FROM items WHERE type='lost' ORDER BY id DESC");std::ostringstream b;b<<"<h1>Possible matches</h1><p class=\"muted\">Suggestions only—not proof of ownership.</p><div class=\"grid\">";bool any=false;while(s.step()==SQLITE_ROW){any=true;smart_lost_found::Item other{s.str(1),s.str(2),s.str(3),s.str(4),s.str(5),s.str(6)};auto m=smart_lost_found::score(base,other);b<<"<article class=\"item\"><span class=\"pill\">"<<m.percentage<<"% match</span><h3>"<<esc(other.title)<<"</h3>"<<photoHtml(s.str(7))<<"<p>"<<esc(other.description)<<"</p><dl><dt>Category</dt><dd>"<<esc(other.category)<<"</dd><dt>Location</dt><dd>"<<esc(other.location)<<"</dd><dt>Date</dt><dd>"<<esc(other.date)<<"</dd></dl><h4>Why it matched</h4><ul>";for(const auto& why:m.reasons)b<<"<li>"<<esc(why)<<"</li>";b<<"</ul></article>";}if(!any)b<<"<section class=\"panel\"><h3>No lost reports yet</h3></section>";b<<"</div><a class=\"btn\" href=\"/\">Back to reports</a>";page(r,"Possible matches",b.str(),u);});
 app.Get("/claim",[](const httplib::Request& q,httplib::Response& r){User u=currentUser(q);if(!loggedIn(u,r))return;long long id=0;try{id=std::stoll(q.get_param_value("id"));}catch(...){r.status=400;return;}std::lock_guard<std::mutex> lock(db_mutex);smart_lost_found::Item x;std::string photo;long long owner=0;if(!getItem(id,x,photo,owner)||x.type!="found"||owner==u.id){r.status=404;page(r,"Report unavailable","<h1>This item cannot be claimed from this account.</h1>",u);return;}std::string b="<h1>Claim an item</h1><section class=\"panel\">"+photoHtml(photo)+"<h2>"+esc(x.title)+"</h2><p>"+esc(x.description)+"</p><form class=\"stack\" method=\"post\" action=\"/claim\"><input type=\"hidden\" name=\"id\" value=\""+std::to_string(id)+"\"><label>Explain how you can identify this item<textarea name=\"message\" maxlength=\"1500\" required></textarea></label><p class=\"muted\">Do not include passwords, identity numbers, or other sensitive details.</p><button>Submit claim</button></form></section>";page(r,"Claim an item",b,u);});
 app.Post("/claim",[](const httplib::Request& q,httplib::Response& r){User u=currentUser(q);if(!loggedIn(u,r))return;long long id=0;try{id=std::stoll(q.get_param_value("id"));}catch(...){r.status=400;return;}std::string msg=trim(q.get_param_value("message"));if(msg.empty()||msg.size()>1500){page(r,"Claim not submitted","<h1>Enter a short explanation</h1><a href=\"/\">Back</a>",u);return;}std::lock_guard<std::mutex> lock(db_mutex);smart_lost_found::Item x;std::string photo;long long owner=0;if(!getItem(id,x,photo,owner)||x.type!="found"||owner==u.id){r.status=404;return;}try{Stmt s("INSERT INTO claims(item_id,claimant_id,message) VALUES(?,?,?)");s.num(1,id);s.num(2,u.id);s.text(3,msg);if(s.step()!=SQLITE_DONE)throw std::runtime_error("Duplicate claim");redirect(r,"/claims?sent=1");}catch(...){page(r,"Claim not submitted","<section class=\"panel\"><h1>Claim not submitted</h1><p class=\"error\">You may already have a claim for this item.</p><a href=\"/claims\">View claims</a></section>",u);}});
 app.Get("/claims",[](const httplib::Request& q,httplib::Response& r){User u=currentUser(q);if(!loggedIn(u,r))return;std::lock_guard<std::mutex> lock(db_mutex);std::ostringstream b;b<<"<h1>Claims</h1>";if(q.has_param("sent"))b<<"<p class=\"notice\">Your claim was submitted.</p>";b<<"<h2>Claims on your found-item reports</h2><div class=\"grid\">";Stmt incoming("SELECT c.id,c.message,c.status,c.created_at,i.id,i.title,i.photo_path,u.name FROM claims c JOIN items i ON i.id=c.item_id JOIN users u ON u.id=c.claimant_id WHERE i.user_id=? ORDER BY c.id DESC");incoming.num(1,u.id);bool any=false;while(incoming.step()==SQLITE_ROW){any=true;auto status=incoming.str(2);b<<"<article class=\"item\"><span class=\"pill "<<esc(status)<<"\">"<<esc(status)<<"</span><h3>"<<esc(incoming.str(5))<<"</h3>"<<photoHtml(incoming.str(6))<<"<p><b>Claimant:</b> "<<esc(incoming.str(7))<<"</p><p>"<<esc(incoming.str(1))<<"</p><p class=\"muted\">"<<esc(incoming.str(3))<<"</p>";if(status=="pending")b<<"<form method=\"post\" action=\"/claim/status\"><input type=\"hidden\" name=\"claim_id\" value=\""<<incoming.integer(0)<<"\"><button name=\"status\" value=\"approved\">Approve claim</button> <button name=\"status\" value=\"rejected\">Reject claim</button></form>";b<<"</article>";}if(!any)b<<"<p class=\"muted\">No one has claimed your found-item reports yet.</p>";b<<"</div><h2>Your submitted claims</h2><div class=\"grid\">";Stmt outgoing("SELECT c.status,c.message,c.created_at,i.title,i.photo_path FROM claims c JOIN items i ON i.id=c.item_id WHERE c.claimant_id=? ORDER BY c.id DESC");outgoing.num(1,u.id);any=false;while(outgoing.step()==SQLITE_ROW){any=true;b<<"<article class=\"item\"><span class=\"pill "<<esc(outgoing.str(0))<<"\">"<<esc(outgoing.str(0))<<"</span><h3>"<<esc(outgoing.str(3))<<"</h3>"<<photoHtml(outgoing.str(4))<<"<p>"<<esc(outgoing.str(1))<<"</p><p class=\"muted\">"<<esc(outgoing.str(2))<<"</p></article>";}if(!any)b<<"<p class=\"muted\">You have not submitted any claims.</p>";b<<"</div>";page(r,"Claims",b.str(),u);});
 app.Post("/claim/status",[](const httplib::Request& q,httplib::Response& r){User u=currentUser(q);if(!loggedIn(u,r))return;long long id=0;try{id=std::stoll(q.get_param_value("claim_id"));}catch(...){r.status=400;return;}std::string status=q.get_param_value("status");if(status!="approved"&&status!="rejected"){r.status=400;return;}std::lock_guard<std::mutex> lock(db_mutex);Stmt s("UPDATE claims SET status=? WHERE id=? AND status='pending' AND item_id IN (SELECT id FROM items WHERE user_id=? AND type='found')");s.text(1,status);s.num(2,id);s.num(3,u.id);s.step();redirect(r,"/claims");});
 const char* e=std::getenv("PORT");int port=e?std::max(1,std::min(65535,std::atoi(e))):8080;
 std::cout<<"Smart Lost & Found server: http://localhost:"<<port<<"\n";
 if(!app.listen("0.0.0.0",port)){std::cerr<<"Could not start server.\n";sqlite3_close(db);return 1;}
 sqlite3_close(db);return 0;
}
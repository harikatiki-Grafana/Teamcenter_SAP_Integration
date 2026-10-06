/*
 * Teamcenter -> SAP Product Master reference sample.
 * Prerequisites: Teamcenter ITK + libcurl + C++17.
 *
 * This is intentionally a reference implementation. Teamcenter custom
 * property names and link options vary by installation.
 */
#include <curl/curl.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cctype>

extern "C" {
#include <tc/tc.h>
#include <tccore/item.h>
#include <tccore/aom.h>
}

struct Config {
    std::string base, client, user, password, service, type, unit, lang;
    long timeout;
};

static std::string envv(const char* n, const char* d = "") {
    const char* v = std::getenv(n); return v ? v : d;
}

static Config config() {
    Config c{envv("SAP_BASE_URL"), envv("SAP_CLIENT","100"),
             envv("SAP_USER"), envv("SAP_PASSWORD"),
             envv("SAP_PRODUCT_SERVICE","/sap/opu/odata/sap/API_PRODUCT_SRV"),
             envv("SAP_PRODUCT_TYPE","FERT"), envv("SAP_BASE_UNIT","PC"),
             envv("SAP_LANGUAGE","EN"), std::strtol(envv("SAP_TIMEOUT_SECONDS","60"),nullptr,10)};
    if (c.base.empty() || c.user.empty() || c.password.empty())
        throw std::runtime_error("SAP_BASE_URL, SAP_USER and SAP_PASSWORD are required");
    return c;
}

static std::string esc(const std::string& s) {
    std::ostringstream o;
    for (char ch : s) {
        if (ch == '\\\\') o << "\\\\\\\\";
        else if (ch == '"') o << "\\\\\"";
        else if (ch == '\\n') o << "\\\\n";
        else if (ch == '\\r') o << "\\\\r";
        else if (ch == '\\t') o << "\\\\t";
        else o << ch;
    }
    return o.str();
}

static size_t body(char* p, size_t sz, size_t n, void* u) {
    static_cast<std::string*>(u)->append(p, sz*n); return sz*n;
}

struct Headers { std::string csrf, cookie; };

static size_t headers(char* p, size_t sz, size_t n, void* u) {
    const size_t len=sz*n; std::string line(p,len), lower=line;
    for(char& c:lower) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto* h=static_cast<Headers*>(u);
    if(lower.rfind("x-csrf-token:",0)==0) {
        h->csrf=line.substr(13);
        while(!h->csrf.empty() && (h->csrf.front()==' '||h->csrf.front()=='\\t')) h->csrf.erase(0,1);
        while(!h->csrf.empty() && (h->csrf.back()=='\\r'||h->csrf.back()=='\\n')) h->csrf.pop_back();
    } else if(lower.rfind("set-cookie:",0)==0) {
        h->cookie += line.substr(11);
    }
    return len;
}

static CURL* curl(const Config& c) {
    CURL* h=curl_easy_init(); if(!h) throw std::runtime_error("curl_easy_init failed");
    curl_easy_setopt(h,CURLOPT_TIMEOUT,c.timeout);
    curl_easy_setopt(h,CURLOPT_USERPWD,(c.user+":"+c.password).c_str());
    // Keep TLS peer/host verification enabled in production.
    return h;
}

static std::string csrf(const Config& c, Headers& h) {
    CURL* x=curl(c); std::string response;
    std::string url=c.base+c.service+"/$metadata?sap-client="+c.client;
    curl_slist* hs=nullptr;
    hs=curl_slist_append(hs,"Accept: application/json");
    hs=curl_slist_append(hs,"X-CSRF-Token: Fetch");
    curl_easy_setopt(x,CURLOPT_URL,url.c_str());
    curl_easy_setopt(x,CURLOPT_HTTPHEADER,hs);
    curl_easy_setopt(x,CURLOPT_WRITEFUNCTION,body);
    curl_easy_setopt(x,CURLOPT_WRITEDATA,&response);
    curl_easy_setopt(x,CURLOPT_HEADERFUNCTION,headers);
    curl_easy_setopt(x,CURLOPT_HEADERDATA,&h);
    CURLcode rc=curl_easy_perform(x); long status=0;
    curl_easy_getinfo(x,CURLINFO_RESPONSE_CODE,&status);
    curl_slist_free_all(hs); curl_easy_cleanup(x);
    if(rc!=CURLE_OK) throw std::runtime_error(curl_easy_strerror(rc));
    if(status<200||status>=300||h.csrf.empty())
        throw std::runtime_error("Could not obtain SAP CSRF token; HTTP "+std::to_string(status));
    return h.csrf;
}

static std::string tcString(tag_t t, const char* prop) {
    char* p=nullptr;
    if(AOM_ask_value_string(t,prop,&p)!=ITK_ok) return {};
    std::string s=p?p:""; if(p) MEM_free(p); return s;
}

static std::string payload(const Config& c,const std::string& id,const std::string& desc) {
    std::ostringstream j;
    j<<"{" 
      <<"\"Product\":\""<<esc(id)<<"\","
      <<"\"ProductType\":\""<<esc(c.type)<<"\","
      <<"\"BaseUnit\":\""<<esc(c.unit)<<"\","
      <<"\"to_Description\":{\"results\":[{"
      <<"\"Product\":\""<<esc(id)<<"\","
      <<"\"Language\":\""<<esc(c.lang)<<"\","
      <<"\"ProductDescription\":\""<<esc(desc)<<"\""
      <<"}]}}";
    return j.str();
}

static void post(const Config& c,const std::string& token,const Headers& h,const std::string& data) {
    CURL* x=curl(c); std::string response;
    std::string url=c.base+c.service+"/A_Product?sap-client="+c.client;
    curl_slist* hs=nullptr;
    hs=curl_slist_append(hs,"Accept: application/json");
    hs=curl_slist_append(hs,"Content-Type: application/json");
    hs=curl_slist_append(hs,("X-CSRF-Token: "+token).c_str());
    if(!h.cookie.empty()) hs=curl_slist_append(hs,("Cookie: "+h.cookie).c_str());
    curl_easy_setopt(x,CURLOPT_URL,url.c_str());
    curl_easy_setopt(x,CURLOPT_POST,1L);
    curl_easy_setopt(x,CURLOPT_POSTFIELDS,data.c_str());
    curl_easy_setopt(x,CURLOPT_HTTPHEADER,hs);
    curl_easy_setopt(x,CURLOPT_WRITEFUNCTION,body);
    curl_easy_setopt(x,CURLOPT_WRITEDATA,&response);
    CURLcode rc=curl_easy_perform(x); long status=0;
    curl_easy_getinfo(x,CURLINFO_RESPONSE_CODE,&status);
    curl_slist_free_all(hs); curl_easy_cleanup(x);
    if(rc!=CURLE_OK) throw std::runtime_error(curl_easy_strerror(rc));
    if(status<200||status>=300)
        throw std::runtime_error("SAP POST failed HTTP "+std::to_string(status)+": "+response);
    std::cout<<"SAP accepted product. HTTP "<<status<<"\\n"<<response<<"\\n";
}

static int transfer(const char* itemId) {
    Config c=config(); tag_t item=NULLTAG, rev=NULLTAG;
    if(ITEM_find_item(itemId,&item)!=ITK_ok || item==NULLTAG)
        throw std::runtime_error("Teamcenter Item not found: "+std::string(itemId));
    if(ITEM_ask_latest_rev(item,&rev)!=ITK_ok || rev==NULLTAG)
        throw std::runtime_error("Latest revision not found for: "+std::string(itemId));

    std::string desc=tcString(rev,"object_desc");
    if(desc.empty()) desc=tcString(rev,"object_name");
    Headers h; std::string token=csrf(c,h);
    std::string data=payload(c,itemId,desc);
    std::cout<<"Transferring Teamcenter Item "<<itemId<<" to SAP\\n";
    post(c,token,h,data);
    return ITK_ok;
}

int main(int argc,char** argv) {
    if(argc!=2){ std::cerr<<"Usage: teamcenter_sap_transfer <ITEM_ID>\\n"; return 2; }
    if(TC_init_module(nullptr)!=ITK_ok){ std::cerr<<"TC_init_module failed\\n"; return 1; }
    try { int r=transfer(argv[1]); TC_exit_module(true); return r; }
    catch(const std::exception& e){ std::cerr<<"Transfer failed: "<<e.what()<<"\\n"; TC_exit_module(false); return 1; }
}

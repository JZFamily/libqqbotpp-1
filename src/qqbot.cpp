#include "qqbot.h"
#include "NetworkWrapper.h"
#include <cpplib/cpplib#gcolor>
#include <algorithm>
#include "util.h"
#include "StringLoader.h"
#include <windows.h>
#include "json.hpp"
using namespace std;

class QQClient::_impl
{
public:
    int GetQRCode();
    int OpenQRCode();
    int CloseQRCode();
    int GetQRScanStatus();
    int GetPtWebQQ();
    int GetVfWebQQ();
    int GetPSessionID_UIN();

    string qrsig;
    string qrcode_filename;
    /// https://ptlogin2.web2.qq.com/check_sig?pttype=1&uin=...
    string login_url;
    string ptwebqq;
    string vfwebqq;

    StringLoader mp;
};

#define USERAGENT "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/61.0.3163.79 Safari/537.36"
#define URL_1 "https://ssl.ptlogin2.qq.com/ptqrshow?appid=501004106&e=0&l=M&s=5&d=72&v=4&t=0.1"
#define URL_2R "https://ssl.ptlogin2.qq.com/ptqrlogin?" \
                    "ptqrtoken={1}&webqq_type=10&remember_uin=1&login2qq=1&aid=501004106&" \
                    "u1=https%3A%2F%2Fw.qq.com%2Fproxy.html%3Flogin2qq%3D1%26webqq_type%3D10&" \
                    "ptredirect=0&ptlang=2052&daid=164&from_ui=1&pttype=1&dumy=&fp=loginerroralert&0-0-157510&" \
                    "mibao_css=m_webqq&t=undefined&g=1&js_type=0&js_ver=10184&login_sig=&pt_randsalt=3"
#define REF_2 "https://ui.ptlogin2.qq.com/cgi-bin/login?daid=164&target=self&style=16&mibao_css=m_webqq&appid=501004106&enable_qlogin=0&no_verifyimg=1 &s_url=http%3A%2F%2Fw.qq.com%2Fproxy.html&f_url=loginerroralert &strong_login=1&login_state=10&t=20131024001"
#define REF_3 "http://s.web2.qq.com/proxy.html?v=20130916001&callback=1&id=1"
#define URL_4R "http://s.web2.qq.com/api/getvfwebqq?ptwebqq={1}&clientid=53999199&psessionid=&t=0.1"
#define REF_4 "http://s.web2.qq.com/proxy.html?v=20130916001&callback=1&id=1"

QQClient::QQClient()
{
    _p=new _impl;
    if(_p)
    {
        _p->mp=StringLoader("strings_utf8.txt");
    }
}

QQClient::~QQClient()
{
    if(_p)
    {
        delete _p;
        _p=nullptr;
    }
}

#define ShowError(fmt,...) cprint(Color::red,Color::black);printf(fmt,##__VA_ARGS__);cprint()
#define ShowMsg(fmt,...) cprint(Color::yellow,Color::black);printf(fmt,##__VA_ARGS__);cprint()

void ReplaceTag(std::string& src,const std::string& tag,const std::string& replaceto)
{
    size_t x;
    while((x=src.find(tag))!=string::npos)
    {
        src.replace(x,tag.size(),replaceto);
    }
}

void _DoStrParse(std::string& out, int current_cnt, const std::string& current_str)
{
    char buf[16];
    sprintf(buf,"{%d}",current_cnt);
    string tag(buf);

    ReplaceTag(out,tag,current_str);
}

template<typename... Args>
void _DoStrParse(std::string& out,int current_cnt,const std::string& current_str,Args&&... args)
{
    _DoStrParse(out,current_cnt,current_str);
    current_cnt++;

    _DoStrParse(out,current_cnt,args...);
}

template<typename... Args>
std::string StrParse(const std::string& src,Args&&... args)
{
    int cnt=1;
    std::string s(src);
    _DoStrParse(s,cnt,args...);
    return s;
}

int QQClient::_impl::GetQRCode()
{
    HTTPConnection t;
    t.setUserAgent(USERAGENT);
    t.setURL(URL_1);
    t.setDataOutputFile(qrcode_filename);
    t.setCookieOutputFile("tmp/cookie1.txt");
    t.perform();
    if(t.getResponseCode()!=200)
    {
        ShowError("Failed to download QRCode. HTTP Response Code: %d\n",t.getResponseCode());
        return -1;
    }

    vector<Cookie> vec=t.getCookies();
    for(auto& c:vec)
    {
        if(c.name=="qrsig")
        {
            qrsig=c.value;
            break;
        }
    }

    OpenQRCode();

    ShowMsg("QRCode is downloaded. Please scan it with your smart phone.\n");

    return 0;
}

int QQClient::_impl::OpenQRCode()
{
    int ret=(int)ShellExecute(NULL,"open",qrcode_filename.c_str(),NULL,NULL,SW_SHOWMAXIMIZED);
    if(ret<32)
    {
        ret=GetLastError();
        ShowError("Failed to open qrcode. GetLastError:%d\n",ret);
        return -1;
    }
    return 0;
}

int QQClient::_impl::CloseQRCode()
{
    return 0;
}

/// Hash function for generating ptqrtoken
string hash33(const std::string& s)
{
    int e = 0;
    int sz=s.size();
    for (int i = 0; i<sz; ++i)
    {
        e += (e << 5) + (int)s[i];
    }
    int result=2147483647 & e;
    char buff[16];
    sprintf(buff,"%d",result);
    return string(buff);
}

int QQClient::_impl::GetQRScanStatus()
{
    char buff[1024];
    memset(buff,0,1024);

    HTTPConnection t;
    t.setUserAgent(USERAGENT);
    t.setURL(StrParse(URL_2R,hash33(qrsig)));
    t.setReferer(REF_2);
    t.setCookieInputFile("tmp/cookie1.txt");
    t.setCookieOutputFile("tmp/cookie2.txt");
    t.setDataOutputBuffer(buff,1024);
    t.perform();

    if(t.getResponseCode()!=200)
    {
        ShowError("Failed to get QRScan status. Response Code: %d\n",t.getResponseCode());
        return -2;
    }

    string response=UTF8ToGBK(buff);

    ShowMsg("Buffer content:\n%s\n",response.c_str());

    if(strstr(buff,mp["qrcode_scanning"].c_str()))
    {
        return 0;
    }
    else if(strstr(buff,mp["qrcode_valid"].c_str()))
    {
        return 0;
    }
    else if(strstr(buff,mp["qrcode_invalid"].c_str()))
    {
        return -1;
    }
    else if(strstr(buff,"http")) /// Now the API returns https instead of http.
    {
        CloseQRCode();

        char xbuf[1024];
        memset(xbuf,0,1024);
        char* p=strstr(buff,"http");
        char* q=strstr(p,"'");
        strncpy(xbuf,p,q-p);
        login_url=xbuf;

        return 1;
    }
    else
    {
        return -3;
    }
}

int QQClient::_impl::GetPtWebQQ()
{
    HTTPConnection t;
    t.setVerbos(true);
    t.setUserAgent(USERAGENT);
    t.setURL(login_url);
    t.setReferer(REF_3);
    t.setCookieInputFile("tmp/cookie2.txt");
    t.setCookieOutputFile("tmp/cookie3.txt");
    t.perform();

    if(t.getResponseCode()!=302)
    {
        ShowError("Failed to get ptwebqq. Response code: %d\n",t.getResponseCode());
    }

    vector<Cookie> vec=t.getCookies();
    vector<Cookie>::iterator iter=find_if(vec.begin(),vec.end(),[](const Cookie& c){return c.name=="ptwebqq";});
    if(iter!=vec.end())
    {
        ptwebqq=iter->value;
    }
    else
    {
        ShowError("Failed to find ptwebqq in cookie.\n");
        return -1;
    }

    return 0;
}

int QQClient::_impl::GetVfWebQQ()
{
    char buff[1024];
    memset(buff,0,1024);

    HTTPConnection t;
    t.setUserAgent(USERAGENT);
    t.setURL(StrParse(URL_4R,ptwebqq));
    t.setReferer(REF_4);
    t.setCookieInputFile("tmp/cookie3.txt");
    t.setCookieOutputFile("tmp/cookie4.txt");
    t.setDataOutputBuffer(buff,1024);
    t.perform();

    if(t.getResponseCode()!=200)
    {
        ShowError("Failed to get vfwebqq. Response Code: %d\n",t.getResponseCode());
        return -1;
    }

    printf("Buffer Received: %s\n",buff);

    nlohmann::json j=nlohmann::json::parse(buff);
    try
    {
        vfwebqq=j["result"]["vfwebqq"];
    }
    catch(...)
    {
        return -2;
    }

    return 0;
}

int QQClient::_impl::GetPSessionID_UIN()
{
    HTTPConnection t;
    t.setVerbos(true);
    t.setUserAgent(USERAGENT);

    return 0;
}

int QQClient::login()
{
    ShowMsg("[Starting] QRCode\n");
    /// Notice that we must write '\\' instead of '/' as we are running on Windows.
    _p->qrcode_filename="tmp\\qrcode.png";
    if(_p->GetQRCode()<0)
    {
        ShowError("Failed to get qrcode.\n");
        return -1;
    }
    ShowMsg("[OK] QRCode.\n");

    ShowMsg("[Starting] ScanStatus\n");
    int status=0;
    while((status=_p->GetQRScanStatus())==0)
    {
        ShowMsg("Waiting for next check.\n");
        Sleep(2000);
    }
    if(status<0)
    {
        ShowError("Failed to get scan status. (or qrcode is invalid.)\n");
        return -2;
    }
    ShowMsg("[OK] ScanStatus.\n");

    ShowMsg("[Starting] ptwebqq\n");
    if(_p->GetPtWebQQ()<0)
    {
        ShowError("Failed to get ptwebqq.\n");
        return -3;
    }
    ShowMsg("[OK] ptwebqq\n");

    ShowMsg("[Starting] vfwebqq\n");
    if(_p->GetVfWebQQ()<0)
    {
        ShowError("Failed to get vfwebqq.\n");
        return -4;
    }
    ShowMsg("[OK] vfwebqq\n");

    ShowMsg("[Starting] psessionid,uin\n");
    if(_p->GetPSessionID_UIN()<0)
    {
        ShowError("Failed to get psessionid,uin.\n");
        return -5;
    }
    ShowMsg("[OK] psessionid,uin\n");

    return 0;
}

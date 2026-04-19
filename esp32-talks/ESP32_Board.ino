#include <esp_wifi.h>
#include <lwip/etharp.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>

const char* apSSID = "CHUPA";
#define ADMIN_MAC "56:4E:59:C0:B2:80"

WebServer server(80);
DNSServer dns;

#define MAX_MESSAGES 20
#define FILE_PATH "/messages.txt"
#define SEPARATOR "||"

struct Message {
  String text;
  String mac;
  String time;
};

Message messages[MAX_MESSAGES];
int msgCount = 0;
bool pageNeedsRebuild = true;
String cachedPage = "";

String escapeHtml(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  s.replace("'", "&#39;");
  return s;
}

String getUptime() {
  unsigned long sec = millis() / 1000;
  int h = sec / 3600;
  int m = (sec % 3600) / 60;
  int s = sec % 60;
  char buf[12];
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  return String(buf);
}

String getClientMac() {
  uint32_t targetIP = (uint32_t)server.client().remoteIP();
  ip4_addr_t *ip = NULL;
  struct netif *nif = NULL;
  struct eth_addr *eth = NULL;
  for (size_t i = 0; i < ARP_TABLE_SIZE; i++) {
    if (etharp_get_entry(i, &ip, &nif, &eth)) {
      if (ip->addr == targetIP) {
        char mac[18];
        sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
          eth->addr[0], eth->addr[1], eth->addr[2],
          eth->addr[3], eth->addr[4], eth->addr[5]);
        return String(mac);
      }
    }
  }
  return "unknown";
}

void loadMessages() {
  if (!SPIFFS.exists(FILE_PATH)) return;
  File f = SPIFFS.open(FILE_PATH, "r");
  if (!f) return;
  msgCount = 0;
  while (f.available() && msgCount < MAX_MESSAGES) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    int sep1 = line.indexOf(SEPARATOR);
    int sep2 = line.indexOf(SEPARATOR, sep1 + 2);
    if (sep1 == -1 || sep2 == -1) {
      messages[msgCount++] = { line, "unknown", "" };
    } else {
      messages[msgCount++] = {
        line.substring(0, sep1),
        line.substring(sep1 + 2, sep2),
        line.substring(sep2 + 2)
      };
    }
  }
  f.close();
}

void saveMessages() {
  File f = SPIFFS.open(FILE_PATH, "w");
  if (!f) return;
  for (int i = 0; i < msgCount; i++) {
    f.println(messages[i].text + SEPARATOR + messages[i].mac + SEPARATOR + messages[i].time);
  }
  f.close();
}

String buildMessagesHtml() {
  if (msgCount == 0) {
    return "<p class='empty'>No messages yet. Be the first!</p>";
  }
  String s = "";
  for (int i = msgCount - 1; i >= 0; i--) {
    s += "<div class='msg' data-idx='" + String(i) + "'>";
    s += "<button class='del-btn' onclick='delMsg(" + String(i) + ",this)'>✕</button>";
    s += "<div class='msg-text'>" + escapeHtml(messages[i].text) + "</div>";
    s += "<div class='msg-meta'><span>" + messages[i].time + "</span><span class='mac-info'>" + messages[i].mac + "</span></div>";
    s += "</div>";
  }
  return s;
}

String buildPage() {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta charset='utf-8'>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  s += "<title>Message Board</title>";
  s += "<style>";
  s += "body{font-family:sans-serif;max-width:500px;margin:0 auto;padding:24px;background:#f5f5f5;color:#111}";
  s += "h2{text-align:center;margin-bottom:16px}";
  s += ".ta-wrap{position:relative}";
  s += "textarea{width:100%;box-sizing:border-box;padding:14px 14px 28px 14px;font-size:16px;border:1px solid #ccc;border-radius:12px;resize:none;background:#fff}";
  s += ".ta-cnt{position:absolute;bottom:10px;right:14px;font-size:11px;color:#aaa;pointer-events:none}";
  s += ".ta-cnt.warn{color:#e53935}";
  s += "button{width:100%;padding:16px;font-size:18px;border:none;border-radius:12px;background:#f0a500;color:#fff;margin-top:10px;cursor:pointer}";
  s += "button:disabled{background:#ccc}";
  s += "#clear-btn{display:none;width:fit-content;padding:6px 14px;font-size:13px;background:#fff;color:#aaa;border:1px solid #ddd;border-radius:8px;margin:0 auto 16px;cursor:pointer}";
  s += "body.admin #clear-btn{display:table}";
  s += ".msg{position:relative;background:#fff;border-radius:12px;padding:14px 16px;margin-top:12px;border:1px solid #e0e0e0;transition:background 0.4s,border 0.4s}";
  s += ".msg.pending{background:#f0f0f0;border-color:#ddd}";
  s += ".msg.error{background:#fff0f0;border-color:#ffcccc}";
  s += ".msg-text{font-size:15px;line-height:1.5;word-break:break-word;white-space:pre-wrap;padding-right:20px}";
  s += ".msg-meta{display:flex;justify-content:space-between;font-size:11px;color:#aaa;margin-top:6px}";
  s += ".mac-info{display:none}";
  s += "body.admin .mac-info{display:inline}";
  s += ".del-btn{display:none;position:absolute;top:10px;right:12px;background:none;border:none;color:#ddd;cursor:pointer;font-size:15px;line-height:1;padding:2px;width:auto;margin:0}";
  s += "body.admin .del-btn{display:block}";
  s += ".msg-status{}";
  s += ".empty{text-align:center;color:#aaa;margin-top:32px;font-size:15px}";
  s += "</style></head><body>";
  s += "<h2>Message Board</h2>";
  s += "<button id='clear-btn' onclick='clearAll()'>Clear all</button>";
  s += "<div class='ta-wrap'><textarea id='t' rows='3' placeholder='Write something...' maxlength='200' oninput='updateCnt()'></textarea>";
  s += "<span class='ta-cnt' id='cnt'>200</span></div>";
  s += "<button id='btn' onclick='send()'>Send</button>";
  s += "<div id='list'>" + buildMessagesHtml() + "</div>";
  s += "<script>";
  s += "var isAdmin=false;";
  s += "fetch('/whoami').then(function(r){return r.text()}).then(function(mac){";
  s += "if(mac==='" ADMIN_MAC "'){isAdmin=true;document.body.classList.add('admin');}";
  s += "});";
  s += "function send(){";
  s += "var t=document.getElementById('t');";
  s += "var text=t.value.trim();";
  s += "if(!text)return;";
  s += "var btn=document.getElementById('btn');";
  s += "btn.disabled=true;";
  s += "var now=new Date();";
  s += "var ts=now.getHours().toString().padStart(2,'0')+':'+now.getMinutes().toString().padStart(2,'0');";
  s += "var list=document.getElementById('list');";
  s += "var empty=list.querySelector('.empty');";
  s += "if(empty)empty.remove();";
  s += "var div=document.createElement('div');";
  s += "div.className='msg pending';";
  s += "div.innerHTML='<div class=msg-text>'+escHtml(text)+'</div><div class=msg-meta><span>'+ts+'</span><span class=msg-status>sending...</span></div>';";
  s += "list.insertBefore(div,list.firstChild);";
  s += "t.value='';updateCnt();";
  s += "fetch('/post',{method:'POST',headers:{'Content-Type':'text/plain;charset=UTF-8','X-Time':ts},body:text})";
  s += ".then(function(r){";
  s += "if(r.ok)return r.text();";
  s += "throw new Error();";
  s += "}).then(function(idx){";
  s += "div.classList.remove('pending');";
  s += "var meta=div.querySelector('.msg-meta');";
  s += "var status=meta.querySelector('.msg-status');";
  s += "status.remove();";
  s += "if(isAdmin){";
  s += "var dbtn=document.createElement('button');";
  s += "dbtn.className='del-btn';dbtn.style.display='block';";
  s += "dbtn.textContent='\\u2715';";
  s += "dbtn.onclick=function(){delMsg(idx,dbtn)};";
  s += "div.insertBefore(dbtn,div.firstChild);";
  s += "}";
  s += "}).catch(function(){";
  s += "div.classList.remove('pending');";
  s += "div.classList.add('error');";
  s += "div.querySelector('.msg-status').textContent='error';";
  s += "});";
  s += "btn.disabled=false;";
  s += "}";
  s += "function delMsg(idx,btn){";
  s += "fetch('/delete?i='+idx).then(function(r){";
  s += "if(r.ok)btn.closest('.msg').remove();";
  s += "});";
  s += "}";
  s += "function clearAll(){";
  s += "fetch('/elephant').then(function(r){";
  s += "if(r.ok){document.getElementById('list').innerHTML='<p class=empty>No messages yet. Be the first!</p>';}";
  s += "});";
  s += "}";
  s += "function updateCnt(){";
  s += "var n=200-document.getElementById('t').value.length;";
  s += "var el=document.getElementById('cnt');";
  s += "el.textContent=n;";
  s += "el.className='ta-cnt'+(n<20?' warn':'');";
  s += "}";
  s += "function escHtml(s){";
  s += "return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;');";
  s += "}";
  s += "</script>";
  s += "</body></html>";
  return s;
}

void handleRoot() {
  if (pageNeedsRebuild) {
    cachedPage = buildPage();
    pageNeedsRebuild = false;
  }
  server.send(200, "text/html", cachedPage);
}

void handleWhoami() {
  server.send(200, "text/plain", getClientMac());
}

void handlePost() {
  String msg = server.arg("plain");
  msg.trim();
  if (msg.length() > 0 && msg.length() <= 600) {
    String clientTime = server.header("X-Time");
    if (clientTime.length() == 0) clientTime = getUptime();
    Message m = { msg, getClientMac(), clientTime };
    if (msgCount < MAX_MESSAGES) {
      messages[msgCount++] = m;
    } else {
      for (int i = 0; i < MAX_MESSAGES - 1; i++) messages[i] = messages[i + 1];
      messages[MAX_MESSAGES - 1] = m;
    }
    saveMessages();
    pageNeedsRebuild = true;
    server.send(200, "text/plain", String(msgCount - 1));
    return;
  }
  server.send(400, "text/plain", "bad request");
}

void handleDelete() {
  if (getClientMac() != ADMIN_MAC) {
    server.send(403, "text/plain", "forbidden");
    return;
  }
  if (!server.hasArg("i")) {
    server.send(400, "text/plain", "bad request");
    return;
  }
  int idx = server.arg("i").toInt();
  if (idx < 0 || idx >= msgCount) {
    server.send(400, "text/plain", "bad request");
    return;
  }
  for (int i = idx; i < msgCount - 1; i++) messages[i] = messages[i + 1];
  msgCount--;
  saveMessages();
  pageNeedsRebuild = true;
  server.send(200, "text/plain", "ok");
}

void handleElephant() {
  if (getClientMac() != ADMIN_MAC) {
    server.send(403, "text/plain", "forbidden");
    return;
  }
  msgCount = 0;
  SPIFFS.remove(FILE_PATH);
  pageNeedsRebuild = true;
  server.send(200, "text/plain", "ok");
}

void handleDownload() {
  File f = SPIFFS.open(FILE_PATH, "r");
  if (!f) {
    server.send(404, "text/plain", "not found");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=messages.txt");
  server.streamFile(f, "text/plain");
  f.close();
}

void handleNotFound() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount error");
  } else {
    Serial.println("SPIFFS mounted");
    loadMessages();
    Serial.println("Messages loaded: " + String(msgCount));
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID);
  Serial.println("IP: " + WiFi.softAPIP().toString());

  dns.start(53, "*", WiFi.softAPIP());

  const char* headers[] = {"X-Time"};
  server.collectHeaders(headers, 1);

  server.on("/", handleRoot);
  server.on("/whoami", handleWhoami);
  server.on("/post", HTTP_POST, handlePost);
  server.on("/delete", handleDelete);
  server.on("/elephant", handleElephant);
  server.on("/download", handleDownload);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  dns.processNextRequest();
  server.handleClient();
}

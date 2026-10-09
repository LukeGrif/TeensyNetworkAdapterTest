// Teensy 4.1 Ethernet adapter test
//
// Brings up the Ethernet port on a fixed (static) IP, so it works with the
// cable plugged straight into a PC, no router needed. Prints the address on Serial, answers ICMP ping, and serves a small dashboard web
// page at http://<ip>/ that measures round-trip latency from your browser.
//
// Library: QNEthernet (Arduino Library Manager: "QNEthernet" by Shawn Silverman)

#include <QNEthernet.h>

using namespace qindesign::network;

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

// Teensy's fixed address. Set your PC's Ethernet port to a different address
// on the same subnet, e.g. 192.168.2.10 with mask 255.255.255.0.
const IPAddress kStaticIP{192, 168, 2, 70};
const IPAddress kSubnetMask{255, 255, 255, 0};
// With a direct PC cable the gateway/DNS aren't used; they just need to be on
// the same subnet.
const IPAddress kGateway{192, 168, 2, 1};
const IPAddress kDNS{192, 168, 2, 1};
const IPAddress kSuggestedPcIP{192, 168, 2, 10};

constexpr char kHostname[] = "teensy-eth";  // also http://teensy-eth.local/
constexpr uint16_t kHttpPort = 80;
constexpr uint32_t kClientTimeoutMs = 2000;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

EthernetServer server{kHttpPort};

uint32_t linkUpCount = 0;
uint32_t linkDownCount = 0;
uint32_t httpRequests = 0;
uint32_t lastLinkChangeMs = 0;

// ---------------------------------------------------------------------------
// Web page (served at "/"). It polls /api/status and times each request, so
// you get a live "ping" graph over HTTP alongside the ICMP ping you run from
// a terminal.
// ---------------------------------------------------------------------------

const char kIndexHtml[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Teensy Ethernet Test</title>
<style>
:root{--bg:#f6f7f9;--card:#fff;--fg:#1d2330;--mute:#6b7280;--ok:#16a34a;--bad:#dc2626;--acc:#2563eb;--line:#e5e7eb}
@media (prefers-color-scheme:dark){:root{--bg:#0f1218;--card:#181c24;--fg:#e6e8ec;--mute:#9aa1ad;--line:#2a303b;--acc:#60a5fa}}
*{box-sizing:border-box}body{margin:0;font:15px/1.4 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:860px;margin:0 auto;padding:20px 16px}
h1{font-size:22px;margin:0 0 4px}.sub{color:var(--mute);margin-bottom:18px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px}
.k{color:var(--mute);font-size:12px;text-transform:uppercase;letter-spacing:.04em}.v{font-size:20px;font-weight:600;margin-top:4px;word-break:break-all}
.ok{color:var(--ok)}.bad{color:var(--bad)}
canvas{width:100%;height:160px;display:block}
button{font:inherit;padding:8px 14px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--fg);cursor:pointer}
code{background:var(--line);padding:2px 6px;border-radius:5px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:14px 0}
</style></head><body><main>
<h1>Teensy Ethernet Test</h1>
<div class="sub" id="conn">Connecting&hellip;</div>
<div class="grid">
 <div class="card"><div class="k">IP address</div><div class="v" id="ip">-</div></div>
 <div class="card"><div class="k">MAC</div><div class="v" id="mac">-</div></div>
 <div class="card"><div class="k">Link</div><div class="v" id="link">-</div></div>
 <div class="card"><div class="k">Uptime</div><div class="v" id="up">-</div></div>
 <div class="card"><div class="k">HTTP ping (last)</div><div class="v" id="rtt">-</div></div>
 <div class="card"><div class="k">min / avg / max</div><div class="v" id="stats">-</div></div>
 <div class="card"><div class="k">Sent / lost</div><div class="v" id="loss">-</div></div>
 <div class="card"><div class="k">Requests served</div><div class="v" id="req">-</div></div>
</div>
<div class="row"><button id="toggle">Pause</button><button id="reset">Reset stats</button></div>
<div class="card"><div class="k">Round-trip time (ms), last 60 requests</div><canvas id="chart"></canvas></div>
<div class="card" style="margin-top:12px">
 <div class="k">ICMP ping from a terminal</div>
 <p>Windows: <code>ping -t <span class="ipv">IP</span></code><br>
 macOS / Linux: <code>ping <span class="ipv">IP</span></code></p>
 <div class="k">Mask / gateway / DNS</div><div id="net">-</div>
</div>
</main>
<script>
const $=id=>document.getElementById(id);
let running=true,sent=0,lost=0,hist=[];
function fmtUp(s){const d=Math.floor(s/86400),h=Math.floor(s/3600)%24,m=Math.floor(s/60)%60;return (d?d+'d ':'')+h+'h '+m+'m '+(s%60)+'s'}
function draw(){const c=$('chart'),r=c.getBoundingClientRect(),dpr=devicePixelRatio||1;
 c.width=r.width*dpr;c.height=r.height*dpr;const g=c.getContext('2d');g.scale(dpr,dpr);
 const cs=getComputedStyle(document.documentElement),W=r.width,H=r.height;
 const vals=hist.filter(v=>v!=null),max=Math.max(5,...vals)*1.2;
 g.strokeStyle=cs.getPropertyValue('--line');g.fillStyle=cs.getPropertyValue('--mute');g.font='11px system-ui';
 for(let i=0;i<=4;i++){const y=H-H*i/4;g.beginPath();g.moveTo(0,y);g.lineTo(W,y);g.stroke();g.fillText((max*i/4).toFixed(1),2,y-2)}
 const step=W/59;g.strokeStyle=cs.getPropertyValue('--acc');g.lineWidth=2;g.beginPath();let pen=false;
 hist.forEach((v,i)=>{const x=i*step;if(v==null){pen=false;g.fillStyle=cs.getPropertyValue('--bad');g.fillRect(x-1,0,2,H);return}
  const y=H-v/max*H;pen?g.lineTo(x,y):g.moveTo(x,y);pen=true});g.stroke()}
async function tick(){
 if(!running)return;sent++;const t0=performance.now();
 try{
  const ctl=new AbortController(),to=setTimeout(()=>ctl.abort(),2000);
  const r=await fetch('/api/status?_='+Date.now(),{cache:'no-store',signal:ctl.signal});
  const j=await r.json();clearTimeout(to);const rtt=performance.now()-t0;
  hist.push(rtt);
  $('conn').innerHTML='<span class="ok">&#9679; Connected</span> &mdash; '+j.hostname;
  $('ip').textContent=j.ip;$('mac').textContent=j.mac;
  $('link').innerHTML=j.link?'<span class="ok">Up</span> '+j.speed+' Mbps '+(j.fullDuplex?'FD':'HD'):'<span class="bad">Down</span>';
  $('up').textContent=fmtUp(j.uptime);$('req').textContent=j.requests;
  $('net').textContent=j.mask+' / '+j.gateway+' / '+j.dns+' (static)';
  document.querySelectorAll('.ipv').forEach(e=>e.textContent=j.ip);
  $('rtt').textContent=rtt.toFixed(1)+' ms';
 }catch(e){lost++;hist.push(null);$('conn').innerHTML='<span class="bad">&#9679; No response</span>';$('rtt').textContent='timeout'}
 if(hist.length>60)hist.shift();
 const v=hist.filter(x=>x!=null);
 if(v.length)$('stats').textContent=Math.min(...v).toFixed(1)+' / '+(v.reduce((a,b)=>a+b,0)/v.length).toFixed(1)+' / '+Math.max(...v).toFixed(1);
 $('loss').textContent=sent+' / '+lost+' ('+(100*lost/sent).toFixed(1)+'%)';
 draw();
}
$('toggle').onclick=()=>{running=!running;$('toggle').textContent=running?'Pause':'Resume'};
$('reset').onclick=()=>{sent=lost=0;hist=[];draw()};
addEventListener('resize',draw);
setInterval(tick,1000);tick();
</script></body></html>
)HTML";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

String ipToString(IPAddress ip) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return String(buf);
}

String macToString() {
  uint8_t mac[6];
  Ethernet.macAddress(mac);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void printNetworkInfo() {
  Serial.println();
  Serial.println("=== Network up ===");
  Serial.printf("  MAC:      %s\r\n", macToString().c_str());
  Serial.printf("  IP:       %s\r\n", ipToString(Ethernet.localIP()).c_str());
  Serial.printf("  Mask:     %s\r\n", ipToString(Ethernet.subnetMask()).c_str());
  Serial.printf("  Gateway:  %s\r\n", ipToString(Ethernet.gatewayIP()).c_str());
  Serial.printf("  DNS:      %s\r\n", ipToString(Ethernet.dnsServerIP()).c_str());
  Serial.printf("  Web page: http://%s/  (or http://%s.local/)\r\n",
                ipToString(Ethernet.localIP()).c_str(), kHostname);
  Serial.printf("  Test:     ping %s\r\n", ipToString(Ethernet.localIP()).c_str());
  Serial.printf("  Set your PC's Ethernet port to %s, mask %s\r\n",
                ipToString(kSuggestedPcIP).c_str(), ipToString(kSubnetMask).c_str());
  Serial.println();
}

String statusJson() {
  String s;
  s.reserve(320);
  s += "{\"hostname\":\"";   s += kHostname;
  s += "\",\"ip\":\"";       s += ipToString(Ethernet.localIP());
  s += "\",\"mask\":\"";     s += ipToString(Ethernet.subnetMask());
  s += "\",\"gateway\":\"";  s += ipToString(Ethernet.gatewayIP());
  s += "\",\"dns\":\"";      s += ipToString(Ethernet.dnsServerIP());
  s += "\",\"mac\":\"";      s += macToString();
  s += "\",\"link\":";         s += Ethernet.linkState() ? "true" : "false";
  LinkInfo li = Ethernet.linkInfo();
  s += ",\"speed\":";        s += li.speed;
  s += ",\"fullDuplex\":";   s += li.fullNotHalfDuplex ? "true" : "false";
  s += ",\"uptime\":";       s += millis() / 1000;
  s += ",\"requests\":";     s += httpRequests;
  s += ",\"linkUps\":";      s += linkUpCount;
  s += ",\"linkDowns\":";    s += linkDownCount;
  s += "}";
  return s;
}

void sendResponse(EthernetClient &client, const char *status,
                  const char *contentType, const char *body, size_t len) {
  client.printf("HTTP/1.1 %s\r\n", status);
  client.printf("Content-Type: %s\r\n", contentType);
  client.printf("Content-Length: %u\r\n", (unsigned)len);
  client.print("Cache-Control: no-store\r\n");
  client.print("Connection: close\r\n\r\n");
  client.write(reinterpret_cast<const uint8_t *>(body), len);
  client.flush();
}

// Reads one line (without the trailing CRLF). Returns false on timeout.
bool readLine(EthernetClient &client, String &line, uint32_t deadline) {
  line = "";
  while (client.connected() && millis() < deadline) {
    int c = client.read();
    if (c < 0) {
      yield();
      continue;
    }
    if (c == '\n') return true;
    if (c != '\r' && line.length() < 256) line += static_cast<char>(c);
  }
  return false;
}

void handleClient(EthernetClient &client) {
  uint32_t deadline = millis() + kClientTimeoutMs;

  String requestLine;
  if (!readLine(client, requestLine, deadline)) {
    client.stop();
    return;
  }
  // Drain headers until the blank line.
  String header;
  while (readLine(client, header, deadline) && header.length() > 0) {
  }

  // "GET /path?query HTTP/1.1" -> "/path"
  int sp1 = requestLine.indexOf(' ');
  int sp2 = requestLine.indexOf(' ', sp1 + 1);
  String path = (sp1 > 0 && sp2 > sp1) ? requestLine.substring(sp1 + 1, sp2) : "/";
  int q = path.indexOf('?');
  if (q >= 0) path = path.substring(0, q);

  httpRequests++;

  if (path == "/") {
    sendResponse(client, "200 OK", "text/html; charset=utf-8",
                 kIndexHtml, sizeof(kIndexHtml) - 1);
  } else if (path == "/api/status") {
    String json = statusJson();
    sendResponse(client, "200 OK", "application/json", json.c_str(), json.length());
  } else if (path == "/ping") {
    sendResponse(client, "200 OK", "text/plain", "pong\n", 5);
  } else {
    sendResponse(client, "404 Not Found", "text/plain", "not found\n", 10);
  }

  Serial.printf("[http] %s %s\r\n", ipToString(client.remoteIP()).c_str(), path.c_str());
  client.stop();
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
    // Wait briefly for the Serial Monitor, but don't block without a PC.
  }

  Serial.println("Teensy Ethernet adapter test");
  Serial.printf("MAC: %s\r\n", macToString().c_str());

  Ethernet.onLinkState([](bool up) {
    lastLinkChangeMs = millis();
    if (up) {
      linkUpCount++;
      LinkInfo li = Ethernet.linkInfo();
      Serial.printf("[link] UP %d Mbps %s duplex\r\n", li.speed,
                    li.fullNotHalfDuplex ? "full" : "half");
    } else {
      linkDownCount++;
      Serial.println("[link] DOWN - check the cable / adapter ribbon");
    }
  });

  Ethernet.setHostname(kHostname);

  Serial.println("Starting Ethernet with static IP...");
  if (!Ethernet.begin(kStaticIP, kSubnetMask, kGateway, kDNS)) {
    Serial.println("ERROR: Ethernet.begin() failed. Is this a Teensy 4.1 with the PHY chip?");
    while (true) {
      digitalToggle(LED_BUILTIN);  // fast blink = hardware failure
      delay(100);
    }
  }

  printNetworkInfo();

  if (MDNS.begin(kHostname)) {
    MDNS.addService("_http", "_tcp", kHttpPort);
  }

  server.begin();
}

void loop() {
  // LED: solid when the cable link is up, slow blink otherwise.
  if (Ethernet.linkState()) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, (millis() / 500) % 2);
  }

  EthernetClient client = server.accept();
  if (client) {
    handleClient(client);
  }
}

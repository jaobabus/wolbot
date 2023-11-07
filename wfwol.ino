#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#include <WakeOnLan.h>
#include <FastBot.h>

#define NO_RTTI
#include "raw_config.h"
#include "defs.h"


#define KE_DEBUG_STR(a) #a

#define KE_DEBUG 1
#define KE_DEBUG_CALL(call, report) do { auto r = call; if (KE_DEBUG && r) { ke_debug_call_serial(r, KE_DEBUG_STR(call)); report(r); } } while (0)
#define KE_DEBUG_CALL_F(call, fmt, ...) do { auto r = call; if (KE_DEBUG && r) { ke_debug_call_serial(r, KE_DEBUG_STR(call)); ke_debug_freport_serial(fmt, __VA_ARGS__); } } while (0)


void ke_dummy(int) {}

void ke_debug_call_serial(int res, const char* str) {
    Serial.print(str);
    Serial.print(" returned ");
    Serial.println(res);
    Serial.flush();
}

void ke_debug_freport_serial(const char* fmt, ...) {
    char buffer[1024];
    va_list va;
    va_start(va, fmt);
    buffer[vsnprintf(buffer, sizeof(buffer), fmt, va)] = '\0';
    Serial.println(buffer);
    Serial.flush();
}


struct Reply {
    uint64_t chatid;
    uint32_t msgid;
    uint32_t replyid;
};

struct ee_struct {
    uint32_t last_size;
    uint32_t magic;
    uint64_t chat_id;
    bool last_restart;
    char ssid[32];
    char passwd[32];
    char bot_token[52];
    char mac_addr[20];
    uint64_t authorized_users[8];
    Reply known_replyes[16];
    uint16_t last_reply_idx;
};


/// sintax: <chat_id>:<msg_id>-<reply_id>
raw_config::Error string_to_reply(void* data, size_t, const char* string) {
    using namespace raw_config;
    const char* it = string;
    const char* chat_id = it;
    while (*it >= '0' && *it <= '9')
        ++it;
    if (*it != ':')
        return Error::fmake(Error::Code::InvalidArgument, it - string,
                            "Unexpected token '%c' (%d)", *it, int(*it));
    const char* msg_id = it;
    while (*it >= '0' && *it <= '9')
        ++it;
    if (*it != '-')
        return Error::fmake(Error::Code::InvalidArgument, it - string,
                            "Unexpected token '%c' (%d)", *it, int(*it));
    const char* reply_id = it;
    while (*it >= '0' && *it <= '9')
        ++it;
    if (*it != '-')
        return Error::fmake(Error::Code::InvalidArgument, it - string,
                            "Expected EOF but given '%c' (%d)", *it, int(*it));
    Reply* reply = (Reply*)data;
    reply->chatid = atoll(chat_id);
    reply->msgid = atoi(msg_id);
    reply->replyid = atoi(reply_id);
    return Error::ok();
}

raw_config::Error reply_to_string(char* string, size_t& size, const void* data, size_t) {
    using namespace raw_config;
    const Reply* reply = (const Reply*)data;
    size_t s = snprintf(string, size, "%llu:%u-%u", reply->chatid, reply->msgid, reply->replyid);
    if (s == size) {
        size = 0;
        return Error::make("Error cast reply", Error::Code::TooSmallBuffer, 0);
    }
    size = s;
    return Error::ok();
}


ee_struct eeval;

raw_config::NodeView ee_nodes[] = {
    raw_config::from_pointer("last_size", &ee_struct::last_size),
    raw_config::from_pointer("magic", &ee_struct::magic),
    raw_config::from_pointer("chat_id", &ee_struct::chat_id),
    raw_config::from_pointer("last_restart", &ee_struct::last_restart),
    raw_config::from_pointer("ssid", &ee_struct::ssid),
    raw_config::from_pointer("passwd", &ee_struct::passwd),
    raw_config::from_pointer("bot_token", &ee_struct::bot_token),
    raw_config::from_pointer("mac_addr", &ee_struct::mac_addr),
    raw_config::from_pointer("authorized_users", &ee_struct::authorized_users),
    raw_config::from_pointer("known_replyes", &ee_struct::known_replyes, string_to_reply, reply_to_string, "Reply"),
    raw_config::from_pointer("last_reply_idx", &ee_struct::last_reply_idx),
};

raw_config::ConfigView ee_view((void*)&eeval, ee_nodes);


WiFiUDP UDP;
WakeOnLan WOL(UDP);

FastBot bot;



int str_find(const String& str, const char* ss) {
    return str.indexOf(ss);
}

String format(raw_config::Error const& err, const char* target) {

    String serr = "";
    serr += strcode(err.code);
    serr += ": ";
    serr += err.description;
    serr += " in ";
    serr += err.pos;
    serr += "\n";
    serr += target;
    serr += "\n";
    for (int i = 0; i < err.pos - 1; i++)
        serr += "-";
    serr += "^";
    return serr;
}

String set_value(const String& key, const String& value) {
    auto err = ee_view.set(key.begin(), value.begin());
    if (err.code) {
      if (get_cathegory(err) == 2)
          return format(err, key.begin());
      else
          return format(err, value.begin());
    }
    EEPROM.put(0, eeval);
    return "Ok";
}

String get_value(const String& key) {
    char buffer[1024];
    auto err = ee_view.get(buffer, sizeof(buffer), key.begin());
    if (err.code) {
      return format(err, key.begin());
    }
    return String(buffer);
}

int parse_set(const String& str, String& key, String& value) {
    int eqpos = str_find(str, "=");
    if (eqpos == -1) {
        key = str.substring(5);
        return 1;
    }
    key = str.substring(5, eqpos);
    value = str.substring(eqpos + 1);
    return 2;
}

void set_default_if(void*) {
  ee_struct* ee = &eeval;
    constexpr uint32_t magic = 0xEF047C00;
    if (ee->magic != magic || ee->last_size != sizeof(ee_struct) || KE_DEBUG) {
        memset(ee, 0, sizeof(ee_struct));
        ee->last_size = sizeof(ee_struct);
        ee->magic = magic;
        ee->last_restart = false;
        ee->chat_id = default_chat_id;
        memcpy(ee->ssid, default_wf_ssid, sizeof(default_wf_ssid));
        memcpy(ee->passwd, default_password, sizeof(default_password));
        memcpy(ee->bot_token, default_bot_token, sizeof(default_bot_token));
        memcpy(ee->mac_addr, default_mac_addr, sizeof(default_mac_addr));
        memcpy(ee->authorized_users, default_authorized_users, sizeof(default_authorized_users));
        EEPROM.put(0, *ee);
    }
    if (KE_DEBUG) {
        Serial.println("EE Key data:");
        for (auto& ke : ee_nodes) {
            Serial.print(ke.key);
            Serial.print("=");
            Serial.print("0"); Serial.flush();
            Serial.println(get_value(ke.key));
        }
    }
}

void tg_report(int r) { ke_dummy(r); }

void msg_handler(FB_msg& fbmsg) {
    Serial.println("Username=" + fbmsg.username);
    Serial.println("text=" + fbmsg.text);
    bool authorized = false;
    for (auto& au : eeval.authorized_users) {
        if (String(au) == fbmsg.userID)
            authorized = true;
    }
    if (not authorized) {
        Serial.println("Denied:");
        Serial.print("user id=");  Serial.println(fbmsg.userID);
        Serial.print("username="); Serial.println(fbmsg.username);
        Serial.print("chat id=");  Serial.println(fbmsg.chatID);
        Serial.print("msg id=");   Serial.println(fbmsg.messageID);
        Serial.print("text=");     Serial.println(fbmsg.text);
        Serial.print("is query="); Serial.println(fbmsg.query);
        Serial.print("is bot=");   Serial.println(fbmsg.isBot);
        Serial.print("time=");     Serial.println(fbmsg.unix);
        Serial.print("reply=");    Serial.println(fbmsg.replyText);
        return;
    }
    for (char c : fbmsg.text) {
        if ((uint8_t)c > 127) {
            bot.replyMessage(String("Unicode commands not allowed"), fbmsg.messageID, fbmsg.chatID);
            return;
        }
        else if (c == ' ')
            break;
    }
    String msg = fbmsg.text;
    String cmd = (str_find(msg, " ") != -1 ? msg.substring(0, str_find(msg, " ")) : msg);
    if (cmd == "/set" || cmd == "/get") {
        KE_DEBUG && Serial.println("/set or /get");
        String reply;
        String key, value;
        int cnt = parse_set(msg, key, value);
        if (cnt == 2) {
            if (cmd == "/get")
                reply += "Error: use /set to write value\n";
            else
                reply += set_value(key, value) + "\n";
        }
        else {
            if (cmd == "/set")
                reply += "Warning: use /get to read value\n";
        }
        reply += get_value(key);
        if (fbmsg.edited) {
            for (auto& replyinfo : eeval.known_replyes) {
                if (replyinfo.chatid == uint64_t(fbmsg.chatID.toInt()) && replyinfo.msgid == uint32_t(fbmsg.messageID))
                    KE_DEBUG_CALL_F(bot.editMessage(replyinfo.replyid, reply, fbmsg.chatID),
                                    "chat=%s, msg=%d, reply=%d", fbmsg.chatID.begin(), fbmsg.messageID, int(replyinfo.replyid));
            }
        }
        else {
            KE_DEBUG_CALL(bot.replyMessage(reply, fbmsg.messageID, fbmsg.chatID), tg_report);
            eeval.last_reply_idx = (eeval.last_reply_idx + 1) % 16;
            eeval.known_replyes[eeval.last_reply_idx].chatid = fbmsg.chatID.toInt();
            eeval.known_replyes[eeval.last_reply_idx].msgid = fbmsg.messageID;
            eeval.known_replyes[eeval.last_reply_idx].replyid = bot.lastBotMsg();
            EEPROM.put(0, eeval);
        }
    }
    else if (cmd == "/list") {
        KE_DEBUG && Serial.println("/list");
        String reply = "";
        reply.reserve(1024);
        for (auto& node : ee_nodes) {
            reply += node.key;
            reply += ":";
            reply += node.type;
            reply += ", ";
        }
        KE_DEBUG_CALL(bot.replyMessage(reply, fbmsg.messageID, fbmsg.chatID), tg_report);
    }
    else if (cmd == "/reboot") {
        if (fbmsg.edited) {
            Serial.println("Warning: /reboot edited");
            return;
        }
        KE_DEBUG && Serial.println("/reboot");
        eeval.last_restart = true;
        EEPROM.put(0, eeval);
        KE_DEBUG_CALL(bot.replyMessage("Restarting...", fbmsg.messageID, fbmsg.chatID), tg_report);
        ESP.restart();
    }
    else if (cmd == "/wakeup") {
        if (fbmsg.edited) {
            Serial.println("Warning: /wakeup edited");
            return;
        }
        KE_DEBUG && Serial.println("/wakeup");
        wakeMyPC();
        KE_DEBUG_CALL(bot.replyMessage("WOL sended", fbmsg.messageID, fbmsg.chatID), tg_report);
    }
    else if (cmd == "/reset") {
        if (fbmsg.edited) {
            Serial.println("Warning: /reset edited");
            return;
        }
        KE_DEBUG && Serial.println("/reset");
        eeval.magic = 0;
        set_default_if(&eeval);
        KE_DEBUG_CALL(bot.replyMessage("Settings resetted", fbmsg.messageID, fbmsg.chatID), tg_report);
    }
    else {
        KE_DEBUG && Serial.println("/<unknown>");
        KE_DEBUG_CALL(bot.replyMessage(String("Unknown command '") + cmd + "'", fbmsg.messageID, fbmsg.chatID), tg_report);
    }
}

void wakeMyPC() {
    WOL.sendMagicPacket(eeval.mac_addr); // Send Wake On Lan packet with the above MAC address. Default to port 9.
    // WOL.sendMagicPacket(eeval.mac_addr, 7); // Change the port number
}

void setup()
{
    Serial.begin(115200);
    Serial.println("------ Start ------");

    Serial.println("EE info:");
    for (auto& node : ee_nodes) {
        Serial.print(" key="); Serial.println(node.key);
        Serial.print("  offset="); Serial.println(node.offset);
        Serial.print("  isize="); Serial.println(node.item_size);
        Serial.print("  tsize="); Serial.println(node.total_size);
    }
    Serial.println(";;");

    WOL.setRepeat(3, 100); // Optional, repeat the packet three times with 100ms between. WARNING delay() is used between send packet function.
    EEPROM.begin(sizeof(ee_struct));

    EEPROM.get(0, eeval);
    set_default_if(&eeval);

    WiFi.mode(WIFI_STA);
    WiFi.begin(eeval.ssid, eeval.passwd);

    const char* strstatus_s[] = {
      "WL_IDLE_STATUS",     // 0, ...
      "WL_NO_SSID_AVAIL",
      "WL_SCAN_COMPLETED",
      "WL_CONNECTED",
      "WL_CONNECT_FAILED",
      "WL_CONNECTION_LOST",
      "WL_DISCONNECTED",
      "UNKNOWN",
      "UNKNOWN",
      "UNKNOWN",
      "UNKNOWN",
      "UNKNOWN",
    };
    int last_status = -1;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (WiFi.status() != last_status) {
          Serial.println();
          Serial.print("Status: ");
          Serial.print(strstatus_s[int(WiFi.status())]);
          last_status = WiFi.status();
      }
      else {
          Serial.print(".");
      }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask()); // Optional  => To calculate the broadcast address, otherwise 255.255.255.255 is used (which is denied in some networks).

    bot.setToken(eeval.bot_token);
    bot.attach(msg_handler);

    if (eeval.last_restart) {
        eeval.last_restart = false;
        bot.sendMessage(String("Restarted"), String(eeval.chat_id));
    }
}


void loop()
{
    bot.tick();
}

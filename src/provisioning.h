//Provides provisioning and wifi management for multiple APs

static const int STATUS_NOAP =  1;
static const int  STATUS_CONNECTED = 2;

typedef struct WifiConfigs {
    char SSID[32];
    char SSID_Pass[64];
    bool Usable;
    int Slot;
} WifiConfig;

WifiConfig* provision();

bool initWifi();
void connectSSID(char* ssid, char* pass);
void connectSSIDCon(WifiConfig* con);
void setSSIDCons(WifiConfig* cons, int conCount);
int connectSSIDCons(WifiConfig* cons, int conCount);
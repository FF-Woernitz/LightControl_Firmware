bool needRestart = false;
int wifiRetry = 0;
int mode = 255;
int state = 0;
int alarm = 0;
int blinkCount = 0;
int blinkDelay = 0;
long lastBlink = 0;

void handleRoot();
void blinkHandler();
void alarmHandler();
void connectToWifi();
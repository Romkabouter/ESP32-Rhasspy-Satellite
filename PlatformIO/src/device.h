#pragma once

int hotword_colors[4] = {0, 255, 0, 0};
int idle_colors[4] = {0, 0, 255, 0};
int wifi_conn_colors[4] = {0, 0, 255, 0};
int wifi_disc_colors[4] = {255, 0, 0, 0};
int ota_colors[4] = {0, 0, 0, 255};
int tts_colors[4] = {80, 10, 185, 0};
int error_colors[4] = {150, 255, 0, 0};
enum {
  COLORS_HOTWORD = 0,
  COLORS_WIFI_CONNECTED = 1,
  COLORS_WIFI_DISCONNECTED = 2,
  COLORS_IDLE = 3,
  COLORS_OTA = 4,
  COLORS_TTS = 5,
  COLORS_ERROR = 6
};
enum DeviceMode {
  MODE_UNUSED = -1, // indicates the no explicit mode change has been stored yet 
  MODE_MIC = 0,
  MODE_SPK = 1
};

//Devices can have several outputs. 
//The Matrix Voice has an jack and a speaker
enum AmpOut {
  AMP_OUT_SPEAKERS = 0,
  AMP_OUT_HEADPHONE = 1,
  AMP_OUT_BOTH = 2,
};

class Device {
  protected:
     // for derived classes which switch between read and write mode, we store there which mode is active. Otherwise it should remain as MODE_UNUSED
    DeviceMode mode = MODE_UNUSED; 
  public:
     //You can init your device here if needed
    virtual void init() {};
     //If your device has leds, override these methods to set the colors and brightness
    virtual void updateColors(int colors) {};
    //You can create some animation here
    virtual void animate(int colors) {};
    virtual void updateBrightness(int brightness) {};
    //It may be needed to switch between read and write, i.e. if the need the same PIN
    //Override both methods
    virtual void setReadMode() {}; 
    virtual void setWriteMode(int sampleRate, int bitDepth, int numChannels) {}; 
    //Override these two methods to read and write
    virtual void writeAudio(uint8_t *data, size_t size, size_t *bytes_written) {};
    virtual bool readAudio(uint8_t *data, size_t size) {return false;};
    //Some devices cause a noise, even when no sound is played. override this to fix it
    virtual void muteOutput(bool mute) {};
    //Some devices have multiple outputs (jack/speeker)
    virtual void ampOutput(int output) {};
    //Some devices support settings of volume
    virtual void setVolume(uint16_t volume) {};
    //Possiblity to set gain
    virtual void setGain(uint16_t gain) {};
    //You can use this method to activated the hotword state (i.e. a hardware button)
    virtual bool isHotwordDetected() {return false;};

    // how many different output configurations does this devices support (1 = single output channel, 2 = 2 output channels, i.e. speaker or headphone, 3 = speaker, headphone, speaker + headphone)
    virtual int numAmpOutConfigurations() { return 2; };
    //
    //You can override these in your device
    int readSize = 256;
    int writeSize = 256;
    int width = 2;
    int rate = 16000;
};

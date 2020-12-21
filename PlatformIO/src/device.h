int hotword_colors[4] = {0, 255, 0, 0};
int stream_colors[4] = {0, 0, 255, 0};
int wifi_conn_colors[4] = {0, 0, 255, 0};
int wifi_disc_colors[4] = {255, 0, 0, 0};
enum {
  COLORS_HOTWORD = 0,
  COLORS_WIFI_CONNECTED = 1,
  COLORS_WIFI_DISCONNECTED = 2,
  COLORS_STREAM = 3
};
enum {
  MODE_MIC = 0,
  MODE_SPK = 1
};

class Device {
  protected:
    int mode;
  public:
    virtual void init() {}; //Init is not needed for every device
    virtual void updateLeds(int colors) {}; //some device have no leds

    //Pure virtual, these should always be implemented in a device
    virtual void setReadMode() = 0;
    virtual void setWriteMode(int sampleRate, int bitDepth, int numChannels) = 0; 
    virtual void writeAudio(uint8_t *data, size_t size, size_t *bytes_written) = 0;
    virtual void readAudio(uint8_t *data, size_t size) = 0;

};

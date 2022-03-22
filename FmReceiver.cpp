#include "si5351.h"
#include <SoftwareSerial.h>


/////////////////////////////////////////////////////////
// GU144X16D-7053B vacuum-flourescent display class
/////////////////////////////////////////////////////////
#define VFD_MAX_FIELDS 8

class VFD {
  
public:
  
  //////////////////////////////
  // UI element class
  //////////////////////////////
  class Field {
    
  private:  
  
    VFD *_vfd;
    uint8_t _x, _y, _width, _xmag, _ymag;
    String _value;
    bool _enabled;
    bool _stale;
  
  public:

    Field() :
      _vfd(NULL), _value(""), _enabled(true), _stale(false)
    {      
    }
    
    void set_vfd(VFD *vfd)
    {
      _vfd = vfd;
    }

    void set_enabled(bool enabled)
    {
      _enabled = enabled;
      _stale = true;
    }
    
    void set_value(String value)
    {
      _stale |= (value != _value);
      _value = value;  
    }

    void set_position(uint8_t x, uint8_t y)
    {
      _stale |= (x != _x) || (y != _y);
      _x = x;
      _y = y;
    }

    void set_font_width(uint8_t width)
    {
      _stale |= (width != _width);
      _width = width;
    }

    void set_font_magnification(uint8_t xmag, uint8_t ymag)
    {
      _stale |= (xmag != _xmag) || (ymag != _ymag);
      _xmag = xmag;
      _ymag = ymag;
    }

    void enstale()
    {
      _stale = true;
    }
    
    void update()
    {
      if (_enabled && _stale) {
        _vfd->set_font_magnification(_xmag, _ymag);
        _vfd->set_font_width(_width);
        _vfd->set_cursor(_x, _y);
        _vfd->write(_value.c_str());
        _stale = false;  
      } 
    }
    
    void set_value_and_update(String value)
    {
      set_value(value);
      update();
    }
  
  };


private:
  Stream &_serial;  
  uint8_t _busy_pin;
  Field _fields[VFD_MAX_FIELDS]; 
  uint8_t _num_fields;

public:

enum WriteMode {
  NORMAL=0, OR, AND, XOR
};

VFD(Stream &serial, uint8_t busy_pin) :
  _serial(serial), _busy_pin(busy_pin), _num_fields(0)
{
}

void begin()
{
  init();
  set_custom_char_download_enabled(true);
  set_write_mode(VFD::NORMAL);
  set_font_width(1);
  set_font_magnification(1, 1);  
  clear();
  set_cursor(0, 0);
}

Field *create_text_field(uint8_t x, uint8_t y, uint8_t width, uint8_t xmag, uint8_t ymag) 
{
  if (_num_fields == VFD_MAX_FIELDS) {
    return NULL;
  }
  _fields[_num_fields].set_vfd(this);
  _fields[_num_fields].set_position(x, y);
  _fields[_num_fields].set_font_width(width);
  _fields[_num_fields].set_font_magnification(xmag, ymag);
  return &_fields[_num_fields++];
}

void update_fields()
{
  for (uint8_t i = 0; i < _num_fields; ++i) {
    _fields[i].update();
  }
}

void enstale_fields()
{
  for (uint8_t i = 0; i < _num_fields; ++i) {
    _fields[i].enstale();
  }
}

void write(char *s)
{
  while(digitalRead(_busy_pin));
  _serial.write(s);
}

void write(uint8_t buf[], size_t len) 
{
  while(digitalRead(_busy_pin));
  _serial.write(buf, len);
}

void clear()
{
  write("\x0c");
  enstale_fields();
}

void init() 
{
  write("\x1b\x40");  
}
  
void set_cursor(uint8_t x, uint8_t y) 
{
  uint8_t buf[] = { 0x1f, 0x24, x, 0, y, 0 };  
  write(buf, sizeof(buf));   
}

void set_font_width(uint8_t w) 
{  
  uint8_t buf[] = { 0x1f, 0x28, 0x67, 0x03, w };  
  write(buf, sizeof(buf));    
}

void set_font_magnification(uint8_t x, uint8_t y) 
{  
  uint8_t buf[] = { 0x1f, 0x28, 0x67, 0x40, x, y };  
  write(buf, sizeof(buf));    
}

void set_write_mode(enum WriteMode mode) 
{
  uint8_t buf[] = { 0x1f, 0x77, mode };  
  write(buf, sizeof(buf));      
}

void draw(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t data[], uint8_t len)
{
  uint8_t buf[] = {
    0x1f, 0x28, 0x64, 0x21, 
    x, 0, y, 0, w, 0, h, 0, 1
  };
  write(buf, sizeof(buf));
  write(data, len); 
}

void set_custom_char_download_enabled(bool enabled)
{
  write("\x1b\x25\x01");
}

void download_custom_char(uint8_t char_code, uint8_t bitmap[5])
{
  uint8_t buf[] = {
    0x1b, 0x26, 1, char_code, char_code, 0x05, 
    bitmap[0], bitmap[1], bitmap[2], bitmap[3], bitmap[4]
  };
  write(buf, sizeof(buf));  
}
   
};


//////////////////////////
// I/O pins
//////////////////////////
const int tuning_pin = A0;
const int lock_detect_pin = A2;
const int vfd_tx_pin = 2;
const int vfd_rx_pin = 4; // unused, assign to unused pin
const int vfd_busy_pin = 3;


//////////////////////////
// Static objects
//////////////////////////
Si5351 si5351;
SoftwareSerial vfd_serial(vfd_rx_pin, vfd_tx_pin, false); // true = inverted logic
VFD vfd(vfd_serial, vfd_busy_pin);


//////////////////////////
// UI elements
//////////////////////////
VFD::Field &fm_field = *vfd.create_text_field(0, 0, 0, 2, 1);
VFD::Field &strength_field = *vfd.create_text_field(0, 1, 0, 1, 1);

VFD::Field &freq_field = *vfd.create_text_field(45, 0, 0, 2, 1);
VFD::Field &tune_field = *vfd.create_text_field(110, 0, 2, 1, 1);

VFD::Field &graticule_field = *vfd.create_text_field(-1, -1, 1, 1, 1);
VFD::Field &spectrum_field = *vfd.create_text_field(-1, -1, 0, 1, 1);

const String graticule = "\xf6\xf7\xf6\xf7\xf6\xf7\xf6\xf7|\xf7\xf6\xf7\xf6\xf7\xf6\xf7\xf6\xf7|\xf7\xf6\xf7\xf6\xf7\xf6\xf7\xf6\xf7|\xf7\xf6\xf7\xf6\xf7\xf6\xf7\xf6\xf7|\xf7\xf6\xf7\xf6\xf7\xf6\xf7\xf6\xf7|\xf7\xf6\xf7\xf6\xf7\xf6\xf7\xf6\xf7|";

const String bars[] = {
  "    ",
  "\xf0   ",
  "\xf0\xf1  ",
  "\xf0\xf1\xf2 ",
  "\xf0\xf1\xf2\xf3"
};

char spectrum[255] = ""; // 100kHz BW per element, 20.48MHz total


//////////////////////////////////////////
// Custom VFD character bitmaps
//////////////////////////////////////////
const uint8_t sig_bar_0[5] = { 0x01<<1, 0x01<<1, 0x01<<1, 0x03<<1, 0x03<<1 };
const uint8_t sig_bar_1[5] = { 0x07<<1, 0x07<<1, 0x07<<1, 0x0F<<1, 0x0F<<1 };
const uint8_t sig_bar_2[5] = { 0x1F<<1, 0x1F<<1, 0x1F<<1, 0x3F<<1, 0x3F<<1 };
const uint8_t sig_bar_3[5] = { 0x7F<<1, 0x7F<<1, 0x7F<<1, 0x7F<<1, 0x7F<<1 };

const uint8_t spec_ch_0_left[5] = { 0x03>>0, 0x0C>>2, 0x10>>2, 0x20>>2, 0x20>>2 };
const uint8_t spec_ch_1_left[5] = { 0x03>>0, 0x0C>>1, 0x10>>1, 0x20>>1, 0x20>>1 };
const uint8_t spec_ch_2_left[5] = { 0x03<<0, 0x0C<<0, 0x10<<0, 0x20<<0, 0x20<<0 };
const uint8_t spec_ch_3_left[5] = { 0x03<<1, 0x0C<<1, 0x10<<1, 0x20<<1, 0x20<<1 };

const uint8_t spec_ch_0_right[5] = { 0x20>>2, 0x20>>2, 0x10>>2, 0x0C>>2, 0x03>>0 };
const uint8_t spec_ch_1_right[5] = { 0x20>>1, 0x20>>1, 0x10>>1, 0x0C>>1, 0x03>>0 };
const uint8_t spec_ch_2_right[5] = { 0x20<<0, 0x20<<0, 0x10<<0, 0x0C<<0, 0x03<<0 };
const uint8_t spec_ch_3_right[5] = { 0x20<<1, 0x20<<1, 0x10<<1, 0x0C<<1, 0x03<<1 };

const uint8_t stereo_wave_left[5] = { 0x1C<<1, 0x22<<1, 0x00<<1, 0x00<<1, 0x00<<1 };
const uint8_t stereo_wave_right[5] = { 0x00<<1, 0x00<<1, 0x00<<1, 0x22<<1, 0x1C<<1 };

const uint8_t graticule_minor_tick[5] = { 0x00<<3, 0x00<<3, 0x07<<3, 0x00<<3, 0x00<<3 };
const uint8_t graticule_minor_dot[5] = { 0x00<<3, 0x00<<3, 0x02<<3, 0x00<<3, 0x00<<3 };


//////////////////////////////////////////
// Application-specific constants
//////////////////////////////////////////
const uint16_t lock_detect_min = 400;
const uint16_t lock_detect_max = 660;

const uint16_t tune_freq_10khz_min = 8776; // 9800 - 1024
const uint16_t tune_freq_10khz_max = 10824; // 9800 + 1024
const uint16_t tune_freq_10khz_span = tune_freq_10khz_max - tune_freq_10khz_min;


//////////////////////////////////////////
// Conversion functions
//////////////////////////////////////////

uint16_t get_lo_freq_10khz_for_rf_freq_10khz(uint16_t rf_freq) 
{
  if (rf_freq >= 8800 && rf_freq < 9800) {
    return rf_freq - 1070;
  }
  else if (rf_freq >= 9800 && rf_freq <= 10800) {
    return rf_freq + 1070;
  }
  else {
    return 0;
  }  
}


uint16_t tuning_value_to_freq_10khz(uint32_t analog_reading) 
{
  return tune_freq_10khz_min + ((uint32_t)tune_freq_10khz_span * analog_reading) / 1024;
}


uint8_t freq_10khz_to_fm_channel(uint32_t freq_10khz)
{
  if (freq_10khz >= tune_freq_10khz_min && freq_10khz < tune_freq_10khz_max) {
    return (2048 * (freq_10khz - tune_freq_10khz_min)) / (10*tune_freq_10khz_span);
  }
  else {
    return 255;
  }  
}


uint16_t rf_freq_10khz_to_channel_freq_10khz(uint16_t rf_freq_10khz) {
  return ((rf_freq_10khz) / 20) * 20 + 10;
}



//////////////////////////////////////////
//
//    setup()
//
//////////////////////////////////////////
void setup() 
{
  delay(500);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.begin(115200);
  
  if (!si5351.init(SI5351_CRYSTAL_LOAD_6PF, 26999370ULL, 0)) {
    while (1) {
      Serial.print("ERROR: si5351.init() failed\n");
      delay(1000);
    }  
  }
  si5351.set_ms_source(SI5351_CLK0, SI5351_PLLA);
  si5351.set_ms_source(SI5351_CLK1, SI5351_PLLA);
  si5351.set_ms_source(SI5351_CLK2, SI5351_PLLB);
  
  vfd_serial.begin(38400);    
  
  vfd.begin();
  
  vfd.download_custom_char(0xf0, sig_bar_0);
  vfd.download_custom_char(0xf1, sig_bar_1);
  vfd.download_custom_char(0xf2, sig_bar_2);
  vfd.download_custom_char(0xf3, sig_bar_3);

  vfd.download_custom_char(0xf4, stereo_wave_left);
  vfd.download_custom_char(0xf5, stereo_wave_right);
  
  vfd.download_custom_char(0xf6, graticule_minor_dot);
  vfd.download_custom_char(0xf7, graticule_minor_tick);
  

  vfd.download_custom_char(0xf8, spec_ch_0_left);
  vfd.download_custom_char(0xf9, spec_ch_1_left);
  vfd.download_custom_char(0xfa, spec_ch_2_left);
  vfd.download_custom_char(0xfb, spec_ch_3_left);

  vfd.download_custom_char(0xfc, spec_ch_0_right);
  vfd.download_custom_char(0xfd, spec_ch_1_right);
  vfd.download_custom_char(0xfe, spec_ch_2_right);
  vfd.download_custom_char(0xff, spec_ch_3_right);

  memset(spectrum, '\xf8', sizeof(spectrum));
  memset(spectrum, '_', sizeof(spectrum));
  
  spectrum_field.set_enabled(true);
  graticule_field.set_enabled(false);

}



//////////////////////////////////////////
//
//    loop()
//
//////////////////////////////////////////
void loop() 
{  
  static uint16_t rf_freq_10khz;
  static uint16_t lo_freq_10khz;
  static uint16_t last_fm_channel = -1;
  static uint16_t last_channel_freq_10khz = 0;  
  static uint16_t tuning_value = 512;
  static uint16_t strength_value = 0;
  
  bool update_si5351 = false;
  
  tuning_value = (3*tuning_value + (uint32_t)analogRead(tuning_pin)) / 4;
  rf_freq_10khz = tuning_value_to_freq_10khz(tuning_value);
  uint16_t channel_freq_10khz = rf_freq_10khz_to_channel_freq_10khz(rf_freq_10khz);  
  uint16_t fm_channel = freq_10khz_to_fm_channel(rf_freq_10khz);
  
  if (channel_freq_10khz != last_channel_freq_10khz) {
    last_channel_freq_10khz = channel_freq_10khz;
    lo_freq_10khz = get_lo_freq_10khz_for_rf_freq_10khz(channel_freq_10khz);
    update_si5351 = true;     
  }  

  if (update_si5351) {
    si5351.set_freq((uint64_t)lo_freq_10khz * 1000000ULL, SI5351_CLK0);
    si5351.set_freq((uint64_t)lo_freq_10khz * 1000000ULL, SI5351_CLK1);
    si5351.set_clock_invert(SI5351_CLK0, 0);
    si5351.set_clock_invert(SI5351_CLK1, 1);        
  }

  fm_field.set_value_and_update("FM");
  
  uint16_t strength_value = constrain(analogRead(lock_detect_pin), lock_detect_min, lock_detect_max);  
  uint8_t num_bars =  (5 * (strength_value-lock_detect_min)) / (lock_detect_max-lock_detect_min);  
  strength_field.set_value_and_update(bars[constrain(num_bars, 0, 4)]);
  
  
  // Graticule: 5 pixels and 2 space pixels == 1 character == 100 kHz
  uint8_t graticule_pixel_offset = 5 + (7 * ((rf_freq_10khz-tune_freq_10khz_min) % 10)) / 10;
  uint8_t graticule_char_offset = 0 + (1 * ((rf_freq_10khz-tune_freq_10khz_min) % 100)) / 10;
  graticule_field.set_position(40-graticule_pixel_offset, 1);
  graticule_field.set_value_and_update(" " + String(graticule.substring(graticule_char_offset, graticule_char_offset+13)) + " ");


  // Spectrum: 5 pixels and 1 space pixel == 1 character == 100 kHz
  uint8_t spectrum_pixel_offset = 0 + (6 * ((rf_freq_10khz-tune_freq_10khz_min) % 10)) / 10;
  uint16_t spectrum_char_offset = 0 + (1 * rf_freq_10khz-tune_freq_10khz_min) / 10;
  uint8_t subspectrum[17];
  strncpy(subspectrum, spectrum+spectrum_char_offset, sizeof(subspectrum)-1);
  subspectrum[0] = ' ';
  subspectrum[sizeof(subspectrum)-2] = ' ';
  subspectrum[sizeof(subspectrum)-1] = '\0';
  spectrum_field.set_position(40-spectrum_pixel_offset, 1);
  spectrum_field.set_value_and_update(subspectrum);


  uint8_t whole = (channel_freq_10khz) / 100;
  uint8_t part = ((channel_freq_10khz) % 100) / 10;
  char buf[8];
  sprintf(buf, "%3d.%1d", whole, part);
  freq_field.set_value_and_update(buf);


  vfd.set_write_mode(VFD::OR);
  uint8_t reticle[] = { 0x00, 0x82, 0xC6, 0x82, 0x00 };
  vfd.draw(83, 9, 5, 7, reticle, sizeof(reticle));
  vfd.set_write_mode(VFD::NORMAL);
  

  if (abs((int16_t)rf_freq_10khz-(int16_t)channel_freq_10khz) < 5) {
    spectrum[2*(spectrum_char_offset/2)+8] = (num_bars == 0) ? '_' : (0xf8 + num_bars - 1);
    spectrum[2*(spectrum_char_offset/2)+9] = (num_bars == 0) ? '_' : (0xfc + num_bars - 1);
  }
}



// index.html file in raw data format for PROGMEM
const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>Satellite Configuration - %SITEID%</title>
<meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">
<style>
* {box-sizing: border-box;}
.input-container {display: flex;width: 350px;margin-bottom: 5px;}
.input-field {width: 200px;padding: 2px;outline: none;}
.input-field:focus {border: 2px solid#2c3e50;}
.btn {background-color:#2c3e50;color: white;padding: 5px 10px;border: none;cursor: pointer;width: 100px;border-radius: 3px;}
.switch {position: relative;display: inline-block;width: 40px;height: 20px;} 
.switch input { opacity: 0;width: 0;height: 0;}
.slider {position: absolute;cursor: pointer;top: 0;left: 0;right: 0;bottom: 0;background-color: #ccc;-webkit-transition: .4s;transition: .4s;}
.slider:before {position: absolute;content: "";height: 16px;width: 16px;left: 2px;bottom: 2px;background-color: white;-webkit-transition: .4s;transition: .4s;}
input:checked + .slider {background-color: #2c3e50;}
input:focus + .slider {box-shadow: 0 0 1px #2c3e50;}
input:checked + .slider:before {-webkit-transform: translateX(19px);-ms-transform: translateX(19px);transform: translateX(19px);}
.slider.round {border-radius: 20px;}
.slider.round:before {border-radius: 12px;}
label {padding: 2px;width:175px;}
.range-slider {width: 60%;}
.range-slider__range {-webkit-appearance: none;width: calc(100%% - (73px));height: 10px;border-radius: 5px;background: #d7dcdf;outline: none;padding: 0;margin: 0;}
.range-slider__range::-webkit-slider-thumb {-webkit-appearance: none;appearance: none;width: 20px;height: 20px;border-radius: 50%%;background: #2c3e50;cursor: pointer;-webkit-transition: background .15s ease-in-out;transition: background .15s ease-in-out;}
.range-slider__range::-webkit-slider-thumb:hover {background: #1abc9c;}
.range-slider__range:active::-webkit-slider-thumb {background: #1abc9c;}
.range-slider__range::-moz-range-thumb {width: 20px;height: 20px;border: 0;border-radius: 50%%;background: #2c3e50;cursor: pointer;-moz-transition: background .15s ease-in-out;transition: background .15s ease-in-out;}
.range-slider__range::-moz-range-thumb:hover {background: #1abc9c;}
.range-slider__range:active::-moz-range-thumb {background: #1abc9c;}
.range-slider__range:focus::-webkit-slider-thumb {box-shadow: 0 0 0 3px #fff, 0 0 0 6px #1abc9c;}
.range-slider__value {display: inline-block;position: relative;width: 60px;color: #fff;line-height: 20px;text-align: center;border-radius: 3px;background: #2c3e50;padding: 5px 10px;margin-left: 8px;}
.range-slider__value:after {position: absolute;top: 8px;left: -7px;width: 0;height: 0;border-top: 7px solid transparent;border-right: 7px solid #2c3e50;border-bottom: 7px solid transparent;content: '';}
::-moz-range-track {background: #d7dcdf;border: 0;}
input::-moz-focus-inner,input::-moz-focus-outer {border: 0;}

.pc-tab > input,
.pc-tab section > div {
  display: none;
}

#tab1:checked ~ section .tab1,
#tab2:checked ~ section .tab2,
#tab3:checked ~ section .tab3,
#tab4:checked ~ section .tab4 {
  display: block;
}

#tab1:checked ~ nav .tab1,
#tab2:checked ~ nav .tab2,
#tab3:checked ~ nav .tab3,
#tab4:checked ~ nav .tab4 {
  color: red;
}

/* Visual Styles */
*, *:after, *:before {
  -webkit-box-sizing: border-box;
  -moz-box-sizing: border-box;
  box-sizing: border-box;
}

body {
  -webkit-font-smoothing: antialiased;
  background: #ecf0f1;
}

h1 {
  text-align: center;
  font-weight: 100;
  font-size: 60px;
  color: #e74c3c;
}

.pc-tab {
  width: 100%;
  margin: 0 auto;
}
.pc-tab ul {
  list-style: none;
  margin: 0;
  padding: 0;
}
.pc-tab ul li label {
  float: left;
  padding: 15px 25px;
  border: 1px solid #ddd;
  border-bottom: 0;
  background: #eeeeee;
  color: #444;
}
.pc-tab ul li label:hover {
  background: #dddddd;
}
.pc-tab ul li label:active {
  background: #ffffff;
}
.pc-tab ul li:not(:last-child) label {
  border-right-width: 0;
}
.pc-tab section {
  clear: both;
}
.pc-tab section div {
  padding: 20px;
  width: 100%;
  border: 1px solid #ddd;
  background: #fff;
  line-height: 1.5em;
  letter-spacing: 0.3px;
  color: #444;
}
.pc-tab section div h2 {
  margin: 0;
  letter-spacing: 1px;
  color: #34495e;
}

#tab1:checked ~ nav .tab1 label,
#tab2:checked ~ nav .tab2 label,
#tab3:checked ~ nav .tab3 label,
#tab4:checked ~ nav .tab4 label {
  background: white;
  color: #111;
  position: relative;
}
#tab1:checked ~ nav .tab1 label:after,
#tab2:checked ~ nav .tab2 label:after,
#tab3:checked ~ nav .tab3 label:after,
#tab4:checked ~ nav .tab4 label:after {
  content: "";
  display: block;
  position: absolute;
  height: 2px;
  width: 100%;
  background: #ffffff;
  left: 0;
  bottom: -1px;
}

footer {
  margin-top: 50px;
  font-size: 14px;
  color: #CCC;
  text-align: center;
}
footer a {
  color: #AAA;
  text-decoration: none;
}

</style>
</head>
<body>
  <form action="/" method="post">
    <h2>Satellite Configuration - %SITEID%</h2>
    <div class="pc-tab">
      <input checked="checked" id="tab1" type="radio" name="pct" />
      <input id="tab2" type="radio" name="pct" />
      <input id="tab3" type="radio" name="pct" />
      <input id="tab4" type="radio" name="pct" />
        <nav>
          <ul>
            <li class="tab1">
              <label for="tab1">Network</label>
            </li>
            <li class="tab2">
              <label for="tab2">Audio</label>
            </li>
            <li class="tab3">
              <label for="tab3">Leds</label>
            </li>
            <li class="tab4">
              <label for="tab4">Colors</label>
            </li>
          </ul>
        </nav>
        <section>
          <div class="tab1">
            <div class="input-container">
              <label for="siteid">siteID:&nbsp;</label>
              <input class="input-field" type="text" placeholder="siteId" name="siteid" value="%SITEID%">
            </div>
            <div class="input-container">
              <label for="mqtt_host">MQTT hostname:&nbsp;</label>
              <input class="input-field" type="text" placeholder="MQTT IP" name="mqtt_host" value="%MQTT_HOST%">
            </div>
            <div class="input-container">
              <label for="mqtt_port">MQTT port:&nbsp;</label>
              <input class="input-field" type="text" placeholder="MQTT port" name="mqtt_port" value="%MQTT_PORT%">
            </div>
            <div class="input-container">
              <label for="mqtt_user">MQTT username:&nbsp;</label>
              <input class="input-field" type="text" placeholder="MQTT username" name="mqtt_user" value="%MQTT_USER%">
            </div>
            <div class="input-container">
              <label for="mqtt_pass">MQTT password:&nbsp;</label>
              <input class="input-field" type="password" placeholder="MQTT password" name="mqtt_pass" value="%MQTT_PASS%">
            </div>
          </div>
          <div class="tab2">
            <div class="input-container">
              <label for="mute_input">Mute input:&nbsp;</label>
              <label class="switch">
                <input type="checkbox" name="mute_input" %MUTE_INPUT%>
                <span class="slider round"></span>
              </label>
            </div>
            <div class="input-container">
              <label for="mute_output">Mute output:&nbsp;</label>
              <label class="switch">
                <input type="checkbox" name="mute_output" %MUTE_OUTPUT%>
                <span class="slider round"></span>
              </label>
            </div>
            <div class="input-container">
              <label for="amp_output">Output to:&nbsp;</label>
              <select name="amp_output">
                <option value="0" %AMP_OUT_SPEAKERS%>Speakers</option>
                <option value="1" %AMP_OUT_HEADPHONE%>Headphone</option>
                <option value="2" %AMP_OUT_BOTH%>Headphone+Speakers</option>
              </select>
            </div>
            <div class="input-container">
              <label for="volume">Volume:&nbsp;</label>
              <div class="range-slider">
                <input type="range" class="range-slider__range" name="volume" min="0" max="100" step="5" value="%VOLUME%">
                <span class="range-slider__value">0</span>
              </div>
            </div>
            <div class="input-container">
              <label for="gain">Gain:&nbsp;</label>
              <div class="range-slider">  
                <input type="range" min="1" max="8" value="%GAIN%" class="range-slider__range" name="gain">
                <span class="range-slider__value">0</span>
              </div>
            </div>
          </div>
          <div class="tab3">
            <div class="input-container">
              <label for="brightness">Brightness:&nbsp;</label>
              <div class="range-slider">
                <input type="range" min="0" max="100" step="5" value="%BRIGHTNESS%" class="range-slider__range" name="brightness">
                <span class="range-slider__value">0</span>
              </div>
            </div>
            <div class="input-container">
              <label for="hotword_brightness">Hotword brightness:&nbsp;</label>
              <div class="range-slider">
                <input type="range" min="0" max="100" step="5" value="%HW_BRIGHTNESS%" class="range-slider__range" name="hw_brightness">
                <span class="range-slider__value">0</span>
              </div>
            </div>
            <div class="input-container" style="display:%ANIMATIONSUPPORT%">
              <label for="animation">Animation mode:&nbsp;</label>
              <select name="animation">
                <option value="0" %ANIM_SOLID%>Solid</option>
                <option value="1" %ANIM_RUNNING%>Running</option>
                <option value="2" %ANIM_PULSING%>Pulsing</option>
                <option value="3" %ANIM_BLINKING%>Blinking</option>
              </select>
            </div>
          </div>
          <div class="tab4">
            <div class="input-container">
              <label for="idle_color">Idle color:&nbsp;</label>
              <input type="color" id="idle_color" name="idle_color" value="%COLOR_IDLE%">
            </div>
            <div class="input-container">
              <label for="hotword_color">Hotworld color:&nbsp;</label>
              <input type="color" id="hotword_color" name="hotword_color" value="%COLOR_HOTWORD%">
            </div>
            <div class="input-container">
              <label for="tts_color">Tts color:&nbsp;</label>
              <input type="color" id="tts_color" name="tts_color" value="%COLOR_TTS%">
            </div>
            <div class="input-container">
              <label for="error_color">Error color:&nbsp;</label>
              <input type="color" id="error_color" name="error_color" value="%COLOR_ERROR%">
            </div>
            <div class="input-container">
              <label for="update_color">Update color:&nbsp;</label>
              <input type="color" id="update_color" name="update_color" value="%COLOR_UPDATE%">
            </div>
            <div class="input-container">
              <label for="wifi_disc_color">Wifi disconnect color:&nbsp;</label>
              <input type="color" id="wifi_disc_color" name="wifi_disc_color" value="%COLOR_WIFIDISC%">
            </div>
            <div class="input-container">
              <label for="wifi_conn_color">Wifi connect color:&nbsp;</label>
              <input type="color" id="wifi_conn_color" name="wifi_conn_color" value="%COLOR_WIFICONN%">
            </div>        
          </div>
        </section>
      </div>
      

    <button type="submit" class="btn">Save</button>
  </form>
</body>
</html>
<script>
  const allRanges = document.querySelectorAll(".range-slider");
  allRanges.forEach(wrap => {
    const range = wrap.querySelector(".range-slider__range");
    const value = wrap.querySelector(".range-slider__value");
    range.addEventListener("input", () => {
      setValue(range, value);
    });
    setValue(range, value);
  });
  function setValue(range, value) {
    const val = range.value;
    value.innerHTML = val;
  }
</script>
)=====" ;
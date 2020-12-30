// index.html file in raw data format for PROGMEM
const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>Satellite Configuration</title>
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
label {padding: 2px;width:140px;}
.range-slider {width: 60%%;}
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
</style>
</head>
<body>
  <form action="/" method="post">
    <h2>Satellite Configuration</h2>
    <div class="input-container">
      <label for="mqtt_host">MQTT ip:&nbsp;</label>
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
    <div class="input-container">
      <label for="hotword_detection">Hotword detection:&nbsp;</label>
      <select name="hotword_detection">
        <option value="0" %HW_LOCAL%>Local</option>
        <option value="1" %HW_REMOTE%>Remote</option>
      </select>
    </div>
    <div class="input-container">
      <label for="gain">Gain:&nbsp;</label>
      <div class="range-slider">  
        <input type="range" min="1" max="8" value="%GAIN%" class="range-slider__range" name="gain">
        <span class="range-slider__value">0</span>
      </div>
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
const char* PARAM_INPUT_1 = "input1";
const char index_html[]  = 
R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Nano ESP32 Flipo clock</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style type="text/css">
  .auto-style2 {
    text-align: center;
  }
  .auto-style3 {
    font-family: Verdana, Geneva, Tahoma, sans-serif;
    background-color: #FFFFFF;
    color: #FF0000;
  }
  .auto-style11 {
	border-left: 0px solid #000000;
	border-right: 0px solid #000000;
	border-top: thin solid #000000;
	border-bottom: thin solid #000000;
	font-family: Verdana, Arial, Helvetica, sans-serif;
  }

  .style1 {font-size: smaller}
  .style2 {font-size: small}
  .style3 {
	font-family: "Courier New", Courier, mono;
	font-weight: bold;
}
  .style4 {
	color: #999999;
	font-weight: bold;
}
  .style5 {
	color: #FF0000;
	font-weight: bold;
	font-size: smaller;
}
.style7 {
	color: #00FF00;
	font-weight: bold;
	font-size: smaller;
}
  .style8 {
	color: #0000FF;
	font-weight: bold;
	font-size: smaller;
}
  .style9 {color: #0066CC}
  .style10 {font-family: "Courier New", Courier, mono; font-weight: bold; color: #0066CC; }
  </style>
  </head>
  <body>
   <table style="width: auto" class="auto-style11">
     <tr>
       <td colspan="3" class="auto-style2">
   <span class="auto-style3"><a href="https://github.com/ednieuw/Arduino-ESP32-Nano-Wordclock">ESP32-Nano Flipo Clock</a></td>
     </tr>
     <tr>
       <td width="123" style="width: 108px"> <strong>A</strong> SSID</td>
       <td width="98" style="width: 98px"><strong>B</strong> Password</td>
       <td width="157" style="width: 125px"><strong>C</strong> BLE beacon</td>
     </tr>
     <tr>
       <td colspan="3"><strong>D</strong> Date <span class="auto-style4 style1">(D15012021)</span><strong>&nbsp; T</strong> Time                <span class="auto-style4 style1">(T132145)</span></td>
     </tr>
     <tr>
       <td colspan="3"><strong>E</strong> Set Timezone <span class="auto-style4 style1"> E<-02>2 or E<+01>-1</span></td>
     </tr>
      <tr>
       <td colspan="3"> <strong>F</strong> Toggle 15 seconds tick </td>
     </tr>     
     <tr>
       <td colspan="3"><strong>J</strong> Toggle NTP or time module </td>
     </tr>
    <tr>
       <td colspan="3"><strong>M</strong> Demo mode (sec) (M2)</td>
     </tr>
     <tr>
       <td colspan="3"><strong>N</strong> Display Off between Nhhhh (N2208)</td>
     </tr>
     <tr>
       <td colspan="3"><strong>O</strong> Display toggle On/Off</td>
     </tr>
     <tr>
       <td colspan="3"> <strong>P</strong> Status LED toggle On/Off </td>
     </tr>
     <tr>
       <td colspan="3"> <strong>Q</strong> Flip disc speed (ms) (Q10) </td>
     </tr>
      <tr>
       <td colspan="3"> <strong>S</strong> Toggle Seconds tick </td>
     </tr>
     <tr>
       <td colspan="3"> <div align="center" class="style10">Communication</div></td>
     </tr>
     <tr>
       <td style="width: 108px"><strong>W</strong> WIFI</td>
       <td style="width: 98px"> <strong>X</strong> NTP<span class="auto-style4"><strong>&amp;</strong></span><br class="auto-style4">       </td>
       <td style="width: 125px"><strong>Y</strong> BLE</td>
     </tr>
     <tr>
       <td style="width: 108px"><strong>R</strong> Reset</td>
       <td style="width: 98px"><strong>@</strong> Restart<br class="auto-style4">       </td>
       <td style="width: 125px"><strong>Z</strong> Fast BLE</td>
     </tr>
     <tr>
       <td style="width: 108px"><a href="https://github.com/ednieuw/FlipDiscClock/blob/main/Manual_ArduinoESP32NanoFlipo.pdf">Manual</a></td>
       <td style="width: 98px">&nbsp;</td>
       <td style="width: 125px"><a href="https://www.ednieuw.nl" class="style2">ednieuw.nl</a></td>
     </tr>
   </table>
   <form action="/get">
       <strong>     
       <input name="submit" type="submit" class="auto-style3" style="height: 22px" value="Send">
       </strong>&nbsp;
     <input type="text" name="input1" style="width: 272px"></form>
</body></html>
)rawliteral";

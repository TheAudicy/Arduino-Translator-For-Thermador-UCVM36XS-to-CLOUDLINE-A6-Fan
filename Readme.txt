Thermador's Output:
Separate wires for each speed (Low/Med/High)
Each wire goes HIGH or LOW when that speed is selected
Usually 12V or 5V signals

Perfect Setup: The UCVM36XS control panel generates 3-speed control signals that normally go to external Thermador blowers. 

Arduino will:
Intercept these 3-speed signals from the UCVM36XS
Translate them to PWM for your Cloudline A6
Act as the "external blower controller"

Next Steps for Implementation:
You need to identify the control wires that come from the UCVM36XS control panel. Based on typical installations, look for:

The Arduino code provided is designed to read these Thermador control signals and convert them to proper PWM for your Cloudline A6.

The installation manual shows exactly how the fan speed control works:

Flexible Blower Wiring (Your Cloudline A6 Setup):
ELECTRICAL PANEL WIRING BOX:
- BLACK (HIGH 4) 
- ORANGE (MED-HIGH 3)
- GRAY (MED 2)
- BLUE (LOW 1) 
- WHITE (NEUTRAL)
- RED (CAP OFF)

Remote Blower Wiring:
ELECTRICAL PANEL WIRING BOX:
- BLACK (HIGH 4)
- ORANGE (MED-HIGH 3) 
- GRAY (MED 2)
- BLUE (LOW 1, CAP OFF)
- WHITE (NEUTRAL)

The UCVM36XS sends separate wires for each speed:

Speed 1 (Low): BLUE wire
Speed 2 (Medium): GRAY wire
Speed 3 (Med-High): ORANGE wire
Speed 4 (High): BLACK wire

Arduino Implementation:
Your Arduino Nano will:

Monitor these 4 speed wires from the UCVM36XS electrical panel
Detect which speed is active (likely 120V signals when ON)
Convert to appropriate PWM for your Cloudline A6
Act as the "flexible blower" replacement

Wiring Plan:
UCVM36XS → Arduino Nano → Cloudline A6
BLUE     → Pin 3        → 
GRAY     → Pin 4        → PWM Pin 9
ORANGE   → Pin 5        →
BLACK    → Pin 6        →

You'll need voltage dividers or optoisolators to safely read the 120V control signals.

Next steps when you're ready:

Voltage dividers or optoisolators to safely read the 120V signals
Test the Arduino code with a basic PWM fan first
Then connect to your Cloudline A6
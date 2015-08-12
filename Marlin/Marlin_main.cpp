/* -*- c++ -*- */

/*
    Reprap firmware based on Sprinter and grbl.
 Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 This firmware is a mashup between Sprinter and grbl.
  (https://github.com/kliment/Sprinter)
  (https://github.com/simen/grbl/tree)

 It has preliminary support for Matthew Roberts advance algorithm
    http://reprap.org/pipermail/reprap-dev/2011-May/003323.html
 */

#include "Marlin.h"

#ifdef ENABLE_AUTO_BED_LEVELING
#include "vector_3.h"
  #ifdef AUTO_BED_LEVELING_GRID
    #include "qr_solve.h"
  #endif
#endif // ENABLE_AUTO_BED_LEVELING

#include "ultralcd.h"
#include "planner.h"
#include "stepper.h"
#include "temperature.h"
#include "motion_control.h"
#include "cardreader.h"
#include "watchdog.h"
#include "ConfigurationStore.h"
#include "language.h"
#include "pins_arduino.h"
#include "math.h"

#ifdef BLINKM
#include "BlinkM.h"
#include "Wire.h"
#endif

#if NUM_SERVOS > 0
#include "Servo.h"
#endif

#if defined(DIGIPOTSS_PIN) && DIGIPOTSS_PIN > -1
#include <SPI.h>
#endif

#define VERSION_STRING  "1.0.0"

// look here for descriptions of G-codes: http://linuxcnc.org/handbook/gcode/g-code.html
// http://objects.reprap.org/wiki/Mendel_User_Manual:_RepRapGCodes

//Implemented Codes
//-------------------
// G0  -> G1
// G1  - Coordinated Movement X Y Z E
// G2  - CW ARC
// G3  - CCW ARC
// G4  - Dwell S<seconds> or P<milliseconds>
// G10 - retract filament according to settings of M207
// G11 - retract recover filament according to settings of M208
// G28 - Home all Axis
// G29 - Detailed Z-Probe, probes the bed at 3 or more points.  Will fail if you haven't homed yet.
// G30 - Bed Probe and Delta geometry Autocalibration
// G31 - Dock sled (Z_PROBE_SLED only)
// G32 - Undock sled (Z_PROBE_SLED only)
// G90 - Use Absolute Coordinates
// G91 - Use Relative Coordinates
// G92 - Set current position to coordinates given

// M Codes
// M0   - Unconditional stop - Wait for user to press a button on the LCD (Only if ULTRA_LCD is enabled)
// M1   - Same as M0
// M17  - Enable/Power all stepper motors
// M18  - Disable all stepper motors; same as M84
// M20  - List SD card
// M21  - Init SD card
// M22  - Release SD card
// M23  - Select SD file (M23 filename.g)
// M24  - Start/resume SD print
// M25  - Pause SD print
// M26  - Set SD position in bytes (M26 S12345)
// M27  - Report SD print status
// M28  - Start SD write (M28 filename.g)
// M29  - Stop SD write
// M30  - Delete file from SD (M30 filename.g)
// M31  - Output time since last M109 or SD card start to serial
// M32  - Select file and start SD print (Can be used _while_ printing from SD card files):
//        syntax "M32 /path/filename#", or "M32 S<startpos bytes> !filename#"
//        Call gcode file : "M32 P !filename#" and return to caller file after finishing (similar to #include).
//        The '#' is necessary when calling from within sd files, as it stops buffer prereading
// M42  - Change pin status via gcode Use M42 Px Sy to set pin x to value y, when omitting Px the onboard led will be used.
// M80  - Turn on Power Supply
// M81  - Turn off Power Supply
// M82  - Set E codes absolute (default)
// M83  - Set E codes relative while in Absolute Coordinates (G90) mode
// M84  - Disable steppers until next move,
//        or use S<seconds> to specify an inactivity timeout, after which the steppers will be disabled.  S0 to disable the timeout.
// M85  - Set inactivity shutdown timer with parameter S<seconds>. To disable set zero (default)
// M92  - Set axis_steps_per_unit - same syntax as G92
// M104 - Set extruder target temp
// M105 - Read current temp
// M106 - Fan on
// M107 - Fan off
// M109 - Sxxx Wait for extruder current temp to reach target temp. Waits only when heating
//        Rxxx Wait for extruder current temp to reach target temp. Waits when heating and cooling
//        IF AUTOTEMP is enabled, S<mintemp> B<maxtemp> F<factor>. Exit autotemp by any M109 without F
// M112 - Emergency stop
// M114 - Output current position to serial port
// M115 - Capabilities string
// M117 - display message
// M119 - Output Endstop status to serial port
// M126 - Solenoid Air Valve Open (BariCUDA support by jmil)
// M127 - Solenoid Air Valve Closed (BariCUDA vent to atmospheric pressure by jmil)
// M128 - EtoP Open (BariCUDA EtoP = electricity to air pressure transducer by jmil)
// M129 - EtoP Closed (BariCUDA EtoP = electricity to air pressure transducer by jmil)
// M140 - Set bed target temp
// M150 - Set BlinkM Color Output R: Red<0-255> U(!): Green<0-255> B: Blue<0-255> over i2c, G for green does not work.
// M190 - Sxxx Wait for bed current temp to reach target temp. Waits only when heating
//        Rxxx Wait for bed current temp to reach target temp. Waits when heating and cooling
// M200 D<millimeters>- set filament diameter and set E axis units to cubic millimeters (use S0 to set back to millimeters).
// M201 - Set max acceleration in units/s^2 for print moves (M201 X1000 Y1000)
// M202 - Set max acceleration in units/s^2 for travel moves (M202 X1000 Y1000) Unused in Marlin!!
// M203 - Set maximum feedrate that your machine can sustain (M203 X200 Y200 Z300 E10000) in mm/sec
// M204 - Set default acceleration: S normal moves T filament only moves (M204 S3000 T7000) in mm/sec^2  also sets minimum segment time in ms (B20000) to prevent buffer under-runs and M20 minimum feedrate
// M205 -  advanced settings:  minimum travel speed S=while printing T=travel only,  B=minimum segment time X= maximum xy jerk, Z=maximum Z jerk, E=maximum E jerk
// M206 - set additional homing offset
// M207 - set retract length S[positive mm] F[feedrate mm/min] Z[additional zlift/hop], stays in mm regardless of M200 setting
// M208 - set recover=unretract length S[positive mm surplus to the M207 S*] F[feedrate mm/sec]
// M209 - S<1=true/0=false> enable automatic retract detect if the slicer did not support G10/11: every normal extrude-only move will be classified as retract depending on the direction.
// M218 - set hotend offset (in mm): T<extruder_number> X<offset_on_X> Y<offset_on_Y>
// M220 S<factor in percent>- set speed factor override percentage
// M221 S<factor in percent>- set extrude factor override percentage
// M226 P<pin number> S<pin state>- Wait until the specified pin reaches the state required
// M240 - Trigger a camera to take a photograph
// M250 - Set LCD contrast C<contrast value> (value 0..63)
// M280 - set servo position absolute. P: servo index, S: angle or microseconds
// M300 - Play beep sound S<frequency Hz> P<duration ms>
// M301 - Set PID parameters P I and D
// M302 - Allow cold extrudes, or set the minimum extrude S<temperature>.
// M303 - PID relay autotune S<temperature> sets the target temperature. (default target temperature = 150C)
// M304 - Set bed PID parameters P I and D
// M400 - Finish all moves
// M401 - Lower z-probe if present
// M402 - Raise z-probe if present
// M404 - N<dia in mm> Enter the nominal filament width (3mm, 1.75mm ) or will display nominal filament width without parameters
// M405 - Turn on Filament Sensor extrusion control.  Optional D<delay in cm> to set delay in centimeters between sensor and extruder
// M406 - Turn off Filament Sensor extrusion control
// M407 - Displays measured filament diameter
// M500 - stores parameters in EEPROM
// M501 - reads parameters from EEPROM (if you need reset them after you changed them temporarily).
// M502 - reverts to the default "factory settings".  You still need to store them in EEPROM afterwards if you want to.
// M503 - print the current settings (from memory not from EEPROM)
// M540 - Use S[0|1] to enable or disable the stop SD card print on endstop hit (requires ABORT_ON_ENDSTOP_HIT_FEATURE_ENABLED)
// M600 - Pause for filament change X[pos] Y[pos] Z[relative lift] E[initial retract] L[later retract distance for removal]
// M605 - Set dual x-carriage movement mode: S<mode> [ X<duplication x-offset> R<duplication temp offset> ]
// M665 - set delta configurations
// M666 - Endstop and delta geometry adjustment
// M907 - Set digital trimpot motor current using axis codes.
// M908 - Control digital trimpot directly.
// M350 - Set microstepping mode.
// M351 - Toggle MS1 MS2 pins directly.

// ************ SCARA Specific - This can change to suit future G-code regulations
// M360 - SCARA calibration: Move to cal-position ThetaA (0 deg calibration)
// M361 - SCARA calibration: Move to cal-position ThetaB (90 deg calibration - steps per degree)
// M362 - SCARA calibration: Move to cal-position PsiA (0 deg calibration)
// M363 - SCARA calibration: Move to cal-position PsiB (90 deg calibration - steps per degree)
// M364 - SCARA calibration: Move to cal-position PSIC (90 deg to Theta calibration position)
// M365 - SCARA calibration: Scaling factor, X, Y, Z axis
//************* SCARA End ***************

// M928 - Start SD logging (M928 filename.g) - ended by M29
// M999 - Restart after being stopped by error

//Stepper Movement Variables

//===========================================================================
//=============================imported variables============================
//===========================================================================


//===========================================================================
//=============================public variables=============================
//===========================================================================
#ifdef SDSUPPORT
CardReader card;
#endif
float homing_feedrate[] = HOMING_FEEDRATE;
float default_z_probe_offset[] = { X_PROBE_OFFSET_FROM_EXTRUDER, Y_PROBE_OFFSET_FROM_EXTRUDER, Z_PROBE_OFFSET_FROM_EXTRUDER };
float z_probe_offset[3];
bool axis_relative_modes[] = AXIS_RELATIVE_MODES;
int feedmultiply=100; //100->1 200->2
int saved_feedmultiply;
int extrudemultiply=100; //100->1 200->2
int extruder_multiply[EXTRUDERS] = {100
  #if EXTRUDERS > 1
    , 100
    #if EXTRUDERS > 2
      , 100
    #endif
  #endif
};
float volumetric_multiplier[EXTRUDERS] = {1.0
  #if EXTRUDERS > 1
    , 1.0
    #if EXTRUDERS > 2
      , 1.0
    #endif
  #endif
};
float current_position[NUM_AXIS] = { 0.0, 0.0, 0.0, 0.0 };
float add_homing[3]={0,0,0};
#ifdef DELTA
float endstop_adj[3]={0,0,0};
  float diagrod_adj[3]={0,0,0};
  float saved_endstop_adj[3]={0,0,0};
  float tower_adj[6]={0,0,0,0,0,0};
  float delta_radius; // = DEFAULT_delta_radius;
  float delta_diagonal_rod; // = DEFAULT_DELTA_DIAGONAL_ROD;
  float DELTA_DIAGONAL_ROD1_2;
  float DELTA_DIAGONAL_ROD2_2;
  float DELTA_DIAGONAL_ROD3_2;
  float ac_prec = AUTOCALIBRATION_PRECISION;
  float bed_radius = BED_DIAMETER / 2;
  float delta_tower1_x, delta_tower1_y;
  float delta_tower2_x, delta_tower2_y;
  float delta_tower3_x, delta_tower3_y;
  float base_max_pos[3] = {X_MAX_POS, Y_MAX_POS, Z_MAX_POS};
  float base_home_pos[3] = {X_HOME_POS, Y_HOME_POS, Z_HOME_POS};
  float max_length[3] = {X_MAX_LENGTH, Y_MAX_LENGTH, Z_MAX_LENGTH};
  float saved_position[3]={0.0,0.0,0.0};
  float saved_positions[7][3] = {
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    };
  float adj_t1_Radius = 0;
  float adj_t2_Radius = 0;
  float adj_t3_Radius = 0;
#endif

float min_pos[3] = { X_MIN_POS, Y_MIN_POS, Z_MIN_POS };
float max_pos[3] = { X_MAX_POS, Y_MAX_POS, Z_MAX_POS };
bool axis_known_position[3] = {false, false, false};
float zprobe_zoffset;

// Extruder offset
#if EXTRUDERS > 1
#ifndef DUAL_X_CARRIAGE
  #define NUM_EXTRUDER_OFFSETS 2 // only in XY plane
#else
  #define NUM_EXTRUDER_OFFSETS 3 // supports offsets in XYZ plane
#endif
float extruder_offset[NUM_EXTRUDER_OFFSETS][EXTRUDERS] = {
#if defined(EXTRUDER_OFFSET_X) && defined(EXTRUDER_OFFSET_Y)
  EXTRUDER_OFFSET_X, EXTRUDER_OFFSET_Y
#endif
};
#endif
uint8_t active_extruder = 0;
int fanSpeed=0;
#ifdef SERVO_ENDSTOPS
  int servo_endstops[] = SERVO_ENDSTOPS;
  int servo_endstop_angles[] = SERVO_ENDSTOP_ANGLES;
#endif
#ifdef BARICUDA
int ValvePressure=0;
int EtoPPressure=0;
#endif

#ifdef FWRETRACT
  bool autoretract_enabled=false;
  bool retracted[EXTRUDERS]={false
    #if EXTRUDERS > 1
    , false
     #if EXTRUDERS > 2
      , false
     #endif
  #endif
  };
  bool retracted_swap[EXTRUDERS]={false
    #if EXTRUDERS > 1
    , false
     #if EXTRUDERS > 2
      , false
     #endif
  #endif
  };

  float retract_length = RETRACT_LENGTH;
  float retract_length_swap = RETRACT_LENGTH_SWAP;
  float retract_feedrate = RETRACT_FEEDRATE;
  float retract_zlift = RETRACT_ZLIFT;
  float retract_recover_length = RETRACT_RECOVER_LENGTH;
  float retract_recover_length_swap = RETRACT_RECOVER_LENGTH_SWAP;
  float retract_recover_feedrate = RETRACT_RECOVER_FEEDRATE;
#endif

#ifdef ULTIPANEL
  #ifdef PS_DEFAULT_OFF
    bool powersupply = false;
  #else
	  bool powersupply = true;
  #endif
#endif

#ifdef DELTA
  float delta[3] = {0.0, 0.0, 0.0};
  float delta_tmp[3] = {0.0, 0.0, 0.0};
  #define SIN_60 0.8660254037844386
  #define COS_60 0.5
  // these are the default values, can be overriden with M665
  // float delta_diagonal_rod_2= sq(delta_diagonal_rod);  //**PJR - this is not now needed
  float delta_segments_per_second= DELTA_SEGMENTS_PER_SECOND;
#endif

#ifdef NONLINEAR_BED_LEVELING
float bed_level[AUTO_BED_LEVELING_GRID_POINTS][AUTO_BED_LEVELING_GRID_POINTS];
#endif //NONLINEAR_BED_LEVELING
#ifdef SCARA                              // Build size scaling
float axis_scaling[3]={1,1,1};  // Build size scaling, default to 1
#endif

bool cancel_heatup = false ;

#ifdef FILAMENT_SENSOR
  //Variables for Filament Sensor input
  float filament_width_nominal=DEFAULT_NOMINAL_FILAMENT_DIA;  //Set nominal filament width, can be changed with M404
  bool filament_sensor=false;  //M405 turns on filament_sensor control, M406 turns it off
  float filament_width_meas=DEFAULT_MEASURED_FILAMENT_DIA; //Stores the measured filament diameter
  signed char measurement_delay[MAX_MEASUREMENT_DELAY+1];  //ring buffer to delay measurement  store extruder factor after subtracting 100
  int delay_index1=0;  //index into ring buffer
  int delay_index2=-1;  //index into ring buffer - set to -1 on startup to indicate ring buffer needs to be initialized
  float delay_dist=0; //delay distance counter
  int meas_delay_cm = MEASUREMENT_DELAY_CM;  //distance delay setting
#endif

//===========================================================================
//=============================Private Variables=============================
//===========================================================================
const char axis_codes[NUM_AXIS] = {'X', 'Y', 'Z', 'E'};
static float destination[NUM_AXIS] = {  0.0, 0.0, 0.0, 0.0};

#ifndef DELTA
static float delta[3] = {0.0, 0.0, 0.0};
#endif

static float offset[3] = {0.0, 0.0, 0.0};
static bool home_all_axis = true;
static float feedrate = 1500.0, next_feedrate, saved_feedrate, z_offset;
static float bed_level_c, bed_level_x, bed_level_y, bed_level_z;
static float bed_safe_z = 50; //45; //used for inital bed probe safe distance (to avoid crashing into bed)
static float bed_level_ox, bed_level_oy, bed_level_oz;
static long gcode_N, gcode_LastN, Stopped_gcode_LastN = 0;
static int loopcount;
static bool relative_mode = false;  //Determines Absolute or Relative Coordinates

static char cmdbuffer[BUFSIZE][MAX_CMD_SIZE];
static bool fromsd[BUFSIZE];
static int bufindr = 0;
static int bufindw = 0;
static int buflen = 0;
//static int i = 0;
static char serial_char;
static int serial_count = 0;
static boolean comment_mode = false;
static char *strchr_pointer; // just a pointer to find chars in the command string like X, Y, Z, E, etc

const int sensitive_pins[] = SENSITIVE_PINS; // Sensitive pin list for M42

//static float tt = 0;
//static float bt = 0;

//Inactivity shutdown variables
static unsigned long previous_millis_cmd = 0;
static unsigned long max_inactive_time = 0;
static unsigned long stepper_inactive_time = DEFAULT_STEPPER_DEACTIVE_TIME*1000l;

unsigned long starttime=0;
unsigned long stoptime=0;

static uint8_t tmp_extruder;


bool Stopped=false;

#if NUM_SERVOS > 0
  Servo servos[NUM_SERVOS];
#endif

bool CooldownNoWait = true;
bool target_direction;

//Insert variables if CHDK is defined
#ifdef CHDK
unsigned long chdkHigh = 0;
boolean chdkActive = false;
#endif

//===========================================================================
//=============================Routines======================================
//===========================================================================

void get_arc_coordinates();
bool setTargetedHotend(int code);

void serial_echopair_P(const char *s_P, float v)
    { serialprintPGM(s_P); SERIAL_ECHO(v); }
void serial_echopair_P(const char *s_P, double v)
    { serialprintPGM(s_P); SERIAL_ECHO(v); }
void serial_echopair_P(const char *s_P, unsigned long v)
    { serialprintPGM(s_P); SERIAL_ECHO(v); }

#ifdef SDSUPPORT
  #include "SdFatUtil.h"
  int freeMemory() { return SdFatUtil::FreeRam(); }
#else
  extern "C" {
    extern unsigned int __bss_end;
    extern unsigned int __heap_start;
    extern void *__brkval;

    int freeMemory() {
      int free_memory;

      if ((int)__brkval == 0)
        free_memory = ((int)&free_memory) - ((int)&__bss_end);
      else
        free_memory = ((int)&free_memory) - ((int)__brkval);

      return free_memory;
    }
  }
#endif //!SDSUPPORT

//adds an command to the main command buffer
//thats really done in a non-safe way.
//needs overworking someday
void enquecommand(const char *cmd)
{
  if(buflen < BUFSIZE)
  {
    //this is dangerous if a mixing of serial and this happens
    strcpy(&(cmdbuffer[bufindw][0]),cmd);
    SERIAL_ECHO_START;
    SERIAL_ECHOPGM(MSG_Enqueing);
    SERIAL_ECHO(cmdbuffer[bufindw]);
    SERIAL_ECHOLNPGM("\"");
    bufindw= (bufindw + 1)%BUFSIZE;
    buflen += 1;
  }
}

void enquecommand_P(const char *cmd)
{
  if(buflen < BUFSIZE)
  {
    //this is dangerous if a mixing of serial and this happens
    strcpy_P(&(cmdbuffer[bufindw][0]),cmd);
    SERIAL_ECHO_START;
    SERIAL_ECHOPGM(MSG_Enqueing);
    SERIAL_ECHO(cmdbuffer[bufindw]);
    SERIAL_ECHOLNPGM("\"");
    bufindw= (bufindw + 1)%BUFSIZE;
    buflen += 1;
  }
}

void setup_killpin()
{
  #if defined(KILL_PIN) && KILL_PIN > -1
    pinMode(KILL_PIN,INPUT);
    WRITE(KILL_PIN,HIGH);
  #endif
}

void setup_photpin()
{
  #if defined(PHOTOGRAPH_PIN) && PHOTOGRAPH_PIN > -1
    SET_OUTPUT(PHOTOGRAPH_PIN);
    WRITE(PHOTOGRAPH_PIN, LOW);
  #endif
}

void setup_powerhold()
{
  #if defined(SUICIDE_PIN) && SUICIDE_PIN > -1
    SET_OUTPUT(SUICIDE_PIN);
    WRITE(SUICIDE_PIN, HIGH);
  #endif
  #if defined(PS_ON_PIN) && PS_ON_PIN > -1
    SET_OUTPUT(PS_ON_PIN);
	#if defined(PS_DEFAULT_OFF)
	  WRITE(PS_ON_PIN, PS_ON_ASLEEP);
    #else
	  WRITE(PS_ON_PIN, PS_ON_AWAKE);
	#endif
  #endif
}

void suicide()
{
  #if defined(SUICIDE_PIN) && SUICIDE_PIN > -1
    SET_OUTPUT(SUICIDE_PIN);
    WRITE(SUICIDE_PIN, LOW);
  #endif
}

void servo_init()
{
  #if (NUM_SERVOS >= 1) && defined(SERVO0_PIN) && (SERVO0_PIN > -1)
    servos[0].attach(SERVO0_PIN);
  #endif
  #if (NUM_SERVOS >= 2) && defined(SERVO1_PIN) && (SERVO1_PIN > -1)
    servos[1].attach(SERVO1_PIN);
  #endif
  #if (NUM_SERVOS >= 3) && defined(SERVO2_PIN) && (SERVO2_PIN > -1)
    servos[2].attach(SERVO2_PIN);
  #endif
  #if (NUM_SERVOS >= 4) && defined(SERVO3_PIN) && (SERVO3_PIN > -1)
    servos[3].attach(SERVO3_PIN);
  #endif
  #if (NUM_SERVOS >= 5)
    #error "TODO: enter initalisation code for more servos"
  #endif

  // Set position of Servo Endstops that are defined
  #ifdef SERVO_ENDSTOPS
  for(int8_t i = 0; i < 3; i++)
  {
    if(servo_endstops[i] > -1) {
      servos[servo_endstops[i]].write(servo_endstop_angles[i * 2 + 1]);
    }
  }
  #endif

  #if defined (ENABLE_AUTO_BED_LEVELING) && (PROBE_SERVO_DEACTIVATION_DELAY > 0)
  delay(PROBE_SERVO_DEACTIVATION_DELAY);
  servos[servo_endstops[Z_AXIS]].detach();
  #endif
}


void setup()
{
  setup_killpin();
  setup_powerhold();
  MYSERIAL.begin(BAUDRATE);
  SERIAL_PROTOCOLLNPGM("start");
  SERIAL_ECHO_START;

  // Check startup - does nothing if bootloader sets MCUSR to 0
  byte mcu = MCUSR;
  if(mcu & 1) SERIAL_ECHOLNPGM(MSG_POWERUP);
  if(mcu & 2) SERIAL_ECHOLNPGM(MSG_EXTERNAL_RESET);
  if(mcu & 4) SERIAL_ECHOLNPGM(MSG_BROWNOUT_RESET);
  if(mcu & 8) SERIAL_ECHOLNPGM(MSG_WATCHDOG_RESET);
  if(mcu & 32) SERIAL_ECHOLNPGM(MSG_SOFTWARE_RESET);
  MCUSR=0;

  SERIAL_ECHOPGM(MSG_MARLIN);
  SERIAL_ECHOLNPGM(VERSION_STRING);
  #ifdef STRING_VERSION_CONFIG_H
    #ifdef STRING_CONFIG_H_AUTHOR
      SERIAL_ECHO_START;
      SERIAL_ECHOPGM(MSG_CONFIGURATION_VER);
      SERIAL_ECHOPGM(STRING_VERSION_CONFIG_H);
      SERIAL_ECHOPGM(MSG_AUTHOR);
      SERIAL_ECHOLNPGM(STRING_CONFIG_H_AUTHOR);
      SERIAL_ECHOPGM("Compiled: ");
      SERIAL_ECHOLNPGM(__DATE__);
    #endif
  #endif
  SERIAL_ECHO_START;
  SERIAL_ECHOPGM(MSG_FREE_MEMORY);
  SERIAL_ECHO(freeMemory());
  SERIAL_ECHOPGM(MSG_PLANNER_BUFFER_BYTES);
  SERIAL_ECHOLN((int)sizeof(block_t)*BLOCK_BUFFER_SIZE);
  for(int8_t i = 0; i < BUFSIZE; i++)
  {
    fromsd[i] = false;
  }

  // loads data from EEPROM if available else uses defaults (and resets step acceleration rate)
  Config_RetrieveSettings();

  tp_init();    // Initialize temperature loop
  plan_init();  // Initialize planner;
  watchdog_init();
  st_init();    // Initialize stepper, this enables interrupts!
  setup_photpin();
  servo_init();


  lcd_init();
  _delay_ms(1000);	// wait 1sec to display the splash screen

  #if defined(CONTROLLERFAN_PIN) && CONTROLLERFAN_PIN > -1
    SET_OUTPUT(CONTROLLERFAN_PIN); //Set pin used for driver cooling fan
  #endif

  #ifdef DIGIPOT_I2C
    digipot_i2c_init();
  #endif
#ifdef Z_PROBE_SLED
  pinMode(SERVO0_PIN, OUTPUT);
  digitalWrite(SERVO0_PIN, LOW); // turn it off
#endif // Z_PROBE_SLED
}


void loop()
{
  if(buflen < (BUFSIZE-1))
    get_command();
  #ifdef SDSUPPORT
  card.checkautostart(false);
  #endif
  if(buflen)
  {
    #ifdef SDSUPPORT
      if(card.saving)
      {
        if(strstr_P(cmdbuffer[bufindr], PSTR("M29")) == NULL)
        {
          card.write_command(cmdbuffer[bufindr]);
          if(card.logging)
          {
            process_commands();
          }
          else
          {
            SERIAL_PROTOCOLLNPGM(MSG_OK);
          }
        }
        else
        {
          card.closefile();
          SERIAL_PROTOCOLLNPGM(MSG_FILE_SAVED);
        }
      }
      else
      {
        process_commands();
      }
    #else
      process_commands();
    #endif //SDSUPPORT
    buflen = (buflen-1);
    bufindr = (bufindr + 1)%BUFSIZE;
  }
  //check heater every n milliseconds
  manage_heater();
  manage_inactivity();
  checkHitEndstops();
  lcd_update();
}

void get_command()
{
  while( MYSERIAL.available() > 0  && buflen < BUFSIZE) {
    serial_char = MYSERIAL.read();
    if(serial_char == '\n' ||
       serial_char == '\r' ||
       (serial_char == ':' && comment_mode == false) ||
       serial_count >= (MAX_CMD_SIZE - 1) )
    {
      if(!serial_count) { //if empty line
        comment_mode = false; //for new command
        return;
      }
      cmdbuffer[bufindw][serial_count] = 0; //terminate string
      if(!comment_mode){
        comment_mode = false; //for new command
        fromsd[bufindw] = false;
        if(strchr(cmdbuffer[bufindw], 'N') != NULL)
        {
          strchr_pointer = strchr(cmdbuffer[bufindw], 'N');
          gcode_N = (strtol(&cmdbuffer[bufindw][strchr_pointer - cmdbuffer[bufindw] + 1], NULL, 10));
          if(gcode_N != gcode_LastN+1 && (strstr_P(cmdbuffer[bufindw], PSTR("M110")) == NULL) ) {
            SERIAL_ERROR_START;
            SERIAL_ERRORPGM(MSG_ERR_LINE_NO);
            SERIAL_ERRORLN(gcode_LastN);
            //Serial.println(gcode_N);
            FlushSerialRequestResend();
            serial_count = 0;
            return;
          }

          if(strchr(cmdbuffer[bufindw], '*') != NULL)
          {
            byte checksum = 0;
            byte count = 0;
            while(cmdbuffer[bufindw][count] != '*') checksum = checksum^cmdbuffer[bufindw][count++];
            strchr_pointer = strchr(cmdbuffer[bufindw], '*');

            if( (int)(strtod(&cmdbuffer[bufindw][strchr_pointer - cmdbuffer[bufindw] + 1], NULL)) != checksum) {
              SERIAL_ERROR_START;
              SERIAL_ERRORPGM(MSG_ERR_CHECKSUM_MISMATCH);
              SERIAL_ERRORLN(gcode_LastN);
              FlushSerialRequestResend();
              serial_count = 0;
              return;
            }
            //if no errors, continue parsing
          }
          else
          {
            SERIAL_ERROR_START;
            SERIAL_ERRORPGM(MSG_ERR_NO_CHECKSUM);
            SERIAL_ERRORLN(gcode_LastN);
            FlushSerialRequestResend();
            serial_count = 0;
            return;
          }

          gcode_LastN = gcode_N;
          //if no errors, continue parsing
        }
        else  // if we don't receive 'N' but still see '*'
        {
          if((strchr(cmdbuffer[bufindw], '*') != NULL))
          {
            SERIAL_ERROR_START;
            SERIAL_ERRORPGM(MSG_ERR_NO_LINENUMBER_WITH_CHECKSUM);
            SERIAL_ERRORLN(gcode_LastN);
            serial_count = 0;
            return;
          }
        }
        if((strchr(cmdbuffer[bufindw], 'G') != NULL)){
          strchr_pointer = strchr(cmdbuffer[bufindw], 'G');
          switch((int)((strtod(&cmdbuffer[bufindw][strchr_pointer - cmdbuffer[bufindw] + 1], NULL)))){
          case 0:
          case 1:
          case 2:
          case 3:
            if (Stopped == true) {
              SERIAL_ERRORLNPGM(MSG_ERR_STOPPED);
              LCD_MESSAGEPGM(MSG_STOPPED);
            }
            break;
          default:
            break;
          }

        }

        //If command was e-stop process now
        if(strcmp(cmdbuffer[bufindw], "M112") == 0)
          kill();

        bufindw = (bufindw + 1)%BUFSIZE;
        buflen += 1;
      }
      serial_count = 0; //clear buffer
    }
    else
    {
      if(serial_char == ';') comment_mode = true;
      if(!comment_mode) cmdbuffer[bufindw][serial_count++] = serial_char;
    }
  }
  #ifdef SDSUPPORT
  if(!card.sdprinting || serial_count!=0){
    return;
  }

  //'#' stops reading from SD to the buffer prematurely, so procedural macro calls are possible
  // if it occurs, stop_buffering is triggered and the buffer is ran dry.
  // this character _can_ occur in serial com, due to checksums. however, no checksums are used in SD printing

  static bool stop_buffering=false;
  if(buflen==0) stop_buffering=false;

  while( !card.eof()  && buflen < BUFSIZE && !stop_buffering) {
    int16_t n=card.get();
    serial_char = (char)n;
    if(serial_char == '\n' ||
       serial_char == '\r' ||
       (serial_char == '#' && comment_mode == false) ||
       (serial_char == ':' && comment_mode == false) ||
       serial_count >= (MAX_CMD_SIZE - 1)||n==-1)
    {
      if(card.eof()){
        SERIAL_PROTOCOLLNPGM(MSG_FILE_PRINTED);
        stoptime=millis();
        char time[30];
        unsigned long t=(stoptime-starttime)/1000;
        int hours, minutes;
        minutes=(t/60)%60;
        hours=t/60/60;
        sprintf_P(time, PSTR("%i hours %i minutes"),hours, minutes);
        SERIAL_ECHO_START;
        SERIAL_ECHOLN(time);
        lcd_setstatus(time);
        card.printingHasFinished();
        card.checkautostart(true);

      }
      if(serial_char=='#')
        stop_buffering=true;

      if(!serial_count)
      {
        comment_mode = false; //for new command
        return; //if empty line
      }
      cmdbuffer[bufindw][serial_count] = 0; //terminate string
//      if(!comment_mode){
        fromsd[bufindw] = true;
        buflen += 1;
        bufindw = (bufindw + 1)%BUFSIZE;
//      }
      comment_mode = false; //for new command
      serial_count = 0; //clear buffer
    }
    else
    {
      if(serial_char == ';') comment_mode = true;
      if(!comment_mode) cmdbuffer[bufindw][serial_count++] = serial_char;
    }
  }

  #endif //SDSUPPORT

}


float code_value()
{
  return (strtod(&cmdbuffer[bufindr][strchr_pointer - cmdbuffer[bufindr] + 1], NULL));
}

long code_value_long()
{
  return (strtol(&cmdbuffer[bufindr][strchr_pointer - cmdbuffer[bufindr] + 1], NULL, 10));
}

bool code_seen(char code)
{
  strchr_pointer = strchr(cmdbuffer[bufindr], code);
  return (strchr_pointer != NULL);  //Return True if a character was found
}

#define DEFINE_PGM_READ_ANY(type, reader)       \
    static inline type pgm_read_any(const type *p)  \
    { return pgm_read_##reader##_near(p); }

DEFINE_PGM_READ_ANY(float,       float);
DEFINE_PGM_READ_ANY(signed char, byte);

#define XYZ_CONSTS_FROM_CONFIG(type, array, CONFIG) \
static const PROGMEM type array##_P[3] =        \
    { X_##CONFIG, Y_##CONFIG, Z_##CONFIG };     \
static inline type array(int axis)          \
    { return pgm_read_any(&array##_P[axis]); }

XYZ_CONSTS_FROM_CONFIG(float, base_min_pos,    MIN_POS);
//XYZ_CONSTS_FROM_CONFIG(float, base_max_pos,    MAX_POS);
//XYZ_CONSTS_FROM_CONFIG(float, base_home_pos,   HOME_POS);
//XYZ_CONSTS_FROM_CONFIG(float, max_length,      MAX_LENGTH);
XYZ_CONSTS_FROM_CONFIG(float, home_retract_mm, HOME_RETRACT_MM);
XYZ_CONSTS_FROM_CONFIG(signed char, home_dir,  HOME_DIR);

#ifdef DUAL_X_CARRIAGE
  #if EXTRUDERS == 1 || defined(COREXY) \
      || !defined(X2_ENABLE_PIN) || !defined(X2_STEP_PIN) || !defined(X2_DIR_PIN) \
      || !defined(X2_HOME_POS) || !defined(X2_MIN_POS) || !defined(X2_MAX_POS) \
      || !defined(X_MAX_PIN) || X_MAX_PIN < 0
    #error "Missing or invalid definitions for DUAL_X_CARRIAGE mode."
  #endif
  #if X_HOME_DIR != -1 || X2_HOME_DIR != 1
    #error "Please use canonical x-carriage assignment" // the x-carriages are defined by their homing directions
  #endif

#define DXC_FULL_CONTROL_MODE 0
#define DXC_AUTO_PARK_MODE    1
#define DXC_DUPLICATION_MODE  2
static int dual_x_carriage_mode = DEFAULT_DUAL_X_CARRIAGE_MODE;

static float x_home_pos(int extruder) {
  if (extruder == 0)
    return base_home_pos[X_AXIS] + add_homing[X_AXIS];
  else
    // In dual carriage mode the extruder offset provides an override of the
    // second X-carriage offset when homed - otherwise X2_HOME_POS is used.
    // This allow soft recalibration of the second extruder offset position without firmware reflash
    // (through the M218 command).
    return (extruder_offset[X_AXIS][1] > 0) ? extruder_offset[X_AXIS][1] : X2_HOME_POS;
}

static int x_home_dir(int extruder) {
  return (extruder == 0) ? X_HOME_DIR : X2_HOME_DIR;
}

static float inactive_extruder_x_pos = X2_MAX_POS; // used in mode 0 & 1
static bool active_extruder_parked = false; // used in mode 1 & 2
static float raised_parked_position[NUM_AXIS]; // used in mode 1
static unsigned long delayed_move_time = 0; // used in mode 1
static float duplicate_extruder_x_offset = DEFAULT_DUPLICATION_X_OFFSET; // used in mode 2
static float duplicate_extruder_temp_offset = 0; // used in mode 2
bool extruder_duplication_enabled = false; // used in mode 2
#endif //DUAL_X_CARRIAGE

static void axis_is_at_home(int axis) {
#ifdef DUAL_X_CARRIAGE
  if (axis == X_AXIS) {
    if (active_extruder != 0) {
      current_position[X_AXIS] = x_home_pos(active_extruder);
      min_pos[X_AXIS] =          X2_MIN_POS;
      max_pos[X_AXIS] =          max(extruder_offset[X_AXIS][1], X2_MAX_POS);
      return;
    }
    else if (dual_x_carriage_mode == DXC_DUPLICATION_MODE && active_extruder == 0) {
      current_position[X_AXIS] = base_home_pos[X_AXIS] + add_homing[X_AXIS];
      min_pos[X_AXIS] =          base_min_pos(X_AXIS) + add_homing[X_AXIS];
      max_pos[X_AXIS] =          min(base_max_pos[X_AXIS] + add_homing[X_AXIS],
                                  max(extruder_offset[X_AXIS][1], X2_MAX_POS) - duplicate_extruder_x_offset);
      return;
    }
  }
#endif
#ifdef SCARA
   float homeposition[3];
   char i;

   if (axis < 2)
   {

     for (i=0; i<3; i++)
     {
        homeposition[i] = base_home_pos[i];
     }
	// SERIAL_ECHOPGM("homeposition[x]= "); SERIAL_ECHO(homeposition[0]);
   //  SERIAL_ECHOPGM("homeposition[y]= "); SERIAL_ECHOLN(homeposition[1]);
   // Works out real Homeposition angles using inverse kinematics,
   // and calculates homing offset using forward kinematics
     calculate_delta(homeposition);

    // SERIAL_ECHOPGM("base Theta= "); SERIAL_ECHO(delta[X_AXIS]);
    // SERIAL_ECHOPGM(" base Psi+Theta="); SERIAL_ECHOLN(delta[Y_AXIS]);

     for (i=0; i<2; i++)
     {
        delta[i] -= add_homing[i];
     }

    // SERIAL_ECHOPGM("addhome X="); SERIAL_ECHO(add_homing[X_AXIS]);
	// SERIAL_ECHOPGM(" addhome Y="); SERIAL_ECHO(add_homing[Y_AXIS]);
    // SERIAL_ECHOPGM(" addhome Theta="); SERIAL_ECHO(delta[X_AXIS]);
    // SERIAL_ECHOPGM(" addhome Psi+Theta="); SERIAL_ECHOLN(delta[Y_AXIS]);

     calculate_SCARA_forward_Transform(delta);

    // SERIAL_ECHOPGM("Delta X="); SERIAL_ECHO(delta[X_AXIS]);
    // SERIAL_ECHOPGM(" Delta Y="); SERIAL_ECHOLN(delta[Y_AXIS]);

    current_position[axis] = delta[axis];

    // SCARA home positions are based on configuration since the actual limits are determined by the
    // inverse kinematic transform.
    min_pos[axis] =          base_min_pos(axis); // + (delta[axis] - base_home_pos[axis]);
    max_pos[axis] =          base_max_pos[axis]; // + (delta[axis] - base_home_pos[axis]);
   }
   else
   {
      current_position[axis] = base_home_pos[axis] + add_homing[axis];
      min_pos[axis] =          base_min_pos(axis) + add_homing[axis];
      max_pos[axis] =          base_max_pos[axis] + add_homing[axis];
   }
#else
  current_position[axis] = base_home_pos[axis] + add_homing[axis];
  min_pos[axis] =          base_min_pos(axis) + add_homing[axis];
  max_pos[axis] =          base_max_pos[axis] + add_homing[axis];
#endif
}

#ifdef ENABLE_AUTO_BED_LEVELING
#ifdef AUTO_BED_LEVELING_GRID
static void set_bed_level_equation_lsq(double *plane_equation_coefficients)
{
    vector_3 planeNormal = vector_3(-plane_equation_coefficients[0], -plane_equation_coefficients[1], 1);
    planeNormal.debug("planeNormal");
    plan_bed_level_matrix = matrix_3x3::create_look_at(planeNormal);
    //bedLevel.debug("bedLevel");

    //plan_bed_level_matrix.debug("bed level before");
    //vector_3 uncorrected_position = plan_get_position_mm();
    //uncorrected_position.debug("position before");

    vector_3 corrected_position = plan_get_position();
//    corrected_position.debug("position after");
    current_position[X_AXIS] = corrected_position.x;
    current_position[Y_AXIS] = corrected_position.y;
    current_position[Z_AXIS] = corrected_position.z;

    // put the bed at 0 so we don't go below it.
    current_position[Z_AXIS] = zprobe_zoffset; // in the lsq we reach here after raising the extruder due to the loop structure

    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
}

#else // not AUTO_BED_LEVELING_GRID

static void set_bed_level_equation_3pts(float z_at_pt_1, float z_at_pt_2, float z_at_pt_3) {

    plan_bed_level_matrix.set_to_identity();

    vector_3 pt1 = vector_3(ABL_PROBE_PT_1_X, ABL_PROBE_PT_1_Y, z_at_pt_1);
    vector_3 pt2 = vector_3(ABL_PROBE_PT_2_X, ABL_PROBE_PT_2_Y, z_at_pt_2);
    vector_3 pt3 = vector_3(ABL_PROBE_PT_3_X, ABL_PROBE_PT_3_Y, z_at_pt_3);

    vector_3 from_2_to_1 = (pt1 - pt2).get_normal();
    vector_3 from_2_to_3 = (pt3 - pt2).get_normal();
    vector_3 planeNormal = vector_3::cross(from_2_to_1, from_2_to_3).get_normal();
    planeNormal = vector_3(planeNormal.x, planeNormal.y, abs(planeNormal.z));

    plan_bed_level_matrix = matrix_3x3::create_look_at(planeNormal);

    vector_3 corrected_position = plan_get_position();
    current_position[X_AXIS] = corrected_position.x;
    current_position[Y_AXIS] = corrected_position.y;
    current_position[Z_AXIS] = corrected_position.z;

    // put the bed at 0 so we don't go below it.
    current_position[Z_AXIS] = zprobe_zoffset;

    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);

}

#endif // AUTO_BED_LEVELING_GRID

static void run_z_probe() {
    plan_bed_level_matrix.set_to_identity();

#ifdef DELTA
    enable_endstops(true);
    float start_z = current_position[Z_AXIS];
    long start_steps = st_get_position(Z_AXIS);

    //feedrate = homing_feedrate[Z_AXIS]/10;
    feedrate = AUTOCAL_PROBERATE *60;
    destination[Z_AXIS] = -10;
    prepare_move_raw();
    st_synchronize();
    endstops_hit_on_purpose();

    enable_endstops(false);
    long stop_steps = st_get_position(Z_AXIS);

    float mm = start_z - float(start_steps - stop_steps) / axis_steps_per_unit[Z_AXIS];
    current_position[Z_AXIS] = mm;
    calculate_delta(current_position);
    plan_set_position(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS]);

    //**PJR - Save tower carriage positions for G30 diagnostic reports
    for(int8_t i=0; i < 3; i++) {
      saved_position[i] = st_get_position_mm(i);
    }
#else
    feedrate = homing_feedrate[Z_AXIS];

    // move down until you find the bed
    float zPosition = -10;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], zPosition, current_position[E_AXIS], feedrate/60, active_extruder);
    st_synchronize();

        // we have to let the planner know where we are right now as it is not where we said to go.
    zPosition = st_get_position_mm(Z_AXIS);
    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], zPosition, current_position[E_AXIS]);

    // move up the retract distance
    zPosition += home_retract_mm(Z_AXIS);
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], zPosition, current_position[E_AXIS], feedrate/60, active_extruder);
    st_synchronize();

    // move back down slowly to find bed
    feedrate = homing_feedrate[Z_AXIS]/4;
    zPosition -= home_retract_mm(Z_AXIS) * 2;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], zPosition, current_position[E_AXIS], feedrate/60, active_extruder);
    st_synchronize();

    current_position[Z_AXIS] = st_get_position_mm(Z_AXIS);
    // make sure the planner knows where we are as it may be a bit different than we last said to move to
    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
#endif
}

static void do_blocking_move_to(float x, float y, float z) {
    float oldFeedRate = feedrate;

    feedrate = XY_TRAVEL_SPEED;

#ifdef DELTA
    destination[X_AXIS] = x;
    destination[Y_AXIS] = y;
    destination[Z_AXIS] = z;
    prepare_move_raw();
#else  // cartesian
    current_position[X_AXIS] = x;
    current_position[Y_AXIS] = y;
    current_position[Z_AXIS] = z;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate/60, active_extruder);
#endif //DELTA
    st_synchronize();

    feedrate = oldFeedRate;
}

static void do_blocking_move_relative(float offset_x, float offset_y, float offset_z) {
    do_blocking_move_to(current_position[X_AXIS] + offset_x, current_position[Y_AXIS] + offset_y, current_position[Z_AXIS] + offset_z);
}

// **PJR - Do a blocking cartesian (delta segmented if appropriate) move to specified location at XY_TRAVEL_SPEED.
// NB this will respect bed level corrections if enabled and not cleared
static void do_blocking_move_cartesian(float x, float y, float z) {
    float oldFeedRate = feedrate;

    feedrate = XY_TRAVEL_SPEED;

#ifdef DELTA
    destination[X_AXIS] = x;
    destination[Y_AXIS] = y;
    destination[Z_AXIS] = z;
    prepare_move();
#else  // cartesian
    current_position[X_AXIS] = x;
    current_position[Y_AXIS] = y;
    current_position[Z_AXIS] = z;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate/60, active_extruder);
#endif //DELTA
    st_synchronize();

    feedrate = oldFeedRate;
}

static void setup_for_endstop_move() {
    saved_feedrate = feedrate;
    saved_feedmultiply = feedmultiply;
    feedmultiply = 100;
    previous_millis_cmd = millis();

#ifndef DELTA
    enable_endstops(true);
#endif //Delta printers enable endstops only during Z probe down move.
}

static void clean_up_after_endstop_move() {
#ifdef ENDSTOPS_ONLY_FOR_HOMING
    enable_endstops(false);
#endif

    feedrate = saved_feedrate;
    feedmultiply = saved_feedmultiply;
    previous_millis_cmd = millis();
}

static void engage_z_probe() {
    // Engage Z Servo endstop if enabled
    #ifdef SERVO_ENDSTOPS
    if (servo_endstops[Z_AXIS] > -1) {
#if defined (ENABLE_AUTO_BED_LEVELING) && (PROBE_SERVO_DEACTIVATION_DELAY > 0)
        servos[servo_endstops[Z_AXIS]].attach(0);
#endif
        servos[servo_endstops[Z_AXIS]].write(servo_endstop_angles[Z_AXIS * 2]);
#if defined (ENABLE_AUTO_BED_LEVELING) && (PROBE_SERVO_DEACTIVATION_DELAY > 0)
        delay(PROBE_SERVO_DEACTIVATION_DELAY);
        servos[servo_endstops[Z_AXIS]].detach();
#endif
    }
    #else // Deploy the Z probe, most likely by touching one of the arms, no servo needed.

    #if defined(TOUCH_PROBE_DEPLOY_1_X) && defined(TOUCH_PROBE_DEPLOY_1_Y) && defined(TOUCH_PROBE_DEPLOY_1_Z)
      destination[X_AXIS] = TOUCH_PROBE_DEPLOY_1_X;
      destination[Y_AXIS] = TOUCH_PROBE_DEPLOY_1_Y;
      destination[Z_AXIS] = TOUCH_PROBE_DEPLOY_1_Z;
      feedrate = TOUCH_PROBE_DEPLOY_1_FEEDRATE;
      prepare_move_raw();
    #endif

    #if defined(TOUCH_PROBE_DEPLOY_2_X) && defined(TOUCH_PROBE_DEPLOY_2_Y) && defined(TOUCH_PROBE_DEPLOY_2_Z)
      destination[X_AXIS] = TOUCH_PROBE_DEPLOY_2_X;
      destination[Y_AXIS] = TOUCH_PROBE_DEPLOY_2_Y;
      destination[Z_AXIS] = TOUCH_PROBE_DEPLOY_2_Z;
      feedrate = TOUCH_PROBE_DEPLOY_2_FEEDRATE;
      prepare_move_raw();
    #endif

    #if defined(TOUCH_PROBE_DEPLOY_3_X) && defined(TOUCH_PROBE_DEPLOY_3_Y) && defined(TOUCH_PROBE_DEPLOY_3_Z)
      destination[X_AXIS] = TOUCH_PROBE_DEPLOY_3_X;
      destination[Y_AXIS] = TOUCH_PROBE_DEPLOY_3_Y;
      destination[Z_AXIS] = TOUCH_PROBE_DEPLOY_3_Z;
      feedrate = TOUCH_PROBE_DEPLOY_3_FEEDRATE;
      prepare_move_raw();
    #endif

    st_synchronize();
    #endif //SERVO_ENDSTOPS
}

static void retract_z_probe() {
    // Retract Z Servo endstop if enabled
    #ifdef SERVO_ENDSTOPS
    if (servo_endstops[Z_AXIS] > -1) {
#if defined (ENABLE_AUTO_BED_LEVELING) && (PROBE_SERVO_DEACTIVATION_DELAY > 0)
        servos[servo_endstops[Z_AXIS]].attach(0);
#endif
        servos[servo_endstops[Z_AXIS]].write(servo_endstop_angles[Z_AXIS * 2 + 1]);
#if defined (ENABLE_AUTO_BED_LEVELING) && (PROBE_SERVO_DEACTIVATION_DELAY > 0)
        delay(PROBE_SERVO_DEACTIVATION_DELAY);
        servos[servo_endstops[Z_AXIS]].detach();
#endif
    }
    #else // Push up the Z probe by moving the end effector, no servo needed.

    // Put the end effector high enough up that it won't crash the head when moving to TOUCH_PROBE_RETRACT_1_*
    // @todo: check that we're not already too high before moving up
    feedrate = homing_feedrate[X_AXIS];
    destination[Z_AXIS] = current_position[Z_AXIS] + 100;
    prepare_move_raw();

    #if defined(TOUCH_PROBE_RETRACT_1_X) && defined(TOUCH_PROBE_RETRACT_1_Y) && defined(TOUCH_PROBE_RETRACT_1_Z)
      destination[X_AXIS] = TOUCH_PROBE_RETRACT_1_X;
      destination[Y_AXIS] = TOUCH_PROBE_RETRACT_1_Y;
      destination[Z_AXIS] = TOUCH_PROBE_RETRACT_1_Z;
      feedrate = TOUCH_PROBE_RETRACT_1_FEEDRATE;
      prepare_move_raw();
    #endif

    #if defined(TOUCH_PROBE_RETRACT_2_X) && defined(TOUCH_PROBE_RETRACT_2_Y) && defined(TOUCH_PROBE_RETRACT_2_Z)
      destination[X_AXIS] = TOUCH_PROBE_RETRACT_2_X;
      destination[Y_AXIS] = TOUCH_PROBE_RETRACT_2_Y;
      destination[Z_AXIS] = TOUCH_PROBE_RETRACT_2_Z;
      feedrate = TOUCH_PROBE_RETRACT_2_FEEDRATE;
      prepare_move_raw();
    #endif

    #if defined(TOUCH_PROBE_RETRACT_3_X) && defined(TOUCH_PROBE_RETRACT_3_Y) && defined(TOUCH_PROBE_RETRACT_3_Z)
      destination[X_AXIS] = TOUCH_PROBE_RETRACT_3_X;
      destination[Y_AXIS] = TOUCH_PROBE_RETRACT_3_Y;
      destination[Z_AXIS] = TOUCH_PROBE_RETRACT_3_Z;
      feedrate = TOUCH_PROBE_RETRACT_3_FEEDRATE;
      prepare_move_raw();
    #endif

    st_synchronize();
    #endif //SERVO_ENDSTOPS
}
/// Probe bed height at position (x,y), returns the measured z value **PJR - Z probe offset must be handled by caller.
static float probe_pt(float x, float y, float z_before) {

#ifdef PROBE_AVG
  int num_probes=PROBE_AVG;
  float total=0.0;
  float probe_bed_array[PROBE_AVG];
#else  
  float probe_bed_array[20];
  boolean probe_done;
#endif
  int probe_count;
  float probe_z; 

  // move to right place
  do_blocking_move_to(current_position[X_AXIS], current_position[Y_AXIS], z_before);
  do_blocking_move_to(x - z_probe_offset[X_AXIS], y - z_probe_offset[Y_AXIS], current_position[Z_AXIS]);

#ifdef PROBE_AVG
  for(probe_count=0;probe_count<num_probes;probe_count++)
    {
    if (probe_count > 0)
      {
      //**PJR - Lift the probe before next sample
      do_blocking_move_to(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS] + Z_RAISE_BETWEEN_PROBINGS);
      }

#if defined(SERVO_ENDSTOPS) && !defined(Z_PROBE_SLED)
    engage_z_probe();   // Engage Z Servo endstop if available
#endif // SERVO_ENDSTOPS && !Z_PROBE_SLED

    run_z_probe();
    probe_z = current_position[Z_AXIS];

#if defined(SERVO_ENDSTOPS) && !defined(Z_PROBE_SLED)
    retract_z_probe();
#endif // SERVO_ENDSTOPS && !Z_PROBE_SLED

    probe_bed_array[probe_count] = probe_z;
    total += probe_z;
 
    }
    probe_z = total / num_probes;

#else
  //**PJR - Probe the bed multiple times until two readings match
  probe_count = 0;
  do {
    //**PJR - Do not lift the zprobe if not first probe
    if (probe_count > 0)
    {
      //**PJR - Lift the probe before next sample
      do_blocking_move_to(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS] + Z_RAISE_BETWEEN_PROBINGS);
    }

#if defined(SERVO_ENDSTOPS) && !defined(Z_PROBE_SLED)
    engage_z_probe();   // Engage Z Servo endstop if available
#endif // SERVO_ENDSTOPS && !Z_PROBE_SLED

    run_z_probe();
    probe_z = current_position[Z_AXIS];

#if defined(SERVO_ENDSTOPS) && !defined(Z_PROBE_SLED)
    retract_z_probe();
#endif // SERVO_ENDSTOPS && !Z_PROBE_SLED

    probe_bed_array[probe_count] = probe_z;
    probe_done = false;
    if (probe_count > 0)
    {
      for(int xx=0; xx < probe_count; xx++)
      {
        if (probe_bed_array[xx] == probe_z)
        {
          probe_done = true;
        }
        /*
        SERIAL_ECHO("probe_z=");
        SERIAL_PROTOCOL_F(probe_z,5);
        SERIAL_ECHO(" probe_bed_array[");
        SERIAL_ECHO(xx);
        SERIAL_ECHO("]=");
        SERIAL_PROTOCOL_F(probe_bed_array[xx],5);
        SERIAL_ECHOLN("");
        */
      }
    }
    probe_count ++;
    //SERIAL_PROTOCOL_F(probe_z,5);
    //SERIAL_ECHOLN("");
    } while ((probe_done == false) and (probe_count < 20));
   //**PJR - Remove confusing diagnostic messages
 #endif 
  //SERIAL_PROTOCOLPGM(MSG_BED);
  SERIAL_PROTOCOLPGM(" x: ");
  SERIAL_PROTOCOL(x);
  SERIAL_PROTOCOLPGM(" y: ");
  SERIAL_PROTOCOL(y);
  SERIAL_PROTOCOLPGM(" z: ");
  SERIAL_PROTOCOL(probe_z); //**PJR - This is the measured Z at probe deployed height - confusing to user
#ifndef PROBE_AVG
  SERIAL_PROTOCOLPGM(" bed_array[] = [");
  for(int xx=0;xx < probe_count; xx++) {
    SERIAL_PROTOCOL(probe_bed_array[xx]);
    SERIAL_PROTOCOLPGM(",");
  }
  
  SERIAL_PROTOCOLPGM("] \n"); 
#else
  SERIAL_PROTOCOLPGM(" bed_array[] = [");
  for(int xx=0;xx < num_probes; xx++) {
    SERIAL_PROTOCOL(probe_bed_array[xx]);
    SERIAL_PROTOCOLPGM(",");
  }
  
  SERIAL_PROTOCOLPGM("] \n");  
#endif 
  return probe_z;
}

#endif // #ifdef ENABLE_AUTO_BED_LEVELING

#ifdef NONLINEAR_BED_LEVELING
static void extrapolate_one_point(int x, int y, int xdir, int ydir) {
  if (bed_level[x][y] != 0.0) {
    return;  // Don't overwrite good values.
  }
  float a = 2*bed_level[x+xdir][y] - bed_level[x+xdir*2][y];  // Left to right.
  float b = 2*bed_level[x][y+ydir] - bed_level[x][y+ydir*2];  // Front to back.
  float c = 2*bed_level[x+xdir][y+ydir] - bed_level[x+xdir*2][y+ydir*2];  // Diagonal.
  float median = c;  // Median is robust (ignores outliers).
  if (a < b) {
    if (b < c) median = b;
    if (c < a) median = a;
  } else {  // b <= a
    if (c < b) median = b;
    if (a < c) median = a;
  }
  bed_level[x][y] = median;
}

// Fill in the unprobed points (corners of circular print surface)
// using linear extrapolation, away from the center.
static void extrapolate_unprobed_bed_level() {
  int half = (AUTO_BED_LEVELING_GRID_POINTS-1)/2;
  for (int y = 0; y <= half; y++) {
    for (int x = 0; x <= half; x++) {
      if (x + y < 3) continue;
      extrapolate_one_point(half-x, half-y, x>1?+1:0, y>1?+1:0);
      extrapolate_one_point(half+x, half-y, x>1?-1:0, y>1?+1:0);
      extrapolate_one_point(half-x, half+y, x>1?+1:0, y>1?-1:0);
      extrapolate_one_point(half+x, half+y, x>1?-1:0, y>1?-1:0);
    }
  }
}

// Print calibration results for plotting or manual frame adjustment.
static void print_bed_level() {
  for (int y = 0; y < AUTO_BED_LEVELING_GRID_POINTS; y++) {
    for (int x = 0; x < AUTO_BED_LEVELING_GRID_POINTS; x++) {
      SERIAL_PROTOCOL_F(bed_level[x][y], 2);
      SERIAL_PROTOCOLPGM(" ");
    }
    SERIAL_ECHOLN("");
  }
}

// Reset calibration results to zero.
static void reset_bed_level() {
  for (int y = 0; y < AUTO_BED_LEVELING_GRID_POINTS; y++) {
    for (int x = 0; x < AUTO_BED_LEVELING_GRID_POINTS; x++) {
      bed_level[x][y] = 0.0;
    }
  }
}
#endif //NONLINEAR_BED_LEVELING

static void homeaxis(int axis) {
#define HOMEAXIS_DO(LETTER) \
  ((LETTER##_MIN_PIN > -1 && LETTER##_HOME_DIR==-1) || (LETTER##_MAX_PIN > -1 && LETTER##_HOME_DIR==1))

  if (axis==X_AXIS ? HOMEAXIS_DO(X) :
      axis==Y_AXIS ? HOMEAXIS_DO(Y) :
      axis==Z_AXIS ? HOMEAXIS_DO(Z) :
      0) {
    int axis_home_dir = home_dir(axis);
#ifdef DUAL_X_CARRIAGE
    if (axis == X_AXIS)
      axis_home_dir = x_home_dir(active_extruder);
#endif

    current_position[axis] = 0;
    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);


#ifndef Z_PROBE_SLED
    // Engage Servo endstop if enabled
    #ifdef SERVO_ENDSTOPS
      #if defined (ENABLE_AUTO_BED_LEVELING) && (PROBE_SERVO_DEACTIVATION_DELAY > 0)
        if (axis==Z_AXIS) {
          engage_z_probe();
        }
	    else
      #endif
      if (servo_endstops[axis] > -1) {
        servos[servo_endstops[axis]].write(servo_endstop_angles[axis * 2]);
      }
    #endif
#endif // Z_PROBE_SLED

    current_position[axis] = 0;
    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
    destination[axis] = 1.5 * max_length[axis] * axis_home_dir;
    feedrate = homing_feedrate[axis];
    plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate/60, active_extruder);
    st_synchronize();

    enable_endstops(false);  // Ignore Z probe while moving away from the top microswitch.
    current_position[axis] = 0;
    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
    destination[axis] = -home_retract_mm(axis) * axis_home_dir;
    plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate/60, active_extruder);
    st_synchronize();
    enable_endstops(true);  // Stop ignoring Z probe while moving up to the top microswitch again.

    destination[axis] = 2*home_retract_mm(axis) * axis_home_dir;
#ifdef DELTA
    feedrate = homing_feedrate[axis]/4;
#else
    feedrate = homing_feedrate[axis]/2 ;
#endif
    plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate/60, active_extruder);
    st_synchronize();
#ifdef DELTA
    // retrace by the amount specified in endstop_adj
    if (endstop_adj[axis] * axis_home_dir < 0) {
      enable_endstops(false);  //PJR - Ignore Z probe while moving away from the top microswitch.
      plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
      destination[axis] = endstop_adj[axis];
      plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate/60, active_extruder);
      st_synchronize();
      enable_endstops(true);  //PJR - Stop ignoring Z probe after endstop_adj retrace is complete.
    }
#endif
    axis_is_at_home(axis);
    destination[axis] = current_position[axis];
    feedrate = 0.0;
    endstops_hit_on_purpose();
    axis_known_position[axis] = true;

    // Retract Servo endstop if enabled
    #ifdef SERVO_ENDSTOPS
      if (servo_endstops[axis] > -1) {
        servos[servo_endstops[axis]].write(servo_endstop_angles[axis * 2 + 1]);
      }
    #endif
#if defined (ENABLE_AUTO_BED_LEVELING) && (PROBE_SERVO_DEACTIVATION_DELAY > 0)
  #ifndef Z_PROBE_SLED
    if (axis==Z_AXIS) retract_z_probe();
  #endif
#endif

  }
}
#define HOMEAXIS(LETTER) homeaxis(LETTER##_AXIS)

void set_default_z_probe_offset()
  {
  z_probe_offset[X_AXIS] = default_z_probe_offset[X_AXIS];
  z_probe_offset[Y_AXIS] = default_z_probe_offset[Y_AXIS];
  z_probe_offset[Z_AXIS] = default_z_probe_offset[Z_AXIS];
  }

void set_delta_constants()
{
  max_length[Z_AXIS] = max_pos[Z_AXIS] - Z_MIN_POS;
  base_max_pos[Z_AXIS]  = max_pos[Z_AXIS];
  base_home_pos[Z_AXIS] = max_pos[Z_AXIS];

  DELTA_DIAGONAL_ROD1_2 = pow(delta_diagonal_rod + diagrod_adj[0],2);
  DELTA_DIAGONAL_ROD2_2 = pow(delta_diagonal_rod + diagrod_adj[1],2);
  DELTA_DIAGONAL_ROD3_2 = pow(delta_diagonal_rod + diagrod_adj[2],2);

  // Effective X/Y positions of the three vertical towers.
  /*
  delta_tower1_x = (-SIN_60 * delta_radius) + tower_adj[0]; // front left tower + xa
  delta_tower1_y = (-COS_60 * delta_radius) - tower_adj[0] ;
  delta_tower2_x = -(-SIN_60 * delta_radius) + tower_adj[1]; // front right tower + xb
  delta_tower2_y = (-COS_60 * delta_radius) + tower_adj[1]; //
  delta_tower3_x = tower_adj[2] ; // back middle tower + xc
  delta_tower3_y = -2 * (-COS_60 * delta_radius);
  */

  delta_tower1_x = (delta_radius + tower_adj[3]) * cos((210 + tower_adj[0]) * PI/180); // front left tower
  delta_tower1_y = (delta_radius + tower_adj[3]) * sin((210 + tower_adj[0]) * PI/180);
  delta_tower2_x = (delta_radius + tower_adj[4]) * cos((330 + tower_adj[1]) * PI/180); // front right tower
  delta_tower2_y = (delta_radius + tower_adj[4]) * sin((330 + tower_adj[1]) * PI/180);
  delta_tower3_x = (delta_radius + tower_adj[5]) * cos((90 + tower_adj[2]) * PI/180);  // back middle tower
  delta_tower3_y = (delta_radius + tower_adj[5]) * sin((90 + tower_adj[2]) * PI/180);
}

void apply_endstop_adjustment(float x_endstop, float y_endstop, float z_endstop)
{
    for(int x=0; x<3 ; x++)
      {
      saved_endstop_adj[x] = endstop_adj[x];
      }
    endstop_adj[X_AXIS] += x_endstop;
    endstop_adj[Y_AXIS] += y_endstop;
    endstop_adj[Z_AXIS] += z_endstop;

    calculate_delta(current_position);
    plan_set_position(delta[X_AXIS] - (endstop_adj[X_AXIS] - saved_endstop_adj[X_AXIS]) , delta[Y_AXIS] - (endstop_adj[Y_AXIS] - saved_endstop_adj[Y_AXIS]), delta[Z_AXIS] - (endstop_adj[Z_AXIS] - saved_endstop_adj[Z_AXIS]), current_position[E_AXIS]);
    st_synchronize();
}

void adj_endstops() {
  boolean x_done = false;
  boolean y_done = false;
  boolean z_done = false;
  float prv_bed_level_x, prv_bed_level_y, prv_bed_level_z;

  do
    {
    bed_level_z = probe_bed(0.0, bed_radius);
    bed_level_x = probe_bed(-SIN_60 * bed_radius, -COS_60 * bed_radius);
    bed_level_y = probe_bed(SIN_60 * bed_radius, -COS_60 * bed_radius);

    apply_endstop_adjustment(bed_level_x, bed_level_y, bed_level_z);

    SERIAL_ECHO("x:");
    SERIAL_PROTOCOL_F(bed_level_x, 4);
    SERIAL_ECHO(" (adj:");
    SERIAL_PROTOCOL_F(endstop_adj[0], 4);
    SERIAL_ECHO(") y:");
    SERIAL_PROTOCOL_F(bed_level_y, 4);
    SERIAL_ECHO(" (adj:");
    SERIAL_PROTOCOL_F(endstop_adj[1], 4);
    SERIAL_ECHO(") z:");
    SERIAL_PROTOCOL_F(bed_level_z, 4);
    SERIAL_ECHO(" (adj:");
    SERIAL_PROTOCOL_F(endstop_adj[2], 4);
    SERIAL_ECHOLN(")");

    if ((bed_level_x >= -ac_prec) and (bed_level_x <= ac_prec))
      {
      x_done = true;
      SERIAL_ECHO("X=OK");
      }
    else
      {
      x_done = false;
      SERIAL_ECHO("X=ERROR");
      }

    if ((bed_level_y >= -ac_prec) and (bed_level_y <= ac_prec))
      {
      y_done = true;
      SERIAL_ECHO(" Y=OK");
      }
    else
      {
      y_done = false;
      SERIAL_ECHO(" Y=ERROR");
      }

    if ((bed_level_z >= -ac_prec) and (bed_level_z <= ac_prec))
      {
      z_done = true;
      SERIAL_ECHO(" Z=OK");
      SERIAL_ECHOLN("");
      }
    else
      {
      z_done = false;
      SERIAL_ECHO(" Z=ERROR");
      SERIAL_ECHOLN("");
      }
    } while (((x_done == false) or (y_done == false) or (z_done == false))); // and (endstop_adj_err == false));

    float high_endstop = 0;
    float low_endstop = 0;
    for(int x=0; x<3; x++)
      {
      if (endstop_adj[x] > high_endstop) high_endstop = endstop_adj[x];
      if (endstop_adj[x] < low_endstop) low_endstop = endstop_adj[x];
      }

    if (high_endstop > 0)
      {
      SERIAL_ECHOPAIR("Reducing Build height by ",high_endstop);
      SERIAL_ECHOLN("");
      for(int x=0; x<3; x++)
        {
        endstop_adj[x] -= high_endstop;
        }
      max_pos[Z_AXIS] -= high_endstop;
      set_delta_constants();
      }
    bed_safe_z = AUTOCAL_PROBELIFT - z_probe_offset[Z_AXIS];
}

void adj_endstops_alt1(){
  float adj_x_prv, adj_y_prv, adj_z_prv;
  float diff_x_prv, diff_y_prv, diff_z_prv;
  float adj_r_target,adj_r, high_endstop;
  float diff_x = 0;
  float diff_y = 0;
  float diff_z = 0;
  float adj_x = 0;
  float adj_y = 0;
  float adj_z = 0;
  float adj_x_mag =0.5;
  float adj_y_mag = 0.5;
  float adj_z_mag = 0.5;

  adj_r_target = bed_level_x + bed_level_y + bed_level_z / 3;
  //set inital direction and magnitude for delta radius adjustment
  adj_r = -2;
  if (adj_r_target > bed_level_c) adj_r = 2;
   //!!
  saved_endstop_adj[X_AXIS] = endstop_adj[X_AXIS];
  saved_endstop_adj[Y_AXIS] = endstop_adj[Y_AXIS];
  saved_endstop_adj[Z_AXIS] = endstop_adj[Z_AXIS];

  do
    {
    //Adjust delta radius
//    delta_radius += adj_r;
//    set_delta_constants();

    endstop_adj[0] += adj_x; // / 1.05;
    endstop_adj[1] += adj_y; //(bed_level_oy - bed_level_y) / 1.05;
    endstop_adj[2] += adj_z; //(bed_level_oz - bed_level_z) / 1.05;

    calculate_delta(current_position);
    plan_set_position(delta[X_AXIS] - (endstop_adj[X_AXIS] - saved_endstop_adj[X_AXIS]) , delta[Y_AXIS] - (endstop_adj[Y_AXIS] - saved_endstop_adj[Y_AXIS]), delta[Z_AXIS] - (endstop_adj[Z_AXIS] - saved_endstop_adj[Z_AXIS]), current_position[E_AXIS]);

    saved_endstop_adj[X_AXIS] = endstop_adj[X_AXIS];
    saved_endstop_adj[Y_AXIS] = endstop_adj[Y_AXIS];
    saved_endstop_adj[Z_AXIS] = endstop_adj[Z_AXIS];


    //bed_probe_all();
    //bed_level_c = probe_bed(0.0, 0.0);
    //bed_level_z = probe_bed(0.0, bed_radius);
    //bed_level_oy = probe_bed(-SIN_60 * bed_radius, COS_60 * bed_radius);
    bed_level_x = probe_bed(-SIN_60 * bed_radius, -COS_60 * bed_radius);
    //bed_level_oz = probe_bed(0.0, -bed_radius);
    //bed_level_y = probe_bed(SIN_60 * bed_radius, -COS_60 * bed_radius);
    bed_level_ox = probe_bed(SIN_60 * bed_radius, COS_60 * bed_radius);

    //adj_r_target = bed_level_x + bed_level_y + bed_level_z / 3;
    //Adjust delta radius
    //if (((adj_r > 0) and (bed_level_c > adj_r_target)) or ((adj_r < 0) and (bed_level_c < adj_r_target))) adj_r = -(adj_r / 2);

    adj_x_prv = adj_x;
    diff_x_prv = diff_x;
    adj_x = 0;
    diff_x = abs(bed_level_x - bed_level_ox);
    if ((diff_x > diff_x_prv) and (diff_x_prv !=0)) adj_x_mag = adj_x_mag * 2;
    if (bed_level_x < bed_level_ox) adj_x = -adj_x_mag;
    if (bed_level_x > bed_level_ox) adj_x = adj_x_mag;
    if (((adj_x > 0) and (adj_x_prv < 0)) or (adj_x < 0) and (adj_x_prv > 0))
      {
        if (adj_x_mag > 0.125)
          {
          adj_x = adj_x / 2;
          adj_x_mag = adj_x_mag /2;
          }
      }
    SERIAL_ECHO("x:");
    SERIAL_PROTOCOL_F(bed_level_x,4);
    SERIAL_ECHO(" ox:");
    SERIAL_PROTOCOL_F(bed_level_ox,4);
    SERIAL_ECHO(" adj_x:");
    SERIAL_PROTOCOL_F(adj_x,4);
    SERIAL_ECHOLN("");

    bed_level_y = probe_bed(SIN_60 * bed_radius, -COS_60 * bed_radius);
    bed_level_oy = probe_bed(-SIN_60 * bed_radius, COS_60 * bed_radius);

    adj_y_prv = adj_y;
    adj_y = 0;
    diff_y_prv = diff_y;
    diff_y = abs(bed_level_y - bed_level_oy);
    if ((diff_y > diff_y_prv) and (diff_y_prv != 0)) adj_y_mag = adj_y_mag * 2;
    if (bed_level_y < bed_level_oy) adj_y = -adj_y_mag;
    if (bed_level_y > bed_level_oy) adj_y = adj_y_mag;
    if (((adj_y > 0) and (adj_y_prv < 0)) or (adj_y < 0) and (adj_y_prv > 0))
      {
        if (adj_y_mag > 0.125)
          {
          adj_y = adj_y / 2;
          adj_y_mag = adj_y_mag /2;
          }
      }
    SERIAL_ECHO("y:");
    SERIAL_PROTOCOL_F(bed_level_y,4);
    SERIAL_ECHO(" oy:");
    SERIAL_PROTOCOL_F(bed_level_oy,4);
    SERIAL_ECHO(" adj_y:");
    SERIAL_PROTOCOL_F(adj_y,4);
    SERIAL_ECHOLN("");


    bed_level_z = probe_bed(0.0, bed_radius);
    bed_level_oz = probe_bed(0.0, -bed_radius);

    adj_z_prv = adj_z;
    adj_z = 0;
    diff_z_prv = diff_z;
    diff_z = abs(bed_level_z - bed_level_oz);
    if ((diff_z > diff_z_prv) and (diff_z_prv != 0)) adj_z_mag = adj_z_mag * 2;
    if (bed_level_z < bed_level_oz) adj_z = -adj_z_mag;
    if (bed_level_z > bed_level_oz) adj_z = adj_z_mag;
    if (((adj_z > 0) and (adj_z_prv < 0)) or (adj_z < 0) and (adj_z_prv > 0))
      {
        if (adj_z_mag > 0.125)
          {
          adj_z = adj_z / 2;
          adj_z_mag = adj_z_mag /2;
          }
      }
    SERIAL_ECHO("z:");
    SERIAL_PROTOCOL_F(bed_level_z,4);
    SERIAL_ECHO(" oz:");
    SERIAL_PROTOCOL_F(bed_level_oz,4);
    SERIAL_ECHO(" adj_z:");
    SERIAL_PROTOCOL_F(adj_z,4);
    SERIAL_ECHOLN("");

    high_endstop = 0;
    for(int x=0; x<3; x++) if (endstop_adj[x] > high_endstop) high_endstop = endstop_adj[x];

    if (high_endstop > 0)
      {
      SERIAL_ECHOPAIR("Reducing Build height by ",high_endstop);
      SERIAL_ECHOLN("");
      for(int x=0; x<3; x++)
        {
        endstop_adj[x] -= high_endstop;
        }
      max_pos[Z_AXIS] -= high_endstop;
      set_delta_constants();
      }
    } while((diff_x > 0.4) or (diff_y > 0.4) or (diff_z > 0.4));
//    } while((bed_level_x < bed_level_ox - ac_prec) or (bed_level_x > bed_level_ox + ac_prec)
//      or (bed_level_y < bed_level_oy - ac_prec) or (bed_level_y > bed_level_oy + ac_prec)
//      or (bed_level_z < bed_level_oz - ac_prec) or (bed_level_z > bed_level_oz + ac_prec));
}

int fix_tower_errors()
{
    boolean t1_err, t2_err, t3_err;
    boolean xy_equal, xz_equal, yz_equal;
    float saved_tower_adj[6];
    float err_tower;
    float low_diff, high_diff;
    float x_diff, y_diff, z_diff;
    float xy_diff, yz_diff, xz_diff;
    float low_opp, high_opp;

    for (int i=0; i<6; i++) saved_tower_adj[i] = tower_adj[i];

    err_tower = 0;

    x_diff = abs(bed_level_x - bed_level_ox);
    high_diff = x_diff;
    y_diff = abs(bed_level_y - bed_level_oy);
    if (y_diff > high_diff) high_diff = y_diff;
    z_diff = abs(bed_level_z - bed_level_oz);
    if (z_diff > high_diff) high_diff = z_diff;

    if (x_diff <= ac_prec) t1_err = false; else t1_err = true;
    if (y_diff <= ac_prec) t2_err = false; else t2_err = true;
    if (z_diff <= ac_prec) t3_err = false; else t3_err = true;

    SERIAL_ECHO("x_diff = ");
    SERIAL_PROTOCOL_F(x_diff,5);
    SERIAL_ECHOLN("");
    SERIAL_ECHO("y_diff = ");
    SERIAL_PROTOCOL_F(y_diff,5);
    SERIAL_ECHOLN("");
    SERIAL_ECHO("z_diff = ");
    SERIAL_PROTOCOL_F(z_diff,5);
    SERIAL_ECHOLN("");
    SERIAL_ECHO("high_diff = ");
    SERIAL_PROTOCOL_F(high_diff,5);
    SERIAL_ECHOLN("");

    //Are all errors equal? (within defined precision)
    xy_equal = false;
    xz_equal = false;
    yz_equal = false;
    if (abs(x_diff - y_diff) <= ac_prec) xy_equal = true;
    if (abs(x_diff - z_diff) <= ac_prec) xz_equal = true;
    if (abs(y_diff - z_diff) <= ac_prec) yz_equal = true;
    SERIAL_ECHO("xy_equal = ");
    if (xy_equal == true) SERIAL_ECHOLN("true"); else SERIAL_ECHOLN("false");
    SERIAL_ECHO("xz_equal = ");
    if (xz_equal == true) SERIAL_ECHOLN("true"); else SERIAL_ECHOLN("false");
    SERIAL_ECHO("yz_equal = ");
    if (yz_equal == true) SERIAL_ECHOLN("true"); else SERIAL_ECHOLN("false");

    low_opp = bed_level_ox;
    high_opp = low_opp;
    if (bed_level_oy < low_opp) low_opp = bed_level_oy;
    if (bed_level_oy > high_opp) high_opp = bed_level_oy;
    if (bed_level_oz < low_opp) low_opp = bed_level_oz;
    if (bed_level_oz > high_opp) high_opp = bed_level_oz;

    SERIAL_ECHO("Opp Range = ");
    SERIAL_PROTOCOL_F(high_opp - low_opp,5);
    SERIAL_ECHOLN("");
    if (high_opp - low_opp  < ac_prec)
      {
      SERIAL_ECHOLN("Opposite Points within Limits - Adjustment not required");
      t1_err = false;
      t2_err = false;
      t3_err = false;
      }

    //All Towers have errors
    if ((t1_err == true) and (t2_err == true) and (t3_err == true))
      {
      //if ((x_diff <= y_diff + ac_prec) and (x_diff > y_diff - ac_prec)) xy_equal = true;
      //if ((x_diff <= z_diff + ac_prec) and (x_diff > z_diff - ac_prec)) xz_equal = true;
      //if ((y_diff <= z_diff + ac_prec) and (y_diff > z_diff - ac_prec)) yz_equal = true;
      if ((xy_equal == false) or (xz_equal == false) or (yz_equal == false))
        {
        //Errors not equal .. select the tower that needs to be adjusted
        if (high_diff == x_diff) err_tower = 1;
        if (high_diff == y_diff) err_tower = 2;
        if (high_diff == z_diff) err_tower = 3;
        SERIAL_ECHOPAIR("Tower ", err_tower);
        SERIAL_ECHOLN(" has largest error");
        }
      if ((xy_equal == true) and (xz_equal == true) and (yz_equal == true))
        {
        SERIAL_ECHOLN("All Towers Errors Equal");
        t1_err = false;
        t2_err = false;
        t3_err = false;
        }
      }

    //Two tower errors
    if ((t1_err == true) and (t2_err == true) and (t3_err == false)) err_tower = 3;
    if ((t1_err == true) and (t2_err == false) and (t3_err == true)) err_tower = 2;
    if ((t1_err == false) and (t2_err == true) and (t3_err == true)) err_tower = 1;

    //Single tower error
    if ((t1_err == true) and (t2_err == false) and (t3_err == false)) err_tower = 1;
    if ((t1_err == false) and (t2_err == true) and (t3_err == false)) err_tower = 2;
    if ((t1_err == false) and (t2_err == false) and (t3_err == true)) err_tower = 3;



    SERIAL_ECHO("t1:");
    if (t1_err == true) SERIAL_ECHO("Err"); else SERIAL_ECHO("OK");
    SERIAL_ECHO(" t2:");
    if (t2_err == true) SERIAL_ECHO("Err"); else SERIAL_ECHO("OK");
    SERIAL_ECHO(" t3:");
    if (t3_err == true) SERIAL_ECHO("Err"); else SERIAL_ECHO("OK");
    SERIAL_ECHOLN("");

    if (err_tower == 0)
      {
      SERIAL_ECHOLN("Tower geometry OK");
      }
    else
      {
      //If a tower has been adjusted previously.. continute to correct by adjusting that tower! (but only if the difference between the opp points is still large)
      if (high_opp - low_opp  > ac_prec * 2)
        {
        if ((tower_adj[0] != 0) or (tower_adj[3] != 0))
          {
          SERIAL_ECHOLN("Tower 1 has already been adjusted");
          err_tower = 1;
          }
        if ((tower_adj[1] != 0) or (tower_adj[4] != 0))
          {
          SERIAL_ECHOLN("Tower 2 has already been adjusted");
          err_tower = 2;
          }
        if ((tower_adj[2] != 0) or (tower_adj[5] != 0))
          {
          SERIAL_ECHOLN("Tower 3 has already been adjusted");
          err_tower = 3;
          }
        }
      SERIAL_ECHO("Tower");
      SERIAL_ECHO(int(err_tower));
      SERIAL_ECHOLN(" Error: Adjusting");
      adj_tower_radius(err_tower);
      adj_tower_delta(err_tower);
      }
      //Set return value to indicate if anything has been changed (0 = no change)
      int retval = 0;
      for (int i=0; i<6; i++) if (saved_tower_adj[i] != tower_adj[i]) retval++;
      return retval;
}

int adj_deltaradius()
{
  float adj_r;
  float prev_c;
  int c_nochange_count = 0;
  float nochange_r;

  //if ((bed_level_c >= -ac_prec) and (bed_level_c <= ac_prec))
  if ((bed_level_c >= -ac_prec/2) and (bed_level_c <= ac_prec/2))
    {
    SERIAL_ECHOLN("Delta Radius OK");
    return 0;
    }
  else
    {
    SERIAL_ECHOLN("Adjusting Delta Radius");
    //set inital direction and magnitude for delta radius adjustment
    adj_r = 0.1;
    if (bed_level_c > 0) adj_r = -0.1;

    bed_safe_z = AUTOCAL_PROBELIFT - z_probe_offset[Z_AXIS];

    do
      {
      delta_radius += adj_r;
      set_delta_constants();

      prev_c = bed_level_c;
      bed_level_c = probe_bed(0.0, 0.0);

      //Show progress
      SERIAL_ECHO("r:");
      SERIAL_PROTOCOL_F(delta_radius, 4);
      SERIAL_ECHO(" (adj:");
      SERIAL_PROTOCOL_F(adj_r,4);
      SERIAL_ECHO(") c:");
      SERIAL_PROTOCOL_F(bed_level_c, 4);
      SERIAL_ECHOLN("");

      //Adjust delta radius
      if (((adj_r > 0) and (bed_level_c < prev_c)) or ((adj_r < 0) and (bed_level_c > prev_c))) adj_r = -(adj_r / 2);

      //Count iterations with no change to c probe point
      if (bed_level_c == prev_c) c_nochange_count ++;
      if (c_nochange_count == 1) nochange_r = delta_radius;

      } while(((bed_level_c < -ac_prec) or (bed_level_c > ac_prec)) and (c_nochange_count < 3));

      if (c_nochange_count > 0)
        {
        delta_radius = nochange_r;
        set_delta_constants();
        bed_safe_z = AUTOCAL_PROBELIFT - z_probe_offset[Z_AXIS];
        }
    return 1;
    }
}

void adj_tower_radius(int tower)
{
  boolean done,t1_done,t2_done,t3_done;
  int nochange_count;
  float target, prev_target, prev_bed_level;
  float temp, adj_target;

    //Set inital tower adjustment values
    adj_t1_Radius = 0;
    adj_t2_Radius = 0;
    adj_t3_Radius = 0;
    nochange_count = 0;

    if ((tower == 1) and (adj_t1_Radius == 0))
      {
      target = (bed_level_oy + bed_level_oz) / 2;
      temp = (bed_level_ox - target) / 2;
      adj_target = target + temp;
      if (bed_level_ox < adj_target) adj_t1_Radius = -0.4; //0.4;
      if (bed_level_ox > adj_target) adj_t1_Radius = 0.4; //-0.4;
      }
    if ((tower == 2) and (adj_t2_Radius == 0))
      {
      target = (bed_level_ox + bed_level_oz) / 2;
      temp = (bed_level_oy - target) / 2;
      adj_target = target + temp;
      if (bed_level_oy < adj_target) adj_t2_Radius = -0.4; //0.4;
      if (bed_level_oy > adj_target) adj_t2_Radius = 0.4; //-0.4;
      }
    if ((tower == 3) and (adj_t3_Radius == 0))
      {
      target = (bed_level_oy + bed_level_ox) / 2;
      temp = (bed_level_oz - target) / 2;
      adj_target = target + temp;
      if (bed_level_oz < adj_target) adj_t3_Radius = -0.4; //0.4;
      if (bed_level_oz > adj_target) adj_t3_Radius = 0.4; //-0.4;
      }

    do
    {
    tower_adj[3] += adj_t1_Radius;
    tower_adj[4] += adj_t2_Radius;
    tower_adj[5] += adj_t3_Radius;
    set_delta_constants();

    //done = false;
    t1_done = false;
    t2_done = false;
    t3_done = false;
    if (tower == 1)
      {
      t2_done = true;
      t3_done = true;
      prev_target = adj_target;
      prev_bed_level = bed_level_ox;

      bed_level_ox = probe_bed(SIN_60 * bed_radius, COS_60 * bed_radius);
      bed_level_oy = probe_bed(-SIN_60 * bed_radius, COS_60 * bed_radius);
      bed_level_oz = probe_bed(0.0, -bed_radius);

      target = (bed_level_oy + bed_level_oz) / 2;
      temp = (bed_level_ox - target) / 2;
      adj_target = target + temp;
      if (((bed_level_ox < adj_target) and (adj_t1_Radius > 0)) or ((bed_level_ox > adj_target) and (adj_t1_Radius < 0))) adj_t1_Radius = -(adj_t1_Radius / 2);
      //if (bed_level_ox == adj_target) t1_done = true;
      temp = bed_level_ox - adj_target;
      if ((temp <= 0.01) && (temp >= -0.01)) t1_done=true;
      if ((bed_level_ox + 0.0001 > prev_bed_level) and (bed_level_ox - 0.0001 < prev_bed_level) and (adj_target + 0.0001 > prev_target) and (adj_target - 0.0001 < prev_target)) nochange_count ++;
      if (nochange_count > 1)
        {
        SERIAL_ECHOLN("Stuck in Loop.. Exiting");
        t1_done = true;
        }

      SERIAL_ECHO(" target:");
      SERIAL_PROTOCOL_F(adj_target, 6);
      SERIAL_ECHO(" ox:");
      SERIAL_PROTOCOL_F(bed_level_ox, 6);
      SERIAL_ECHO(" tower radius adj:");
      SERIAL_PROTOCOL_F(tower_adj[3], 8);
      if (t1_done == true) SERIAL_ECHOLN(" done:true"); else SERIAL_ECHOLN(" done:false");
      }

    if (tower == 2)
      {
      t1_done = true;
      t3_done = true;
      prev_target = adj_target;
      prev_bed_level = bed_level_oy;

      bed_level_ox = probe_bed(SIN_60 * bed_radius, COS_60 * bed_radius);
      bed_level_oy = probe_bed(-SIN_60 * bed_radius, COS_60 * bed_radius);
      bed_level_oz = probe_bed(0.0, -bed_radius);

      target = (bed_level_ox + bed_level_oz) /2;
      temp = (bed_level_oy - target) / 2;
      adj_target = target + temp;
      if (((bed_level_oy < adj_target) and (adj_t2_Radius > 0)) or ((bed_level_oy > adj_target) and (adj_t2_Radius < 0))) adj_t2_Radius = -(adj_t2_Radius / 2);
      //if (bed_level_oy == adj_target) t2_done = true;
      temp = bed_level_oy - adj_target;
      if ((temp <= 0.01) && (temp >= -0.01)) t2_done=true;
      if ((bed_level_oy + 0.0001 > prev_bed_level) and (bed_level_oy - 0.0001 < prev_bed_level) and (adj_target + 0.0001 > prev_target) and (adj_target - 0.0001 < prev_target)) nochange_count ++;
      if (nochange_count > 1)
        {
        SERIAL_ECHOLN("Stuck in Loop.. Exiting");
        t2_done = true;
        }

      SERIAL_ECHO(" target:");
      SERIAL_PROTOCOL_F(adj_target,4);
      SERIAL_ECHO(" oy:");
      SERIAL_PROTOCOL_F(bed_level_oy,4);
      SERIAL_ECHO(" tower radius adj:");
      SERIAL_PROTOCOL_F(tower_adj[4], 8);
      if (t2_done == true) SERIAL_ECHOLN(" done:true"); else SERIAL_ECHOLN(" done:false");
      }

    if (tower == 3)
      {
      t1_done = true;
      t2_done = true;
      prev_target = adj_target;
      prev_bed_level = bed_level_oz;

      bed_level_ox = probe_bed(SIN_60 * bed_radius, COS_60 * bed_radius);
      bed_level_oy = probe_bed(-SIN_60 * bed_radius, COS_60 * bed_radius);
      bed_level_oz = probe_bed(0.0, -bed_radius);

      target = (bed_level_oy + bed_level_ox) / 2;
      temp = (bed_level_oz - target) / 2;
      adj_target = target + temp;
      if (((bed_level_oz < adj_target) and (adj_t3_Radius > 0)) or ((bed_level_oz > adj_target) and (adj_t3_Radius < 0))) adj_t3_Radius = -(adj_t3_Radius / 2);
      //if (bed_level_oz == adj_target) t3_done = true;
      temp = bed_level_ox - adj_target;
      if ((temp <= 0.01) && (temp >= -0.01)) t3_done=true;

      if ((bed_level_oz + 0.0001 > prev_bed_level) and (bed_level_oz - 0.0001 < prev_bed_level) and (adj_target + 0.0001 > prev_target) and (adj_target - 0.0001 < prev_target)) nochange_count ++;
      if (nochange_count > 1)
        {
        SERIAL_ECHOLN("Stuck in Loop.. Exiting");
        t3_done = true;
        }

      SERIAL_ECHO(" target:");
      SERIAL_PROTOCOL_F(adj_target,6);
      SERIAL_ECHO(" oz:");
      SERIAL_PROTOCOL_F(bed_level_oz,6);
      SERIAL_ECHO(" tower radius adj:");
      SERIAL_PROTOCOL_F(tower_adj[5], 8);
      if (t3_done == true) SERIAL_ECHOLN(" done:true"); else SERIAL_ECHOLN(" done:false");
      }

   } while ((t1_done == false) or (t2_done == false) or (t3_done == false));
}

void adj_tower_delta(int tower)
{
   float adj_val = 0;
   float adj_mag = 0.2;
   float adj_prv;

   do  {

	tower_adj[tower - 1] += adj_val;
	set_delta_constants();

  	if ((tower == 1) or (tower == 3)) bed_level_oy = probe_bed(-SIN_60 * bed_radius, COS_60 * bed_radius);
    	if ((tower == 1) or (tower == 2)) bed_level_oz = probe_bed(0.0, -bed_radius);
    	if ((tower == 2) or (tower == 3)) bed_level_ox = probe_bed(SIN_60 * bed_radius, COS_60 * bed_radius);

        adj_prv = adj_val;
        adj_val = 0;

        if (tower == 1)
          {
          //if (bed_level_oy < bed_level_oz - 0.00001) adj_val = adj_mag;
	  //if (bed_level_oy > bed_level_oz + 0.00001) adj_val = -adj_mag;
          if (bed_level_oy < bed_level_oz) adj_val = adj_mag;
	  if (bed_level_oy > bed_level_oz) adj_val = -adj_mag;
          }
        if (tower == 2)
          {
	  if (bed_level_oz < bed_level_ox) adj_val = adj_mag;
	  if (bed_level_oz > bed_level_ox) adj_val = -adj_mag;
          }
        if (tower == 3)
          {
	  if (bed_level_ox < bed_level_oy) adj_val = adj_mag;
	  if (bed_level_ox > bed_level_oy) adj_val = -adj_mag;
	  }

        if ((adj_val > 0) and (adj_prv < 0))
	  {
	  adj_mag = adj_mag / 2;
          adj_val = adj_mag;
	  }
        if ((adj_val < 0) and (adj_prv > 0))
          {
          adj_mag = adj_mag / 2;
          adj_val = -adj_mag;
          }

        //Show Adjustments made
        if (tower == 1)
          {
          SERIAL_ECHO("oy:");
	  SERIAL_PROTOCOL_F(bed_level_oy,4);
          SERIAL_ECHO(" oz:");
	  SERIAL_PROTOCOL_F(bed_level_oz,4);
          }
        if (tower == 2)
          {
          SERIAL_ECHO("ox:");
	  SERIAL_PROTOCOL_F(bed_level_ox,4);
          SERIAL_ECHO(" oz:");
	  SERIAL_PROTOCOL_F(bed_level_oz,4);
          }
        if (tower == 3)
          {
          SERIAL_ECHO("ox:");
	  SERIAL_PROTOCOL_F(bed_level_ox,4);
          SERIAL_ECHO(" oy:");
	  SERIAL_PROTOCOL_F(bed_level_oy,4);
          }
        SERIAL_ECHO(" tower delta adj:");
        SERIAL_PROTOCOL_F(adj_val,5);
        SERIAL_ECHOLN("");

	} while(adj_val != 0);
}

float adj_diagrod_length()
{
  float adj_val = 0;
  float adj_mag = 0.2;
  float adj_prv, target;
  float prev_diag_rod = delta_diagonal_rod;

  do {
     delta_diagonal_rod += adj_val;
     set_delta_constants();

     bed_level_oy = probe_bed(-SIN_60 * bed_radius, COS_60 * bed_radius);
     bed_level_oz = probe_bed(0.0, -bed_radius);
     bed_level_ox = probe_bed(SIN_60 * bed_radius, COS_60 * bed_radius);
     bed_level_c = probe_bed(0,0);

     target = (bed_level_ox + bed_level_oy + bed_level_oz) / 3;
     adj_prv = adj_val;
     adj_val = 0;

     if (bed_level_c - 0.005 < target) adj_val = -adj_mag;
     if (bed_level_c + 0.005 > target) adj_val = adj_mag;
     //if ((bed_level_c - ac_prec < target) and (bed_level_c + ac_prec > target)) adj_val = 0;

     if (((adj_val > 0) and (adj_prv < 0)) or ((adj_val <0) and (adj_prv > 0)))
        {
	adj_val = adj_val / 2;
	adj_mag = adj_mag / 2;
	}

     if ((bed_level_c - 0.005 < target) and (bed_level_c + 0.005 > target)) adj_val = 0;

     //If adj magnatude is very small.. quit adjusting
     if ((abs(adj_val) < 0.001) and (adj_val != 0)) adj_val = 0;

     SERIAL_ECHO("target:");
     SERIAL_PROTOCOL_F(target,4);
     SERIAL_ECHO(" c:");
     SERIAL_PROTOCOL_F(bed_level_c,4);
     SERIAL_ECHO(" adj:");
     SERIAL_PROTOCOL_F(adj_val, 5);
     SERIAL_ECHOLN("");
     } while(adj_val != 0);
     return (delta_diagonal_rod - prev_diag_rod);
}
/* //**PJR re-write to use run_z_probe()
float z_probe() {
  feedrate = AUTOCAL_TRAVELRATE * 60;
  prepare_move(); //**PJR - This is safe to use if the bed level array is cleared.  It is necessary to store new values in a temp array for G29 to avoid affecting probed heights.
  //prepare_move_raw(); //**PJR - This avoids influence from the bed height array as it is created but means you need extra probe-lift clearance for safe travel moves (all parabolas).
  st_synchronize();

  enable_endstops(true);
  float start_z = current_position[Z_AXIS];
  long start_steps = st_get_position(Z_AXIS);

  feedrate = AUTOCAL_PROBERATE * 60;
  destination[Z_AXIS] = -20;
  prepare_move_raw();
  st_synchronize();
  endstops_hit_on_purpose();

  enable_endstops(false);
  long stop_steps = st_get_position(Z_AXIS);

  //saved_position[X_AXIS] = float((st_get_position(X_AXIS)) / axis_steps_per_unit[X_AXIS]);
  //saved_position[Y_AXIS] = float((st_get_position(Y_AXIS)) / axis_steps_per_unit[Y_AXIS]);
  //saved_position[Z_AXIS] = float((st_get_position(Z_AXIS)) / axis_steps_per_unit[Z_AXIS]);

  float mm = start_z -
    float(start_steps - stop_steps) / axis_steps_per_unit[Z_AXIS];
  current_position[Z_AXIS] = mm;
  calculate_delta(current_position);
  plan_set_position(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS],
		    current_position[E_AXIS]);

  for(int8_t i=0; i < 3; i++) {
    saved_position[i] = float(st_get_position(i) / axis_steps_per_unit[i]);
    }

  feedrate = AUTOCAL_PROBERATE * 60;
  destination[Z_AXIS] = mm + AUTOCAL_PROBELIFT;
  prepare_move_raw();
  st_synchronize();
  return mm;
}
*/
//Sorting function (Author: Bill Gentles, Nov. 12, 2010)
void isort(double *a, int n)
// *a is an array pointer function
{
  for (int i = 1; i < n; ++i) {
double j = a[i];
int k;
for (k = i - 1; (k >= 0) && (j < a[k]); k--) {
      a[k + 1] = a[k];
    }
    a[k + 1] = j;
  }
}

//Mode function, returning the mode or median
double probe_mode(double *x,int n){
  int i = 0;
  int count = 0;
  int maxCount = 0;
  int prevCount = 0;
  double mode = NULL;
  int bimodal;

  while(i<(n-1)){
    count=0;
    while(x[i]==x[i+1]){
      count++;
      i++;
    }
    if(count>0 & count>=maxCount){
      mode=x[i];
      if(count>maxCount){
        bimodal=0;
      }
      prevCount=maxCount;
      maxCount=count;
    }
    if(count>0 & prevCount==maxCount){//If the dataset has 2 or more modes.
      bimodal=1;
    }
    if(count==0){
      i++;
    }
   }
   if(mode==NULL||bimodal==1){//Return the median if there is no mode.
      mode=x[(n/2)];
   }
   return mode;
}

//**PJR - Re-written to use probe_pt(x, y, z) returns measured bed level corrected for probe offset
float probe_bed(float x, float y)
  {
    float probe_z;
    //**PJR - Lift the probe if below minimum level (eg sat on bed after a previous probing)
    if (current_position[Z_AXIS] < (AUTOCAL_PROBELIFT - z_probe_offset[Z_AXIS]))
    {
      feedrate = AUTOCAL_TRAVELRATE * 60;
      destination[X_AXIS] = current_position[X_AXIS];
      destination[Y_AXIS] = current_position[Y_AXIS];
      destination[Z_AXIS] = current_position[Z_AXIS] + AUTOCAL_PROBELIFT;
      prepare_move();
    }

    //**PJR - Move to probing point using a delta safe move.
    feedrate = AUTOCAL_TRAVELRATE * 60;
    destination[X_AXIS] = x - z_probe_offset[X_AXIS];
    destination[Y_AXIS] = y - z_probe_offset[Y_AXIS];
    destination[Z_AXIS] = current_position[Z_AXIS];
    prepare_move();
    st_synchronize();

    //**PJR - Probe bed at specified location and return z height of bed
    probe_z = probe_pt(x, y, current_position[Z_AXIS]) + z_probe_offset[Z_AXIS];

    bed_safe_z = probe_z - z_probe_offset[Z_AXIS] + AUTOCAL_PROBELIFT; //3; //2; **PJR - probe_z is the *nozzle* position - we need to allow for deployed probe as well

    //**PJR - Make sure we keep things monitored during auto-calibrate operations
    manage_heater();
    manage_inactivity();
    lcd_update();

    return probe_z;
  }


void bed_probe_all()
  {
  //Do inital move to safe z level above bed
  feedrate = AUTOCAL_TRAVELRATE * 60; // homing_feedrate[Z_AXIS];
  destination[Z_AXIS] = bed_safe_z;
  prepare_move_raw();
  st_synchronize();

  //Probe all bed positions & store carriage positions
  bed_level_c = probe_bed(0.0, 0.0);
  save_carriage_positions(0);
  //bed_safe_z = bed_level_c + 2;
  //SERIAL_ECHOPAIR("6.bed_safe_z = ",bed_safe_z);
  bed_level_z = probe_bed(0.0, bed_radius);
  save_carriage_positions(1);
  bed_level_oy = probe_bed(-SIN_60 * bed_radius, COS_60 * bed_radius);
  save_carriage_positions(2);
  bed_level_x = probe_bed(-SIN_60 * bed_radius, -COS_60 * bed_radius);
  save_carriage_positions(3);
  bed_level_oz = probe_bed(0.0, -bed_radius);
  save_carriage_positions(4);
  bed_level_y = probe_bed(SIN_60 * bed_radius, -COS_60 * bed_radius);
  save_carriage_positions(5);
  bed_level_ox = probe_bed(SIN_60 * bed_radius, COS_60 * bed_radius);
  save_carriage_positions(6);
  }

void calibration_report()
  {
  //Display Report
  SERIAL_ECHOLN("|\tZ-Tower\t\t\tEndstop Offsets");

  SERIAL_ECHO("| \t");
  if (bed_level_z >=0) {SERIAL_ECHO(" ");}
  SERIAL_PROTOCOL_F(bed_level_z, 4);
  SERIAL_ECHOPAIR("\t\t\tX:",endstop_adj[0]);
  SERIAL_ECHOPAIR(" Y:",endstop_adj[1]);
  SERIAL_ECHOPAIR(" Z:",endstop_adj[2]);
  SERIAL_ECHOLN("");

  SERIAL_ECHO("| ");
  SERIAL_PROTOCOL_F(bed_level_oy, 4);
  //if (bed_level_oy >= 0) SERIAL_ECHO("\t");
  SERIAL_ECHO("\t\t");
  //if (bed_level_ox >= 0) SERIAL_ECHO(" ");
  SERIAL_PROTOCOL_F(bed_level_ox, 4);
  SERIAL_ECHO("\tTower Offsets");  // **PJR - Try one less tab
  SERIAL_ECHOLN("");

  SERIAL_PROTOCOLPGM("| \t");
  if (bed_level_c >= 0) SERIAL_ECHO(" ");
  SERIAL_PROTOCOL_F(bed_level_c,4);
  SERIAL_ECHOPAIR("\t\t\tA:",tower_adj[0]);
  SERIAL_ECHOPAIR(" b:",tower_adj[1]); // Either Repetier Host or ECHOPAIR barfs on the string containing "B"
  SERIAL_ECHOPAIR(" C:",tower_adj[2]);
  SERIAL_ECHOLN(" ");

  SERIAL_ECHO("| ");
  SERIAL_PROTOCOL_F(bed_level_x, 4);
  //if (bed_level_x >= 0) SERIAL_ECHO("\t");
  SERIAL_ECHO("\t\t");
  //if (bed_level_y >=0) SERIAL_ECHO("\t ");
  SERIAL_PROTOCOL_F(bed_level_y, 4);
  SERIAL_ECHOPAIR("\tI:",tower_adj[3]);  // **PJR - Try one less tab
  SERIAL_ECHOPAIR(" J:",tower_adj[4]);
  SERIAL_ECHOPAIR(" K:",tower_adj[5]);
  SERIAL_ECHOLN("");

  SERIAL_PROTOCOLPGM("| \t");
  if (bed_level_oz >=0) {SERIAL_ECHO(" ");}
  SERIAL_PROTOCOL_F(bed_level_oz, 4);
  SERIAL_PROTOCOLPGM("\t\t\tDelta Radius: ");
  SERIAL_PROTOCOL_F(delta_radius, 4);
  SERIAL_ECHOLN("");

  SERIAL_ECHO("| X-Tower\t\tY-Tower\tDiagonal Rod: "); // **PJR - Try one less tab
  SERIAL_PROTOCOL_F(delta_diagonal_rod, 4);
  SERIAL_ECHOLN("");
  SERIAL_ECHOLN("");
}

void save_carriage_positions(int position_num) {
  for(int8_t i=0; i < 3; i++) {
    saved_positions[position_num][i] = saved_position[i];
  }
}

void home_delta_axis() {
    saved_feedrate = feedrate;
    saved_feedmultiply = feedmultiply;
    feedmultiply = 100;
    previous_millis_cmd = millis();

    enable_endstops(true);

    for(int8_t i=0; i < NUM_AXIS; i++) {
      destination[i] = current_position[i];
      }
    feedrate = 0.0;
    // Move all carriages up together until the first endstop is hit.
    current_position[X_AXIS] = 0;
    current_position[Y_AXIS] = 0;
    current_position[Z_AXIS] = 0;
    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);

    destination[X_AXIS] = 3 * max_length[Z_AXIS];
    destination[Y_AXIS] = 3 * max_length[Z_AXIS];
    destination[Z_AXIS] = 3 * max_length[Z_AXIS];
    feedrate = 1.732 * homing_feedrate[X_AXIS];
    plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate/60, active_extruder);
    st_synchronize();
    endstops_hit_on_purpose();

    current_position[X_AXIS] = destination[X_AXIS];
    current_position[Y_AXIS] = destination[Y_AXIS];
    current_position[Z_AXIS] = destination[Z_AXIS];

    // take care of back off and rehome now we are all at the top
    HOMEAXIS(X);
    HOMEAXIS(Y);
    HOMEAXIS(Z);

    calculate_delta(current_position);
    plan_set_position(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS]);

    #ifdef ENDSTOPS_ONLY_FOR_HOMING
       enable_endstops(false);
    #endif

    feedrate = saved_feedrate;
    feedmultiply = saved_feedmultiply;
    previous_millis_cmd = millis();
    endstops_hit_on_purpose();
}

void refresh_cmd_timeout(void)
{
  previous_millis_cmd = millis();
}

#ifdef FWRETRACT
  void retract(bool retracting, bool swapretract = false) {
    if(retracting && !retracted[active_extruder]) {
      destination[X_AXIS]=current_position[X_AXIS];
      destination[Y_AXIS]=current_position[Y_AXIS];
      destination[Z_AXIS]=current_position[Z_AXIS];
      destination[E_AXIS]=current_position[E_AXIS];
      if (swapretract) {
        current_position[E_AXIS]+=retract_length_swap/volumetric_multiplier[active_extruder];
      } else {
        current_position[E_AXIS]+=retract_length/volumetric_multiplier[active_extruder];
      }
      plan_set_e_position(current_position[E_AXIS]);
      float oldFeedrate = feedrate;
      feedrate=retract_feedrate*60;
      retracted[active_extruder]=true;
      prepare_move();
      current_position[Z_AXIS]-=retract_zlift;
#ifdef DELTA
      calculate_delta(current_position); // change cartesian kinematic to  delta kinematic;
      plan_set_position(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS]);
#else
      plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
#endif
      prepare_move();
      feedrate = oldFeedrate;
    } else if(!retracting && retracted[active_extruder]) {
      destination[X_AXIS]=current_position[X_AXIS];
      destination[Y_AXIS]=current_position[Y_AXIS];
      destination[Z_AXIS]=current_position[Z_AXIS];
      destination[E_AXIS]=current_position[E_AXIS];
      current_position[Z_AXIS]+=retract_zlift;
#ifdef DELTA
      calculate_delta(current_position); // change cartesian kinematic  to  delta kinematic;
      plan_set_position(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS]);
#else
      plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
#endif
      //prepare_move();
      if (swapretract) {
        current_position[E_AXIS]-=(retract_length_swap+retract_recover_length_swap)/volumetric_multiplier[active_extruder];
      } else {
        current_position[E_AXIS]-=(retract_length+retract_recover_length)/volumetric_multiplier[active_extruder];
      }
      plan_set_e_position(current_position[E_AXIS]);
      float oldFeedrate = feedrate;
      feedrate=retract_recover_feedrate*60;
      retracted[active_extruder]=false;
      prepare_move();
      feedrate = oldFeedrate;
    }
  } //retract
#endif //FWRETRACT

#ifdef Z_PROBE_SLED
//
// Method to dock/undock a sled designed by Charles Bell.
//
// dock[in]     If true, move to MAX_X and engage the electromagnet
// offset[in]   The additional distance to move to adjust docking location
//
static void dock_sled(bool dock, int offset=0) {
 int z_loc;

 if (!((axis_known_position[X_AXIS]) && (axis_known_position[Y_AXIS]))) {
   LCD_MESSAGEPGM(MSG_POSITION_UNKNOWN);
   SERIAL_ECHO_START;
   SERIAL_ECHOLNPGM(MSG_POSITION_UNKNOWN);
   return;
 }

 if (dock) {
   do_blocking_move_to(X_MAX_POS + SLED_DOCKING_OFFSET + offset,
                       current_position[Y_AXIS],
                       current_position[Z_AXIS]);
   // turn off magnet
   digitalWrite(SERVO0_PIN, LOW);
 } else {
   if (current_position[Z_AXIS] < (Z_RAISE_BEFORE_PROBING + 5))
     z_loc = Z_RAISE_BEFORE_PROBING;
   else
     z_loc = current_position[Z_AXIS];
   do_blocking_move_to(X_MAX_POS + SLED_DOCKING_OFFSET + offset,
                       Y_PROBE_OFFSET_FROM_EXTRUDER, z_loc);
   // turn on magnet
   digitalWrite(SERVO0_PIN, HIGH);
 }
}
#endif

void process_commands()
{
  unsigned long codenum; //throw away variable
  char *starpos = NULL;
#ifdef ENABLE_AUTO_BED_LEVELING
  float x_tmp, y_tmp, z_tmp, real_z;
#endif
  if(code_seen('G'))
  {
    switch((int)code_value())
    {
    case 0: // G0 -> G1
    case 1: // G1
      if(Stopped == false) {
        get_coordinates(); // For X Y Z E F
          #ifdef FWRETRACT
            if(autoretract_enabled)
            if( !(code_seen('X') || code_seen('Y') || code_seen('Z')) && code_seen('E')) {
              float echange=destination[E_AXIS]-current_position[E_AXIS];
              if((echange<-MIN_RETRACT && !retracted) || (echange>MIN_RETRACT && retracted)) { //move appears to be an attempt to retract or recover
                  current_position[E_AXIS] = destination[E_AXIS]; //hide the slicer-generated retract/recover from calculations
                  plan_set_e_position(current_position[E_AXIS]); //AND from the planner
                  retract(!retracted);
                  return;
              }
            }
          #endif //FWRETRACT
        prepare_move();
        //ClearToSend();
      }
      break;
#ifndef SCARA //disable arc support
    case 2: // G2  - CW ARC
      if(Stopped == false) {
        get_arc_coordinates();
        prepare_arc_move(true);
      }
      break;
    case 3: // G3  - CCW ARC
      if(Stopped == false) {
        get_arc_coordinates();
        prepare_arc_move(false);
      }
      break;
#endif
    case 4: // G4 dwell
      LCD_MESSAGEPGM(MSG_DWELL);
      codenum = 0;
      if(code_seen('P')) codenum = code_value(); // milliseconds to wait
      if(code_seen('S')) codenum = code_value() * 1000; // seconds to wait

      st_synchronize();
      codenum += millis();  // keep track of when we started waiting
      previous_millis_cmd = millis();
      while(millis() < codenum) {
        manage_heater();
        manage_inactivity();
        lcd_update();
      }
      break;
      #ifdef FWRETRACT
      case 10: // G10 retract
       #if EXTRUDERS > 1
        retracted_swap[active_extruder]=(code_seen('S') && code_value_long() == 1); // checks for swap retract argument
        retract(true,retracted_swap[active_extruder]);
       #else
        retract(true);
       #endif
      break;
      case 11: // G11 retract_recover
       #if EXTRUDERS > 1
        retract(false,retracted_swap[active_extruder]);
       #else
        retract(false);
       #endif
      break;
      #endif //FWRETRACT
    case 28: //G28 Home all Axis one at a time
#ifdef ENABLE_AUTO_BED_LEVELING
      plan_bed_level_matrix.set_to_identity();  //Reset the plane ("erase" all leveling data)
#endif //ENABLE_AUTO_BED_LEVELING

#ifdef NONLINEAR_BED_LEVELING
      reset_bed_level();
#endif //NONLINEAR_BED_LEVELING

      saved_feedrate = feedrate;
      saved_feedmultiply = feedmultiply;
      feedmultiply = 100;
      previous_millis_cmd = millis();

      enable_endstops(true);

      for(int8_t i=0; i < NUM_AXIS; i++) {
        destination[i] = current_position[i];
      }
      feedrate = 0.0;

#ifdef DELTA
          // A delta can only safely home all axis at the same time
          // all axis have to home at the same time

          // Move all carriages up together until the first endstop is hit.
          current_position[X_AXIS] = 0;
          current_position[Y_AXIS] = 0;
          current_position[Z_AXIS] = 0;
          plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);

          destination[X_AXIS] = 3 * max_length[Z_AXIS];
          destination[Y_AXIS] = 3 * max_length[Z_AXIS];
          destination[Z_AXIS] = 3 * max_length[Z_AXIS];
          feedrate = 1.732 * homing_feedrate[X_AXIS];
          plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate/60, active_extruder);
          st_synchronize();
          endstops_hit_on_purpose();

          current_position[X_AXIS] = destination[X_AXIS];
          current_position[Y_AXIS] = destination[Y_AXIS];
          current_position[Z_AXIS] = destination[Z_AXIS];

          // take care of back off and rehome now we are all at the top
          HOMEAXIS(X);
          HOMEAXIS(Y);
          HOMEAXIS(Z);

          calculate_delta(current_position);
          plan_set_position(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS]);

#else // NOT DELTA

      home_all_axis = !((code_seen(axis_codes[X_AXIS])) || (code_seen(axis_codes[Y_AXIS])) || (code_seen(axis_codes[Z_AXIS])));

      #if Z_HOME_DIR > 0                      // If homing away from BED do Z first
      if((home_all_axis) || (code_seen(axis_codes[Z_AXIS]))) {
        HOMEAXIS(Z);
      }
      #endif

      #ifdef QUICK_HOME
      if((home_all_axis)||( code_seen(axis_codes[X_AXIS]) && code_seen(axis_codes[Y_AXIS])) )  //first diagonal move
      {
        current_position[X_AXIS] = 0;current_position[Y_AXIS] = 0;

       #ifndef DUAL_X_CARRIAGE
        int x_axis_home_dir = home_dir(X_AXIS);
       #else
        int x_axis_home_dir = x_home_dir(active_extruder);
        extruder_duplication_enabled = false;
       #endif

        plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
        destination[X_AXIS] = 1.5 * max_length[X_AXIS] * x_axis_home_dir;destination[Y_AXIS] = 1.5 * max_length[Y_AXIS] * home_dir(Y_AXIS);
        feedrate = homing_feedrate[X_AXIS];
        if(homing_feedrate[Y_AXIS]<feedrate)
          feedrate = homing_feedrate[Y_AXIS];
        if (max_length[X_AXIS] > max_length[Y_AXIS]) {
          feedrate *= sqrt(pow(max_length[Y_AXIS] / max_length[X_AXIS], 2) + 1);
        } else {
          feedrate *= sqrt(pow(max_length[X_AXIS] / max_length[Y_AXIS], 2) + 1);
        }
        plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate/60, active_extruder);
        st_synchronize();

        axis_is_at_home(X_AXIS);
        axis_is_at_home(Y_AXIS);
        plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
        destination[X_AXIS] = current_position[X_AXIS];
        destination[Y_AXIS] = current_position[Y_AXIS];
        plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate/60, active_extruder);
        feedrate = 0.0;
        st_synchronize();
        endstops_hit_on_purpose();

        current_position[X_AXIS] = destination[X_AXIS];
        current_position[Y_AXIS] = destination[Y_AXIS];
		#ifndef SCARA
        current_position[Z_AXIS] = destination[Z_AXIS];
		#endif
      }
      #endif

      if((home_all_axis) || (code_seen(axis_codes[X_AXIS])))
      {
      #ifdef DUAL_X_CARRIAGE
        int tmp_extruder = active_extruder;
        extruder_duplication_enabled = false;
        active_extruder = !active_extruder;
        HOMEAXIS(X);
        inactive_extruder_x_pos = current_position[X_AXIS];
        active_extruder = tmp_extruder;
        HOMEAXIS(X);
        // reset state used by the different modes
        memcpy(raised_parked_position, current_position, sizeof(raised_parked_position));
        delayed_move_time = 0;
        active_extruder_parked = true;
      #else
        HOMEAXIS(X);
      #endif
      }

      if((home_all_axis) || (code_seen(axis_codes[Y_AXIS]))) {
        HOMEAXIS(Y);
      }

      if(code_seen(axis_codes[X_AXIS]))
      {
        if(code_value_long() != 0) {
		#ifdef SCARA
		   current_position[X_AXIS]=code_value();
		#else
		   current_position[X_AXIS]=code_value()+add_homing[X_AXIS];
		#endif
        }
      }

      if(code_seen(axis_codes[Y_AXIS])) {
        if(code_value_long() != 0) {
         #ifdef SCARA
		   current_position[Y_AXIS]=code_value();
		#else
		   current_position[Y_AXIS]=code_value()+add_homing[Y_AXIS];
		#endif
        }
      }

      #if Z_HOME_DIR < 0                      // If homing towards BED do Z last
        #ifndef Z_SAFE_HOMING
          if((home_all_axis) || (code_seen(axis_codes[Z_AXIS]))) {
            #if defined (Z_RAISE_BEFORE_HOMING) && (Z_RAISE_BEFORE_HOMING > 0)
              destination[Z_AXIS] = Z_RAISE_BEFORE_HOMING * home_dir(Z_AXIS) * (-1);    // Set destination away from bed
              feedrate = max_feedrate[Z_AXIS];
              plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate, active_extruder);
              st_synchronize();
            #endif
            HOMEAXIS(Z);
          }
        #else                      // Z Safe mode activated.
          if(home_all_axis) {
            destination[X_AXIS] = round(Z_SAFE_HOMING_X_POINT - X_PROBE_OFFSET_FROM_EXTRUDER);
            destination[Y_AXIS] = round(Z_SAFE_HOMING_Y_POINT - Y_PROBE_OFFSET_FROM_EXTRUDER);
            destination[Z_AXIS] = Z_RAISE_BEFORE_HOMING * home_dir(Z_AXIS) * (-1);    // Set destination away from bed
            feedrate = XY_TRAVEL_SPEED;
            current_position[Z_AXIS] = 0;

            plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
            plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate, active_extruder);
            st_synchronize();
            current_position[X_AXIS] = destination[X_AXIS];
            current_position[Y_AXIS] = destination[Y_AXIS];

            HOMEAXIS(Z);
          }
                                                // Let's see if X and Y are homed and probe is inside bed area.
          if(code_seen(axis_codes[Z_AXIS])) {
            if ( (axis_known_position[X_AXIS]) && (axis_known_position[Y_AXIS]) \
              && (current_position[X_AXIS]+X_PROBE_OFFSET_FROM_EXTRUDER >= X_MIN_POS) \
              && (current_position[X_AXIS]+X_PROBE_OFFSET_FROM_EXTRUDER <= X_MAX_POS) \
              && (current_position[Y_AXIS]+Y_PROBE_OFFSET_FROM_EXTRUDER >= Y_MIN_POS) \
              && (current_position[Y_AXIS]+Y_PROBE_OFFSET_FROM_EXTRUDER <= Y_MAX_POS)) {

              current_position[Z_AXIS] = 0;
              plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
              destination[Z_AXIS] = Z_RAISE_BEFORE_HOMING * home_dir(Z_AXIS) * (-1);    // Set destination away from bed
              feedrate = max_feedrate[Z_AXIS];
              plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate, active_extruder);
              st_synchronize();

              HOMEAXIS(Z);
            } else if (!((axis_known_position[X_AXIS]) && (axis_known_position[Y_AXIS]))) {
                LCD_MESSAGEPGM(MSG_POSITION_UNKNOWN);
                SERIAL_ECHO_START;
                SERIAL_ECHOLNPGM(MSG_POSITION_UNKNOWN);
            } else {
                LCD_MESSAGEPGM(MSG_ZPROBE_OUT);
                SERIAL_ECHO_START;
                SERIAL_ECHOLNPGM(MSG_ZPROBE_OUT);
            }
          }
        #endif
      #endif



      if(code_seen(axis_codes[Z_AXIS])) {
        if(code_value_long() != 0) {
          current_position[Z_AXIS]=code_value()+add_homing[Z_AXIS];
        }
      }
      #ifdef ENABLE_AUTO_BED_LEVELING
        if((home_all_axis) || (code_seen(axis_codes[Z_AXIS]))) {
          current_position[Z_AXIS] += zprobe_zoffset;  //Add Z_Probe offset (the distance is negative)
        }
      #endif
      plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
#endif // else DELTA

#ifdef SCARA
	  calculate_delta(current_position);
      plan_set_position(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS]);
#endif // SCARA

      #ifdef ENDSTOPS_ONLY_FOR_HOMING
        enable_endstops(false);
      #endif

      feedrate = saved_feedrate;
      feedmultiply = saved_feedmultiply;
      previous_millis_cmd = millis();
      endstops_hit_on_purpose();
      break;

#ifdef ENABLE_AUTO_BED_LEVELING
    case 29: // G29 Detailed Z-Probe, probes the bed at 3 or more points.
        {
            #if Z_MIN_PIN == -1
            #error "You must have a Z_MIN endstop in order to enable Auto Bed Leveling feature!!! Z_MIN_PIN must point to a valid hardware pin."
            #endif

            // Prevent user from running a G29 without first homing in X and Y
            if (! (axis_known_position[X_AXIS] && axis_known_position[Y_AXIS]) )
            {
                LCD_MESSAGEPGM(MSG_POSITION_UNKNOWN);
                SERIAL_ECHO_START;
                SERIAL_ECHOLNPGM(MSG_POSITION_UNKNOWN);
                break; // abort G29, since we don't know where we are
            }

#ifdef Z_PROBE_SLED
            dock_sled(false);
#endif // Z_PROBE_SLED
            st_synchronize();
            // make sure the bed_level_rotation_matrix is identity or the planner will get it incorectly
            //vector_3 corrected_position = plan_get_position_mm();
            //corrected_position.debug("position before G29");
            plan_bed_level_matrix.set_to_identity();

          #ifdef NONLINEAR_BED_LEVELING
            reset_bed_level();
          #else //not defined NONLINEAR_BED_LEVELING
            vector_3 uncorrected_position = plan_get_position();
            //uncorrected_position.debug("position durring G29");
            current_position[X_AXIS] = uncorrected_position.x;
            current_position[Y_AXIS] = uncorrected_position.y;
            current_position[Z_AXIS] = uncorrected_position.z;
            plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
          #endif //NONLINEAR_BED_LEVELING

          #ifndef SERVO_ENDSTOPS
            engage_z_probe();   // Engage Z probe by moving the end effector.
          #endif //SERVO_ENDSTOPS

            setup_for_endstop_move();

            feedrate = homing_feedrate[Z_AXIS];
#ifdef AUTO_BED_LEVELING_GRID
            // solve the plane equation ax + by + d = z
            // A is the matrix with rows [x y 1] for all the probed points
            // B is the vector of the Z positions
            // the normal vector to the plane is formed by the coefficients of the plane equation in the standard form, which is Vx*x+Vy*y+Vz*z+d = 0
            // so Vx = -a Vy = -b Vz = 1 (we want the vector facing towards positive Z

            // "A" matrix of the linear system of equations
            double eqnAMatrix[AUTO_BED_LEVELING_GRID_POINTS*AUTO_BED_LEVELING_GRID_POINTS*3];
            // "B" vector of Z points
            double eqnBVector[AUTO_BED_LEVELING_GRID_POINTS*AUTO_BED_LEVELING_GRID_POINTS];

            #ifdef NONLINEAR_BED_LEVELING
            float z_offset = Z_PROBE_OFFSET_FROM_EXTRUDER;
            if (code_seen(axis_codes[Z_AXIS])) {
              z_offset += code_value();
            }
            #endif //NONLINEAR_BED_LEVELING

            int probePointCounter = 0;
            for (int yCount=0; yCount < AUTO_BED_LEVELING_GRID_POINTS; yCount++)
            {
              float yProbe = FRONT_PROBE_BED_POSITION + AUTO_BED_LEVELING_GRID_Y * yCount;
              int xStart, xStop, xInc;
              if (yCount % 2) {
                xStart = 0;
                xStop = AUTO_BED_LEVELING_GRID_POINTS;
                xInc = 1;
              } else {
                xStart = AUTO_BED_LEVELING_GRID_POINTS - 1;
                xStop = -1;
                xInc = -1;
              }

              for (int xCount=xStart; xCount != xStop; xCount += xInc)
              {
                float xProbe = LEFT_PROBE_BED_POSITION + AUTO_BED_LEVELING_GRID_X * xCount;
                float z_before;
                if (probePointCounter == 0)
                {
                  // raise before probing
                  z_before = Z_RAISE_BEFORE_PROBING;
                } else
                {
                  // raise extruder
                  z_before = current_position[Z_AXIS] + Z_RAISE_BETWEEN_PROBINGS;
                }

                #ifdef DELTA
                // Avoid probing the corners (outside the round or hexagon print surface) on a delta printer.
                float distance_from_center = sqrt(xProbe*xProbe + yProbe*yProbe);
               /* SERIAL_PROTOCOLPGM("DELTA_RADIUS: ");
                SERIAL_PROTOCOL(DELTA_PROBABLE_RADIUS);
                SERIAL_PROTOCOLPGM("  X: ");
                SERIAL_PROTOCOL(xProbe);
                SERIAL_PROTOCOLPGM(" Y: ");
                SERIAL_PROTOCOL(yProbe);
                SERIAL_PROTOCOLPGM(" DIST: ");
                SERIAL_PROTOCOLLN(distance_from_center); */
                if (distance_from_center > DELTA_PROBABLE_RADIUS) continue;
                
                #endif //DELTA

                float measured_z = probe_pt(xProbe, yProbe, z_before);

                #ifdef NONLINEAR_BED_LEVELING
                // @todo: take x and y offset into account
                bed_level[xCount][yCount] = measured_z + z_offset;
                #endif //NONLINEAR_BED_LEVELING

                eqnBVector[probePointCounter] = measured_z;

                eqnAMatrix[probePointCounter + 0*AUTO_BED_LEVELING_GRID_POINTS*AUTO_BED_LEVELING_GRID_POINTS] = xProbe;
                eqnAMatrix[probePointCounter + 1*AUTO_BED_LEVELING_GRID_POINTS*AUTO_BED_LEVELING_GRID_POINTS] = yProbe;
                eqnAMatrix[probePointCounter + 2*AUTO_BED_LEVELING_GRID_POINTS*AUTO_BED_LEVELING_GRID_POINTS] = 1;
                probePointCounter++;

                manage_heater();
                manage_inactivity();
                lcd_update();
              }
            }
            clean_up_after_endstop_move();

          #ifdef NONLINEAR_BED_LEVELING
            extrapolate_unprobed_bed_level();
            print_bed_level();
          #else //NONLINEAR_BED_LEVELING
            // solve lsq problem
            double *plane_equation_coefficients = qr_solve(AUTO_BED_LEVELING_GRID_POINTS*AUTO_BED_LEVELING_GRID_POINTS, 3, eqnAMatrix, eqnBVector);

            SERIAL_PROTOCOLPGM("Eqn coefficients: a: ");
            SERIAL_PROTOCOL(plane_equation_coefficients[0]);
            SERIAL_PROTOCOLPGM(" b: ");
            SERIAL_PROTOCOL(plane_equation_coefficients[1]);
            SERIAL_PROTOCOLPGM(" d: ");
            SERIAL_PROTOCOLLN(plane_equation_coefficients[2]);


            set_bed_level_equation_lsq(plane_equation_coefficients);

            free(plane_equation_coefficients);
          #endif //NONLINEAR_BED_LEVELING

#else // AUTO_BED_LEVELING_GRID not defined

            // Probe at 3 arbitrary points
            // probe 1
            float z_at_pt_1 = probe_pt(ABL_PROBE_PT_1_X, ABL_PROBE_PT_1_Y, Z_RAISE_BEFORE_PROBING);

            // probe 2
            float z_at_pt_2 = probe_pt(ABL_PROBE_PT_2_X, ABL_PROBE_PT_2_Y, current_position[Z_AXIS] + Z_RAISE_BETWEEN_PROBINGS);

            // probe 3
            float z_at_pt_3 = probe_pt(ABL_PROBE_PT_3_X, ABL_PROBE_PT_3_Y, current_position[Z_AXIS] + Z_RAISE_BETWEEN_PROBINGS);

            clean_up_after_endstop_move();

            set_bed_level_equation_3pts(z_at_pt_1, z_at_pt_2, z_at_pt_3);


#endif // AUTO_BED_LEVELING_GRID
            st_synchronize();

          #ifndef SERVO_ENDSTOPS
            retract_z_probe();   // Retract Z probe by moving the end effector.
          #endif //SERVO_ENDSTOPS

          #ifndef NONLINEAR_BED_LEVELING
            // The following code correct the Z height difference from z-probe position and hotend tip position.
            // The Z height on homing is measured by Z-Probe, but the probe is quite far from the hotend.
            // When the bed is uneven, this height must be corrected.
            real_z = float(st_get_position(Z_AXIS))/axis_steps_per_unit[Z_AXIS];  //get the real Z (since the auto bed leveling is already correcting the plane)
            x_tmp = current_position[X_AXIS] + X_PROBE_OFFSET_FROM_EXTRUDER;
            y_tmp = current_position[Y_AXIS] + Y_PROBE_OFFSET_FROM_EXTRUDER;
            z_tmp = current_position[Z_AXIS];

            apply_rotation_xyz(plan_bed_level_matrix, x_tmp, y_tmp, z_tmp);         //Apply the correction sending the probe offset
            current_position[Z_AXIS] = z_tmp - real_z + current_position[Z_AXIS];   //The difference is added to current position and sent to planner.
            plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
          #endif //NONLINEAR_BED_LEVELING

#ifdef Z_PROBE_SLED
            dock_sled(true, -SLED_DOCKING_OFFSET); // correct for over travel.
#endif // Z_PROBE_SLED
        }
        break;
#ifndef Z_PROBE_SLED
#ifdef DELTA  //**PJR - Only relevant to Deltabots at the moment - allow single probe later
    case 30: //G30 Delta AutoCalibration

        #if Z_MIN_PIN == -1
        #error "You must have a Z_MIN endstop in order to enable Auto Delta Calibration feature!!! Z_MIN_PIN must point to a valid hardware pin."
        #endif

        // Prevent user from running a G30 without first homing in X and Y
        if (! (axis_known_position[X_AXIS] && axis_known_position[Y_AXIS]) )
        {
            LCD_MESSAGEPGM(MSG_POSITION_UNKNOWN);
            SERIAL_ECHO_START;
            SERIAL_ECHOLNPGM(MSG_POSITION_UNKNOWN);
            break; // abort G30, since we don't know where we are
        }


        st_synchronize();
        // make sure the bed_level_rotation_matrix is identity or the planner will get it incorectly
        //vector_3 corrected_position = plan_get_position_mm();
        //corrected_position.debug("position before G29");
        plan_bed_level_matrix.set_to_identity();

      #ifdef NONLINEAR_BED_LEVELING
        reset_bed_level();
      #else //not defined NONLINEAR_BED_LEVELING
        vector_3 uncorrected_position = plan_get_position();
        //uncorrected_position.debug("position durring G29");
        current_position[X_AXIS] = uncorrected_position.x;
        current_position[Y_AXIS] = uncorrected_position.y;
        current_position[Z_AXIS] = uncorrected_position.z;
        plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
      #endif //NONLINEAR_BED_LEVELING

      if (code_seen('C'))
        {
        //Show carriage positions
        SERIAL_ECHOLN("Carriage Positions for last scan:");
        for(int8_t i=0; i < 7; i++)
          {
          SERIAL_ECHO("[");
          SERIAL_ECHO(saved_positions[i][X_AXIS]);
          SERIAL_ECHO(", ");
          SERIAL_ECHO(saved_positions[i][Y_AXIS]);
          SERIAL_ECHO(", ");
          SERIAL_ECHO(saved_positions[i][Z_AXIS]);
          SERIAL_ECHOLN("]");
          }
        break;
        }
       if (code_seen('X') and code_seen('Y'))
          {
          //Probe specified X,Y point
          float x = code_seen('X') ? code_value():0.00;
          float y = code_seen('Y') ? code_value():0.00;
          float probe_value;

          engage_z_probe();
          probe_value = probe_bed(x, y);
          SERIAL_ECHO("Bed Z-Height at X:");
          SERIAL_ECHO(x);
          SERIAL_ECHO(" Y:");
          SERIAL_ECHO(y);
          SERIAL_ECHO(" = ");
          SERIAL_PROTOCOL_F(probe_value, 4);
          SERIAL_ECHOLN("");

          SERIAL_ECHO("Carriage Positions: [");
          SERIAL_ECHO(saved_position[X_AXIS]);
          SERIAL_ECHO(", ");
          SERIAL_ECHO(saved_position[Y_AXIS]);
          SERIAL_ECHO(", ");
          SERIAL_ECHO(saved_position[Z_AXIS]);
          SERIAL_ECHOLN("]");
          retract_z_probe();
          break;
          }

       saved_feedrate = feedrate;
       saved_feedmultiply = feedmultiply;
       feedmultiply = 100;

       if (code_seen('A'))
         {
         SERIAL_ECHOLN("Starting Auto Calibration..");
         if (code_value() != 0) ac_prec = code_value();
         SERIAL_ECHO("Calibration precision: +/-");
         SERIAL_PROTOCOL_F(ac_prec,3);
         SERIAL_ECHOLN("mm");
         }


       home_delta_axis();
       engage_z_probe();
       bed_safe_z = current_position[Z_AXIS]; //20; // **PJR - Since we are at a safe Z height after engaging the probe

       //Probe all points
       bed_probe_all();

       //Show calibration report
       calibration_report();

       if (code_seen('E'))
         {
         int iteration = 0;

         do {
            iteration ++;
            SERIAL_ECHO("Iteration: ");
            SERIAL_ECHOLN(iteration);

            SERIAL_ECHOLN("Checking/Adjusting endstop offsets");
            adj_endstops();

            bed_probe_all();
            calibration_report();
            } while ((bed_level_x < -ac_prec) or (bed_level_x > ac_prec)
                      or (bed_level_y < -ac_prec) or (bed_level_y > ac_prec)
                      or (bed_level_z < -ac_prec) or (bed_level_z > ac_prec));

          SERIAL_ECHOLN("Endstop adjustment complete");
          }

       if (code_seen('R'))
         {
         int err_tower;
         int iteration = 0;

         do {
            iteration ++;
            SERIAL_ECHO("Iteration: ");
            SERIAL_ECHOLN(iteration);

            SERIAL_ECHOLN("Checking/Adjusting endstop offsets");
            adj_endstops();

            bed_probe_all();
            calibration_report();

            SERIAL_ECHOLN("Checking delta radius");
            adj_deltaradius();

            } while ((bed_level_c < -ac_prec) or (bed_level_c > ac_prec)
                      or (bed_level_x < -ac_prec) or (bed_level_x > ac_prec)
                      or (bed_level_y < -ac_prec) or (bed_level_y > ac_prec)
                      or (bed_level_z < -ac_prec) or (bed_level_z > ac_prec));
         }

       if (code_seen('I'))
         {
         SERIAL_ECHO("Adjusting Tower Delta for tower");
         SERIAL_ECHO(code_value());
         adj_tower_delta(code_value());
         SERIAL_ECHOLN("Tower Delta adjustment complete");
         }

       if (code_seen('D'))
         {
         SERIAL_ECHOLN("Adjusting Diagional Rod Length");
         adj_diagrod_length();
         SERIAL_ECHOLN("Diagional Rod Length adjustment complete");
         }

       if (code_seen('T'))
         {
         SERIAL_ECHOLN("Adjusting Tower Radius for tower");
         SERIAL_ECHO(code_value());
         adj_tower_radius(code_value());
         SERIAL_ECHOLN("Tower Radius adjustment complete");
         }

       if (code_seen('A'))
         {
         int err_tower;
         int iteration = 0;
         int dr_adjusted;
       //do {
         do {
            do {
               iteration ++;
               SERIAL_ECHO("Iteration: ");
               SERIAL_ECHOLN(iteration);

               SERIAL_ECHOLN("Checking/Adjusting endstop offsets");
               adj_endstops();

               bed_probe_all();
               calibration_report();

               if ((bed_level_c < -ac_prec) or (bed_level_c > ac_prec))
                 {
                 SERIAL_ECHOLN("Checking delta radius");
                 dr_adjusted = adj_deltaradius();
                 }
               else dr_adjusted = 0;

               } while ((bed_level_c < -ac_prec) or (bed_level_c > ac_prec)
                         or (bed_level_x < -ac_prec) or (bed_level_x > ac_prec)
                         or (bed_level_y < -ac_prec) or (bed_level_y > ac_prec)
                         or (bed_level_z < -ac_prec) or (bed_level_z > ac_prec)
                         or (dr_adjusted != 0));

             if ((bed_level_ox < -ac_prec) or (bed_level_ox > ac_prec) or
                 (bed_level_oy < -ac_prec) or (bed_level_oy > ac_prec) or
                 (bed_level_oz < -ac_prec) or (bed_level_oz > ac_prec))
               {
               SERIAL_ECHOLN("Checking for tower geometry errors..");
               if (fix_tower_errors() != 0 )
                 {
                 //Tower positions have been changed .. home to endstops
                 SERIAL_ECHOLN("Tower Postions changed .. Homing Endstops");
                 home_delta_axis();
                 bed_safe_z = AUTOCAL_PROBELIFT - z_probe_offset[Z_AXIS];
                 }
               else
                {
                SERIAL_ECHOLN("Checking DiagRod Length");
                if (adj_diagrod_length() != 0)
                  {
                  //If diag rod length has been changed .. home to endstops
                  SERIAL_ECHOLN("Diag Rod Length changed .. Homing Endstops");
                  home_delta_axis();
                  bed_safe_z = AUTOCAL_PROBELIFT - z_probe_offset[Z_AXIS];
                  }
                }
               bed_probe_all();
               calibration_report();
               }

             } while((bed_level_c < -ac_prec) or (bed_level_c > ac_prec)
                  or (bed_level_x < -ac_prec) or (bed_level_x > ac_prec)
                  or (bed_level_y < -ac_prec) or (bed_level_y > ac_prec)
                  or (bed_level_z < -ac_prec) or (bed_level_z > ac_prec)
                  or (bed_level_ox < -ac_prec) or (bed_level_ox > ac_prec)
                  or (bed_level_oy < -ac_prec) or (bed_level_oy > ac_prec)
                  or (bed_level_oz < -ac_prec) or (bed_level_oz > ac_prec));

         SERIAL_ECHOLN("Autocalibration Complete");
         }

  	retract_z_probe();

        //Restore saved variables
        feedrate = saved_feedrate;
        feedmultiply = saved_feedmultiply;
        break;
#endif //**PJR - DELTA
#else
    case 31: // dock the sled
        dock_sled(true);
        break;
    case 32: // undock the sled
        dock_sled(false);
        break;
#endif // Z_PROBE_SLED
#endif // ENABLE_AUTO_BED_LEVELING
    case 90: // G90
      relative_mode = false;
      break;
    case 91: // G91
      relative_mode = true;
      break;
    case 92: // G92
      if(!code_seen(axis_codes[E_AXIS]))
        st_synchronize();
      for(int8_t i=0; i < NUM_AXIS; i++) {
        if(code_seen(axis_codes[i])) {
           if(i == E_AXIS) {
             current_position[i] = code_value();
             plan_set_e_position(current_position[E_AXIS]);
           }
           else {
#ifdef SCARA
		if (i == X_AXIS || i == Y_AXIS) {
                	current_position[i] = code_value();
		}
		else {
                current_position[i] = code_value()+add_homing[i];
            	}
#else
		current_position[i] = code_value()+add_homing[i];
#endif
            plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
           }
        }
      }
      break;
    }
  }

  else if(code_seen('M'))
  {
    switch( (int)code_value() )
    {
#ifdef ULTIPANEL
    case 0: // M0 - Unconditional stop - Wait for user button press on LCD
    case 1: // M1 - Conditional stop - Wait for user button press on LCD
    {
      char *src = strchr_pointer + 2;

      codenum = 0;

      bool hasP = false, hasS = false;
      if (code_seen('P')) {
        codenum = code_value(); // milliseconds to wait
        hasP = codenum > 0;
      }
      if (code_seen('S')) {
        codenum = code_value() * 1000; // seconds to wait
        hasS = codenum > 0;
      }
      starpos = strchr(src, '*');
      if (starpos != NULL) *(starpos) = '\0';
      while (*src == ' ') ++src;
      if (!hasP && !hasS && *src != '\0') {
        lcd_setstatus(src);
      } else {
        LCD_MESSAGEPGM(MSG_USERWAIT);
      }

      lcd_ignore_click();
      st_synchronize();
      previous_millis_cmd = millis();
      if (codenum > 0){
        codenum += millis();  // keep track of when we started waiting
        while(millis() < codenum && !lcd_clicked()){
          manage_heater();
          manage_inactivity();
          lcd_update();
        }
        lcd_ignore_click(false);
      }else{
          if (!lcd_detected())
            break;
        while(!lcd_clicked()){
          manage_heater();
          manage_inactivity();
          lcd_update();
        }
      }
      if (IS_SD_PRINTING)
        LCD_MESSAGEPGM(MSG_RESUMING);
      else
        LCD_MESSAGEPGM(WELCOME_MSG);
    }
    break;
#endif
    case 17:
        LCD_MESSAGEPGM(MSG_NO_MOVE);
        enable_x();
        enable_y();
        enable_z();
        enable_e0();
        enable_e1();
        enable_e2();
      break;

#ifdef SDSUPPORT
    case 20: // M20 - list SD card
      SERIAL_PROTOCOLLNPGM(MSG_BEGIN_FILE_LIST);
      card.ls();
      SERIAL_PROTOCOLLNPGM(MSG_END_FILE_LIST);
      break;
    case 21: // M21 - init SD card

      card.initsd();

      break;
    case 22: //M22 - release SD card
      card.release();

      break;
    case 23: //M23 - Select file
      starpos = (strchr(strchr_pointer + 4,'*'));
      if(starpos!=NULL)
        *(starpos)='\0';
      card.openFile(strchr_pointer + 4,true);
      break;
    case 24: //M24 - Start SD print
      card.startFileprint();
      starttime=millis();
      break;
    case 25: //M25 - Pause SD print
      card.pauseSDPrint();
      break;
    case 26: //M26 - Set SD index
      if(card.cardOK && code_seen('S')) {
        card.setIndex(code_value_long());
      }
      break;
    case 27: //M27 - Get SD status
      card.getStatus();
      break;
    case 28: //M28 - Start SD write
      starpos = (strchr(strchr_pointer + 4,'*'));
      if(starpos != NULL){
        char* npos = strchr(cmdbuffer[bufindr], 'N');
        strchr_pointer = strchr(npos,' ') + 1;
        *(starpos) = '\0';
      }
      card.openFile(strchr_pointer+4,false);
      break;
    case 29: //M29 - Stop SD write
      //processed in write to file routine above
      //card,saving = false;
      break;
    case 30: //M30 <filename> Delete File
      if (card.cardOK){
        card.closefile();
        starpos = (strchr(strchr_pointer + 4,'*'));
        if(starpos != NULL){
          char* npos = strchr(cmdbuffer[bufindr], 'N');
          strchr_pointer = strchr(npos,' ') + 1;
          *(starpos) = '\0';
        }
        card.removeFile(strchr_pointer + 4);
      }
      break;
    case 32: //M32 - Select file and start SD print
    {
      if(card.sdprinting) {
        st_synchronize();

      }
      starpos = (strchr(strchr_pointer + 4,'*'));

      char* namestartpos = (strchr(strchr_pointer + 4,'!'));   //find ! to indicate filename string start.
      if(namestartpos==NULL)
      {
        namestartpos=strchr_pointer + 4; //default name position, 4 letters after the M
      }
      else
        namestartpos++; //to skip the '!'

      if(starpos!=NULL)
        *(starpos)='\0';

      bool call_procedure=(code_seen('P'));

      if(strchr_pointer>namestartpos)
        call_procedure=false;  //false alert, 'P' found within filename

      if( card.cardOK )
      {
        card.openFile(namestartpos,true,!call_procedure);
        if(code_seen('S'))
          if(strchr_pointer<namestartpos) //only if "S" is occuring _before_ the filename
            card.setIndex(code_value_long());
        card.startFileprint();
        if(!call_procedure)
          starttime=millis(); //procedure calls count as normal print time.
      }
    } break;
    case 928: //M928 - Start SD write
      starpos = (strchr(strchr_pointer + 5,'*'));
      if(starpos != NULL){
        char* npos = strchr(cmdbuffer[bufindr], 'N');
        strchr_pointer = strchr(npos,' ') + 1;
        *(starpos) = '\0';
      }
      card.openLogFile(strchr_pointer+5);
      break;

#endif //SDSUPPORT

    case 31: //M31 take time since the start of the SD print or an M109 command
      {
      stoptime=millis();
      char time[30];
      unsigned long t=(stoptime-starttime)/1000;
      int sec,min;
      min=t/60;
      sec=t%60;
      sprintf_P(time, PSTR("%i min, %i sec"), min, sec);
      SERIAL_ECHO_START;
      SERIAL_ECHOLN(time);
      lcd_setstatus(time);
      autotempShutdown();
      }
      break;
    case 42: //M42 -Change pin status via gcode
      if (code_seen('S'))
      {
        int pin_status = code_value();
        int pin_number = LED_PIN;
        if (code_seen('P') && pin_status >= 0 && pin_status <= 255)
          pin_number = code_value();
        for(int8_t i = 0; i < (int8_t)(sizeof(sensitive_pins)/sizeof(int)); i++)
        {
          if (sensitive_pins[i] == pin_number)
          {
            pin_number = -1;
            break;
          }
        }
      #if defined(FAN_PIN) && FAN_PIN > -1
        if (pin_number == FAN_PIN)
          fanSpeed = pin_status;
      #endif
        if (pin_number > -1)
        {
          pinMode(pin_number, OUTPUT);
          digitalWrite(pin_number, pin_status);
          analogWrite(pin_number, pin_status);
        }
      }
     break;

// M48 Z-Probe repeatability measurement function.
//
// Usage:   M48 <J #_samples> <X X_position_for_samples> <Y Y_position_for_samples> <V Verbose_Level> <Engage_probe_for_each_reading> <L legs_of_movement_prior_to_doing_probe>
//
// This function assumes the bed has been homed.  Specificaly, that a G28 command
// as been issued prior to invoking the M48 Z-Probe repeatability measurement function.
// Any information generated by a prior G29 Bed leveling command will be lost and need to be
// regenerated.
//
// The number of samples will default to 10 if not specified.  You can use upper or lower case
// letters for any of the options. J is used because n cannot be used. Most user tools
// will convert G-code commands to upper case and Marlin uses a capital N for its communication
// protocol and will get horribly confused if you send it a capital N.
//

#ifdef ENABLE_AUTO_BED_LEVELING
#ifdef Z_PROBE_REPEATABILITY_TEST

    case 48: // M48 Z-Probe repeatability
        {
            #if Z_MIN_PIN == -1
            #error "You must have a Z_MIN endstop in order to enable calculation of Z-Probe repeatability."
            #endif

        // Prevent user from running a M48 without first homing in X and Y
        if (! (axis_known_position[X_AXIS] && axis_known_position[Y_AXIS]) )
        {
            LCD_MESSAGEPGM(MSG_POSITION_UNKNOWN);
            SERIAL_ECHO_START;
            SERIAL_ECHOLNPGM(MSG_POSITION_UNKNOWN);
            break; // abort M48, since we don't know where we are
        }

		// **PJR - Clear bed level correction to ensure correct z readings
		  st_synchronize();
          plan_bed_level_matrix.set_to_identity();
	    #ifdef NONLINEAR_BED_LEVELING
          reset_bed_level();
        #else //not defined NONLINEAR_BED_LEVELING
          vector_3 uncorrected_position = plan_get_position();
          //uncorrected_position.debug("position durring G29");
          current_position[X_AXIS] = uncorrected_position.x;
          current_position[Y_AXIS] = uncorrected_position.y;
          current_position[Z_AXIS] = uncorrected_position.z;
          plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
        #endif //NONLINEAR_BED_LEVELING


	double sum=0.0;
	double mean=0.0;
	double sigma=0.0;
	double sample_set[50];
	int verbose_level=1, n=0, j, n_samples = 10, n_legs=0, engage_probe_for_each_reading=0 ;
	double X_current, Y_current, Z_current;
	double X_probe_location, Y_probe_location, Z_start_location, ext_position;

	if (code_seen('V') || code_seen('v')) {
        	verbose_level = code_value();
		if (verbose_level<0 || verbose_level>4 ) {
			SERIAL_PROTOCOLPGM("?Verbose Level not plausible.\n");
			goto Sigma_Exit;
		}
	}

//	if (verbose_level > 0)   {
//		SERIAL_PROTOCOLPGM("M48 Z-Probe Repeatability test.   Version 2.00\n");
//		SERIAL_PROTOCOLPGM("Full support at: http://3dprintboard.com/forum.php\n");
//	}

	if (code_seen('J') || code_seen('j')) {
        	n_samples = code_value();
		if (n_samples<4 || n_samples>50 ) {
			SERIAL_PROTOCOLPGM("?Specified sample size not plausible.\n");
			goto Sigma_Exit;
		}
	}

	X_current = X_probe_location = current_position[X_AXIS]; //st_get_position_mm(X_AXIS); **PJR - These would be tower positions on a delta
	Y_current = Y_probe_location = current_position[Y_AXIS]; //st_get_position_mm(Y_AXIS);
	Z_current = current_position[Z_AXIS]; //st_get_position_mm(Z_AXIS);
	Z_start_location = Z_RAISE_BEFORE_PROBING; //st_get_position_mm(Z_AXIS) + Z_RAISE_BEFORE_PROBING; **PJR - set a specific 'safe' z location
	ext_position	 = st_get_position_mm(E_AXIS);

	if (code_seen('E') || code_seen('e') )
		engage_probe_for_each_reading++;

	if (code_seen('X') || code_seen('x') ) {
        	X_probe_location = (code_seen('X') ? code_value():0.00) -  X_PROBE_OFFSET_FROM_EXTRUDER;
		if (X_probe_location<X_MIN_POS || X_probe_location>X_MAX_POS ) {
			SERIAL_PROTOCOLPGM("?Specified X position out of range.\n");
			goto Sigma_Exit;
		}
	}

	if (code_seen('Y') || code_seen('y') ) {
        	Y_probe_location = (code_seen('Y') ? code_value():0.00) -  Y_PROBE_OFFSET_FROM_EXTRUDER;
		if (Y_probe_location<Y_MIN_POS || Y_probe_location>Y_MAX_POS ) {
			SERIAL_PROTOCOLPGM("?Specified Y position out of range.\n");
			goto Sigma_Exit;
		}
	}

	if (code_seen('L') || code_seen('l') ) {
        	n_legs = code_value();
		if ( n_legs==1 )
			n_legs = 2;
		if ( n_legs<0 || n_legs>15 ) {
			SERIAL_PROTOCOLPGM("?Specified number of legs in movement not plausible.\n");
			goto Sigma_Exit;
		}
	}

//
// Do all the preliminary setup work.   First raise the probe.
//

 	do_blocking_move_to(X_current, Y_current, Z_start_location);

//
// Now get everything to the specified probe point So we can safely do a probe to
// get us close to the bed.  If the Z-Axis is far from the bed, we don't want to
// use that as a starting point for each probe.
//
	if (verbose_level > 2)
		SERIAL_PROTOCOL("Positioning probe for the test.\n");

	engage_z_probe();

    do_blocking_move_cartesian(X_probe_location, Y_probe_location, Z_start_location); // **PJR - 'delta safe' move to probe position

// **PJR - prepare_move() sets destination[]
    X_current = current_position[X_AXIS];  // = st_get_position_mm(X_AXIS);
	Y_current = current_position[Y_AXIS];  // = st_get_position_mm(Y_AXIS);
	Z_current = current_position[Z_AXIS];  // = st_get_position_mm(Z_AXIS);
	ext_position = current_position[E_AXIS];  // = st_get_position_mm(E_AXIS);

//
// OK, do the inital probe to get us close to the bed.
// Then retrace the right amount and use that in subsequent probes
//

	setup_for_endstop_move();
	run_z_probe();

	// **PJR - run_z_probe() sets current_position[Z_AXIS]
	Z_current = current_position[Z_AXIS]; // = Z_current = st_get_position_mm(Z_AXIS);
	Z_start_location = Z_current + Z_RAISE_BETWEEN_PROBINGS;  //st_get_position_mm(Z_AXIS) + Z_RAISE_BEFORE_PROBING;

	// **PJR - Raise the probe - is Z only so delta safe
	do_blocking_move_to(X_current, Y_current, Z_start_location);

    // **PJR - Need to return to probe location after engage/retract probe operations.
	if (engage_probe_for_each_reading) {
        	retract_z_probe();
			}

    for( n=0; n<n_samples; n++) {

          do_blocking_move_cartesian( X_probe_location, Y_probe_location, Z_start_location); // Make sure we are at the probe location and lift the probe if needed

		if ( n_legs)  {
		double radius=0.0, theta=0.0, x_sweep, y_sweep;
		int rotational_direction, l;

			rotational_direction = (unsigned long) millis() & 0x0001;			// clockwise or counter clockwise
			radius = (unsigned long) millis() % (long) (X_MAX_LENGTH/4); 			// limit how far out to go
			theta = (float) ((unsigned long) millis() % (long) 360) / (360./(2*3.1415926));	// turn into radians

//SERIAL_ECHOPAIR("starting radius: ",radius);
//SERIAL_ECHOPAIR("   theta: ",theta);
//SERIAL_ECHOPAIR("   direction: ",rotational_direction);
//SERIAL_PROTOCOLLNPGM("");

			for( l=0; l<n_legs-1; l++) {

			  #ifdef DELTA // **PJR - If delta we need to clip X & Y to delta probable area
			  do {
			  #endif

				if (rotational_direction==1)
					theta += (float) ((unsigned long) millis() % (long) 20) / (360.0/(2*3.1415926)); // turn into radians
				else
					theta -= (float) ((unsigned long) millis() % (long) 20) / (360.0/(2*3.1415926)); // turn into radians

				radius += (float) ( ((long) ((unsigned long) millis() % (long) 10)) - 5);
				if ( radius<0.0 )
					radius = -radius;

				X_current = X_probe_location + cos(theta) * radius;
				Y_current = Y_probe_location + sin(theta) * radius;

				if ( X_current<X_MIN_POS)		// Make sure our X & Y are sane
					 X_current = X_MIN_POS;
				if ( X_current>X_MAX_POS)
					 X_current = X_MAX_POS;

				if ( Y_current<Y_MIN_POS)		// Make sure our X & Y are sane
					 Y_current = Y_MIN_POS;
				if ( Y_current>Y_MAX_POS)
					 Y_current = Y_MAX_POS;

			  #ifdef DELTA // **PJR Check within delta probable area else get another X and Y
			  } while (sqrt(X_current*X_current + Y_current*Y_current) > DELTA_PROBABLE_RADIUS);
			  #endif

				if (verbose_level>3 ) {
					SERIAL_ECHOPAIR("x: ", X_current);
					SERIAL_ECHOPAIR("y: ", Y_current);
					SERIAL_PROTOCOLLNPGM("");
				}

				do_blocking_move_cartesian( X_current, Y_current, Z_start_location ); // **PJR - Go to randomised point (delta segmented if needed)
			}
			do_blocking_move_cartesian( X_probe_location, Y_probe_location, Z_start_location); // Go back to the probe location (delta segmented if needed)
		}

		if (engage_probe_for_each_reading)  {
        		engage_z_probe();
          		delay(1000);
                do_blocking_move_cartesian( X_probe_location, Y_probe_location, Z_start_location); // Go back to the probe location (delta segmented if needed)
                }

		setup_for_endstop_move();
                run_z_probe();

		sample_set[n] = current_position[Z_AXIS]; // **PJR - This s the probe position at bed level - NOT corrected for Z_probe offset
              

//
// Get the current mean for the data points we have so far
//
		sum=0.0;
		for( j=0; j<=n; j++) {
			sum = sum + sample_set[j];
		}
		mean = sum / (double (n+1));
//
// Now, use that mean to calculate the standard deviation for the
// data points we have so far
//

		sum=0.0;
		for( j=0; j<=n; j++) {
			sum = sum + (sample_set[j]-mean) * (sample_set[j]-mean);
		}
		sigma = sqrt( sum / (double (n+1)) );

		if (verbose_level > 1) {
			SERIAL_PROTOCOL(n+1);
			SERIAL_PROTOCOL(" of ");
			SERIAL_PROTOCOL(n_samples);
			SERIAL_PROTOCOLPGM("   z: ");
			SERIAL_PROTOCOL_F(current_position[Z_AXIS], 6);
		}

		if (verbose_level > 2) {
			SERIAL_PROTOCOL(" mean: ");
			SERIAL_PROTOCOL_F(mean,6);

			SERIAL_PROTOCOL("   sigma: ");
			SERIAL_PROTOCOL_F(sigma,6);
		}

		if (verbose_level > 0)
			SERIAL_PROTOCOLPGM("\n");

		// **PJR - Lift the probe again (raw move is OK since Z only)
        do_blocking_move_to(X_probe_location, Y_probe_location, Z_start_location);
        delay(500);  //Give FSR time to reset avoid bounce

		if (engage_probe_for_each_reading)  {
        		retract_z_probe();
          		delay(1000);
		}
	}

        retract_z_probe();
        delay(1000);

        clean_up_after_endstop_move();

//      enable_endstops(true);

	SERIAL_PROTOCOLPGM("X, Y, L, Mean, Median, Mode, Low, High, StdDev:");
	SERIAL_PROTOCOL_F(X_probe_location, 6);
	SERIAL_PROTOCOLPGM(",");
        SERIAL_PROTOCOL_F(Y_probe_location, 6);
	SERIAL_PROTOCOLPGM(",");
        SERIAL_PROTOCOL_F(n_legs, 6);
	SERIAL_PROTOCOLPGM(",");
        SERIAL_PROTOCOL_F(mean, 6);
	SERIAL_PROTOCOLPGM(",");
        isort(sample_set,n_samples);
        //SERIAL_PROTOCOLPGM("Median: ");
        SERIAL_PROTOCOL_F(sample_set[(n_samples/2)], 6);
        SERIAL_PROTOCOLPGM(",");
        //SERIAL_PROTOCOLPGM("Mode: ");
        SERIAL_PROTOCOL_F(probe_mode(sample_set, n_samples), 6);
        SERIAL_PROTOCOLPGM(",");
        //SERIAL_PROTOCOLPGM("Range: ");
        SERIAL_PROTOCOL_F(sample_set[0], 6);
        SERIAL_PROTOCOLPGM(",");
        SERIAL_PROTOCOL_F(sample_set[(n_samples-1)], 6);
        SERIAL_PROTOCOLPGM(",");
        
        //SERIAL_PROTOCOLPGM("Standard Deviation: ");
        SERIAL_PROTOCOL_F(sigma, 6);
        SERIAL_PROTOCOLPGM("\n");



Sigma_Exit:
        break;
	}
#endif		// Z_PROBE_REPEATABILITY_TEST
#endif		// ENABLE_AUTO_BED_LEVELING

    case 104: // M104
      if(setTargetedHotend(104)){
        break;
      }
      if (code_seen('S')) setTargetHotend(code_value(), tmp_extruder);
#ifdef DUAL_X_CARRIAGE
      if (dual_x_carriage_mode == DXC_DUPLICATION_MODE && tmp_extruder == 0)
        setTargetHotend1(code_value() == 0.0 ? 0.0 : code_value() + duplicate_extruder_temp_offset);
#endif
      setWatch();
      break;
    case 112: //  M112 -Emergency Stop
      kill();
      break;
    case 140: // M140 set bed temp
      if (code_seen('S')) setTargetBed(code_value());
      break;
    case 105 : // M105
      if(setTargetedHotend(105)){
        break;
        }
      #if defined(TEMP_0_PIN) && TEMP_0_PIN > -1
        SERIAL_PROTOCOLPGM("ok T:");
        SERIAL_PROTOCOL_F(degHotend(tmp_extruder),1);
        SERIAL_PROTOCOLPGM(" /");
        SERIAL_PROTOCOL_F(degTargetHotend(tmp_extruder),1);
        #if defined(TEMP_BED_PIN) && TEMP_BED_PIN > -1
          SERIAL_PROTOCOLPGM(" B:");
          SERIAL_PROTOCOL_F(degBed(),1);
          SERIAL_PROTOCOLPGM(" /");
          SERIAL_PROTOCOL_F(degTargetBed(),1);
        #endif //TEMP_BED_PIN
        for (int8_t cur_extruder = 0; cur_extruder < EXTRUDERS; ++cur_extruder) {
          SERIAL_PROTOCOLPGM(" T");
          SERIAL_PROTOCOL(cur_extruder);
          SERIAL_PROTOCOLPGM(":");
          SERIAL_PROTOCOL_F(degHotend(cur_extruder),1);
          SERIAL_PROTOCOLPGM(" /");
          SERIAL_PROTOCOL_F(degTargetHotend(cur_extruder),1);
        }
      #else
        SERIAL_ERROR_START;
        SERIAL_ERRORLNPGM(MSG_ERR_NO_THERMISTORS);
      #endif

        SERIAL_PROTOCOLPGM(" @:");
      #ifdef EXTRUDER_WATTS
        SERIAL_PROTOCOL((EXTRUDER_WATTS * getHeaterPower(tmp_extruder))/127);
        SERIAL_PROTOCOLPGM("W");
      #else
        SERIAL_PROTOCOL(getHeaterPower(tmp_extruder));
      #endif

        SERIAL_PROTOCOLPGM(" B@:");
      #ifdef BED_WATTS
        SERIAL_PROTOCOL((BED_WATTS * getHeaterPower(-1))/127);
        SERIAL_PROTOCOLPGM("W");
      #else
        SERIAL_PROTOCOL(getHeaterPower(-1));
      #endif

        #ifdef SHOW_TEMP_ADC_VALUES
          #if defined(TEMP_BED_PIN) && TEMP_BED_PIN > -1
            SERIAL_PROTOCOLPGM("    ADC B:");
            SERIAL_PROTOCOL_F(degBed(),1);
            SERIAL_PROTOCOLPGM("C->");
            SERIAL_PROTOCOL_F(rawBedTemp()/OVERSAMPLENR,0);
          #endif
          for (int8_t cur_extruder = 0; cur_extruder < EXTRUDERS; ++cur_extruder) {
            SERIAL_PROTOCOLPGM("  T");
            SERIAL_PROTOCOL(cur_extruder);
            SERIAL_PROTOCOLPGM(":");
            SERIAL_PROTOCOL_F(degHotend(cur_extruder),1);
            SERIAL_PROTOCOLPGM("C->");
            SERIAL_PROTOCOL_F(rawHotendTemp(cur_extruder)/OVERSAMPLENR,0);
          }
        #endif

        SERIAL_PROTOCOLLN("");
      return;
      break;
    case 109:
    {// M109 - Wait for extruder heater to reach target.
      if(setTargetedHotend(109)){
        break;
      }
      LCD_MESSAGEPGM(MSG_HEATING);
      #ifdef AUTOTEMP
        autotemp_enabled=false;
      #endif
      if (code_seen('S')) {
        setTargetHotend(code_value(), tmp_extruder);
#ifdef DUAL_X_CARRIAGE
        if (dual_x_carriage_mode == DXC_DUPLICATION_MODE && tmp_extruder == 0)
          setTargetHotend1(code_value() == 0.0 ? 0.0 : code_value() + duplicate_extruder_temp_offset);
#endif
        CooldownNoWait = true;
      } else if (code_seen('R')) {
        setTargetHotend(code_value(), tmp_extruder);
#ifdef DUAL_X_CARRIAGE
        if (dual_x_carriage_mode == DXC_DUPLICATION_MODE && tmp_extruder == 0)
          setTargetHotend1(code_value() == 0.0 ? 0.0 : code_value() + duplicate_extruder_temp_offset);
#endif
        CooldownNoWait = false;
      }
      #ifdef AUTOTEMP
        if (code_seen('S')) autotemp_min=code_value();
        if (code_seen('B')) autotemp_max=code_value();
        if (code_seen('F'))
        {
          autotemp_factor=code_value();
          autotemp_enabled=true;
        }
      #endif

      setWatch();
      codenum = millis();

      /* See if we are heating up or cooling down */
      target_direction = isHeatingHotend(tmp_extruder); // true if heating, false if cooling

      cancel_heatup = false;

      #ifdef TEMP_RESIDENCY_TIME
        long residencyStart;
        residencyStart = -1;
        /* continue to loop until we have reached the target temp
          _and_ until TEMP_RESIDENCY_TIME hasn't passed since we reached it */
        while((!cancel_heatup)&&((residencyStart == -1) ||
              (residencyStart >= 0 && (((unsigned int) (millis() - residencyStart)) < (TEMP_RESIDENCY_TIME * 1000UL)))) ) {
      #else
        while ( target_direction ? (isHeatingHotend(tmp_extruder)) : (isCoolingHotend(tmp_extruder)&&(CooldownNoWait==false)) ) {
      #endif //TEMP_RESIDENCY_TIME
          if( (millis() - codenum) > 1000UL )
          { //Print Temp Reading and remaining time every 1 second while heating up/cooling down
            SERIAL_PROTOCOLPGM("T:");
            SERIAL_PROTOCOL_F(degHotend(tmp_extruder),1);
            SERIAL_PROTOCOLPGM(" E:");
            SERIAL_PROTOCOL((int)tmp_extruder);
            #ifdef TEMP_RESIDENCY_TIME
              SERIAL_PROTOCOLPGM(" W:");
              if(residencyStart > -1)
              {
                 codenum = ((TEMP_RESIDENCY_TIME * 1000UL) - (millis() - residencyStart)) / 1000UL;
                 SERIAL_PROTOCOLLN( codenum );
              }
              else
              {
                 SERIAL_PROTOCOLLN( "?" );
              }
            #else
              SERIAL_PROTOCOLLN("");
            #endif
            codenum = millis();
          }
          manage_heater();
          manage_inactivity();
          lcd_update();
        #ifdef TEMP_RESIDENCY_TIME
            /* start/restart the TEMP_RESIDENCY_TIME timer whenever we reach target temp for the first time
              or when current temp falls outside the hysteresis after target temp was reached */
          if ((residencyStart == -1 &&  target_direction && (degHotend(tmp_extruder) >= (degTargetHotend(tmp_extruder)-TEMP_WINDOW))) ||
              (residencyStart == -1 && !target_direction && (degHotend(tmp_extruder) <= (degTargetHotend(tmp_extruder)+TEMP_WINDOW))) ||
              (residencyStart > -1 && labs(degHotend(tmp_extruder) - degTargetHotend(tmp_extruder)) > TEMP_HYSTERESIS) )
          {
            residencyStart = millis();
          }
        #endif //TEMP_RESIDENCY_TIME
        }
        LCD_MESSAGEPGM(MSG_HEATING_COMPLETE);
        starttime=millis();
        previous_millis_cmd = millis();
      }
      break;
    case 190: // M190 - Wait for bed heater to reach target.
    #if defined(TEMP_BED_PIN) && TEMP_BED_PIN > -1
        LCD_MESSAGEPGM(MSG_BED_HEATING);
        if (code_seen('S')) {
          setTargetBed(code_value());
          CooldownNoWait = true;
        } else if (code_seen('R')) {
          setTargetBed(code_value());
          CooldownNoWait = false;
        }
        codenum = millis();

        cancel_heatup = false;
        target_direction = isHeatingBed(); // true if heating, false if cooling

        while ( (target_direction)&&(!cancel_heatup) ? (isHeatingBed()) : (isCoolingBed()&&(CooldownNoWait==false)) )
        {
          if(( millis() - codenum) > 1000 ) //Print Temp Reading every 1 second while heating up.
          {
            float tt=degHotend(active_extruder);
            SERIAL_PROTOCOLPGM("T:");
            SERIAL_PROTOCOL(tt);
            SERIAL_PROTOCOLPGM(" E:");
            SERIAL_PROTOCOL((int)active_extruder);
            SERIAL_PROTOCOLPGM(" B:");
            SERIAL_PROTOCOL_F(degBed(),1);
            SERIAL_PROTOCOLLN("");
            codenum = millis();
          }
          manage_heater();
          manage_inactivity();
          lcd_update();
        }
        LCD_MESSAGEPGM(MSG_BED_DONE);
        previous_millis_cmd = millis();
    #endif
        break;

    #if defined(FAN_PIN) && FAN_PIN > -1
      case 106: //M106 Fan On
        if (code_seen('S')){
           fanSpeed=constrain(code_value(),0,255);
        }
        else {
          fanSpeed=255;
        }
        break;
      case 107: //M107 Fan Off
        fanSpeed = 0;
        break;
    #endif //FAN_PIN
    #ifdef BARICUDA
      // PWM for HEATER_1_PIN
      #if defined(HEATER_1_PIN) && HEATER_1_PIN > -1
        case 126: //M126 valve open
          if (code_seen('S')){
             ValvePressure=constrain(code_value(),0,255);
          }
          else {
            ValvePressure=255;
          }
          break;
        case 127: //M127 valve closed
          ValvePressure = 0;
          break;
      #endif //HEATER_1_PIN

      // PWM for HEATER_2_PIN
      #if defined(HEATER_2_PIN) && HEATER_2_PIN > -1
        case 128: //M128 valve open
          if (code_seen('S')){
             EtoPPressure=constrain(code_value(),0,255);
          }
          else {
            EtoPPressure=255;
          }
          break;
        case 129: //M129 valve closed
          EtoPPressure = 0;
          break;
      #endif //HEATER_2_PIN
    #endif

    #if defined(PS_ON_PIN) && PS_ON_PIN > -1
      case 80: // M80 - Turn on Power Supply
        SET_OUTPUT(PS_ON_PIN); //GND
        WRITE(PS_ON_PIN, PS_ON_AWAKE);

        // If you have a switch on suicide pin, this is useful
        // if you want to start another print with suicide feature after
        // a print without suicide...
        #if defined SUICIDE_PIN && SUICIDE_PIN > -1
            SET_OUTPUT(SUICIDE_PIN);
            WRITE(SUICIDE_PIN, HIGH);
        #endif

        #ifdef ULTIPANEL
          powersupply = true;
          LCD_MESSAGEPGM(WELCOME_MSG);
          lcd_update();
        #endif
        break;
      #endif

      case 81: // M81 - Turn off Power Supply
        disable_heater();
        st_synchronize();
        disable_e0();
        disable_e1();
        disable_e2();
        finishAndDisableSteppers();
        fanSpeed = 0;
        delay(1000); // Wait a little before to switch off
      #if defined(SUICIDE_PIN) && SUICIDE_PIN > -1
        st_synchronize();
        suicide();
      #elif defined(PS_ON_PIN) && PS_ON_PIN > -1
        SET_OUTPUT(PS_ON_PIN);
        WRITE(PS_ON_PIN, PS_ON_ASLEEP);
      #endif
      #ifdef ULTIPANEL
        powersupply = false;
        LCD_MESSAGEPGM(MACHINE_NAME" "MSG_OFF".");
        lcd_update();
      #endif
	  break;

    case 82:
      axis_relative_modes[3] = false;
      break;
    case 83:
      axis_relative_modes[3] = true;
      break;
    case 18: //compatibility
    case 84: // M84
      if(code_seen('S')){
        stepper_inactive_time = code_value() * 1000;
      }
      else
      {
        bool all_axis = !((code_seen(axis_codes[X_AXIS])) || (code_seen(axis_codes[Y_AXIS])) || (code_seen(axis_codes[Z_AXIS]))|| (code_seen(axis_codes[E_AXIS])));
        if(all_axis)
        {
          st_synchronize();
          disable_e0();
          disable_e1();
          disable_e2();
          finishAndDisableSteppers();
        }
        else
        {
          st_synchronize();
          if(code_seen('X')) disable_x();
          if(code_seen('Y')) disable_y();
          if(code_seen('Z')) disable_z();
          #if ((E0_ENABLE_PIN != X_ENABLE_PIN) && (E1_ENABLE_PIN != Y_ENABLE_PIN)) // Only enable on boards that have seperate ENABLE_PINS
            if(code_seen('E')) {
              disable_e0();
              disable_e1();
              disable_e2();
            }
          #endif
        }
      }
      break;
    case 85: // M85
      if(code_seen('S')) {
        max_inactive_time = code_value() * 1000;
      }
      break;
    case 92: // M92
      for(int8_t i=0; i < NUM_AXIS; i++)
      {
        if(code_seen(axis_codes[i]))
        {
          if(i == 3) { // E
            float value = code_value();
            if(value < 20.0) {
              float factor = axis_steps_per_unit[i] / value; // increase e constants if M92 E14 is given for netfab.
              max_e_jerk *= factor;
              max_feedrate[i] *= factor;
              axis_steps_per_sqr_second[i] *= factor;
            }
            axis_steps_per_unit[i] = value;
          }
          else {
            axis_steps_per_unit[i] = code_value();
          }
        }
      }
      break;
    case 115: // M115
      SERIAL_PROTOCOLPGM(MSG_M115_REPORT);
      break;
    case 117: // M117 display message
      starpos = (strchr(strchr_pointer + 5,'*'));
      if(starpos!=NULL)
        *(starpos)='\0';
      lcd_setstatus(strchr_pointer + 5);
      break;
    case 114: // M114
      SERIAL_PROTOCOLPGM("X:");
      SERIAL_PROTOCOL(current_position[X_AXIS]);
      SERIAL_PROTOCOLPGM(" Y:");
      SERIAL_PROTOCOL(current_position[Y_AXIS]);
      SERIAL_PROTOCOLPGM(" Z:");
      SERIAL_PROTOCOL(current_position[Z_AXIS]);
      SERIAL_PROTOCOLPGM(" E:");
      SERIAL_PROTOCOL(current_position[E_AXIS]);

      SERIAL_PROTOCOLPGM(MSG_COUNT_X);
      SERIAL_PROTOCOL(float(st_get_position(X_AXIS))/axis_steps_per_unit[X_AXIS]);
      SERIAL_PROTOCOLPGM(" Y:");
      SERIAL_PROTOCOL(float(st_get_position(Y_AXIS))/axis_steps_per_unit[Y_AXIS]);
      SERIAL_PROTOCOLPGM(" Z:");
      SERIAL_PROTOCOL(float(st_get_position(Z_AXIS))/axis_steps_per_unit[Z_AXIS]);

      SERIAL_PROTOCOLLN("");
#ifdef SCARA
	  SERIAL_PROTOCOLPGM("SCARA Theta:");
      SERIAL_PROTOCOL(delta[X_AXIS]);
      SERIAL_PROTOCOLPGM("   Psi+Theta:");
      SERIAL_PROTOCOL(delta[Y_AXIS]);
      SERIAL_PROTOCOLLN("");

      SERIAL_PROTOCOLPGM("SCARA Cal - Theta:");
      SERIAL_PROTOCOL(delta[X_AXIS]+add_homing[X_AXIS]);
      SERIAL_PROTOCOLPGM("   Psi+Theta (90):");
      SERIAL_PROTOCOL(delta[Y_AXIS]-delta[X_AXIS]-90+add_homing[Y_AXIS]);
      SERIAL_PROTOCOLLN("");

      SERIAL_PROTOCOLPGM("SCARA step Cal - Theta:");
      SERIAL_PROTOCOL(delta[X_AXIS]/90*axis_steps_per_unit[X_AXIS]);
      SERIAL_PROTOCOLPGM("   Psi+Theta:");
      SERIAL_PROTOCOL((delta[Y_AXIS]-delta[X_AXIS])/90*axis_steps_per_unit[Y_AXIS]);
      SERIAL_PROTOCOLLN("");
      SERIAL_PROTOCOLLN("");
#endif
      break;
    case 120: // M120
      enable_endstops(false) ;
      break;
    case 121: // M121
      enable_endstops(true) ;
      break;
    case 119: // M119
    SERIAL_PROTOCOLLN(MSG_M119_REPORT);
      #if defined(X_MIN_PIN) && X_MIN_PIN > -1
        SERIAL_PROTOCOLPGM(MSG_X_MIN);
        SERIAL_PROTOCOLLN(((READ(X_MIN_PIN)^X_MIN_ENDSTOP_INVERTING)?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
      #endif
      #if defined(X_MAX_PIN) && X_MAX_PIN > -1
        SERIAL_PROTOCOLPGM(MSG_X_MAX);
        SERIAL_PROTOCOLLN(((READ(X_MAX_PIN)^X_MAX_ENDSTOP_INVERTING)?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
      #endif
      #if defined(Y_MIN_PIN) && Y_MIN_PIN > -1
        SERIAL_PROTOCOLPGM(MSG_Y_MIN);
        SERIAL_PROTOCOLLN(((READ(Y_MIN_PIN)^Y_MIN_ENDSTOP_INVERTING)?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
      #endif
      #if defined(Y_MAX_PIN) && Y_MAX_PIN > -1
        SERIAL_PROTOCOLPGM(MSG_Y_MAX);
        SERIAL_PROTOCOLLN(((READ(Y_MAX_PIN)^Y_MAX_ENDSTOP_INVERTING)?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
      #endif
      #if defined(Z_MIN_PIN) && Z_MIN_PIN > -1
        SERIAL_PROTOCOLPGM(MSG_Z_MIN);
        SERIAL_PROTOCOLLN(((READ(Z_MIN_PIN)^Z_MIN_ENDSTOP_INVERTING)?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
      #endif
      #if defined(Z_MAX_PIN) && Z_MAX_PIN > -1
        SERIAL_PROTOCOLPGM(MSG_Z_MAX);
        SERIAL_PROTOCOLLN(((READ(Z_MAX_PIN)^Z_MAX_ENDSTOP_INVERTING)?MSG_ENDSTOP_HIT:MSG_ENDSTOP_OPEN));
      #endif
      break;
      //TODO: update for all axis, use for loop
    #ifdef BLINKM
    case 150: // M150
      {
        byte red;
        byte grn;
        byte blu;

        if(code_seen('R')) red = code_value();
        if(code_seen('U')) grn = code_value();
        if(code_seen('B')) blu = code_value();

        SendColors(red,grn,blu);
      }
      break;
    #endif //BLINKM
    case 200: // M200 D<millimeters> set filament diameter and set E axis units to cubic millimeters (use S0 to set back to millimeters).
      {
        float area = .0;
        float radius = .0;
        if(code_seen('D')) {
          radius = (float)code_value() * .5;
          if(radius == 0) {
            area = 1;
          } else {
            area = M_PI * pow(radius, 2);
          }
        } else {
          //reserved for setting filament diameter via UFID or filament measuring device
          break;


        }
        tmp_extruder = active_extruder;
        if(code_seen('T')) {
          tmp_extruder = code_value();
          if(tmp_extruder >= EXTRUDERS) {
            SERIAL_ECHO_START;
            SERIAL_ECHO(MSG_M200_INVALID_EXTRUDER);
            break;
          }
        }
        volumetric_multiplier[tmp_extruder] = 1 / area;
      }
      break;
    case 201: // M201
      for(int8_t i=0; i < NUM_AXIS; i++)
      {
        if(code_seen(axis_codes[i]))
        {
          max_acceleration_units_per_sq_second[i] = code_value();
        }
      }
      // steps per sq second need to be updated to agree with the units per sq second (as they are what is used in the planner)
      reset_acceleration_rates();
      break;
    #if 0 // Not used for Sprinter/grbl gen6
    case 202: // M202
      for(int8_t i=0; i < NUM_AXIS; i++) {
        if(code_seen(axis_codes[i])) axis_travel_steps_per_sqr_second[i] = code_value() * axis_steps_per_unit[i];
      }
      break;
    #endif
    case 203: // M203 max feedrate mm/sec
      for(int8_t i=0; i < NUM_AXIS; i++) {
        if(code_seen(axis_codes[i])) max_feedrate[i] = code_value();
      }
      break;
    case 204: // M204 acclereration S normal moves T filmanent only moves
      {
        if(code_seen('S')) acceleration = code_value() ;
        if(code_seen('T')) retract_acceleration = code_value() ;
      }
      break;
    case 205: //M205 advanced settings:  minimum travel speed S=while printing T=travel only,  B=minimum segment time X= maximum xy jerk, Z=maximum Z jerk
    {
      if(code_seen('S')) minimumfeedrate = code_value();
      if(code_seen('T')) mintravelfeedrate = code_value();
      if(code_seen('B')) minsegmenttime = code_value() ;
      if(code_seen('X')) max_xy_jerk = code_value() ;
      if(code_seen('Z')) max_z_jerk = code_value() ;
      if(code_seen('E')) max_e_jerk = code_value() ;
    }
    break;
    case 206: // M206 additional homing offset
      for(int8_t i=0; i < 3; i++)
      {
        if(code_seen(axis_codes[i])) add_homing[i] = code_value();
      }
	  #ifdef SCARA
	   if(code_seen('T'))       // Theta
      {
        add_homing[X_AXIS] = code_value() ;
      }
      if(code_seen('P'))       // Psi
      {
        add_homing[Y_AXIS] = code_value() ;
      }
	  #endif
      break;
    #ifdef DELTA
	case 665: // M665 set delta configurations L<diagonal_rod> R<delta_radius> S<segments_per_sec>
		if(code_seen('L')) {
			delta_diagonal_rod= code_value();
		}
		if(code_seen('R')) {
			delta_radius= code_value();
		}
		if(code_seen('S')) {
			delta_segments_per_second= code_value();
		}

		set_delta_constants(); //recalc_delta_settings(delta_radius, delta_diagonal_rod);
		break;
      case 666: // M666 set delta endstop and geometry adjustment

	   if (code_seen('A')) {
		tower_adj[0] = code_value();
		set_delta_constants();
	   }
	   if (code_seen('B')) {
		tower_adj[1] = code_value();
		set_delta_constants();
	   }
	   if (code_seen('C')) {
		tower_adj[2] = code_value();
		set_delta_constants();
	   }
           if (code_seen('I')) {
		tower_adj[3] = code_value();
		set_delta_constants();
	   }
	   if (code_seen('J')) {
		tower_adj[4] = code_value();
		set_delta_constants();
	   }
	   if (code_seen('K')) {
		tower_adj[5] = code_value();
		set_delta_constants();
	   }
	   if (code_seen('U')) {
		diagrod_adj[0] = code_value();
		set_delta_constants();
	   }
	   if (code_seen('V')) {
		diagrod_adj[1] = code_value();
		set_delta_constants();
	   }
	   if (code_seen('W')) {
		diagrod_adj[2] = code_value();
		set_delta_constants();
	   }
           if (code_seen('R')) {
           delta_radius = code_value();
           set_delta_constants();
         }
           if (code_seen('D')) {
             delta_diagonal_rod = code_value();
             set_delta_constants();
         }
           if (code_seen('H')) {
             max_pos[Z_AXIS]= code_value();
	     set_delta_constants();
         }
	   if (code_seen('P')) {
             boolean axis_done = false;
             float p_val = code_value();
             for(int8_t i=0; i < 3; i++)
               {
               if (code_seen(axis_codes[i]))
                 {
                 z_probe_offset[i] = code_value();
                 axis_done = true;
                 }
               }
               if (axis_done == false) z_probe_offset[Z_AXIS]= p_val;
	   }
           else
           {
            for(int8_t i=0; i < 3; i++)
              {
              if (code_seen(axis_codes[i])) endstop_adj[i] = code_value();
              }
           }
	   if (code_seen('L')) {
	     SERIAL_ECHOLN("Current Delta geometry values:");
	     SERIAL_ECHO("X (Endstop Adj): ");
             SERIAL_PROTOCOL_F(endstop_adj[0],3);
             SERIAL_ECHOLN("");
	     SERIAL_ECHO("Y (Endstop Adj): ");
             SERIAL_PROTOCOL_F(endstop_adj[1],3);
             SERIAL_ECHOLN("");
	     SERIAL_ECHO("Z (Endstop Adj): ");
             SERIAL_PROTOCOL_F(endstop_adj[2],3);
             SERIAL_ECHOLN("");
             SERIAL_ECHOPAIR("P (Z-Probe Offset): X", z_probe_offset[0]);
             SERIAL_ECHOPAIR(" Y", z_probe_offset[1]);
             SERIAL_ECHOPAIR(" Z", z_probe_offset[2]);
             SERIAL_ECHOLN("");
             SERIAL_ECHO("A (Tower A Position Correction): ");
             SERIAL_PROTOCOL_F(tower_adj[0],3);
             SERIAL_ECHOLN("");
             SERIAL_ECHO("B (Tower B Position Correction): ");
             SERIAL_PROTOCOL_F(tower_adj[1],3);
             SERIAL_ECHOLN("");
             SERIAL_ECHO("C (Tower C Position Correction): ");
             SERIAL_PROTOCOL_F(tower_adj[2],3);
	     SERIAL_ECHOLN("");
             SERIAL_ECHO("I (Tower A Radius Correction): ");
             SERIAL_PROTOCOL_F(tower_adj[3],3);
             SERIAL_ECHOLN("");
             SERIAL_ECHO("J (Tower B Radius Correction): ");
             SERIAL_PROTOCOL_F(tower_adj[4],3);
             SERIAL_ECHOLN("");
             SERIAL_ECHO("K (Tower C Radius Correction): ");
             SERIAL_PROTOCOL_F(tower_adj[5],3);
	     SERIAL_ECHOLN("");
             SERIAL_ECHO("U (Tower A Diagional Rod Correction): ");
             SERIAL_PROTOCOL_F(diagrod_adj[0],3);
             SERIAL_ECHOLN("");
             SERIAL_ECHO("V (Tower B Diagonal Rod Correction): ");
             SERIAL_PROTOCOL_F(diagrod_adj[1],3);
             SERIAL_ECHOLN("");
             SERIAL_ECHO("W (Tower C Diagonal Rod Correction): ");
             SERIAL_PROTOCOL_F(diagrod_adj[2],3);
	     SERIAL_ECHOLN("");
             SERIAL_ECHOPAIR("R (Delta Radius): ",delta_radius);
             SERIAL_ECHOLN("");
             SERIAL_ECHOPAIR("D (Diagonal Rod Length): ",delta_diagonal_rod);
	     SERIAL_ECHOLN("");
             SERIAL_ECHOPAIR("H (Z-Height): ",max_pos[Z_AXIS]);
             SERIAL_ECHOLN("");
             }
         break;
    case 667:
        float tempx,tempy,tempz;
        if (code_seen('X')) tempx = code_value();
        if (code_seen('Y')) tempy = code_value();
        if (code_seen('Z')) tempz = code_value();

        calculate_delta(current_position);
        plan_set_position(delta[X_AXIS] + tempx, delta[Y_AXIS] + tempy, delta[Z_AXIS] + tempz, current_position[E_AXIS]);
        break;
    #endif
    #ifdef FWRETRACT
    case 207: //M207 - set retract length S[positive mm] F[feedrate mm/min] Z[additional zlift/hop]
    {
      if(code_seen('S'))
      {
        retract_length = code_value() ;
      }
      if(code_seen('F'))
      {
        retract_feedrate = code_value()/60 ;
      }
      if(code_seen('Z'))
      {
        retract_zlift = code_value() ;
      }
    }break;
    case 208: // M208 - set retract recover length S[positive mm surplus to the M207 S*] F[feedrate mm/min]
    {
      if(code_seen('S'))
      {
        retract_recover_length = code_value() ;
      }
      if(code_seen('F'))
      {
        retract_recover_feedrate = code_value()/60 ;
      }
    }break;
    case 209: // M209 - S<1=true/0=false> enable automatic retract detect if the slicer did not support G10/11: every normal extrude-only move will be classified as retract depending on the direction.
    {
      if(code_seen('S'))
      {
        int t= code_value() ;
        switch(t)
        {
          case 0:
          {
            autoretract_enabled=false;
            retracted[0]=false;
            #if EXTRUDERS > 1
              retracted[1]=false;
            #endif
            #if EXTRUDERS > 2
              retracted[2]=false;
            #endif
          }break;
          case 1:
          {
            autoretract_enabled=true;
            retracted[0]=false;
            #if EXTRUDERS > 1
              retracted[1]=false;
            #endif
            #if EXTRUDERS > 2
              retracted[2]=false;
            #endif
          }break;
          default:
            SERIAL_ECHO_START;
            SERIAL_ECHOPGM(MSG_UNKNOWN_COMMAND);
            SERIAL_ECHO(cmdbuffer[bufindr]);
            SERIAL_ECHOLNPGM("\"");
        }
      }

    }break;
    #endif // FWRETRACT
    #if EXTRUDERS > 1
    case 218: // M218 - set hotend offset (in mm), T<extruder_number> X<offset_on_X> Y<offset_on_Y>
    {
      if(setTargetedHotend(218)){
        break;
      }
      if(code_seen('X'))
      {
        extruder_offset[X_AXIS][tmp_extruder] = code_value();
      }
      if(code_seen('Y'))
      {
        extruder_offset[Y_AXIS][tmp_extruder] = code_value();
      }
      #ifdef DUAL_X_CARRIAGE
      if(code_seen('Z'))
      {
        extruder_offset[Z_AXIS][tmp_extruder] = code_value();
      }
      #endif
      SERIAL_ECHO_START;
      SERIAL_ECHOPGM(MSG_HOTEND_OFFSET);
      for(tmp_extruder = 0; tmp_extruder < EXTRUDERS; tmp_extruder++)
      {
         SERIAL_ECHO(" ");
         SERIAL_ECHO(extruder_offset[X_AXIS][tmp_extruder]);
         SERIAL_ECHO(",");
         SERIAL_ECHO(extruder_offset[Y_AXIS][tmp_extruder]);
      #ifdef DUAL_X_CARRIAGE
         SERIAL_ECHO(",");
         SERIAL_ECHO(extruder_offset[Z_AXIS][tmp_extruder]);
      #endif
      }
      SERIAL_ECHOLN("");
    }break;
    #endif
    case 220: // M220 S<factor in percent>- set speed factor override percentage
    {
      if(code_seen('S'))
      {
        feedmultiply = code_value() ;
      }
    }
    break;
    case 221: // M221 S<factor in percent>- set extrude factor override percentage
    {
      if(code_seen('S'))
      {
        int tmp_code = code_value();
        if (code_seen('T'))
        {
          if(setTargetedHotend(221)){
            break;
          }
          extruder_multiply[tmp_extruder] = tmp_code;
        }
        else
        {
          extrudemultiply = tmp_code ;
        }
      }
    }
    break;

	case 226: // M226 P<pin number> S<pin state>- Wait until the specified pin reaches the state required
	{
      if(code_seen('P')){
        int pin_number = code_value(); // pin number
        int pin_state = -1; // required pin state - default is inverted

        if(code_seen('S')) pin_state = code_value(); // required pin state

        if(pin_state >= -1 && pin_state <= 1){

          for(int8_t i = 0; i < (int8_t)(sizeof(sensitive_pins)/sizeof(int)); i++)
          {
            if (sensitive_pins[i] == pin_number)
            {
              pin_number = -1;
              break;
            }
          }

          if (pin_number > -1)
          {
            int target = LOW;

            st_synchronize();

            pinMode(pin_number, INPUT);

            switch(pin_state){
            case 1:
              target = HIGH;
              break;

            case 0:
              target = LOW;
              break;

            case -1:
              target = !digitalRead(pin_number);
              break;
            }

            while(digitalRead(pin_number) != target){
              manage_heater();
              manage_inactivity();
              lcd_update();
            }
          }
        }
      }
    }
    break;

    #if NUM_SERVOS > 0
    case 280: // M280 - set servo position absolute. P: servo index, S: angle or microseconds
      {
        int servo_index = -1;
        int servo_position = 0;
        if (code_seen('P'))
          servo_index = code_value();
        if (code_seen('S')) {
          servo_position = code_value();
          if ((servo_index >= 0) && (servo_index < NUM_SERVOS)) {
#if defined (ENABLE_AUTO_BED_LEVELING) && (PROBE_SERVO_DEACTIVATION_DELAY > 0)
		      servos[servo_index].attach(0);
#endif
            servos[servo_index].write(servo_position);
#if defined (ENABLE_AUTO_BED_LEVELING) && (PROBE_SERVO_DEACTIVATION_DELAY > 0)
              delay(PROBE_SERVO_DEACTIVATION_DELAY);
              servos[servo_index].detach();
#endif
          }
          else {
            SERIAL_ECHO_START;
            SERIAL_ECHO("Servo ");
            SERIAL_ECHO(servo_index);
            SERIAL_ECHOLN(" out of range");
          }
        }
        else if (servo_index >= 0) {
          SERIAL_PROTOCOL(MSG_OK);
          SERIAL_PROTOCOL(" Servo ");
          SERIAL_PROTOCOL(servo_index);
          SERIAL_PROTOCOL(": ");
          SERIAL_PROTOCOL(servos[servo_index].read());
          SERIAL_PROTOCOLLN("");
        }
      }
      break;
    #endif // NUM_SERVOS > 0

    #if (LARGE_FLASH == true && ( BEEPER > 0 || defined(ULTRALCD) || defined(LCD_USE_I2C_BUZZER)))
    case 300: // M300
    {
      int beepS = code_seen('S') ? code_value() : 110;
      int beepP = code_seen('P') ? code_value() : 1000;
      if (beepS > 0)
      {
        #if BEEPER > 0
          tone(BEEPER, beepS);
          delay(beepP);
          noTone(BEEPER);
        #elif defined(ULTRALCD)
		  lcd_buzz(beepS, beepP);
		#elif defined(LCD_USE_I2C_BUZZER)
		  lcd_buzz(beepP, beepS);
        #endif
      }
      else
      {
        delay(beepP);
      }
    }
    break;
    #endif // M300

    #ifdef PIDTEMP
    case 301: // M301
      {
        if(code_seen('P')) Kp = code_value();
        if(code_seen('I')) Ki = scalePID_i(code_value());
        if(code_seen('D')) Kd = scalePID_d(code_value());

        #ifdef PID_ADD_EXTRUSION_RATE
        if(code_seen('C')) Kc = code_value();
        #endif

        updatePID();
        SERIAL_PROTOCOL(MSG_OK);
        SERIAL_PROTOCOL(" p:");
        SERIAL_PROTOCOL(Kp);
        SERIAL_PROTOCOL(" i:");
        SERIAL_PROTOCOL(unscalePID_i(Ki));
        SERIAL_PROTOCOL(" d:");
        SERIAL_PROTOCOL(unscalePID_d(Kd));
        #ifdef PID_ADD_EXTRUSION_RATE
        SERIAL_PROTOCOL(" c:");
        //Kc does not have scaling applied above, or in resetting defaults
        SERIAL_PROTOCOL(Kc);
        #endif
        SERIAL_PROTOCOLLN("");
      }
      break;
    #endif //PIDTEMP
    #ifdef PIDTEMPBED
    case 304: // M304
      {
        if(code_seen('P')) bedKp = code_value();
        if(code_seen('I')) bedKi = scalePID_i(code_value());
        if(code_seen('D')) bedKd = scalePID_d(code_value());

        updatePID();
        SERIAL_PROTOCOL(MSG_OK);
        SERIAL_PROTOCOL(" p:");
        SERIAL_PROTOCOL(bedKp);
        SERIAL_PROTOCOL(" i:");
        SERIAL_PROTOCOL(unscalePID_i(bedKi));
        SERIAL_PROTOCOL(" d:");
        SERIAL_PROTOCOL(unscalePID_d(bedKd));
        SERIAL_PROTOCOLLN("");
      }
      break;
    #endif //PIDTEMP
    case 240: // M240  Triggers a camera by emulating a Canon RC-1 : http://www.doc-diy.net/photo/rc-1_hacked/
     {
     	#ifdef CHDK

         SET_OUTPUT(CHDK);
         WRITE(CHDK, HIGH);
         chdkHigh = millis();
         chdkActive = true;

       #else

      	#if defined(PHOTOGRAPH_PIN) && PHOTOGRAPH_PIN > -1
	const uint8_t NUM_PULSES=16;
	const float PULSE_LENGTH=0.01524;
	for(int i=0; i < NUM_PULSES; i++) {
        WRITE(PHOTOGRAPH_PIN, HIGH);
        _delay_ms(PULSE_LENGTH);
        WRITE(PHOTOGRAPH_PIN, LOW);
        _delay_ms(PULSE_LENGTH);
        }
        delay(7.33);
        for(int i=0; i < NUM_PULSES; i++) {
        WRITE(PHOTOGRAPH_PIN, HIGH);
        _delay_ms(PULSE_LENGTH);
        WRITE(PHOTOGRAPH_PIN, LOW);
        _delay_ms(PULSE_LENGTH);
        }
      	#endif
      #endif //chdk end if
     }
    break;
#ifdef DOGLCD
    case 250: // M250  Set LCD contrast value: C<value> (value 0..63)
     {
	  if (code_seen('C')) {
	   lcd_setcontrast( ((int)code_value())&63 );
          }
          SERIAL_PROTOCOLPGM("lcd contrast value: ");
          SERIAL_PROTOCOL(lcd_contrast);
          SERIAL_PROTOCOLLN("");
     }
    break;
#endif
    #ifdef PREVENT_DANGEROUS_EXTRUDE
    case 302: // allow cold extrudes, or set the minimum extrude temperature
    {
	  float temp = .0;
	  if (code_seen('S')) temp=code_value();
      set_extrude_min_temp(temp);
    }
    break;
	#endif
    case 303: // M303 PID autotune
    {
      float temp = 150.0;
      int e=0;
      int c=5;
      if (code_seen('E')) e=code_value();
        if (e<0)
          temp=70;
      if (code_seen('S')) temp=code_value();
      if (code_seen('C')) c=code_value();
      PID_autotune(temp, e, c);
    }
    break;
	#ifdef SCARA
	case 360:  // M360 SCARA Theta pos1
      SERIAL_ECHOLN(" Cal: Theta 0 ");
      //SoftEndsEnabled = false;              // Ignore soft endstops during calibration
      //SERIAL_ECHOLN(" Soft endstops disabled ");
      if(Stopped == false) {
        //get_coordinates(); // For X Y Z E F
        delta[X_AXIS] = 0;
        delta[Y_AXIS] = 120;
        calculate_SCARA_forward_Transform(delta);
        destination[X_AXIS] = delta[X_AXIS]/axis_scaling[X_AXIS];
        destination[Y_AXIS] = delta[Y_AXIS]/axis_scaling[Y_AXIS];

        prepare_move();
        //ClearToSend();
        return;
      }
    break;

    case 361:  // SCARA Theta pos2
      SERIAL_ECHOLN(" Cal: Theta 90 ");
      //SoftEndsEnabled = false;              // Ignore soft endstops during calibration
      //SERIAL_ECHOLN(" Soft endstops disabled ");
      if(Stopped == false) {
        //get_coordinates(); // For X Y Z E F
        delta[X_AXIS] = 90;
        delta[Y_AXIS] = 130;
        calculate_SCARA_forward_Transform(delta);
        destination[X_AXIS] = delta[X_AXIS]/axis_scaling[X_AXIS];
        destination[Y_AXIS] = delta[Y_AXIS]/axis_scaling[Y_AXIS];

        prepare_move();
        //ClearToSend();
        return;
      }
    break;
    case 362:  // SCARA Psi pos1
      SERIAL_ECHOLN(" Cal: Psi 0 ");
      //SoftEndsEnabled = false;              // Ignore soft endstops during calibration
      //SERIAL_ECHOLN(" Soft endstops disabled ");
      if(Stopped == false) {
        //get_coordinates(); // For X Y Z E F
        delta[X_AXIS] = 60;
        delta[Y_AXIS] = 180;
        calculate_SCARA_forward_Transform(delta);
        destination[X_AXIS] = delta[X_AXIS]/axis_scaling[X_AXIS];
        destination[Y_AXIS] = delta[Y_AXIS]/axis_scaling[Y_AXIS];

        prepare_move();
        //ClearToSend();
        return;
      }
    break;
    case 363:  // SCARA Psi pos2
      SERIAL_ECHOLN(" Cal: Psi 90 ");
      //SoftEndsEnabled = false;              // Ignore soft endstops during calibration
      //SERIAL_ECHOLN(" Soft endstops disabled ");
      if(Stopped == false) {
        //get_coordinates(); // For X Y Z E F
        delta[X_AXIS] = 50;
        delta[Y_AXIS] = 90;
        calculate_SCARA_forward_Transform(delta);
        destination[X_AXIS] = delta[X_AXIS]/axis_scaling[X_AXIS];
        destination[Y_AXIS] = delta[Y_AXIS]/axis_scaling[Y_AXIS];

        prepare_move();
        //ClearToSend();
        return;
      }
    break;
    case 364:  // SCARA Psi pos3 (90 deg to Theta)
      SERIAL_ECHOLN(" Cal: Theta-Psi 90 ");
     // SoftEndsEnabled = false;              // Ignore soft endstops during calibration
      //SERIAL_ECHOLN(" Soft endstops disabled ");
      if(Stopped == false) {
        //get_coordinates(); // For X Y Z E F
        delta[X_AXIS] = 45;
        delta[Y_AXIS] = 135;
        calculate_SCARA_forward_Transform(delta);
        destination[X_AXIS] = delta[X_AXIS]/axis_scaling[X_AXIS];
        destination[Y_AXIS] = delta[Y_AXIS]/axis_scaling[Y_AXIS];

        prepare_move();
        //ClearToSend();
        return;
      }
    break;
    case 365: // M364  Set SCARA scaling for X Y Z
      for(int8_t i=0; i < 3; i++)
      {
        if(code_seen(axis_codes[i]))
        {

            axis_scaling[i] = code_value();

        }
      }
      break;
	#endif
    case 400: // M400 finish all moves
    {
      st_synchronize();
    }
    break;
#if defined(ENABLE_AUTO_BED_LEVELING) && !defined(Z_PROBE_SLED)
    case 401:
    {
        engage_z_probe();    // Engage Z Servo endstop if available
    }
    break;

    case 402:
    {
        retract_z_probe();    // Retract Z Servo endstop if enabled
    }
    break;
#endif

#ifdef FILAMENT_SENSOR
case 404:  //M404 Enter the nominal filament width (3mm, 1.75mm ) N<3.0> or display nominal filament width
    {
    #if (FILWIDTH_PIN > -1)
    if(code_seen('N')) filament_width_nominal=code_value();
    else{
    SERIAL_PROTOCOLPGM("Filament dia (nominal mm):");
    SERIAL_PROTOCOLLN(filament_width_nominal);
    }
    #endif
    }
    break;

    case 405:  //M405 Turn on filament sensor for control
    {


    if(code_seen('D')) meas_delay_cm=code_value();

       if(meas_delay_cm> MAX_MEASUREMENT_DELAY)
       	meas_delay_cm = MAX_MEASUREMENT_DELAY;

       if(delay_index2 == -1)  //initialize the ring buffer if it has not been done since startup
    	   {
    	   int temp_ratio = widthFil_to_size_ratio();

       	    for (delay_index1=0; delay_index1<(MAX_MEASUREMENT_DELAY+1); ++delay_index1 ){
       	              measurement_delay[delay_index1]=temp_ratio-100;  //subtract 100 to scale within a signed byte
       	        }
       	    delay_index1=0;
       	    delay_index2=0;
    	   }

    filament_sensor = true ;

    //SERIAL_PROTOCOLPGM("Filament dia (measured mm):");
    //SERIAL_PROTOCOL(filament_width_meas);
    //SERIAL_PROTOCOLPGM("Extrusion ratio(%):");
    //SERIAL_PROTOCOL(extrudemultiply);
    }
    break;

    case 406:  //M406 Turn off filament sensor for control
    {
    filament_sensor = false ;
    }
    break;

    case 407:   //M407 Display measured filament diameter
    {



    SERIAL_PROTOCOLPGM("Filament dia (measured mm):");
    SERIAL_PROTOCOLLN(filament_width_meas);
    }
    break;
    #endif





    case 500: // M500 Store settings in EEPROM
    {
        Config_StoreSettings();
    }
    break;
    case 501: // M501 Read settings from EEPROM
    {
        Config_RetrieveSettings();
    }
    break;
    case 502: // M502 Revert to default settings
    {
        Config_ResetDefault();
    }
    break;
    case 503: // M503 print settings currently in memory
    {
        Config_PrintSettings();
    }
    break;
    #ifdef ABORT_ON_ENDSTOP_HIT_FEATURE_ENABLED
    case 540:
    {
        if(code_seen('S')) abort_on_endstop_hit = code_value() > 0;
    }
    break;
    #endif

    #ifdef CUSTOM_M_CODE_SET_Z_PROBE_OFFSET
    case CUSTOM_M_CODE_SET_Z_PROBE_OFFSET:
    {
      float value;
      if (code_seen('Z'))
      {
        value = code_value();
        if ((Z_PROBE_OFFSET_RANGE_MIN <= value) && (value <= Z_PROBE_OFFSET_RANGE_MAX))
        {
          zprobe_zoffset = -value; // compare w/ line 278 of ConfigurationStore.cpp
          SERIAL_ECHO_START;
          SERIAL_ECHOLNPGM(MSG_ZPROBE_ZOFFSET " " MSG_OK);
          SERIAL_PROTOCOLLN("");
        }
        else
        {
          SERIAL_ECHO_START;
          SERIAL_ECHOPGM(MSG_ZPROBE_ZOFFSET);
          SERIAL_ECHOPGM(MSG_Z_MIN);
          SERIAL_ECHO(Z_PROBE_OFFSET_RANGE_MIN);
          SERIAL_ECHOPGM(MSG_Z_MAX);
          SERIAL_ECHO(Z_PROBE_OFFSET_RANGE_MAX);
          SERIAL_PROTOCOLLN("");
        }
      }
      else
      {
          SERIAL_ECHO_START;
          SERIAL_ECHOLNPGM(MSG_ZPROBE_ZOFFSET " : ");
          SERIAL_ECHO(-zprobe_zoffset);
          SERIAL_PROTOCOLLN("");
      }
      break;
    }
    #endif // CUSTOM_M_CODE_SET_Z_PROBE_OFFSET

    #ifdef FILAMENTCHANGEENABLE
    case 600: //Pause for filament change X[pos] Y[pos] Z[relative lift] E[initial retract] L[later retract distance for removal]
    {
        float target[4];
        float lastpos[4];
        target[X_AXIS]=current_position[X_AXIS];
        target[Y_AXIS]=current_position[Y_AXIS];
        target[Z_AXIS]=current_position[Z_AXIS];
        target[E_AXIS]=current_position[E_AXIS];
        lastpos[X_AXIS]=current_position[X_AXIS];
        lastpos[Y_AXIS]=current_position[Y_AXIS];
        lastpos[Z_AXIS]=current_position[Z_AXIS];
        lastpos[E_AXIS]=current_position[E_AXIS];
        //retract by E
        if(code_seen('E'))
        {
          target[E_AXIS]+= code_value();
        }
        else
        {
          #ifdef FILAMENTCHANGE_FIRSTRETRACT
            target[E_AXIS]+= FILAMENTCHANGE_FIRSTRETRACT ;
          #endif
        }
        plan_buffer_line(target[X_AXIS], target[Y_AXIS], target[Z_AXIS], target[E_AXIS], feedrate/60, active_extruder);

        //lift Z
        if(code_seen('Z'))
        {
          target[Z_AXIS]+= code_value();
        }
        else
        {
          #ifdef FILAMENTCHANGE_ZADD
            target[Z_AXIS]+= FILAMENTCHANGE_ZADD ;
          #endif
        }
        plan_buffer_line(target[X_AXIS], target[Y_AXIS], target[Z_AXIS], target[E_AXIS], feedrate/60, active_extruder);

        //move xy
        if(code_seen('X'))
        {
          target[X_AXIS]+= code_value();
        }
        else
        {
          #ifdef FILAMENTCHANGE_XPOS
            target[X_AXIS]= FILAMENTCHANGE_XPOS ;
          #endif
        }
        if(code_seen('Y'))
        {
          target[Y_AXIS]= code_value();
        }
        else
        {
          #ifdef FILAMENTCHANGE_YPOS
            target[Y_AXIS]= FILAMENTCHANGE_YPOS ;
          #endif
        }

        plan_buffer_line(target[X_AXIS], target[Y_AXIS], target[Z_AXIS], target[E_AXIS], feedrate/60, active_extruder);

        if(code_seen('L'))
        {
          target[E_AXIS]+= code_value();
        }
        else
        {
          #ifdef FILAMENTCHANGE_FINALRETRACT
            target[E_AXIS]+= FILAMENTCHANGE_FINALRETRACT ;
          #endif
        }

        plan_buffer_line(target[X_AXIS], target[Y_AXIS], target[Z_AXIS], target[E_AXIS], feedrate/60, active_extruder);

        //finish moves
        st_synchronize();
        //disable extruder steppers so filament can be removed
        disable_e0();
        disable_e1();
        disable_e2();
        delay(100);
        LCD_ALERTMESSAGEPGM(MSG_FILAMENTCHANGE);
        uint8_t cnt=0;
        while(!lcd_clicked()){
          cnt++;
          manage_heater();
          manage_inactivity();
          lcd_update();
          if(cnt==0)
          {
          #if BEEPER > 0
            SET_OUTPUT(BEEPER);

            WRITE(BEEPER,HIGH);
            delay(3);
            WRITE(BEEPER,LOW);
            delay(3);
          #else
			#if !defined(LCD_FEEDBACK_FREQUENCY_HZ) || !defined(LCD_FEEDBACK_FREQUENCY_DURATION_MS)
              lcd_buzz(1000/6,100);
			#else
			  lcd_buzz(LCD_FEEDBACK_FREQUENCY_DURATION_MS,LCD_FEEDBACK_FREQUENCY_HZ);
			#endif
          #endif
          }
        }

        //return to normal
        if(code_seen('L'))
        {
          target[E_AXIS]+= -code_value();
        }
        else
        {
          #ifdef FILAMENTCHANGE_FINALRETRACT
            target[E_AXIS]+=(-1)*FILAMENTCHANGE_FINALRETRACT ;
          #endif
        }
        current_position[E_AXIS]=target[E_AXIS]; //the long retract of L is compensated by manual filament feeding
        plan_set_e_position(current_position[E_AXIS]);
        plan_buffer_line(target[X_AXIS], target[Y_AXIS], target[Z_AXIS], target[E_AXIS], feedrate/60, active_extruder); //should do nothing
        plan_buffer_line(lastpos[X_AXIS], lastpos[Y_AXIS], target[Z_AXIS], target[E_AXIS], feedrate/60, active_extruder); //move xy back
        plan_buffer_line(lastpos[X_AXIS], lastpos[Y_AXIS], lastpos[Z_AXIS], target[E_AXIS], feedrate/60, active_extruder); //move z back
        plan_buffer_line(lastpos[X_AXIS], lastpos[Y_AXIS], lastpos[Z_AXIS], lastpos[E_AXIS], feedrate/60, active_extruder); //final untretract
    }
    break;
    #endif //FILAMENTCHANGEENABLE
    #ifdef DUAL_X_CARRIAGE
    case 605: // Set dual x-carriage movement mode:
              //    M605 S0: Full control mode. The slicer has full control over x-carriage movement
              //    M605 S1: Auto-park mode. The inactive head will auto park/unpark without slicer involvement
              //    M605 S2 [Xnnn] [Rmmm]: Duplication mode. The second extruder will duplicate the first with nnn
              //                         millimeters x-offset and an optional differential hotend temperature of
              //                         mmm degrees. E.g., with "M605 S2 X100 R2" the second extruder will duplicate
              //                         the first with a spacing of 100mm in the x direction and 2 degrees hotter.
              //
              //    Note: the X axis should be homed after changing dual x-carriage mode.
    {
        st_synchronize();

        if (code_seen('S'))
          dual_x_carriage_mode = code_value();

        if (dual_x_carriage_mode == DXC_DUPLICATION_MODE)
        {
          if (code_seen('X'))
            duplicate_extruder_x_offset = max(code_value(),X2_MIN_POS - x_home_pos(0));

          if (code_seen('R'))
            duplicate_extruder_temp_offset = code_value();

          SERIAL_ECHO_START;
          SERIAL_ECHOPGM(MSG_HOTEND_OFFSET);
          SERIAL_ECHO(" ");
          SERIAL_ECHO(extruder_offset[X_AXIS][0]);
          SERIAL_ECHO(",");
          SERIAL_ECHO(extruder_offset[Y_AXIS][0]);
          SERIAL_ECHO(" ");
          SERIAL_ECHO(duplicate_extruder_x_offset);
          SERIAL_ECHO(",");
          SERIAL_ECHOLN(extruder_offset[Y_AXIS][1]);
        }
        else if (dual_x_carriage_mode != DXC_FULL_CONTROL_MODE && dual_x_carriage_mode != DXC_AUTO_PARK_MODE)
        {
          dual_x_carriage_mode = DEFAULT_DUAL_X_CARRIAGE_MODE;
        }

        active_extruder_parked = false;
        extruder_duplication_enabled = false;
        delayed_move_time = 0;
    }
    break;
    #endif //DUAL_X_CARRIAGE

    case 907: // M907 Set digital trimpot motor current using axis codes.
    {
      #if defined(DIGIPOTSS_PIN) && DIGIPOTSS_PIN > -1
        for(int i=0;i<NUM_AXIS;i++) if(code_seen(axis_codes[i])) digipot_current(i,code_value());
        if(code_seen('B')) digipot_current(4,code_value());
        if(code_seen('S')) for(int i=0;i<=4;i++) digipot_current(i,code_value());
      #endif
      #ifdef MOTOR_CURRENT_PWM_XY_PIN
        if(code_seen('X')) digipot_current(0, code_value());
      #endif
      #ifdef MOTOR_CURRENT_PWM_Z_PIN
        if(code_seen('Z')) digipot_current(1, code_value());
      #endif
      #ifdef MOTOR_CURRENT_PWM_E_PIN
        if(code_seen('E')) digipot_current(2, code_value());
      #endif
      #ifdef DIGIPOT_I2C
        // this one uses actual amps in floating point
        for(int i=0;i<NUM_AXIS;i++) if(code_seen(axis_codes[i])) digipot_i2c_set_current(i, code_value());
        // for each additional extruder (named B,C,D,E..., channels 4,5,6,7...)
        for(int i=NUM_AXIS;i<DIGIPOT_I2C_NUM_CHANNELS;i++) if(code_seen('B'+i-NUM_AXIS)) digipot_i2c_set_current(i, code_value());
      #endif
    }
    break;
    case 908: // M908 Control digital trimpot directly.
    {
      #if defined(DIGIPOTSS_PIN) && DIGIPOTSS_PIN > -1
        uint8_t channel,current;
        if(code_seen('P')) channel=code_value();
        if(code_seen('S')) current=code_value();
        digitalPotWrite(channel, current);
      #endif
    }
    break;
    case 350: // M350 Set microstepping mode. Warning: Steps per unit remains unchanged. S code sets stepping mode for all drivers.
    {
      #if defined(X_MS1_PIN) && X_MS1_PIN > -1
        if(code_seen('S')) for(int i=0;i<=4;i++) microstep_mode(i,code_value());
        for(int i=0;i<NUM_AXIS;i++) if(code_seen(axis_codes[i])) microstep_mode(i,(uint8_t)code_value());
        if(code_seen('B')) microstep_mode(4,code_value());
        microstep_readings();
      #endif
    }
    break;
    case 351: // M351 Toggle MS1 MS2 pins directly, S# determines MS1 or MS2, X# sets the pin high/low.
    {
      #if defined(X_MS1_PIN) && X_MS1_PIN > -1
      if(code_seen('S')) switch((int)code_value())
      {
        case 1:
          for(int i=0;i<NUM_AXIS;i++) if(code_seen(axis_codes[i])) microstep_ms(i,code_value(),-1);
          if(code_seen('B')) microstep_ms(4,code_value(),-1);
          break;
        case 2:
          for(int i=0;i<NUM_AXIS;i++) if(code_seen(axis_codes[i])) microstep_ms(i,-1,code_value());
          if(code_seen('B')) microstep_ms(4,-1,code_value());
          break;
      }
      microstep_readings();
      #endif
    }
    break;
    case 999: // M999: Restart after being stopped
      Stopped = false;
      lcd_reset_alert_level();
      gcode_LastN = Stopped_gcode_LastN;
      FlushSerialRequestResend();
    break;
    }
  }

  else if(code_seen('T'))
  {
    tmp_extruder = code_value();
    if(tmp_extruder >= EXTRUDERS) {
      SERIAL_ECHO_START;
      SERIAL_ECHO("T");
      SERIAL_ECHO(tmp_extruder);
      SERIAL_ECHOLN(MSG_INVALID_EXTRUDER);
    }
    else {
      boolean make_move = false;
      if(code_seen('F')) {
        make_move = true;
        next_feedrate = code_value();
        if(next_feedrate > 0.0) {
          feedrate = next_feedrate;
        }
      }
      #if EXTRUDERS > 1
      if(tmp_extruder != active_extruder) {
        // Save current position to return to after applying extruder offset
        memcpy(destination, current_position, sizeof(destination));
      #ifdef DUAL_X_CARRIAGE
        if (dual_x_carriage_mode == DXC_AUTO_PARK_MODE && Stopped == false &&
            (delayed_move_time != 0 || current_position[X_AXIS] != x_home_pos(active_extruder)))
        {
          // Park old head: 1) raise 2) move to park position 3) lower
          plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS] + TOOLCHANGE_PARK_ZLIFT,
                current_position[E_AXIS], max_feedrate[Z_AXIS], active_extruder);
          plan_buffer_line(x_home_pos(active_extruder), current_position[Y_AXIS], current_position[Z_AXIS] + TOOLCHANGE_PARK_ZLIFT,
                current_position[E_AXIS], max_feedrate[X_AXIS], active_extruder);
          plan_buffer_line(x_home_pos(active_extruder), current_position[Y_AXIS], current_position[Z_AXIS],
                current_position[E_AXIS], max_feedrate[Z_AXIS], active_extruder);
          st_synchronize();
        }

        // apply Y & Z extruder offset (x offset is already used in determining home pos)
        current_position[Y_AXIS] = current_position[Y_AXIS] -
                     extruder_offset[Y_AXIS][active_extruder] +
                     extruder_offset[Y_AXIS][tmp_extruder];
        current_position[Z_AXIS] = current_position[Z_AXIS] -
                     extruder_offset[Z_AXIS][active_extruder] +
                     extruder_offset[Z_AXIS][tmp_extruder];

        active_extruder = tmp_extruder;

        // This function resets the max/min values - the current position may be overwritten below.
        axis_is_at_home(X_AXIS);

        if (dual_x_carriage_mode == DXC_FULL_CONTROL_MODE)
        {
          current_position[X_AXIS] = inactive_extruder_x_pos;
          inactive_extruder_x_pos = destination[X_AXIS];
        }
        else if (dual_x_carriage_mode == DXC_DUPLICATION_MODE)
        {
          active_extruder_parked = (active_extruder == 0); // this triggers the second extruder to move into the duplication position
          if (active_extruder == 0 || active_extruder_parked)
            current_position[X_AXIS] = inactive_extruder_x_pos;
          else
            current_position[X_AXIS] = destination[X_AXIS] + duplicate_extruder_x_offset;
          inactive_extruder_x_pos = destination[X_AXIS];
          extruder_duplication_enabled = false;
        }
        else
        {
          // record raised toolhead position for use by unpark
          memcpy(raised_parked_position, current_position, sizeof(raised_parked_position));
          raised_parked_position[Z_AXIS] += TOOLCHANGE_UNPARK_ZLIFT;
          active_extruder_parked = true;
          delayed_move_time = 0;
        }
      #else
        // Offset extruder (only by XY)
        int i;
        for(i = 0; i < 2; i++) {
           current_position[i] = current_position[i] -
                                 extruder_offset[i][active_extruder] +
                                 extruder_offset[i][tmp_extruder];
        }
        // Set the new active extruder and position
        active_extruder = tmp_extruder;
      #endif //else DUAL_X_CARRIAGE
#ifdef DELTA

  calculate_delta(current_position); // change cartesian kinematic  to  delta kinematic;
   //sent position to plan_set_position();
  plan_set_position(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS],current_position[E_AXIS]);

#else
        plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);

#endif
        // Move to the old position if 'F' was in the parameters
        if(make_move && Stopped == false) {
           prepare_move();
        }
      }
      #endif
      SERIAL_ECHO_START;
      SERIAL_ECHO(MSG_ACTIVE_EXTRUDER);
      SERIAL_PROTOCOLLN((int)active_extruder);
    }
  }

  else
  {
    SERIAL_ECHO_START;
    SERIAL_ECHOPGM(MSG_UNKNOWN_COMMAND);
    SERIAL_ECHO(cmdbuffer[bufindr]);
    SERIAL_ECHOLNPGM("\"");
  }

  ClearToSend();
}

void FlushSerialRequestResend()
{
  //char cmdbuffer[bufindr][100]="Resend:";
  MYSERIAL.flush();
  SERIAL_PROTOCOLPGM(MSG_RESEND);
  SERIAL_PROTOCOLLN(gcode_LastN + 1);
  ClearToSend();
}

void ClearToSend()
{
  previous_millis_cmd = millis();
  #ifdef SDSUPPORT
  if(fromsd[bufindr])
    return;
  #endif //SDSUPPORT
  SERIAL_PROTOCOLLNPGM(MSG_OK);
}

void get_coordinates()
{
  bool seen[4]={false,false,false,false};
  for(int8_t i=0; i < NUM_AXIS; i++) {
    if(code_seen(axis_codes[i]))
    {
      destination[i] = (float)code_value() + (axis_relative_modes[i] || relative_mode)*current_position[i];
      seen[i]=true;
    }
    else destination[i] = current_position[i]; //Are these else lines really needed?
  }
  if(code_seen('F')) {
    next_feedrate = code_value();
    if(next_feedrate > 0.0) feedrate = next_feedrate;
  }
}

void get_arc_coordinates()
{
#ifdef SF_ARC_FIX
   bool relative_mode_backup = relative_mode;
   relative_mode = true;
#endif
   get_coordinates();
#ifdef SF_ARC_FIX
   relative_mode=relative_mode_backup;
#endif

   if(code_seen('I')) {
     offset[0] = code_value();
   }
   else {
     offset[0] = 0.0;
   }
   if(code_seen('J')) {
     offset[1] = code_value();
   }
   else {
     offset[1] = 0.0;
   }
}

void clamp_to_software_endstops(float target[3])
{
  if (min_software_endstops) {
    if (target[X_AXIS] < min_pos[X_AXIS]) target[X_AXIS] = min_pos[X_AXIS];
    if (target[Y_AXIS] < min_pos[Y_AXIS]) target[Y_AXIS] = min_pos[Y_AXIS];
    if (target[Z_AXIS] < min_pos[Z_AXIS]) target[Z_AXIS] = min_pos[Z_AXIS];
  }

  if (max_software_endstops) {
    if (target[X_AXIS] > max_pos[X_AXIS]) target[X_AXIS] = max_pos[X_AXIS];
    if (target[Y_AXIS] > max_pos[Y_AXIS]) target[Y_AXIS] = max_pos[Y_AXIS];
    if (target[Z_AXIS] > max_pos[Z_AXIS]) target[Z_AXIS] = max_pos[Z_AXIS];
  }
}

#ifdef DELTA
//**PJR - Use set_delta_constants() instead
//void recalc_delta_settings(float radius, float diagonal_rod)
//{
//	 delta_tower1_x= -SIN_60*radius; // front left tower
//	 delta_tower1_y= -COS_60*radius;
//	 delta_tower2_x=  SIN_60*radius; // front right tower
//	 delta_tower2_y= -COS_60*radius;
//	 delta_tower3_x= 0.0;                  // back middle tower
//	 delta_tower3_y= radius;
//	 delta_diagonal_rod_2= sq(diagonal_rod);
//}

void calculate_delta(float cartesian[3])
{
  delta[X_AXIS] = sqrt(DELTA_DIAGONAL_ROD1_2
                       - sq(delta_tower1_x-cartesian[X_AXIS])
                       - sq(delta_tower1_y-cartesian[Y_AXIS])
                       ) + cartesian[Z_AXIS];
  delta[Y_AXIS] = sqrt(DELTA_DIAGONAL_ROD2_2
                       - sq(delta_tower2_x-cartesian[X_AXIS])
                       - sq(delta_tower2_y-cartesian[Y_AXIS])
                       ) + cartesian[Z_AXIS];
  delta[Z_AXIS] = sqrt(DELTA_DIAGONAL_ROD3_2
                       - sq(delta_tower3_x-cartesian[X_AXIS])
                       - sq(delta_tower3_y-cartesian[Y_AXIS])
                       ) + cartesian[Z_AXIS];
  /*
  SERIAL_ECHOPGM("cartesian x="); SERIAL_ECHO(cartesian[X_AXIS]);
  SERIAL_ECHOPGM(" y="); SERIAL_ECHO(cartesian[Y_AXIS]);
  SERIAL_ECHOPGM(" z="); SERIAL_ECHOLN(cartesian[Z_AXIS]);

  SERIAL_ECHOPGM("delta x="); SERIAL_ECHO(delta[X_AXIS]);
  SERIAL_ECHOPGM(" y="); SERIAL_ECHO(delta[Y_AXIS]);
  SERIAL_ECHOPGM(" z="); SERIAL_ECHOLN(delta[Z_AXIS]);
  */
}

// Adjust print surface height by linear interpolation over the bed_level array.
void adjust_delta(float cartesian[3])
{
  int half = (AUTO_BED_LEVELING_GRID_POINTS - 1) / 2;
  float grid_x = max(0.001-half, min(half-0.001, cartesian[X_AXIS] / AUTO_BED_LEVELING_GRID_X));
  float grid_y = max(0.001-half, min(half-0.001, cartesian[Y_AXIS] / AUTO_BED_LEVELING_GRID_Y));
  int floor_x = floor(grid_x);
  int floor_y = floor(grid_y);
  float ratio_x = grid_x - floor_x;
  float ratio_y = grid_y - floor_y;
  float z1 = bed_level[floor_x+half][floor_y+half];
  float z2 = bed_level[floor_x+half][floor_y+half+1];
  float z3 = bed_level[floor_x+half+1][floor_y+half];
  float z4 = bed_level[floor_x+half+1][floor_y+half+1];
  float left = (1-ratio_y)*z1 + ratio_y*z2;
  float right = (1-ratio_y)*z3 + ratio_y*z4;
  float offset = (1-ratio_x)*left + ratio_x*right;

  delta[X_AXIS] += offset;
  delta[Y_AXIS] += offset;
  delta[Z_AXIS] += offset;

  /*
  SERIAL_ECHOPGM("grid_x="); SERIAL_ECHO(grid_x);
  SERIAL_ECHOPGM(" grid_y="); SERIAL_ECHO(grid_y);
  SERIAL_ECHOPGM(" floor_x="); SERIAL_ECHO(floor_x);
  SERIAL_ECHOPGM(" floor_y="); SERIAL_ECHO(floor_y);
  SERIAL_ECHOPGM(" ratio_x="); SERIAL_ECHO(ratio_x);
  SERIAL_ECHOPGM(" ratio_y="); SERIAL_ECHO(ratio_y);
  SERIAL_ECHOPGM(" z1="); SERIAL_ECHO(z1);
  SERIAL_ECHOPGM(" z2="); SERIAL_ECHO(z2);
  SERIAL_ECHOPGM(" z3="); SERIAL_ECHO(z3);
  SERIAL_ECHOPGM(" z4="); SERIAL_ECHO(z4);
  SERIAL_ECHOPGM(" left="); SERIAL_ECHO(left);
  SERIAL_ECHOPGM(" right="); SERIAL_ECHO(right);
  SERIAL_ECHOPGM(" offset="); SERIAL_ECHOLN(offset);
  */
}

void prepare_move_raw()
{
  previous_millis_cmd = millis();
  calculate_delta(destination);
  plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS],
                   destination[E_AXIS], feedrate*feedmultiply/60/100.0,
                   active_extruder);
  for(int8_t i=0; i < NUM_AXIS; i++) {
    current_position[i] = destination[i];
  }
}
#endif //DELTA

void prepare_move()
{
  clamp_to_software_endstops(destination);
  previous_millis_cmd = millis();

  #ifdef SCARA //for now same as delta-code

float difference[NUM_AXIS];
for (int8_t i=0; i < NUM_AXIS; i++) {
	difference[i] = destination[i] - current_position[i];
}

float cartesian_mm = sqrt(	sq(difference[X_AXIS]) +
							sq(difference[Y_AXIS]) +
							sq(difference[Z_AXIS]));
if (cartesian_mm < 0.000001) { cartesian_mm = abs(difference[E_AXIS]); }
if (cartesian_mm < 0.000001) { return; }
float seconds = 6000 * cartesian_mm / feedrate / feedmultiply;
int steps = max(1, int(scara_segments_per_second * seconds));
 //SERIAL_ECHOPGM("mm="); SERIAL_ECHO(cartesian_mm);
 //SERIAL_ECHOPGM(" seconds="); SERIAL_ECHO(seconds);
 //SERIAL_ECHOPGM(" steps="); SERIAL_ECHOLN(steps);
for (int s = 1; s <= steps; s++) {
	float fraction = float(s) / float(steps);
	for(int8_t i=0; i < NUM_AXIS; i++) {
		destination[i] = current_position[i] + difference[i] * fraction;
	}


	calculate_delta(destination);
         //SERIAL_ECHOPGM("destination[X_AXIS]="); SERIAL_ECHOLN(destination[X_AXIS]);
         //SERIAL_ECHOPGM("destination[Y_AXIS]="); SERIAL_ECHOLN(destination[Y_AXIS]);
         //SERIAL_ECHOPGM("destination[Z_AXIS]="); SERIAL_ECHOLN(destination[Z_AXIS]);
         //SERIAL_ECHOPGM("delta[X_AXIS]="); SERIAL_ECHOLN(delta[X_AXIS]);
         //SERIAL_ECHOPGM("delta[Y_AXIS]="); SERIAL_ECHOLN(delta[Y_AXIS]);
         //SERIAL_ECHOPGM("delta[Z_AXIS]="); SERIAL_ECHOLN(delta[Z_AXIS]);

	plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS],
	destination[E_AXIS], feedrate*feedmultiply/60/100.0,
	active_extruder);
}
#endif // SCARA

#ifdef DELTA
  float difference[NUM_AXIS];
  for (int8_t i=0; i < NUM_AXIS; i++) {
    difference[i] = destination[i] - current_position[i];
  }
  float cartesian_mm = sqrt(sq(difference[X_AXIS]) +
                            sq(difference[Y_AXIS]) +
                            sq(difference[Z_AXIS]));
  if (cartesian_mm < 0.000001) { cartesian_mm = abs(difference[E_AXIS]); }
  if (cartesian_mm < 0.000001) { return; }
  float seconds = 6000 * cartesian_mm / feedrate / feedmultiply;
  int steps = max(1, int(delta_segments_per_second * seconds));
  // SERIAL_ECHOPGM("mm="); SERIAL_ECHO(cartesian_mm);
  // SERIAL_ECHOPGM(" seconds="); SERIAL_ECHO(seconds);
  // SERIAL_ECHOPGM(" steps="); SERIAL_ECHOLN(steps);
  for (int s = 1; s <= steps; s++) {
    float fraction = float(s) / float(steps);
    for(int8_t i=0; i < NUM_AXIS; i++) {
      destination[i] = current_position[i] + difference[i] * fraction;
    }
    calculate_delta(destination);
    #ifdef NONLINEAR_BED_LEVELING
      adjust_delta(destination);
    #endif
    plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS],
                     destination[E_AXIS], feedrate*feedmultiply/60/100.0,
                     active_extruder);
  }

#endif // DELTA

#ifdef DUAL_X_CARRIAGE
  if (active_extruder_parked)
  {
    if (dual_x_carriage_mode == DXC_DUPLICATION_MODE && active_extruder == 0)
    {
      // move duplicate extruder into correct duplication position.
      plan_set_position(inactive_extruder_x_pos, current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
      plan_buffer_line(current_position[X_AXIS] + duplicate_extruder_x_offset, current_position[Y_AXIS], current_position[Z_AXIS],
          current_position[E_AXIS], max_feedrate[X_AXIS], 1);
      plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
      st_synchronize();
      extruder_duplication_enabled = true;
      active_extruder_parked = false;
    }
    else if (dual_x_carriage_mode == DXC_AUTO_PARK_MODE) // handle unparking of head
    {
      if (current_position[E_AXIS] == destination[E_AXIS])
      {
        // this is a travel move - skit it but keep track of current position (so that it can later
        // be used as start of first non-travel move)
        if (delayed_move_time != 0xFFFFFFFFUL)
        {
          memcpy(current_position, destination, sizeof(current_position));
          if (destination[Z_AXIS] > raised_parked_position[Z_AXIS])
            raised_parked_position[Z_AXIS] = destination[Z_AXIS];
          delayed_move_time = millis();
          return;
        }
      }
      delayed_move_time = 0;
      // unpark extruder: 1) raise, 2) move into starting XY position, 3) lower
      plan_buffer_line(raised_parked_position[X_AXIS], raised_parked_position[Y_AXIS], raised_parked_position[Z_AXIS],    current_position[E_AXIS], max_feedrate[Z_AXIS], active_extruder);
      plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], raised_parked_position[Z_AXIS],
          current_position[E_AXIS], min(max_feedrate[X_AXIS],max_feedrate[Y_AXIS]), active_extruder);
      plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS],
          current_position[E_AXIS], max_feedrate[Z_AXIS], active_extruder);
      active_extruder_parked = false;
    }
  }
#endif //DUAL_X_CARRIAGE

#if ! (defined DELTA || defined SCARA)
  // Do not use feedmultiply for E or Z only moves
  if( (current_position[X_AXIS] == destination [X_AXIS]) && (current_position[Y_AXIS] == destination [Y_AXIS])) {
      plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate/60, active_extruder);
  }
  else {
    plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], feedrate*feedmultiply/60/100.0, active_extruder);
  }
#endif // !(DELTA || SCARA)

  for(int8_t i=0; i < NUM_AXIS; i++) {
    current_position[i] = destination[i];
  }
}

void prepare_arc_move(char isclockwise) {
  float r = hypot(offset[X_AXIS], offset[Y_AXIS]); // Compute arc radius for mc_arc

  // Trace the arc
  mc_arc(current_position, destination, offset, X_AXIS, Y_AXIS, Z_AXIS, feedrate*feedmultiply/60/100.0, r, isclockwise, active_extruder);

  // As far as the parser is concerned, the position is now == target. In reality the
  // motion control system might still be processing the action and the real tool position
  // in any intermediate location.
  for(int8_t i=0; i < NUM_AXIS; i++) {
    current_position[i] = destination[i];
  }
  previous_millis_cmd = millis();
}

#if defined(CONTROLLERFAN_PIN) && CONTROLLERFAN_PIN > -1

#if defined(FAN_PIN)
  #if CONTROLLERFAN_PIN == FAN_PIN
    #error "You cannot set CONTROLLERFAN_PIN equal to FAN_PIN"
  #endif
#endif

unsigned long lastMotor = 0; //Save the time for when a motor was turned on last
unsigned long lastMotorCheck = 0;

void controllerFan()
{
  if ((millis() - lastMotorCheck) >= 2500) //Not a time critical function, so we only check every 2500ms
  {
    lastMotorCheck = millis();

    if(!READ(X_ENABLE_PIN) || !READ(Y_ENABLE_PIN) || !READ(Z_ENABLE_PIN) || (soft_pwm_bed > 0)
    #if EXTRUDERS > 2
       || !READ(E2_ENABLE_PIN)
    #endif
    #if EXTRUDER > 1
      #if defined(X2_ENABLE_PIN) && X2_ENABLE_PIN > -1
       || !READ(X2_ENABLE_PIN)
      #endif
       || !READ(E1_ENABLE_PIN)
    #endif
       || !READ(E0_ENABLE_PIN)) //If any of the drivers are enabled...
    {
      lastMotor = millis(); //... set time to NOW so the fan will turn on
    }

    if ((millis() - lastMotor) >= (CONTROLLERFAN_SECS*1000UL) || lastMotor == 0) //If the last time any driver was enabled, is longer since than CONTROLLERSEC...
    {
        digitalWrite(CONTROLLERFAN_PIN, 0);
        analogWrite(CONTROLLERFAN_PIN, 0);
    }
    else
    {
        // allows digital or PWM fan output to be used (see M42 handling)
        digitalWrite(CONTROLLERFAN_PIN, CONTROLLERFAN_SPEED);
        analogWrite(CONTROLLERFAN_PIN, CONTROLLERFAN_SPEED);
    }
  }
}
#endif

#ifdef SCARA
void calculate_SCARA_forward_Transform(float f_scara[3])
{
  // Perform forward kinematics, and place results in delta[3]
  // The maths and first version has been done by QHARLEY . Integrated into masterbranch 06/2014 and slightly restructured by Joachim Cerny in June 2014

  float x_sin, x_cos, y_sin, y_cos;

    //SERIAL_ECHOPGM("f_delta x="); SERIAL_ECHO(f_scara[X_AXIS]);
    //SERIAL_ECHOPGM(" y="); SERIAL_ECHO(f_scara[Y_AXIS]);

    x_sin = sin(f_scara[X_AXIS]/SCARA_RAD2DEG) * Linkage_1;
    x_cos = cos(f_scara[X_AXIS]/SCARA_RAD2DEG) * Linkage_1;
    y_sin = sin(f_scara[Y_AXIS]/SCARA_RAD2DEG) * Linkage_2;
    y_cos = cos(f_scara[Y_AXIS]/SCARA_RAD2DEG) * Linkage_2;

  //  SERIAL_ECHOPGM(" x_sin="); SERIAL_ECHO(x_sin);
  //  SERIAL_ECHOPGM(" x_cos="); SERIAL_ECHO(x_cos);
  //  SERIAL_ECHOPGM(" y_sin="); SERIAL_ECHO(y_sin);
  //  SERIAL_ECHOPGM(" y_cos="); SERIAL_ECHOLN(y_cos);

    delta[X_AXIS] = x_cos + y_cos + SCARA_offset_x;  //theta
    delta[Y_AXIS] = x_sin + y_sin + SCARA_offset_y;  //theta+phi

    //SERIAL_ECHOPGM(" delta[X_AXIS]="); SERIAL_ECHO(delta[X_AXIS]);
    //SERIAL_ECHOPGM(" delta[Y_AXIS]="); SERIAL_ECHOLN(delta[Y_AXIS]);
}

void calculate_delta(float cartesian[3]){
  //reverse kinematics.
  // Perform reversed kinematics, and place results in delta[3]
  // The maths and first version has been done by QHARLEY . Integrated into masterbranch 06/2014 and slightly restructured by Joachim Cerny in June 2014

  float SCARA_pos[2];
  static float SCARA_C2, SCARA_S2, SCARA_K1, SCARA_K2, SCARA_theta, SCARA_psi;

  SCARA_pos[X_AXIS] = cartesian[X_AXIS] * axis_scaling[X_AXIS] - SCARA_offset_x;  //Translate SCARA to standard X Y
  SCARA_pos[Y_AXIS] = cartesian[Y_AXIS] * axis_scaling[Y_AXIS] - SCARA_offset_y;  // With scaling factor.

  #if (Linkage_1 == Linkage_2)
    SCARA_C2 = ( ( sq(SCARA_pos[X_AXIS]) + sq(SCARA_pos[Y_AXIS]) ) / (2 * (float)L1_2) ) - 1;
  #else
    SCARA_C2 =   ( sq(SCARA_pos[X_AXIS]) + sq(SCARA_pos[Y_AXIS]) - (float)L1_2 - (float)L2_2 ) / 45000;
  #endif

  SCARA_S2 = sqrt( 1 - sq(SCARA_C2) );

  SCARA_K1 = Linkage_1 + Linkage_2 * SCARA_C2;
  SCARA_K2 = Linkage_2 * SCARA_S2;

  SCARA_theta = ( atan2(SCARA_pos[X_AXIS],SCARA_pos[Y_AXIS])-atan2(SCARA_K1, SCARA_K2) ) * -1;
  SCARA_psi   =   atan2(SCARA_S2,SCARA_C2);

  delta[X_AXIS] = SCARA_theta * SCARA_RAD2DEG;  // Multiply by 180/Pi  -  theta is support arm angle
  delta[Y_AXIS] = (SCARA_theta + SCARA_psi) * SCARA_RAD2DEG;  //       -  equal to sub arm angle (inverted motor)
  delta[Z_AXIS] = cartesian[Z_AXIS];

  /*
  SERIAL_ECHOPGM("cartesian x="); SERIAL_ECHO(cartesian[X_AXIS]);
  SERIAL_ECHOPGM(" y="); SERIAL_ECHO(cartesian[Y_AXIS]);
  SERIAL_ECHOPGM(" z="); SERIAL_ECHOLN(cartesian[Z_AXIS]);

  SERIAL_ECHOPGM("scara x="); SERIAL_ECHO(SCARA_pos[X_AXIS]);
  SERIAL_ECHOPGM(" y="); SERIAL_ECHOLN(SCARA_pos[Y_AXIS]);

  SERIAL_ECHOPGM("delta x="); SERIAL_ECHO(delta[X_AXIS]);
  SERIAL_ECHOPGM(" y="); SERIAL_ECHO(delta[Y_AXIS]);
  SERIAL_ECHOPGM(" z="); SERIAL_ECHOLN(delta[Z_AXIS]);

  SERIAL_ECHOPGM("C2="); SERIAL_ECHO(SCARA_C2);
  SERIAL_ECHOPGM(" S2="); SERIAL_ECHO(SCARA_S2);
  SERIAL_ECHOPGM(" Theta="); SERIAL_ECHO(SCARA_theta);
  SERIAL_ECHOPGM(" Psi="); SERIAL_ECHOLN(SCARA_psi);
  SERIAL_ECHOLN(" ");*/
}

#endif

#ifdef TEMP_STAT_LEDS
static bool blue_led = false;
static bool red_led = false;
static uint32_t stat_update = 0;

void handle_status_leds(void) {
  float max_temp = 0.0;
  if(millis() > stat_update) {
    stat_update += 500; // Update every 0.5s
    for (int8_t cur_extruder = 0; cur_extruder < EXTRUDERS; ++cur_extruder) {
       max_temp = max(max_temp, degHotend(cur_extruder));
       max_temp = max(max_temp, degTargetHotend(cur_extruder));
    }
    #if defined(TEMP_BED_PIN) && TEMP_BED_PIN > -1
      max_temp = max(max_temp, degTargetBed());
      max_temp = max(max_temp, degBed());
    #endif
    if((max_temp > 55.0) && (red_led == false)) {
      digitalWrite(STAT_LED_RED, 1);
      digitalWrite(STAT_LED_BLUE, 0);
      red_led = true;
      blue_led = false;
    }
    if((max_temp < 54.0) && (blue_led == false)) {
      digitalWrite(STAT_LED_RED, 0);
      digitalWrite(STAT_LED_BLUE, 1);
      red_led = false;
      blue_led = true;
    }
  }
}
#endif

void manage_inactivity()
{
  if(buflen < (BUFSIZE-1))
    get_command();

  if( (millis() - previous_millis_cmd) >  max_inactive_time )
    if(max_inactive_time)
      kill();
  if(stepper_inactive_time)  {
    if( (millis() - previous_millis_cmd) >  stepper_inactive_time )
    {
      if(blocks_queued() == false) {
        disable_x();
        disable_y();
        disable_z();
        disable_e0();
        disable_e1();
        disable_e2();
      }
    }
  }

  #ifdef CHDK //Check if pin should be set to LOW after M240 set it to HIGH
    if (chdkActive && (millis() - chdkHigh > CHDK_DELAY))
    {
      chdkActive = false;
      WRITE(CHDK, LOW);
    }
  #endif

  #if defined(KILL_PIN) && KILL_PIN > -1
    if( 0 == READ(KILL_PIN) )
      kill();
  #endif
  #if defined(CONTROLLERFAN_PIN) && CONTROLLERFAN_PIN > -1
    controllerFan(); //Check if fan should be turned on to cool stepper drivers down
  #endif
  #ifdef EXTRUDER_RUNOUT_PREVENT
    if( (millis() - previous_millis_cmd) >  EXTRUDER_RUNOUT_SECONDS*1000 )
    if(degHotend(active_extruder)>EXTRUDER_RUNOUT_MINTEMP)
    {
     bool oldstatus=READ(E0_ENABLE_PIN);
     enable_e0();
     float oldepos=current_position[E_AXIS];
     float oldedes=destination[E_AXIS];
     plan_buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS],
                      destination[E_AXIS]+EXTRUDER_RUNOUT_EXTRUDE*EXTRUDER_RUNOUT_ESTEPS/axis_steps_per_unit[E_AXIS],
                      EXTRUDER_RUNOUT_SPEED/60.*EXTRUDER_RUNOUT_ESTEPS/axis_steps_per_unit[E_AXIS], active_extruder);
     current_position[E_AXIS]=oldepos;
     destination[E_AXIS]=oldedes;
     plan_set_e_position(oldepos);
     previous_millis_cmd=millis();
     st_synchronize();
     WRITE(E0_ENABLE_PIN,oldstatus);
    }
  #endif
  #if defined(DUAL_X_CARRIAGE)
    // handle delayed move timeout
    if (delayed_move_time != 0 && (millis() - delayed_move_time) > 1000 && Stopped == false)
    {
      // travel moves have been received so enact them
      delayed_move_time = 0xFFFFFFFFUL; // force moves to be done
      memcpy(destination,current_position,sizeof(destination));
      prepare_move();
    }
  #endif
  #ifdef TEMP_STAT_LEDS
      handle_status_leds();
  #endif
  check_axes_activity();
}

void kill()
{
  cli(); // Stop interrupts
  disable_heater();

  disable_x();
  disable_y();
  disable_z();
  disable_e0();
  disable_e1();
  disable_e2();

#if defined(PS_ON_PIN) && PS_ON_PIN > -1
  pinMode(PS_ON_PIN,INPUT);
#endif
  SERIAL_ERROR_START;
  SERIAL_ERRORLNPGM(MSG_ERR_KILLED);
  LCD_ALERTMESSAGEPGM(MSG_KILLED);
  suicide();
  while(1) { /* Intentionally left empty */ } // Wait for reset
}

void Stop()
{
  disable_heater();
  if(Stopped == false) {
    Stopped = true;
    Stopped_gcode_LastN = gcode_LastN; // Save last g_code for restart
    SERIAL_ERROR_START;
    SERIAL_ERRORLNPGM(MSG_ERR_STOPPED);
    LCD_MESSAGEPGM(MSG_STOPPED);
  }
}

bool IsStopped() { return Stopped; };

#ifdef FAST_PWM_FAN
void setPwmFrequency(uint8_t pin, int val)
{
  val &= 0x07;
  switch(digitalPinToTimer(pin))
  {

    #if defined(TCCR0A)
    case TIMER0A:
    case TIMER0B:
//         TCCR0B &= ~(_BV(CS00) | _BV(CS01) | _BV(CS02));
//         TCCR0B |= val;
         break;
    #endif

    #if defined(TCCR1A)
    case TIMER1A:
    case TIMER1B:
//         TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
//         TCCR1B |= val;
         break;
    #endif

    #if defined(TCCR2)
    case TIMER2:
    case TIMER2:
         TCCR2 &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
         TCCR2 |= val;
         break;
    #endif

    #if defined(TCCR2A)
    case TIMER2A:
    case TIMER2B:
         TCCR2B &= ~(_BV(CS20) | _BV(CS21) | _BV(CS22));
         TCCR2B |= val;
         break;
    #endif

    #if defined(TCCR3A)
    case TIMER3A:
    case TIMER3B:
    case TIMER3C:
         TCCR3B &= ~(_BV(CS30) | _BV(CS31) | _BV(CS32));
         TCCR3B |= val;
         break;
    #endif

    #if defined(TCCR4A)
    case TIMER4A:
    case TIMER4B:
    case TIMER4C:
         TCCR4B &= ~(_BV(CS40) | _BV(CS41) | _BV(CS42));
         TCCR4B |= val;
         break;
   #endif

    #if defined(TCCR5A)
    case TIMER5A:
    case TIMER5B:
    case TIMER5C:
         TCCR5B &= ~(_BV(CS50) | _BV(CS51) | _BV(CS52));
         TCCR5B |= val;
         break;
   #endif

  }
}
#endif //FAST_PWM_FAN

bool setTargetedHotend(int code){
  tmp_extruder = active_extruder;
  if(code_seen('T')) {
    tmp_extruder = code_value();
    if(tmp_extruder >= EXTRUDERS) {
      SERIAL_ECHO_START;
      switch(code){
        case 104:
          SERIAL_ECHO(MSG_M104_INVALID_EXTRUDER);
          break;
        case 105:
          SERIAL_ECHO(MSG_M105_INVALID_EXTRUDER);
          break;
        case 109:
          SERIAL_ECHO(MSG_M109_INVALID_EXTRUDER);
          break;
        case 218:
          SERIAL_ECHO(MSG_M218_INVALID_EXTRUDER);
          break;
        case 221:
          SERIAL_ECHO(MSG_M221_INVALID_EXTRUDER);
          break;
      }
      SERIAL_ECHOLN(tmp_extruder);
      return true;
    }
  }
  return false;
}


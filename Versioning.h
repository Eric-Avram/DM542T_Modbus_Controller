#ifndef Versioning_h
#define Versioning_h
#include "Arduino.h"

#pragma region Version
/******************************************************************************************************************************************
* Overview of version control definition
*     MAJOR -> breaking changes
*     MINOR -> new features, backwards-compatibility, board compatibility
*     PATCH -> bug fixes, minor tweaks, typos, no new features
*
* Versioning start point guidelines:
*     Stable enough to use          -> start at 1.0.0
*     Experimental (testing ideas)  -> start at 0.1.0
*     
* Version increments guidelines
*  PATCH - No new features, nothing that changes user-facing behavior. 
*     Fix a typo in debug messages
*     Correct wrong calculations
*     Fix a bug in control or timings
*     
*  MINOR - Existing code should work exactly as before. On increment reset PATCH to 0.
*     Add a new functionality
*     Add a new sensor
*     Add new macros/debug levels
*     Add an optional function without breaking existing behavior
*
*  MAJOR - Users of the previous version cannot use the new version without adapting their code. On increment reset MINOR & PATCH to 0
*     Change function signature ( void foo (bool execute) -> void foo (int execute_times) )
*     Remove or rename global variables or macros
*     Change hardware interface in a way that existing code must be modified to work
*******************************************************************************************************************************************/
#ifndef VERSION_MAJOR
  #define VERSION_MAJOR 1  
#endif  

#ifndef VERSION_MINOR
  #define VERSION_MINOR 0
#endif

#ifdef VERSION_PATCH
  #define VERSION_PATCH 0
#endif

#define STRINGIFY(x) #x          // Stringify whatever
#define TOSTRING(x) STRINGIFY(x) // Unwrap macro, then stringify its value

#define PROJECT_VERSION TOSTRING(VERSION_MAJOR) "." TOSTRING(VERSION_MINOR) "." TOSTRING(VERSION_PATCH)
#pragma endregion


// Set verbosity level (0 = nothing, 1 = errors only, 2 = errors + warnings, 3 = full debug)

#pragma region Macros

#ifndef VERBOSE_LEVEL
  // Defaults to full debug unless specified otherwise
  #define VERBOSE_LEVEL 3
#endif

// Errors Only
#if VERBOSE_LEVEL >=1 
   #define ERR_PRINT(x)        do { Serial.print("ERROR: ");   Serial.print  (F(x)); } while(0)
   #define ERR_PRINTLN(x)      do { Serial.print("ERROR: ");   Serial.println(F(x)); } while(0)
#else 
   #define ERR_PRINT(x)
   #define ERR_PRINTLN(x)
#endif

// Errors & Warnings
#if VERBOSE_LEVEL >= 2
   #define WARN_PRINT(x)      do {  Serial.print("WARNING: "); Serial.print  (F(x));  } while(0)
   #define WARN_PRINTLN(x)    do {  Serial.print("WARNING: "); Serial.println(F(x));  } while(0)
#else 
   #define WARN_PRINT(x)      
   #define WARN_PRINTLN(x)
#endif

// Full Debug
#if VERBOSE_LEVEL >= 3
   #define LOG_PRINT(x)      Serial.print  (F(x))
   #define LOG_PRINTLN(x)    Serial.println(F(x))
#else
   #define LOG_PRINT(x)      
   #define LOG_PRINTLN(x)    
#endif

#pragma endregion
#endif

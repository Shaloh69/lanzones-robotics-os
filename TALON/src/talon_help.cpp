// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON Help reference (spec 4.1): plain-language explanation + a concrete
// worked example per entry. Branding is the permanent first entry (spec 8).
#include <LzOS.h>
#include <LzUi.h>

#include "build_id.h"

static const LzHelpEntry HELP[] = {
    {"About / Branding",
     "TALON Sumobot OS\n" LZ_FW_VERSION "\n"
     "Owned by\n Team Lanzones\nPartnered by\n Koogs Robotics\n"
     "Build ID:\n " TALON_BUILD_ID},
    {"Navigation Basics",
     "UP/DOWN moves\nbetween items.\nSELECT opens or\nconfirms. BACK\n"
     "exits without\nsaving. Hold\nSELECT or BACK\nfor 1 second to\n"
     "confirm a delete."},
    {"PID Tuning",
     "Kp reacts to the\ncurrent error, Ki\ncorrects long-term\ndrift, Kd "
     "smooths\nsudden changes.\nExample: if your\nbot overshoots the\n"
     "opponent and\nwobbles, lower Kp\nfrom 2.0 to 1.5 or\nraise Kd from "
     "0.1\nto 0.3."},
    {"Strategy & Phases",
     "A Strategy is a\nnamed playbook\nmade of Phases.\nExample strategy\n"
     "vs_HeavyBot:\nPhase 1 = Sweep\n(search) for 2s\nPhase 2 = Curve\n"
     "Attack until\nopponent detected\nPhase 3 = Straight\nRam until edge\n"
     "detected."},
    {"Transition Trigger",
     "Each phase ends\none of three ways:\nafter a fixed\nTime, once the\n"
     "opponent is\nDetected, or once\nan Edge is seen.\nExample: a Sweep\n"
     "phase set to\nUntil-opponent\nkeeps spinning in\nplace until a ToF\n"
     "sensor sees\nsomething in\nrange."},
    {"Give-Up Timer",
     "If your strategy\nis not gaining\nground, this\nsafety timer\nforces "
     "a Retreat\ninstead of\nstalling into a\nloss. Example: set\nto 4.0s - "
     "if 4s\npass in an ATTACK\nphase with no\ndistance decrease\nor push "
     "impact,\nthe bot switches\nto your chosen\nRetreat."},
    {"Sensor Health",
     "Shows live reads\nfor all 7 sensors\n(5 ToF + 2 edge).\nPASS means "
     "the\nsensor responded\nwith a valid value\nthis cycle; FAIL\nmeans it "
     "timed out\nor returned zero -\ncheck wiring\nbefore your match."},
    {"Calibration",
     "Edge Threshold\nWizard: place the\nbot on the white\nline when "
     "asked,\nthen on the dark\nclay; it computes\nthe cutoff.\nExample: "
     "white 850\ndark 120 puts the\nthreshold at ~485.\nToF Zero: confirm\n"
     "accuracy against\na 100mm target."},
    {"Orientation (IMU)",
     "Detects tilt or\nflip. Example:\nwith Auto-Stop on\nFlip enabled, if\n"
     "the bot gets\nflipped in a match\nthe motors cut\nimmediately "
     "instead\nof spinning\nuselessly."},
    {"Motor Test",
     "Jog each motor\nbefore a match to\nconfirm wiring\ndirection.\n"
     "Example: if Left\nForward spins the\nwheel backward,\nswap that "
     "motor's\ntwo leads."},
    {"Profiles",
     "Save named\nsnapshots of your\nfull config.\nExample: save\n"
     "vs_HeavyBot\n(aggressive, high\ngive-up timeout)\nand vs_FastBot\n"
     "(Curve Attack\nfocus, short\ntimeout), then\nLoad whichever\nfits "
     "your next\nopponent."},
    {"Lock Config",
     "Locks all\nConfigure screens\nso a stray press\ncannot change "
     "your\nsettings between\nmatches. Example:\nenable right after\nyour "
     "final practice\nrun, before\nwalking to the\ndohyo."},
};

static LzHelpIndexScreen helpIndex(HELP, sizeof(HELP) / sizeof(HELP[0]));
void openHelpScreen() { OS.push(&helpIndex); }

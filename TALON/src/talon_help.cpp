// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON Help reference (spec 4.1): plain-language explanation + a concrete
// worked example per entry. Branding is the permanent first entry (spec 8).
#include <LzOS.h>
#include <LzUi.h>

#include "build_id.h"

static const LzHelpEntry HELP[] = {
    // branding line as the permanent top row of the index (spec 8)
    {"LANZONES x KOOGS",
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
     "Each phase ends\none of four ways:\nafter a fixed\nTime, once the\n"
     "opponent is\nDetected, once an\nEdge is seen, or\non Contact (the\n"
     "bumper switch).\nExample: a Sweep\nphase set to\nUntil-opponent\n"
     "keeps spinning in\nplace until a ToF\nsensor sees\nsomething in "
     "range."},
    {"Give-Up Timer",
     "If your strategy\nis not gaining\nground, this\nsafety timer\nforces "
     "a Retreat\ninstead of\nstalling into a\nloss. Example: set\nto 4.0s - "
     "if 4s\npass in an ATTACK\nphase with no\ndistance decrease\nor push "
     "impact,\nthe bot switches\nto your chosen\nRetreat."},
    {"Sensor Health",
     "Shows live reads\nfor all 8 sensors\n(5 ToF + 2 edge +\n1 bump). PASS "
     "means\nthe sensor\nresponded with a\nvalid value this\ncycle; FAIL "
     "means it\ntimed out or the\nI2C expander\ndidn't answer -\ncheck "
     "wiring before\nyour match."},
    {"Calibration",
     "Edge Verification\nCheck: place the\nbot on the white\nboundary, "
     "confirm\nboth sensors TRIP;\nthen on dark clay,\nconfirm both "
     "CLEAR.\nIf backwards, flip\nEdge Polarity\n(Active-High/Low).\nThe "
     "actual\nthreshold lives on\nthe module's trim\npot. ToF Zero:\n"
     "confirm accuracy\nagainst a 100mm\ntarget."},
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
     "final practice\nrun, before\nwalking to the\ndohyo. Survives\npower "
     "cycles."},
    {"Traction Control",
     "Detects wheels\nspinning without\nmoving the robot -\nusually a hard\n"
     "push against a\nheavier opponent.\nExample: Response\n= Reduce Power:\n"
     "on slip mid-attack\nthe bot briefly\neases throttle to\nregain grip\n"
     "instead of\ngrinding wheels."},
    {"Ramp-Up/Ramp-Down",
     "Instead of\nsnapping to full\nspeed on attack\n(which spins the\n"
     "wheels on launch),\nRamp-Up builds\nspeed gradually.\nExample: 300ms\n"
     "Ramp-Up on a\nStraight Ram for a\ncleaner, more\ncontrolled charge."},
    {"Search Radius",
     "How tight or wide\nyour search arcs.\nExample: Radius 0\nspins in "
     "place\n(cramped dohyo\ncenter); higher\nradius sweeps a\nwider arc "
     "(better\nfor open ring\nspace). Spin=0 and\nSweep=wide are\npresets "
     "on the\nsame dial."},
    {"Crawl",
     "A slow, cautious\nforward creep -\ndistinct from a\nfull charge.\n"
     "Example: Crawl\nnear a suspected\nboundary to probe\ncarefully, or "
     "to\nbait an opponent\ninto revealing its\nposition before\n"
     "committing."},
    {"Angled Turn",
     "Turns to a precise\nangle instead of a\nfull spin or fixed\n90. "
     "Example: 45\ndeg Clockwise sets\na specific attack\nangle against a\n"
     "partially-detected\nopponent without\novershooting."},
    {"Sensor Ignore Win",
     "After a sharp turn\nsensors can give\nfalse readings\nwhile the "
     "chassis\nrotates. Pick\nwhich sensors are\nmuted for a short\nwindow "
     "at phase\nstart. Example:\nFront ToF only,\n200ms, on an "
     "Angled\nTurn - the front\nis ignored during\nthe turn, angled\n"
     "sensors stay live."},
    {"Edge Escape",
     "Whole-strategy\nsafety: any edge\ntrip interrupts\nthe current "
     "phase\nand runs your\nescape maneuver,\nthen your resume\nchoice. "
     "Example:\nBackup+Turn then\nFall Back to\nSearch. Only a\nphase's "
     "ignore\nwindow with Edge\nsensors listed can\nsuppress it, and\nonly "
     "for that\nwindow."},
    {"Match Timer/Boost",
     "Many rulesets give\nthe win to the\nmost aggressive\nbot if time "
     "runs\nout. Example:\nboost threshold =\nlast 20s - with no\nclear "
     "win by then\nthe bot jumps to\nyour Boost Target\nPhase (e.g. a "
     "max\ncommitment Ram)\ninstead of waiting\nout the clock."},
    {"Quick Rematch",
     "A round often has\nmultiple bouts.\nAfter MATCH OVER,\npress START "
     "on the\nRun screen to\nreset state and\nre-arm the 5s\ncountdown - "
     "no\nmenu backtracking\nbetween bouts."},
    {"Bump Sensor",
     "The front bumper\nmicroswitch\nconfirms actual\nphysical contact,\n"
     "not just nearby\nproximity like the\nToF sensors.\nExample: set a "
     "Ram\nphase's trigger to\nContact instead of\nOpponent so it\ncommits "
     "until it\ntruly connects,\nnot just gets\nclose."},
    {"Strategy Switch",
     "A physical DIP or\nrotary switch lets\nyou pick a saved\nstrategy by "
     "flipping\na switch instead\nof the OLED menu -\nhandy for quick\n"
     "swaps between\nbouts. It only\ntakes effect while\nidle or after a\n"
     "match - never\nmid-fight - and\nonly changes which\nsaved strategy "
     "runs\nnext, not its\nsettings."},
    {"Config Exp/Import",
     "Copies a tuned\nsetup to another\nboard over serial\n(PA9/PA10, "
     "115200)\ninstead of\nre-tuning. Example:\nexport vs_HeavyBot\nfrom "
     "your main bot\nand import it on\nthe backup unit\nfrom the "
     "Profiles\nmenu."},
};

static LzHelpIndexScreen helpIndex(HELP, sizeof(HELP) / sizeof(HELP[0]));
void openHelpScreen() { OS.push(&helpIndex); }

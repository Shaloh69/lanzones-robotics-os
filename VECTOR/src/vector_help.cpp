// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR Help reference (spec 4.2). Branding is the permanent first entry.
#include <LzOS.h>
#include <LzUi.h>

#include "build_id.h"

static const LzHelpEntry HELP[] = {
    {"About / Branding",
     "VECTOR Line\nFollower OS\n" LZ_FW_VERSION "\n"
     "Owned by\n Team Lanzones\nPartnered by\n Koogs Robotics\n"
     "Build ID:\n " VECTOR_BUILD_ID},
    {"Navigation Basics",
     "UP/DOWN moves\nbetween items.\nSELECT opens or\nconfirms. BACK\n"
     "exits without\nsaving. Hold\nSELECT or BACK\nfor 1 second to\n"
     "confirm a delete."},
    {"PID Tuning",
     "Kp reacts to the\ncurrent line-\nposition error, Ki\ncorrects steady\n"
     "drift to one side,\nKd smooths jitter.\nExample: if the\nbot oscillates\n"
     "side-to-side on\nstraights, lower\nKd from 0.5 to\n0.2."},
    {"Line Color Mode",
     "Tells the sensors\nwhether to look\nfor a dark line on\na light surface\n"
     "or the reverse.\nExample: white\ntrack with black\nline = Black-on-\n"
     "white; black track\nwith white line =\nWhite-on-black."},
    {"Path & Learn Mode",
     "Learn Mode drives\nthe maze once and\nrecords each turn\nas F/L/R/U.\n"
     "Example recorded\narray:\nF,R,F,L,U,R,F\nmeaning forward,\nright at "
     "junction\n2, forward, left\nat junction 4,\ndead-end reverse\nat "
     "junction 5,\nright, forward to\nfinish."},
    {"Editing the Array",
     "After Learn Mode,\nopen Review/Edit\nArray to fix any\njunction it "
     "got\nwrong. Example: if\nJunction 3 should\nhave been Right\nbut "
     "was recorded\nas Forward, select\nJunction 3 and\ncycle to R."},
    {"Speed Run",
     "Runs the maze\nagain using your\n(possibly edited)\npath array "
     "instead\nof re-deciding at\neach junction live\n- faster because\n"
     "it is not re-\nevaluating sensors\nat every\nintersection."},
    {"Sensor Health",
     "Bar-graph shows\nall 8 IR sensors\nlive. A sensor\nstuck at 0 or "
     "max\nwhile others vary\nnormally usually\nmeans a bad\nconnection or "
     "a\ndead LED on that\nsensor."},
    {"Calibration",
     "Auto-Cal Wizard:\nthe bot slowly\nsweeps the sensor\narray across "
     "the\nline for about 3\nseconds. Example:\nit records min\n~50 (over "
     "line)\nand max ~900 (over\nbackground), then\nsets the threshold\nat "
     "their midpoint."},
    {"Motor Test",
     "Jog each drive\nmotor before a run\nto confirm wiring\ndirection.\n"
     "Example: if Left\nForward spins the\nwheel backward,\nswap that "
     "motor's\ntwo leads."},
    {"Profiles",
     "Example: save\nTrackA_fast for a\nstraight-heavy\ntrack using "
     "higher\nbase speed, and\nTrackB_technical\nfor tight corners\nusing "
     "a lower\ncornering speed.\nProfiles carry the\npath array too."},
    {"Lock Config",
     "Locks all\nConfigure screens\nso a stray press\ncannot change "
     "your\nsettings between\nruns. Example:\nenable right after\nyour "
     "final practice\nrun, before the\ncompetition heat.\nSurvives power\n"
     "cycles."},
    {"Per-Junction Cfg",
     "Beyond the turn\ndirection, each\njunction can have\nits own "
     "Approach\nSpeed, Brake time,\nTurn Style, Post-\nTurn Speed and\n"
     "Reacq Timeout.\nExample: long\nstraight into a\nsharp corner -\nhigh "
     "approach,\nlong brake, Point-\nTurn, low post-\nturn speed. Unset\n"
     "fields use the\nglobal profile.\nIn the editor:\nSELECT a "
     "junction\nthen START opens\nits config."},
    {"Finish Marker",
     "The finish line\nuses a distinct\npattern - a\nperpendicular\n"
     "DOUBLE line -\ndifferent from a\nnormal junction.\nSeeing both "
     "bars\nin quick\nsuccession stops\nthe robot\nautomatically\ninstead "
     "of\ntreating it as\nanother turn."},
    {"Config Exp/Import",
     "Copies a tuned\nsetup (including\nsaved path arrays)\nto another "
     "board\nover serial\n(PA9/PA10, 115200).\nExample: export\nTrackA_fast "
     "from\nyour main bot and\nimport it on the\nbackup unit from\nthe "
     "Profiles menu."},
};

static LzHelpIndexScreen helpIndex(HELP, sizeof(HELP) / sizeof(HELP[0]));
void openHelpScreen() { OS.push(&helpIndex); }

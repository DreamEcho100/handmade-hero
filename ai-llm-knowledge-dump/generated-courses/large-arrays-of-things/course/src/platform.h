/* ══════════════════════════════════════════════════════════════════════ */
/*               platform.h — Minimal Platform Contract                  */
/*                                                                       */
/*  Defines the interface between game code and platform backends.        */
/*  Game code includes this header. Platform backends implement it.       */
/*                                                                       */
/*  This is a SIMPLIFIED version of the Platform Foundation Course        */
/*  contract — just enough for the LOATs game demo (lessons 11-12).      */
/* ══════════════════════════════════════════════════════════════════════ */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

/* Re-export game types so backends can use them. */
#include "game/game.h"

/* ── Window constants ── */
#ifndef GAME_TITLE
#define GAME_TITLE "Large Arrays of Things — Demo"
#endif

#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif

#endif /* PLATFORM_H */

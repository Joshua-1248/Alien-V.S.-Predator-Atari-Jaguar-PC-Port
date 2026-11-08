# RE #19 R189 — Facehugger Egg type audit

**R188 oversight:** its seven-entry cheat type table omitted the original `AC_EGG` creature (`enum AvpCreatureType`), although eggs are killable level actors and the natural egg death routine exists.

**Original death behavior in `src/game/amp.c`:** `make_dead_egg()` clears the egg collision owner, removes `AMP_KILLABLE`, sets frame 5 and template ID 56 (`DEADEGG`), then makes the object static. The new opt-in cheat uses this routine instead of deleting its AMP slot or using an ordinary Facehugger death animation. It clears the cheat's synthetic hit flag before the routine's score check, so selecting Kill does not create free points. Real weapon deaths still use the original scoring logic.

**F12 UI change:** the type selector now includes `Facehugger Egg` immediately after `Facehugger`; selection wrap advances over eight entries. Neither the menu size nor other index assignments change. The original `Kill All Enemies` chooser uses the same type switch, so eggs are now included there. This does not modify Facehugger attachment prevention, enemy attacks, audio options or ROM data.

**Limits:** GCC/Clang focused test coverage and strict builds passed; no interactive video/device testing of the F12 interface in this pass. No changes to original, unmodified retail `AVP.jag` or inclusion of ROM assets in distribution.

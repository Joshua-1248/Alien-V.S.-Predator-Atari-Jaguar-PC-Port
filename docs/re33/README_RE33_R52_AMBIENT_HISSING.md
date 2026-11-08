# Alien vs. Predator (Atari Jaguar) – RE #33 R52
## PC Audio: independent random ambient hissing

Based on the exact RE #32 R51 complete source. The PC-only `F12 → Audio`
menu now offers:

- **Ambient Hissing Noise Volume:** 0–100%, default **100%**.
- **Ambient Hissing Noises:** **On** (default) / **Off**. Off is a hard
  output mute, not a change to the original sound scheduling.
- The existing **Ambient Noises** On/Off option is now labeled **Ambience**.
  Its separate **Ambience Volume** remains unchanged.

These switches have different jobs. **Ambience** controls the level's continuous
background soundtrack in the reserved ambient channel and existing related
ambient effects. **Ambient Hissing Noises** controls specifically the intermittent
swishing/hissing sounds fired by the original `PLAYER.S::BGSounds` logic:
`AVP_FX_AMB2` and `AVP_FX_AMB3`. The latter run as priority-managed sound
voices, not the persistent ambience loop. Their independent mixer lane also
prevents changing Ambience Volume from changing these random effects.

The original effect selection, random-number sequence, frequency, CPU voice
allocation, sound timing, pitch, and synth descriptors are **not** modified.
New controls scale the *PC output only* after the exact original FullSynth
sample is computed. Silent effects continue progressing and can be unmuted
without restarting. Other SFX, dialogue, music, and main ambience remain
unaffected. The behavior applies to both single-player and local multiplayer.

### Settings compatibility

Saved in `settings.cfg`:

```ini
audio.ambience=true
audio.ambient_hissing_noises=true
audio.ambient_hissing_noise_volume=100
```

The old `audio.ambient_noises` key is still read as the prior *Ambience*
setting for backward compatibility. The new key is written on save.
Absent new keys default to hissing On and 100%. Existing volume and toggle
settings continue to load normally. Both Linux ALSA/PulseAudio and Windows
WinMM invoke the shared FullSynth mixer controls.

### Tests

- `test_pc_audio_mix` with synthetic FullSynth descriptors, new hard-mute,
  zero-volume, nonzero-volume, and bus-independence checks. Both actual
  original random effect addresses are covered; tests contain no retail ROM.
- The optional `new/audio/tests/test_f12_ambient_hissing_x11.py` uses a
  user-owned original ROM and Xvfb/XTest to navigate the actual F12 menu
  and check saved settings and toggle independence without changing or
  distributing ROM bytes.
- All original `src/game/` files are byte-for-byte R51.

### Build/release layout

`LINUX/avp_pc` is the GCC multiplayer-enabled build, and
`LINUX/avp_pc_clang` is the Clang multiplayer-enabled build. Neither release
contains the user's ROM, ROM-derived caches or multiplayer-disabled executables.
Apply the R51 → R52 patch from the root of an untouched R51 `SOURCE/` with
`patch -p1` or use the complete R52 source ZIP.

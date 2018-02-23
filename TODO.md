- Useful default sequences so you can just wire up your lights and do things without needing a programmer around
- Use the remaining pin for
  - beat
  - reset to defaults (long hold)
    - flash lights during long hold to indicate what's going to be done
- Save sequences to EEPROM or whatever
- Add per-channel control mode:
  - sequenced
  - triggered envelopes
  - on/off
- Allow direct triggering via OSC over UDP
- Auto-documentation about forth commands

## Done
* Broadcast presence on LAN
  - Since we're just doing FORTH packets
    send one out like "# Hi I'm so-and-so!"

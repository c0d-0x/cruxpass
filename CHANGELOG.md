# Changelog
## [v1.2.1](https://github.com/c0d-0x/cruxpass/releases/tag/v1.2.1)

- Public header and API revisions:
    - Description length increased: 100 → 255
    - Master password length increased: 45 → 48
    - New limit added: generated secrets up to 256 characters

- Storage vs generation size implication:
  - Generator now supports up to 256 characters while stored secrets remain limited to 128 (SECRET_MAX_LEN). Ensure storage schema or workflow is updated if you plan to store >128 characters.

### Features
- TUI improvements and helpers:
  - View long descriptions in a dedicated viewer.
  - Preview generated secrets and save them to the DB from the TUI.
  - Numeric input helper for TUI prompts.
  - TUI helper to generate random secrets and present the result.
- New TUI actions and keybindings:
  - `L` — open full description viewer for selected record.
  - `r` + generation keys — generate secrets from TUI:
    - `ra` — lowercase
    - `rA` — uppercase
    - `rp` — digits (PIN)
    - `rr` — lowercase+uppercase+digits+symbols
    - `rx` — all characters excluding ambiguous ones
  - `Ctrl+r` — reload TUI (useful after saving a secret externally or from the TUI).

### Changed
- TUI layout and behavior:
  - Table column labels updated (e.g., “E-MAIL/USERNAME”).
  - Search behavior now ignores DELETED entries when building search results.
- Input validation and helpers:
  - Stronger input validation (IS_VALID / IS_DIGIT macros) and corrected key/event handling in `get_input()`/`get_secret()`.
  - Safer and clearer input flows in the TUI (backspace, enter, ESC, resize handling).

- TUI/Rendering bugs:
  - Fixed border drawing and table redrawing glitches.
  - Resolved incorrect cell positioning and off-by-one formatting in several TUI functions.
  - Fixed behavior for deleted-record notifications.
- Event loop correctness:
  - Stabilized event polling loops to avoid missed or dropped events and to properly handle resize events.

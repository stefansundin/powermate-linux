## Dependencies

```
pip3 install pygatt
```

## Run

Run as root to scan and find your PowerMate address:
```
sudo ./powermate-poc.py
```

Then update `powermate-poc.py` with the address and run without root.

LED values behavior:
- 00 to 7F: ?
- 80: completely off
- 81: completely on
- 82 to 9F: no change?
- A0: quick blink then low intensity
- A1 to BF: from low to high intensity
- C0 to FF: blinking at different speeds

## Misc

Use `gatttool` to explore and send commands interactively:
```
gatttool -b 00:12:34:56:78:90 -I
```

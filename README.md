
# Steam AppID patcher

SM extension to patch engine to let standalone CS:GO (4465480) players join a standard AppID 730/(740) server.

Recreated since no public source existed.

## IMPORTANT

USE AT YOUR OWN RISK!

Notice that using this will let players with VAC/Game bans on CS:GO/CS2 (730) join your server, unless you add extra checks via something else!

Users joining with standalone CS:GO will most likely not trigger steamauth properly.

**Usage is not recommended, as this allows for misuse and impersonation on servers.*

## Better solution

It is better to instruct users to run csgo.exe externally (with `-steam`) from Steam or as a "non-Steam game", to get it to run as AppID 730.

Or to just use the `csgo_legacy` beta on cs2...

## Building

### AMBuild

```py
mkdir build && cd build

python ../configure.py --sm-path ../sourcemod --mms-path ../metamod-source --targets x86

ambuild
```

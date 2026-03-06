
# Steam AppID patcher

SM extension to patch engine to let standalone CS:GO (4465480) players join a standard AppID 730/(740) server.

## IMPORTANT

Notice that using this will let players with VAC/Game bans on CS:GO/CS2 (730) join your server, unless you add extra checks via something else!

Users joining with standalone CS:GO may not trigger steamauth properly.

**Usage is not recommended, as this allows for misuse and impersonation on servers.*

## Building

### AMBuild

```py
mkdir build && cd build

python ../configure.py --sm-path ../sourcemod --mms-path ../metamod-source --targets x86

ambuild
```

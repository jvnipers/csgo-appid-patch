
# Steam AppID patcher

SM extension to patch engine to let standalone CS:GO (4465480) players join a standard AppID 730 server.

Linux only.

## Building

### AMBuild

```py
mkdir build && cd build

python ../configure.py --sm-path ../sourcemod --mm-path ../metamod-source --targets x86

ambuild
```

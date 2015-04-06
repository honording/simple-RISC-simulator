# simple-RISC-simulator
A simple RISC cycle-accurate simulator.
## Features
- Cycle-accurate simulator.
- 8 pipeline stages: **IF1**, **IF2**, **ID**, **EXE**, **MEM1**, **MEM2**, **MEM3**, **WB**. In fact this 8-stage is nothing different with classic 5-stage one. Extra stages do nothing but delay cycles.
- 5 instructions: **DADD**, **SUB**, **LD**, **SD**, **BNEZ**.
- Addressing methods: immediate, register, and indirect.
- Support forwarding.

## Compile
``` shell
g++ -o simulator main.cpp
```
### Usage
``` shell
./simulator
```
It looks stupid to ask user to give asm files in an interactive way. Feel shamed about that.:) 


Happy hacking.
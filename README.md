# lora_rover_basestation

The lora_rover basestation communicates via lora with the rover and forwards incoming data from the rover to websocket clients.
Websockets can be connected to via port 9002, every incoming data will be forwarded to the rover.

## Installation
To compile the project run <br>
`bash rebuild.sh` <br>
rebuild.sh compiles and runs the main executables


## Executables

- main:
main data forwarding node

- command prompt
node to directly communicate with the lora rover

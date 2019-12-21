# LoRa Rover Basestation

The lora_rover basestation communicates via lora with the [lora rover](https://github.com/cy8berpunk/lora_rover_) and forwards incoming data from the rover to websocket clients. Websockets can be connected to via port 9002, all incoming data sent to the rover will be forwarded to the rover.
The command line executable offers a direct interface to the rover and is mostly used for purposes for debugging purposes.

## Installation
To compile the project run <br>
`bash rebuild.sh` <br>
rebuild.sh compiles and runs the main executables

## Executables

- main:
main data forwarding node

- command prompt
node to directly communicate with the lora rover

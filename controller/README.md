# Waschen in Freiheit - Management Interface

This is the Waschmaschinen management interface implemented in python.
It takes care in setting up the network, monitoring the network, and, if
neccessary, recover it in order to keep all nodes running.

It is designed to connect directly via a serial interface to the master node.

## Getting started

Create a virtual environment and install the requirements into it.

```
python3 -m venv venv
. ./venv/bin/activate
pip install -r requirements.txt
```

Now set up the nodes as they are supposed to work within the json config.
The management node can now be started with: (set the config file and the
serial connection according to your local setup!)

```
python3 main.py --config ./nodes.json --serial /dev/ttyUSB0
```

Now the interface is booting up. Have fun!

## Testing

You do not have a real master at hand? Therefore we have a very simplified
tester that reacts like the real thing.
It exposes a serial interface via socat, feeding in one of three test cases:

- Everything is fine, your network will *just* work, always, be happy!
- Node 2 is broken! Oh noes! It will never respond to you.
- You network is really bad. 80% of all your messages will timeout. Deal wit it

These have proven quite valuable to evaluate network performance and possible
deadlocks before the real hardware (network setup time: 0.0016s vs. 40s)

## Configuring

The configuration is split into two types of files: the main config and the
sensor specifications.

### Main Config

Follow this example config:

```javascript
{
  // Number of times a retransmit should be tried before a node is assumed dead,
  // and a network recovery is started
  "retransmissionlimit": 3,
  // Time in seconds, how often a network check should be performed
  "networkcheckintervall": 600,
  // How much time does one step in the network take, at max (in seconds)
  "single_hop_timeout": 2,

  // Configuring the different nodes
  "nodes": [
    // Every setup _needs_ a node named 'MASTER', it will be used as entry point
    // into the route resolution.
    {
      // The ID given here has to match the hardware-id of the actual node!
      "id": 0,
      // This name is used for reference later
      "name":"MASTER",
      // What sensors are connected (not used on the master node)
      "channels": [],
      // Sample rate of the sensors (not used on the master node)
      "samplerate": 0,
      // How to reach which other node.
      "routes":{
        "HOUSEA-ROOM1":"HOUSEA-ROOM1",
      }
    },
    {
      // The ID given here has to match the hardware-id of the actual node!
      "id": 1,
      // This name is used for reference later (see it in the master-routing?)
      "name":"HOUSEA-ROOM1",
      // Samples taken for the connected sensors per second
      "samplerate": 500,
      // Which sensors are connected?
      "channels": [
        {
          // Channel Number 0 is always the left plug
          "number": 0,
          "name":"left",
          "model":"A",
          // additional sensor data for configuring the sensor
          "config": "sensor_normal.json"
        },
        {
          "number": 1,
          "name":"right",
          "model":"B",
          "config": "sensor_normal.json"
        }
      ],
      // How to reach back to the master
      "routes":{
        "MASTER":"MASTER",
      }
    },
  ]
}
```

### Sensor Config

The sensor configuration file describes the details coming from sensor and
machine.

```javascript
{
	"input_filter":
	{
		"mid_adjustment_speed": 20,
		"lowpass_weight": 50,
		"frame_size": 100
	},
	"transition_matrix": [
		[   0,    0,   80,    0],
		[ -24,    0,    0,  160],
		[ -48,    0,    0,  160],
		[   0,  -80,    0,    0]
	],
	"window_sizes": [
		150,
		1500,
		1500,
		1500
	],
	"reject_filter": {
		"threshold": 104,
		"consec_count": 15
	}
}
```

These need to be adjusted for any machine and sensor connected.

## Routing

The most important configuration to reach all nodes is routing.

Every message is routed independently, so the sending node has to know via
which gateway it can reach which node.

If you have a setup like this in your building, and you can only reach from ones
adjacent.

```
| - - - - |
| OG 30   |
| OG 20   |
| OG 10   |
| MASTER  |
| - - - - |
```

You have to configure for the master

```
{
  'OG 30':'OG 10',
  'OG 20':'OG 10',
  'OG 10':'OG 10'
}
```

that way you tell him, it can reach every room via `OG 10`.

If you did some tests, and fortunately can reach nodes `OG 10` and `OG 20` from master,
then you configure:

```
{
  'OG 30':'OG 20',
  'OG 20':'OG 20',
  'OG 10':'OG 10'
}
```

## Failure States

It is normal, that from time to time a message does not reach its destination
and the system therefore logs a lost message. thats why we do try it again.

If a node does not respond to multiple retries, we start reinitializing it
in the hope, that it just has had a hickup (like some power-outages).

If that does not succeed, we issue an error - because then it is most likely
that the node has died, or something is not fine with the RF.

In the case of multiple failures, always check in order of distance (in terms of
routes from the master), because nodes that live behind a gateway cannot be
reached, if the gateway is broken.

Since the nodes themselve assume they are dead or have forgotten their routes,
if they do not receive messages once in a while, the network should mostly be
self-healing.
Only if a node continues to be dead, a manual restart or replacement might be
required.

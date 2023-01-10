# ZeroMQ Plugin for SigDigger

This repository contains the code of the ZeroMQ plugin for SigDigger. This plugin enables audio and raw data forwarding from several channels to 3rd party apps like [JAERO](https://github.com/jontio/JAERO) via ZeroMQ. Although development has been focused on JAERO (and it is the only application known to work with this plugin), support for additional applications is in the TODO.

The plugin enables channel definition with the mouse, pretty much like how you would do with SigDigger's filterbox. This is basically how you would use it:

* Start by defining a **Master channel**. This is basically a wide channel that covers all the frequencies of the channels you want to forward, and performs an initial decimation of the SDR rate.
* Define several **VFO Channels**. These are the channels you want to forward via ZeroMQ. You need to provide some extra settings, like modulation of the audio channel or sample rate (usually USB + certain integer decimation).
* Click **Start Publishing**. If you got no errors, congratulations, samples are being forwarded to a ZeroMQ topic with the name of the channel.
* Open JAERO and open its settings dialog. Enable the ZMQ Audio as the Audio source, with a topic that matches the name of the channel you want to retrieve samples from.
* Congratulations! That's pretty much it.


### Building
This plugin relies on SigDigger's plugin API, which is only known to work in GNU/Linux. While support for additional OSes is certainly possible, GNU/Linux is the only OS I have at hand (of course, if you have macOS and are good at C/C++, you are more than welcome to help! *wink wink*)

This plugin needs **the latest changes in the `develop` branch** of [sigutils](https://github.com/BatchDrake/sigutils/tree/develop), [suscan](https://github.com/BatchDrake/suscan/tree/develop), [SuWidgets](https://github.com/BatchDrake/SuWidgets/tree/develop) and [SigDigger](https://github.com/BatchDrake/SigDigger/tree/develop), and can be built both with Qt5 and Qt6. You will also need [**cppzmq**](https://github.com/zeromq/cppzmq) development files (which you could also find in the official package repos of the major GNU/Linux distributions with names like `cppzmq-dev`).

Open a terminal, and run:
```
% qmake ZeroMQPlugin.pro
% make
% make install
```

Make sure that you run `make install` as **regular user** (do NOT run ~~sudo make install~~). This will copy the plugin files to SigDigger's plugin folder (usually in `$HOME/.suscan/plugins`).
